/* ============================================================================
 * tray.cpp — Shell_NotifyIcon 封装
 * ----------------------------------------------------------------------------
 * 注意版本：本程序**不**调用 NIM_SETVERSION(v4)。
 *   - 默认版本：wParam = 图标 ID，lParam = 鼠标消息（WM_LBUTTONUP 等）。
 *   - v4 版本：wParam = MAKEWPARAM(图标ID, 消息)，lParam = MAKELPARAM(x, y)。
 * 为降低复杂度按需求使用默认版本，坐标由调用方 GetCursorPos() 取。
 * ========================================================================== */
#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>

#include "tray.h"

/* 老 SDK 可能没有 NIF_SHOWTIP（Vista+ 才有），补个定义保证可编译。 */
#ifndef NIF_SHOWTIP
#define NIF_SHOWTIP 0x00000080u
#endif

namespace {

const UINT kTrayIconId = 1;

NOTIFYICONDATAW g_nid;
BOOL            g_added = FALSE;
/* NIF_SHOWTIP 只有 Vista+ 认识；万一 NIM_ADD 因此失败，就永久降级不再带它，
 * 否则后续 NIM_MODIFY 也会被同一个标志连累。 */
BOOL            g_showtip = TRUE;

/* 每次 NIM_* 前按当前能力组装 uFlags 的公共部分。 */
UINT BaseFlags(void)
{
    return g_showtip ? (NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP)
                     : (NIF_MESSAGE | NIF_TIP);
}

/* 重置内部结构并写入固定字段。tip 由调用方决定是否拷贝。 */
void ResetNid(HWND hwnd)
{
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = (DWORD)sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = kTrayIconId;
    g_nid.uCallbackMessage = HWM_TRAY_CALLBACK;
}

/* 极简安全拷贝到 szTip（szTip 长度为 128 或 64，取决于 SDK 版本）。 */
void CopyTip(const WCHAR* tip)
{
    if (tip == NULL || tip[0] == L'\0')
        StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip),
                       L"华为笔记本调控工具");
    else
        StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip), tip);
}

} /* namespace */

int Tray_Add(HWND hwnd, HICON icon, const WCHAR* tip)
{
    if (hwnd == NULL) return 0;

    ResetNid(hwnd);
    CopyTip(tip);

    for (;;) {
        g_nid.uFlags = BaseFlags();
        if (icon != NULL) {
            g_nid.uFlags |= NIF_ICON;
            g_nid.hIcon = icon;
        }
        g_added = Shell_NotifyIconW(NIM_ADD, &g_nid) ? TRUE : FALSE;
        if (g_added) break;
        if (!g_showtip) break;   /* 已经是不带 NIF_SHOWTIP 的版本，不再重试 */
        g_showtip = FALSE;       /* 极罕见：Shell 不认 NIF_SHOWTIP，去掉重试一次 */
    }
    return g_added ? 1 : 0;
}

int Tray_UpdateTip(const WCHAR* tip)
{
    if (!g_added || tip == NULL) return 0;

    CopyTip(tip);
    g_nid.uFlags = NIF_TIP | (g_showtip ? NIF_SHOWTIP : 0);
    return Shell_NotifyIconW(NIM_MODIFY, &g_nid) ? 1 : 0;
}

int Tray_UpdateIcon(HICON icon)
{
    if (!g_added || icon == NULL) return 0;

    g_nid.uFlags = NIF_ICON;
    g_nid.hIcon = icon;
    return Shell_NotifyIconW(NIM_MODIFY, &g_nid) ? 1 : 0;
}

int Tray_Remove(void)
{
    if (!g_added) return 1;

    g_nid.uFlags = 0;
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    g_added = FALSE;
    return 1;
}

int Tray_Rebuild(HWND hwnd, HICON icon, const WCHAR* tip)
{
    if (hwnd == NULL) return 0;

    /* Explorer 重启后托盘是空的，旧图标的删除通常已无意义，
     * 但仍然先删一次：防止「Explorer 只是崩溃重启、图标句柄还挂着」的情况。 */
    if (g_added) {
        g_nid.uFlags = 0;
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        g_added = FALSE;
    }
    return Tray_Add(hwnd, icon, tip);
}

BOOL Tray_IsAdded(void)
{
    return g_added;
}
