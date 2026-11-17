/* ============================================================================
 * main.cpp — 入口、隐藏顶层窗口、消息循环、状态缓存与命令分发
 * ----------------------------------------------------------------------------
 * 设计要点：
 *   1. 没有任何窗体。只创建一个「隐藏的普通顶层窗口」用于接收托盘回调。
 *      绝不能用 HWND_MESSAGE：消息窗口收不到 TaskbarCreated 广播，
 *      Explorer 重启后托盘图标就再也回不来了。
 *   2. 菜单渲染只读 AppState 缓存，弹出即时；只有「刷新状态」与 30 秒
 *      传感器定时器会真正访问固件。
 *   3. 硬件访问一律通过 hwapi.h 的 hw_* 函数，不直接碰 COM/WMI、不自定义协议。
 *   4. 全 UTF-16 宽字符串，与 /utf-8 编译选项配合。
 * ========================================================================== */
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <strsafe.h>

#include "hwapi.h"
#include "tray.h"
#include "menu.h"
#include "icon.h"

/* 私有消息：第二个实例被启动时，请求已有实例弹出菜单。
 * 取 WM_APP+2，避开托盘回调 WM_APP+1。 */
enum { HWM_SHOW_TRAY_MENU = WM_APP + 2 };

static const WCHAR kWindowClass[]     = L"HuaweiWmiControlNativeTrayWnd";
static const WCHAR kMutexNameGlobal[] = L"Global\\HuaweiWmiControlNative";
static const WCHAR kMutexNameLocal[]  = L"Local\\HuaweiWmiControlNative";
static const WCHAR kAppTitle[]        = L"华为笔记本调控工具";

static const UINT_PTR kSensorTimerId = 1;
static const UINT     kSensorTimerMs = 30000;   /* 30 秒 */

/* ------------------------------------------------------------------ 全局 --- */
static AppState g_state;                 /* 唯一的硬件状态缓存（见 menu.h） */
static HICON    g_trayIcon      = NULL;
static int      g_trayIconOwned = 0;     /* 1 = 本进程创建，退出时需 DestroyIcon */
static UINT     g_taskbarCreated = 0;    /* RegisterWindowMessage 的结果 */
static int      g_menuOpen      = 0;     /* 菜单弹出期间跳过定时器刷新 */
static DWORD    g_lastDblClkTick = 0;    /* 抑制双击后紧跟着的那次单击弹菜单 */

/* -------------------------------------------------- 前向声明（本文件内） --- */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void ShowTrayMenu(HWND hwnd);
static void DispatchCommand(HWND hwnd, int cmd);
static void RefreshAll(void);
static void UpdateTrayTip(void);
static void BuildTipText(WCHAR* buf, size_t cch);
static void OpenLogFile(HWND hwnd);
static void ShowAbout(HWND hwnd);

/* ============================================================================
 * 传感器表项数量
 * ----------------------------------------------------------------------------
 * kHwTempZoneCount 是 extern const int（非常量表达式），因此数组维度用
 * APP_MAX_TEMP_ZONES，运行期再夹一次。
 * ========================================================================== */
static int TempZoneCount(void)
{
    int n = kHwTempZoneCount;
    if (n < 0) n = 0;
    if (n > APP_MAX_TEMP_ZONES) n = APP_MAX_TEMP_ZONES;
    return n;
}

/* ============================================================================
 * 缓存刷新
 * ----------------------------------------------------------------------------
 * 约定（与需求一致）：
 *   - HW_OK                -> 标记可用并更新缓存值；
 *   - HW_ERR_NOT_SUPPORTED -> 标记不可用（菜单置灰），属预期情况，不写日志刷屏；
 *   - 其他错误             -> 只写日志，保留旧缓存（可能是临时 COM 故障）。
 * ========================================================================== */
void AppProbeBattery(AppState* st)
{
    int start = 0;
    int end = 0;
    HwStatus s = hw_battery_get(&st->dev, &start, &end);

    if (s == HW_OK) {
        st->avail_battery = 1;
        st->battery_start = start;
        st->battery_end = end;
    } else if (s == HW_ERR_NOT_SUPPORTED) {
        st->avail_battery = 0;
        st->battery_start = 0;
        st->battery_end = 0;
    } else {
        hw_log(L"电池保护：读取失败（%s）", hw_status_text(s));
    }
}

void AppProbeFnLock(AppState* st)
{
    int on = 0;
    HwStatus s = hw_fnlock_get(&st->dev, &on);

    if (s == HW_OK) {
        st->avail_fnlock = 1;
        st->fnlock_on = on ? 1 : 0;
    } else if (s == HW_ERR_NOT_SUPPORTED) {
        st->avail_fnlock = 0;
    } else {
        hw_log(L"Fn-Lock：读取失败（%s）", hw_status_text(s));
    }
}

void AppProbeKbd(AppState* st)
{
    int level = 0;
    HwStatus s = hw_kbd_get_level(&st->dev, &level);

    if (s == HW_OK) {
        st->avail_kbd = 1;
        /* 读得到不代表写得了：先乐观置 1，首次写失败时由 menu.cpp 置 0。 */
        st->kbd_level_rw = 1;
        if (level < 0) level = 0;
        if (level > 2) level = 2;
        st->kbd_level = level;
    } else if (s == HW_ERR_NOT_SUPPORTED) {
        st->avail_kbd = 0;
    } else {
        hw_log(L"键盘背光：读取级别失败（%s）", hw_status_text(s));
    }
}

void AppProbeKbdTimeout(AppState* st)
{
    int sec = 0;
    HwStatus s = hw_kbd_get_timeout(&st->dev, &sec);

    if (s == HW_OK) {
        st->kbd_timeout_ok = 1;
        st->kbd_timeout = sec;
    } else if (s == HW_ERR_NOT_SUPPORTED) {
        st->kbd_timeout_ok = 0;
    } else {
        hw_log(L"键盘背光：读取熄灭超时失败（%s）", hw_status_text(s));
    }
}

void AppProbeSmartCharge(AppState* st)
{
    int mode = 0;
    int unknown = 0;
    int start = 0;
    int end = 0;
    int param = 0;
    HwStatus s;
    HwStatus sp;

    s = hw_smartcharge_get(&st->dev, &mode, &unknown, &start, &end);
    if (s == HW_OK) {
        st->avail_smartcharge = 1;
        st->charge_mode = mode;
        st->charge_unknown = unknown;
        st->charge_start = start;
        st->charge_end = end;
    } else if (s == HW_ERR_NOT_SUPPORTED) {
        st->avail_smartcharge = 0;
    } else {
        hw_log(L"智能充电：读取模式失败（%s）", hw_status_text(s));
    }

    sp = hw_smartcharge_get_param(&st->dev, &param);
    if (sp == HW_OK) {
        st->charge_param = param;
    } else if (sp != HW_ERR_NOT_SUPPORTED) {
        hw_log(L"智能充电：读取参数失败（%s）", hw_status_text(sp));
    }
}

void AppProbePowerUnlock(AppState* st)
{
    int on = 0;
    HwStatus s = hw_powerunlock_get(&st->dev, &on);

    if (s == HW_OK) {
        st->avail_powerunlock = 1;
        st->powerunlock_on = on ? 1 : 0;
    } else if (s == HW_ERR_NOT_SUPPORTED) {
        st->avail_powerunlock = 0;
    } else {
        hw_log(L"电源解锁：读取失败（%s）", hw_status_text(s));
    }
}

void AppProbeSensors(AppState* st)
{
    int i;

    if (st->temp_count <= 0) st->temp_count = TempZoneCount();

    for (i = 0; i < APP_FAN_COUNT; ++i) {
        int rpm = 0;
        HwStatus s = hw_fan_speed(&st->dev, i, &rpm);
        if (s == HW_OK) {
            st->fan_ok[i] = 1;
            st->fan_rpm[i] = rpm;
        } else {
            st->fan_ok[i] = 0;
            if (s != HW_ERR_NOT_SUPPORTED)
                hw_log(L"传感器：风扇 %d 读取失败（%s）", i + 1, hw_status_text(s));
        }
    }

    for (i = 0; i < st->temp_count && i < APP_MAX_TEMP_ZONES; ++i) {
        int celsius = 0;
        HwStatus s = hw_temp(&st->dev, kHwTempZones[i].idx, &celsius);
        if (s == HW_OK) {
            st->temp_ok[i] = 1;
            st->temp_celsius[i] = celsius;
        } else {
            st->temp_ok[i] = 0;
            if (s != HW_ERR_NOT_SUPPORTED)
                hw_log(L"传感器：%s 读取失败（%s）", kHwTempZones[i].label,
                       hw_status_text(s));
        }
    }
}

void AppProbeAll(AppState* st)
{
    st->temp_count = TempZoneCount();

    if (!st->dev.available) {
        /* 设备没打开：不要把 hw_* 打到空句柄上，直接把所有功能标灰。 */
        st->avail_battery = 0;
        st->avail_fnlock = 0;
        st->avail_kbd = 0;
        st->kbd_timeout_ok = 0;
        st->avail_smartcharge = 0;
        st->avail_powerunlock = 0;
        st->avail_micled = 0;
        return;
    }

    AppProbeBattery(st);
    AppProbeFnLock(st);
    AppProbeKbd(st);
    AppProbeKbdTimeout(st);
    AppProbeSmartCharge(st);
    AppProbePowerUnlock(st);
    AppProbeSensors(st);

    /* 麦克风 LED 只有 SET 没有 GET，无法探测：
     * 设备在线时乐观认为可用，直到 set 明确返回 NOT_SUPPORTED 才置灰。 */
    st->avail_micled = 1;

    /* 把探测结论落进日志：真机排查「哪项被置灰」时一眼可见。 */
    hw_log(L"功能探测：电池=%d FnLock=%d 背光=%d(超时=%d) 智能充电=%d 电源解锁=%d MIC=%d",
           st->avail_battery, st->avail_fnlock, st->avail_kbd,
           st->kbd_timeout_ok, st->avail_smartcharge,
           st->avail_powerunlock, st->avail_micled);
}

/* ============================================================================
 * 托盘提示文本
 * ========================================================================== */
static void BuildTipText(WCHAR* buf, size_t cch)
{
    if (g_state.dev.available) {
        StringCchPrintfW(buf, cch, L"%s — 已连接 (%s)",
                         kAppTitle, hw_protocol_name(&g_state.dev));
    } else {
        const WCHAR* reason = (g_state.dev.reason[0] != L'\0')
                                  ? g_state.dev.reason
                                  : L"未知原因";
        StringCchPrintfW(buf, cch, L"%s — 未连接：%s", kAppTitle, reason);
    }
}

static void UpdateTrayTip(void)
{
    WCHAR tip[128];   /* NOTIFYICONDATAW.szTip 就是这个长度（新 SDK 为 128） */

    BuildTipText(tip, ARRAYSIZE(tip));
    Tray_UpdateTip(tip);
}

/* ============================================================================
 * 顶层命令实现
 * ========================================================================== */
static void RefreshAll(void)
{
    HwStatus s;

    /* hw_open 幂等：手动刷新时重连一次，顺带从临时 COM 故障里恢复。 */
    s = hw_open(&g_state.dev);
    AppProbeAll(&g_state);
    if (s == HW_OK) {
        hw_log(L"状态刷新完成（协议 %s，背光编码 %s）",
               hw_protocol_name(&g_state.dev),
               hw_kbd_encoding_name(&g_state.dev));
    } else {
        hw_log(L"状态刷新：设备不可用（%s）", hw_status_text(s));
    }
    UpdateTrayTip();
}

static void OpenLogFile(HWND hwnd)
{
    hw_log(L"用户操作：打开日志文件");
    if (!hw_log_open_in_shell()) {
        MessageBoxW(hwnd, L"无法打开日志文件（可能尚未生成）。",
                    kAppTitle, MB_OK | MB_ICONWARNING);
    }
}

static void ShowAbout(HWND hwnd)
{
    WCHAR text[1024];

    StringCchPrintfW(text, ARRAYSIZE(text),
                     L"华为笔记本调控工具（原生 Win32 托盘版）\n\n"
                     L"协议：%s\n"
                     L"键盘背光编码：%s\n"
                     L"设备实例：%s\n\n"
                     L"左键或右键点击托盘图标打开菜单；\n"
                     L"双击托盘图标打开日志。",
                     hw_protocol_name(&g_state.dev),
                     hw_kbd_encoding_name(&g_state.dev),
                     (g_state.dev.instance[0] != L'\0')
                         ? g_state.dev.instance
                         : L"（未连接）");
    MessageBoxW(hwnd, text, L"关于", MB_OK | MB_ICONINFORMATION);
}

/* ============================================================================
 * 命令分发
 * ----------------------------------------------------------------------------
 * 顶层命令（刷新/日志/关于/退出）在 main.cpp 处理；
 * 其余硬件命令统一交给 menu.cpp 的 HandleMenuCommand，避免两处 switch 打架。
 * ========================================================================== */
static void DispatchCommand(HWND hwnd, int cmd)
{
    switch (cmd) {
    case CMD_NONE:
        return;
    case CMD_REFRESH:
        RefreshAll();
        return;
    case CMD_OPEN_LOG:
        OpenLogFile(hwnd);
        return;
    case CMD_ABOUT:
        ShowAbout(hwnd);
        return;
    case CMD_EXIT:
        DestroyWindow(hwnd);
        return;
    default:
        break;
    }

    HandleMenuCommand(hwnd, cmd, &g_state);
}

static void ShowTrayMenu(HWND hwnd)
{
    int cmd;

    g_menuOpen = 1;                      /* 菜单期间别让定时器插进来读传感器 */
    cmd = BuildAppMenu(hwnd, &g_state);  /* 内部：建菜单 + TrackPopupMenu + 销毁 */
    g_menuOpen = 0;

    if (cmd != CMD_NONE) DispatchCommand(hwnd, cmd);
}

/* ============================================================================
 * 窗口过程
 * ----------------------------------------------------------------------------
 * 窗口本身永远不可见，只用来接收托盘回调与 TaskbarCreated 广播。
 * ========================================================================== */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    /* 动态注册的消息必须在 switch 之前比较。 */
    if (g_taskbarCreated != 0 && msg == g_taskbarCreated) {
        WCHAR tip[128];

        BuildTipText(tip, ARRAYSIZE(tip));
        if (Tray_Rebuild(hwnd, g_trayIcon, tip)) {
            hw_log(L"收到 TaskbarCreated：托盘图标已重建。");
        } else {
            hw_log(L"收到 TaskbarCreated，但重建托盘图标失败。");
        }
        return 0;
    }

    switch (msg) {
    case HWM_SHOW_TRAY_MENU:
        ShowTrayMenu(hwnd);
        return 0;

    case HWM_TRAY_CALLBACK:
        /* 默认版本（非 v4）：lParam 是鼠标消息本身。 */
        switch (LOWORD(lParam)) {
        case WM_LBUTTONDBLCLK:
            g_lastDblClkTick = GetTickCount();
            OpenLogFile(hwnd);
            return 0;

        case WM_LBUTTONUP:
            /* 双击序列是 DOWN/UP/DBLCLK/UP，最后那个 UP 会再弹一次菜单，
             * 用 400ms 时间窗把它吞掉。 */
            if (g_lastDblClkTick != 0 &&
                (GetTickCount() - g_lastDblClkTick) < 400) {
                g_lastDblClkTick = 0;
                return 0;
            }
            ShowTrayMenu(hwnd);
            return 0;

        case WM_RBUTTONUP:
            ShowTrayMenu(hwnd);
            return 0;

        default:
            return 0;
        }

    case WM_COMMAND:
        /* 预留：万一将来改成不用 TPM_RETURNCMD 的写法。 */
        DispatchCommand(hwnd, (int)LOWORD(wParam));
        return 0;

    case WM_TIMER:
        /* 只刷新传感器缓存，失败静默（AppProbeSensors 内部已静默处理）。 */
        if (wParam == kSensorTimerId && !g_menuOpen && g_state.dev.available)
            AppProbeSensors(&g_state);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, kSensorTimerId);
        Tray_Remove();
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* ============================================================================
 * 入口
 * ========================================================================== */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow)
{
    int          rc = 0;
    HANDLE       hMutex = NULL;
    BOOL         alreadyRunning = FALSE;
    HWND         hwnd = NULL;
    MSG          msg;
    WNDCLASSEXW  wc;
    HwStatus     hs;
    WCHAR        tip[128];
    WCHAR        logPathBuf[MAX_PATH * 2];
    const WCHAR* logPath = NULL;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    /* 日志最先初始化：后面任何失败都留痕。 */
    logPath = hw_log_init();
    StringCchCopyW(logPathBuf, ARRAYSIZE(logPathBuf),
                   (logPath != NULL) ? logPath : L"(仅调试输出)");
    hw_log(L"=== 华为笔记本调控工具（原生 Win32 托盘版）启动 ===");
    hw_log(L"日志文件：%s", logPathBuf);

    /* ---------------------------------------------------------- 单实例 --- */
    SetLastError(ERROR_SUCCESS);
    hMutex = CreateMutexW(NULL, TRUE, kMutexNameGlobal);
    if (hMutex != NULL) {
        alreadyRunning = (GetLastError() == ERROR_ALREADY_EXISTS) ? TRUE : FALSE;
    } else {
        /* Global\ 需要 SeCreateGlobalPrivilege（管理员一般都有），失败退到 Local\。 */
        DWORD errGlobal = GetLastError();
        hMutex = CreateMutexW(NULL, TRUE, kMutexNameLocal);
        if (hMutex != NULL) {
            alreadyRunning =
                (GetLastError() == ERROR_ALREADY_EXISTS) ? TRUE : FALSE;
        } else {
            DWORD errLocal = GetLastError();
            hw_log(L"创建互斥体失败（Global=%lu，Local=%lu），跳过单实例检查。",
                   errGlobal, errLocal);
        }
    }

    if (alreadyRunning) {
        HWND oldWnd = FindWindowW(kWindowClass, NULL);
        hw_log(L"检测到已有实例，通知其弹出菜单后退出。");
        if (oldWnd != NULL) {
            SetForegroundWindow(oldWnd);
            PostMessageW(oldWnd, HWM_SHOW_TRAY_MENU, 0, 0);
        }
        if (hMutex != NULL) {
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
        }
        hw_log_shutdown();
        return 0;
    }

    /* ------------------------------------------------------- 打开设备 --- */
    hw_device_init(&g_state.dev);
    g_state.temp_count = TempZoneCount();
    g_state.reset_before_disable = 0;

    hs = hw_open(&g_state.dev);
    if (hs == HW_ERR_NOT_ADMIN) {
        hw_log(L"启动中止：未以管理员身份运行。");
        MessageBoxW(NULL,
                    L"本程序需要以管理员身份运行才能访问华为 ACPI-WMI 接口。\n\n"
                    L"请关闭后右键选择「以管理员身份运行」。",
                    kAppTitle, MB_OK | MB_ICONERROR);
        rc = 1;
        goto cleanup;
    }
    if (hs != HW_OK) {
        hw_log(L"设备打开失败：%s（%s）", hw_status_text(hs),
               g_state.dev.reason);
    } else {
        hw_log(L"设备已连接：协议 %s，背光编码 %s",
               hw_protocol_name(&g_state.dev),
               hw_kbd_encoding_name(&g_state.dev));
    }

    /* --------------------------------------------------- 隐藏顶层窗口 --- */
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kWindowClass;

    if (!RegisterClassExW(&wc)) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            hw_log(L"注册窗口类失败（错误码 %lu）。", err);
            MessageBoxW(NULL, L"注册窗口类失败，程序无法启动。",
                        kAppTitle, MB_OK | MB_ICONERROR);
            rc = 1;
            goto cleanup;
        }
        hw_log(L"窗口类已存在，复用。");
    }

    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    if (g_taskbarCreated == 0) {
        DWORD err = GetLastError();
        hw_log(L"注册 TaskbarCreated 失败（错误码 %lu），"
               L"Explorer 重启后图标可能不会自动恢复。", err);
    }

    /* 隐藏的普通顶层窗口：无 WS_VISIBLE、无父窗口。
     * WS_EX_TOOLWINDOW 让它不会出现在任务栏和 Alt+Tab 里。 */
    hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClass, kAppTitle,
                           WS_POPUP, 0, 0, 0, 0,
                           NULL, NULL, hInstance, NULL);
    if (hwnd == NULL) {
        DWORD err = GetLastError();
        hw_log(L"创建隐藏窗口失败（错误码 %lu）。", err);
        MessageBoxW(NULL, L"创建隐藏窗口失败，程序无法启动。",
                    kAppTitle, MB_OK | MB_ICONERROR);
        rc = 1;
        goto cleanup;
    }

    /* ------------------------------------------- 首批缓存 + 托盘图标 --- */
    AppProbeAll(&g_state);

    g_trayIcon = CreateTrayIcon();
    if (g_trayIcon != NULL) {
        g_trayIconOwned = 1;
    } else {
        g_trayIcon = LoadIconW(NULL, IDI_APPLICATION);
        g_trayIconOwned = 0;
        hw_log(L"运行时图标创建失败，回退到系统默认图标。");
    }

    BuildTipText(tip, ARRAYSIZE(tip));
    if (!Tray_Add(hwnd, g_trayIcon, tip)) {
        DWORD err = GetLastError();
        hw_log(L"添加托盘图标失败（错误码 %lu）。", err);
        MessageBoxW(NULL, L"无法在任务栏创建托盘图标，程序无法启动。",
                    kAppTitle, MB_OK | MB_ICONERROR);
        rc = 1;
        goto cleanup;
    }
    hw_log(L"托盘图标已添加；提示：%s", tip);

    if (SetTimer(hwnd, kSensorTimerId, kSensorTimerMs, NULL) == 0)
        hw_log(L"创建传感器定时器失败，将不自动刷新传感器。");

    /* ------------------------------------------------------------ 循环 --- */
    ZeroMemory(&msg, sizeof(msg));
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    rc = (int)msg.wParam;

cleanup:
    /* 所有资源在这里统一释放；goto 跳转不跨任何带初始化的声明
     * （本函数的局部变量全部在顶部声明）。 */
    if (hwnd != NULL) {
        KillTimer(hwnd, kSensorTimerId);
        if (IsWindow(hwnd)) DestroyWindow(hwnd);
        hwnd = NULL;
    }
    Tray_Remove();
    if (g_trayIconOwned && g_trayIcon != NULL) DestroyIcon(g_trayIcon);
    g_trayIcon = NULL;
    g_trayIconOwned = 0;

    hw_close(&g_state.dev);
    hw_log(L"=== 程序退出（rc=%d）===", rc);
    hw_log_shutdown();

    if (hMutex != NULL) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        hMutex = NULL;
    }
    return rc;
}
