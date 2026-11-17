/* ============================================================================
 * log.cpp — 日志：%LOCALAPPDATA%\HuaweiWmiControl\hwc.log
 * ----------------------------------------------------------------------------
 * 设计要点：
 *   1. 文件以 UTF-16LE（带 BOM）追加写入 —— 不走 CRT 的 narrow 转换，
 *      因此中文不受系统区域设置影响；Notepad / VS Code 都能直接看。
 *   2. 写入后立即 fflush，同时 OutputDebugStringW，便于调试器实时观察。
 *   3. 超过 512KB 轮转为 hwc.log.old。
 *   4. 用 CRITICAL_SECTION 串行化；锁内不调用任何 hw_log*（避免自锁死）。
 *   5. 任何失败都静默降级为「仅调试输出」：日志不能成为主流程的故障点。
 * ========================================================================== */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

/* 允许 _wfopen / _snwprintf_s 等（与工程级定义重复也无害）。 */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>   /* ShellExecuteW（hw_log_open_in_shell）；漏了会报 C3861 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "hwapi.h"

/* ------------------------------------------------------------------ 常量 */

#define HWC_LOG_DIR      L"HuaweiWmiControl"
#define HWC_LOG_FILE     L"hwc.log"
#define HWC_LOG_OLD      L"hwc.log.old"
#define HWC_LOG_MAX_SIZE (512u * 1024u) /* 512KB 轮转阈值 */
#define HWC_LOG_BODY_MAX 2048           /* 单条消息正文上限（宽字符） */
#define HWC_LOG_LINE_MAX 2100           /* 正文 + 时间戳 + CRLF */

/* ------------------------------------------------------------------ 状态 */

static CRITICAL_SECTION g_cs;
static int              g_lock_ok = 0;   /* 1 = g_cs 已初始化 */
static volatile LONG    g_cs_state = 0;  /* 0=未初始化 1=初始化中 2=就绪 */

static int                g_inited = 0;
static FILE*              g_fp = NULL;
static unsigned long long g_written = 0; /* 文件当前字节数（含 BOM） */
static WCHAR              g_dir[MAX_PATH] = { 0 };   /* ...\HuaweiWmiControl */
static WCHAR              g_path[MAX_PATH] = { 0 };  /* 完整日志文件路径 */

/* ------------------------------------------------------------------ 工具 */

/* 一次性初始化临界区（多线程安全；不抛异常）。 */
static void hwc_ensure_lock(void)
{
    LONG prev = InterlockedCompareExchange(&g_cs_state, 1, 0);

    if (prev == 0) {
        InitializeCriticalSection(&g_cs);
        g_lock_ok = 1;
        InterlockedExchange(&g_cs_state, 2);
        return;
    }
    /* 其他线程正在初始化（1）或已完成（2）：等到 2。 */
    while (InterlockedCompareExchange(&g_cs_state, 2, 2) != 2) {
        Sleep(0);
    }
    g_lock_ok = 1;
}

/* 有界追加，返回新的结束偏移。 */
static size_t hwc_append(WCHAR* dst, size_t cap, size_t off, const WCHAR* src)
{
    if (!dst || cap == 0) return 0;
    if (off >= cap) return cap - 1;
    while (src && *src != L'\0' && off + 1 < cap) dst[off++] = *src++;
    dst[off] = L'\0';
    return off;
}

/* "[HH:mm:ss] "，本地时间。 */
static void hwc_make_stamp(WCHAR* dst, size_t cap)
{
    SYSTEMTIME st;

    if (!dst || cap < 12) {
        if (dst && cap) dst[0] = L'\0';
        return;
    }

    GetLocalTime(&st);
    dst[0] = L'\0';
    (void)_snwprintf_s(dst, cap, _TRUNCATE, L"[%02d:%02d:%02d] ",
                       (int)st.wHour, (int)st.wMinute, (int)st.wSecond);
    dst[cap - 1] = L'\0';
}

/* 已存在文件的字节数（不存在则为 0）。 */
static unsigned long long hwc_file_size(const WCHAR* path)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;

    if (!path || path[0] == L'\0') return 0;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad)) return 0;
    return ((unsigned long long)fad.nFileSizeHigh << 32) |
           (unsigned long long)fad.nFileSizeLow;
}

/* --------------------------------------------------- 路径（需持有锁） */

static int hwc_build_dir(void)
{
    WCHAR base[MAX_PATH];

    if (g_dir[0] != L'\0') return 1;

    base[0] = L'\0';
    if (FAILED(SHGetFolderPathW(NULL,
                                CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE,
                                NULL, SHGFP_TYPE_CURRENT, base)))
        return 0;
    if (base[0] == L'\0') return 0;

    g_dir[0] = L'\0';
    (void)hwc_append(g_dir, _countof(g_dir), 0, base);
    (void)hwc_append(g_dir, _countof(g_dir), wcslen(g_dir),
                     L"\\" HWC_LOG_DIR);

    if (!CreateDirectoryW(g_dir, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            g_dir[0] = L'\0';
            return 0;
        }
    }
    return 1;
}

static int hwc_build_path(void)
{
    if (g_path[0] != L'\0') return 1;
    if (!hwc_build_dir()) return 0;

    g_path[0] = L'\0';
    (void)hwc_append(g_path, _countof(g_path), 0, g_dir);
    (void)hwc_append(g_path, _countof(g_path), wcslen(g_path),
                     L"\\" HWC_LOG_FILE);

    return (g_path[0] != L'\0') ? 1 : 0;
}

/* ------------------------------------------------ 文件打开/轮转（需持锁） */

static void hwc_close_locked(void)
{
    if (g_fp) {
        fclose(g_fp);
        g_fp = NULL;
    }
}

/* 以二进制追加方式打开；自行写 UTF-16LE 字节，规避 CRT 区域设置转换。 */
static int hwc_open_locked(void)
{
    g_fp = NULL;
    if (g_path[0] == L'\0') return 0;

    g_fp = _wfopen(g_path, L"ab");
    if (!g_fp) return 0;

    if (g_written == 0) {
        static const unsigned char kBom[2] = { 0xFF, 0xFE };
        if (fwrite(kBom, 1, 2, g_fp) == 2) g_written = 2;
        fflush(g_fp);
    }
    return 1;
}

static void hwc_rotate_locked(void)
{
    WCHAR old_path[MAX_PATH];

    hwc_close_locked();

    old_path[0] = L'\0';
    (void)hwc_append(old_path, _countof(old_path), 0, g_dir);
    (void)hwc_append(old_path, _countof(old_path), wcslen(old_path),
                     L"\\" HWC_LOG_OLD);
    (void)MoveFileExW(g_path, old_path, MOVEFILE_REPLACE_EXISTING);

    g_written = 0;
}

static void hwc_write_locked(const WCHAR* line, size_t len)
{
    if (!g_fp || !line || len == 0) return;

    if (g_written > (unsigned long long)HWC_LOG_MAX_SIZE) {
        hwc_rotate_locked();
        if (!hwc_open_locked()) return;
    }

    if (fwrite(line, sizeof(WCHAR), len, g_fp) == len) {
        g_written += (unsigned long long)len * sizeof(WCHAR);
        fflush(g_fp);
    } else {
        /* 磁盘满 / 文件被删等：关闭文件，降级为仅调试输出 */
        hwc_close_locked();
    }
}

/* ==========================================================================
 * 对外 API
 * ========================================================================== */

const WCHAR* hw_log_init(void)
{
    int announce = 0;

    hwc_ensure_lock();
    if (!g_lock_ok) return g_path;

    EnterCriticalSection(&g_cs);
    if (!g_inited) {
        g_inited = 1;
        if (hwc_build_path()) {
            g_written = hwc_file_size(g_path);
            if (g_written > (unsigned long long)HWC_LOG_MAX_SIZE) {
                hwc_rotate_locked();
                g_written = 0;
            }
            if (hwc_open_locked()) announce = 1;
        }
    }
    LeaveCriticalSection(&g_cs);

    /* 会话分隔线写在锁外（hw_log 自己会加锁，锁内调用会自锁死）。 */
    if (announce) {
        SYSTEMTIME st;
        WCHAR banner[HWC_LOG_BODY_MAX];
        GetLocalTime(&st);
        banner[0] = L'\0';
        (void)_snwprintf_s(banner, _countof(banner), _TRUNCATE,
                           L"=== 会话开始 %04d-%02d-%02d %02d:%02d:%02d ===",
                           (int)st.wYear, (int)st.wMonth, (int)st.wDay,
                           (int)st.wHour, (int)st.wMinute, (int)st.wSecond);
        banner[_countof(banner) - 1] = L'\0';
        hw_log(L"%s", banner);
    }

    return g_path;
}

void hw_log_shutdown(void)
{
    if (!g_lock_ok) return;

    hw_log(L"=== 会话结束 ===");

    EnterCriticalSection(&g_cs);
    hwc_close_locked();
    g_inited = 0; /* 允许之后重新 hw_log_init 打开新文件 */
    LeaveCriticalSection(&g_cs);
}

void hw_log(const WCHAR* fmt, ...)
{
    WCHAR body[HWC_LOG_BODY_MAX];
    WCHAR stamp[16];
    WCHAR line[HWC_LOG_LINE_MAX];
    va_list ap;
    size_t len;

    if (!fmt) return;

    body[0] = L'\0';
    va_start(ap, fmt);
    /* _vsnwprintf_s + _TRUNCATE：超长时安全截断，不会触发无效参数处理器 */
    (void)_vsnwprintf_s(body, _countof(body), _TRUNCATE, fmt, ap);
    va_end(ap);
    body[_countof(body) - 1] = L'\0';

    hwc_make_stamp(stamp, _countof(stamp));

    line[0] = L'\0';
    (void)hwc_append(line, _countof(line), 0, stamp);
    (void)hwc_append(line, _countof(line), wcslen(line), body);
    (void)hwc_append(line, _countof(line), wcslen(line), L"\r\n");
    len = wcslen(line);

    OutputDebugStringW(line);

    hwc_ensure_lock();
    if (!g_lock_ok) return;

    EnterCriticalSection(&g_cs);
    hwc_write_locked(line, len);
    LeaveCriticalSection(&g_cs);
}

void hw_log_result(const WCHAR* action, HwStatus st)
{
    const WCHAR* text = hw_status_text(st);

    hw_log(L"%s：%s",
           (action && action[0] != L'\0') ? action : L"操作",
           (text && text[0] != L'\0') ? text : L"未知错误");
}

int hw_log_open_in_shell(void)
{
    WCHAR path[MAX_PATH];
    HINSTANCE h;

    hwc_ensure_lock();
    if (!g_lock_ok) return 0;

    path[0] = L'\0';
    EnterCriticalSection(&g_cs);
    if (g_path[0] != L'\0') {
        (void)wcsncpy_s(path, _countof(path), g_path, _TRUNCATE);
    }
    LeaveCriticalSection(&g_cs);

    if (path[0] == L'\0') return 0;

    h = ShellExecuteW(NULL, L"open", path, NULL, NULL, SW_SHOWNORMAL);
    return ((INT_PTR)h > 32) ? 1 : 0;
}
