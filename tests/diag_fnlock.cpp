/* ============================================================================
 * diag_fnlock.cpp — Fn-Lock 专项诊断（需管理员）
 * 把 C_FN_LOCK_GET 的原始返回字节打出来，并做 SET→GET 往返，
 * 判断「写入返回成功但状态没变」还是「状态变了但解析错」。
 * ========================================================================== */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "hwapi.h"

extern "C" HwStatus hw_wmi_call(HwDevice* dev, uint64_t cmd,
                                uint8_t* out, int* out_len);

static FILE* g_out = NULL;

static void out(const WCHAR* fmt, ...)
{
    WCHAR buf[1024]; va_list ap;
    va_start(ap, fmt); _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap); va_end(ap);
    if (g_out) fwprintf(g_out, L"%s\n", buf);
    fflush(stdout);
}

static void dump(const WCHAR* tag, const uint8_t* b, int n)
{
    WCHAR line[512]; int i, pos = 0;
    pos += _snwprintf_s(line + pos, _countof(line) - pos, _TRUNCATE, L"%s  len=%d  bytes:", tag, n);
    for (i = 0; i < n && i < 12 && pos < 400; ++i)
        pos += _snwprintf_s(line + pos, _countof(line) - pos, _TRUNCATE, L" %02X", b[i]);
    out(L"%s", line);
}

int main(void)
{
    HwDevice dev; WCHAR path[MAX_PATH]; HwStatus s;
    uint8_t buf[HW_OUT_MAX]; int n = 0, on = -1, orig = -1;

    GetTempPathW(MAX_PATH, path); wcscat_s(path, MAX_PATH, L"hwc_fnlock.txt");
    g_out = _wfopen(path, L"wb");
    if (g_out) { fputc(0xFF, g_out); fputc(0xFE, g_out); }

    out(L"=== Fn-Lock 专项诊断 ===");
    hw_device_init(&dev);
    s = hw_open(&dev);
    out(L"hw_open -> %s protocol=%s", hw_status_text(s), hw_protocol_name(&dev));
    if (s != HW_OK) goto done;

    out(L"命令号: GET=0x%04llX SET=0x%04llX",
        (unsigned long long)HW_C_FN_LOCK_GET,
        (unsigned long long)HW_C_FN_LOCK_SET);

    /* ---- 1. 原始 GET ---- */
    n = 0;
    s = hw_wmi_call(&dev, hw_cmd_simple(HW_C_FN_LOCK_GET), buf, &n);
    out(L""); out(L"[1] GET 原始返回 -> %s", hw_status_text(s));
    dump(L"    raw", buf, n);
    hw_fnlock_get(&dev, &orig);
    out(L"    解析结果 on=%d", orig);

    /* ---- 2. SET 1（C# 语义 = 关）后回读 ---- */
    n = 0;
    s = hw_wmi_call(&dev, hw_cmd1(HW_C_FN_LOCK_SET, 1), buf, &n);
    out(L""); out(L"[2] SET arg=1 -> %s", hw_status_text(s));
    Sleep(300);
    n = 0;
    hw_wmi_call(&dev, hw_cmd_simple(HW_C_FN_LOCK_GET), buf, &n);
    dump(L"    GET 后 raw", buf, n);
    hw_fnlock_get(&dev, &on);
    out(L"    解析结果 on=%d", on);

    /* ---- 3. SET 2（C# 语义 = 开）后回读 ---- */
    n = 0;
    s = hw_wmi_call(&dev, hw_cmd1(HW_C_FN_LOCK_SET, 2), buf, &n);
    out(L""); out(L"[3] SET arg=2 -> %s", hw_status_text(s));
    Sleep(300);
    n = 0;
    hw_wmi_call(&dev, hw_cmd_simple(HW_C_FN_LOCK_GET), buf, &n);
    dump(L"    GET 后 raw", buf, n);
    hw_fnlock_get(&dev, &on);
    out(L"    解析结果 on=%d", on);

    /* ---- 3b. 扫描有效取值：逐个写入并回读 ---- */
    out(L"");
    out(L"[3b] 取值扫描（写入 -> 回读）");
    {
        int arg;
        for (arg = 0; arg <= 3; ++arg) {
            int rb = -1;
            n = 0;
            s = hw_wmi_call(&dev, hw_cmd1(HW_C_FN_LOCK_SET, (uint8_t)arg), buf, &n);
            Sleep(250);
            n = 0;
            hw_wmi_call(&dev, hw_cmd_simple(HW_C_FN_LOCK_GET), buf, &n);
            dump(L"    SET/GET raw", buf, n);
            hw_fnlock_get(&dev, &rb);
            out(L"    SET %d -> %s, 回读 on=%d", arg, hw_status_text(s), rb);
        }
    }

    /* ---- 4. 还原 ---- */
    if (orig >= 0) {
        hw_fnlock_set(&dev, orig);
        out(L""); out(L"[4] 已还原为原始状态 on=%d", orig);
    }

done:
    hw_close(&dev);
    out(L"=== 结束 ===");
    if (g_out) fclose(g_out);
    return 0;
}
