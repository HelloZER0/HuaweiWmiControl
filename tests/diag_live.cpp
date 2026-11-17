/* ============================================================================
 * diag_live.cpp — 真机诊断程序（需管理员）
 * ----------------------------------------------------------------------------
 * 逐项调用 hwapi.h 的 getter，打印 HwStatus 与固件状态码，
 * 用来区分「机型真的不支持」和「客户端代码有问题」。
 * 输出同时写到 stdout 与 %TEMP%\hwc_diag.txt（提权重定向不可靠）。
 * ========================================================================== */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "hwapi.h"

static FILE* g_out = NULL;

static void out(const WCHAR* fmt, ...)
{
    WCHAR buf[1024];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (g_out) fwprintf(g_out, L"%s\n", buf);
    wprintf(L"%s\n", buf);
    fflush(stdout);
}

static const WCHAR* st(HwStatus s) { return hw_status_text(s); }

int main(void)
{
    HwDevice dev;
    WCHAR path[MAX_PATH];
    DWORD len;
    HwStatus s;
    int a = 0, b = 0, c = 0, d = 0, v = 0;
    int i;

    len = GetTempPathW(MAX_PATH, path);
    if (len > 0) wcscat_s(path, MAX_PATH, L"hwc_diag.txt");
    g_out = _wfopen(path, L"wb");
    if (g_out) { fputc(0xFF, g_out); fputc(0xFE, g_out); }

    out(L"=== HWC 真机诊断 ===");
    out(L"临时输出: %s", path);

    hw_log_init();
    hw_device_init(&dev);

    s = hw_open(&dev);
    out(L"hw_open                -> %s", st(s));
    out(L"  available=%d protocol=%d kbd_enc=%d last_fw=%d",
        dev.available, (int)dev.protocol, (int)dev.kbd_encoding, dev.last_fw_status);
    out(L"  instance=%s", dev.instance);
    out(L"  reason=%s", dev.reason);
    if (s != HW_OK) { out(L"中止：设备未打开"); goto done; }

    out(L"protocol=%s kbd=%s", hw_protocol_name(&dev), hw_kbd_encoding_name(&dev));

    /* ---- 电池保护 ---- */
    s = hw_battery_get(&dev, &a, &b);
    out(L"battery_get            -> %s  start=%d end=%d  fw=0x%02X",
        st(s), a, b, dev.last_fw_status);

    /* ---- Fn-Lock ---- */
    s = hw_fnlock_get(&dev, &a);
    out(L"fnlock_get             -> %s  on=%d  fw=0x%02X", st(s), a, dev.last_fw_status);

    /* ---- 键盘背光 ---- */
    s = hw_kbd_get_level(&dev, &a);
    out(L"kbd_get_level          -> %s  level=%d  fw=0x%02X", st(s), a, dev.last_fw_status);
    s = hw_kbd_get_timeout(&dev, &b);
    out(L"kbd_get_timeout        -> %s  sec=%d  fw=0x%02X", st(s), b, dev.last_fw_status);

    /* ---- 智能充电 ---- */
    s = hw_smartcharge_get(&dev, &a, &b, &c, &d);
    out(L"smartcharge_get        -> %s  mode=%d unk=%d %d-%d  fw=0x%02X",
        st(s), a, b, c, d, dev.last_fw_status);
    s = hw_smartcharge_get_param(&dev, &v);
    out(L"smartcharge_get_param  -> %s  param=%d  fw=0x%02X", st(s), v, dev.last_fw_status);

    /* ---- 电源解锁 ---- */
    s = hw_powerunlock_get(&dev, &a);
    out(L"powerunlock_get        -> %s  on=%d  fw=0x%02X", st(s), a, dev.last_fw_status);

    /* ---- 传感器 ---- */
    for (i = 0; i < 2; ++i) {
        s = hw_fan_speed(&dev, i, &a);
        out(L"fan_speed(%d)           -> %s  rpm=%d  fw=0x%02X", i, st(s), a, dev.last_fw_status);
    }
    for (i = 0; i < kHwTempZoneCount; ++i) {
        s = hw_temp(&dev, kHwTempZones[i].idx, &a);
        out(L"temp(0x%02X %-6s)       -> %s  %d C", kHwTempZones[i].idx,
            kHwTempZones[i].label, st(s), a);
    }

    /* ---- 写回测试：把当前值原样写回，不改变任何状态，只验证固件接受度 ---- */
    out(L"");
    out(L"--- 写入路径验证（写回当前值，不改变状态）---");

    s = hw_battery_get(&dev, &a, &b);
    if (s == HW_OK) {
        s = hw_battery_set(&dev, a, b, 0);
        out(L"battery_set(%d,%d)       -> %s  fw=0x%02X", a, b, st(s), dev.last_fw_status);
    }

    s = hw_fnlock_get(&dev, &a);
    if (s == HW_OK) {
        s = hw_fnlock_set(&dev, a);
        out(L"fnlock_set(%d)            -> %s  fw=0x%02X", a, st(s), dev.last_fw_status);
    }

    s = hw_kbd_get_level(&dev, &a);
    if (s == HW_OK) {
        s = hw_kbd_set_level(&dev, a);
        out(L"kbd_set_level(%d)         -> %s  fw=0x%02X", a, st(s), dev.last_fw_status);
    }

    s = hw_kbd_get_timeout(&dev, &b);
    if (s == HW_OK) {
        s = hw_kbd_set_timeout(&dev, b);
        out(L"kbd_set_timeout(%d)       -> %s  fw=0x%02X", b, st(s), dev.last_fw_status);
    }

    s = hw_powerunlock_get(&dev, &a);
    if (s == HW_OK) {
        s = hw_powerunlock_set(&dev, a);
        out(L"powerunlock_set(%d)       -> %s  fw=0x%02X", a, st(s), dev.last_fw_status);
    }

done:
    hw_close(&dev);
    hw_log_shutdown();
    out(L"=== 诊断结束 ===");
    if (g_out) fclose(g_out);
    return 0;
}
