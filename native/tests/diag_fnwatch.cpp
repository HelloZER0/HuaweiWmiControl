/* ============================================================================
 * diag_fnwatch.cpp — 监视 Fn-Lock 的 WMI 值变化（需管理员）
 * 目的：判断「物理 Fn+Esc 按键」与「WMI 里的 Fn-Lock 值」是不是同一份状态。
 *   若按 Fn+Esc 时该值变化  -> 两者同源，写入应当也有物理效果
 *   若按 Fn+Esc 时该值不变  -> WMI 标志与物理行为无关，写入改变不了键盘
 * 结果写到 %TEMP%\hwc_fnwatch.txt
 * ========================================================================== */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "hwapi.h"

int main(int argc, char** argv)
{
    HwDevice dev; WCHAR path[MAX_PATH]; HwStatus s;
    int secs = 180, last = -1, i, cur = 0;
    FILE* f;
    SYSTEMTIME st;

    if (argc > 1) { secs = atoi(argv[1]); if (secs <= 0) secs = 180; }

    GetTempPathW(MAX_PATH, path); wcscat_s(path, MAX_PATH, L"hwc_fnwatch.txt");
    f = _wfopen(path, L"wb");
    if (f) { fputc(0xFF, f); fputc(0xFE, f); }

    hw_device_init(&dev);
    s = hw_open(&dev);
    if (f) fwprintf(f, L"hw_open -> %s\n", hw_status_text(s));
    if (s != HW_OK) { if (f) fclose(f); return 1; }

    if (hw_fnlock_get(&dev, &last) != HW_OK) last = -1;
    if (f) {
        fwprintf(f, L"开始监视 %d 秒，初始值 on=%d\n", secs, last);
        fwprintf(f, L">>> 请现在按几次 Fn+Esc <<<\n");
        fflush(f);
    }

    /* secs*2 次，每次 500ms */
    for (i = 0; i < secs * 2; ++i) {
        Sleep(500);
        if (hw_fnlock_get(&dev, &cur) != HW_OK) continue;
        if (cur != last) {
            GetLocalTime(&st);
            if (f) {
                fwprintf(f, L"[%02d:%02d:%02d] 值变化: %d -> %d\n",
                         st.wHour, st.wMinute, st.wSecond, last, cur);
                fflush(f);
            }
            last = cur;
        }
    }

    if (f) {
        fwprintf(f, L"监视结束，最终值 on=%d\n", last);
        fclose(f);
    }
    hw_close(&dev);
    return 0;
}
