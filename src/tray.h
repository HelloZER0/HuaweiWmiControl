/* ============================================================================
 * tray.h — Shell_NotifyIcon 的精简封装
 * ----------------------------------------------------------------------------
 * 只暴露「加 / 改图标 / 改提示 / 删 / Explorer 重启后重建」五件事。
 * 内部保存唯一的 NOTIFYICONDATAW，调用方无需关心 cbSize/uID/uCallbackMessage。
 * ========================================================================== */
#ifndef HWC_TRAY_H
#define HWC_TRAY_H

#include <windows.h>

/* 托盘回调消息：wParam = 图标 ID，lParam = 鼠标消息（WM_LBUTTONUP 等）。
 * 默认版本（未调用 NIM_SETVERSION v4），所以 lParam 不是坐标。 */
#define HWM_TRAY_CALLBACK (WM_APP + 1)

/* 添加托盘图标。tip 可为 NULL（用默认提示）。
 * icon 为 NULL 时只加消息+提示、不加图标（调用方应尽量给一个图标）。
 * 返回 1 = 成功，0 = 失败（可用 GetLastError 看原因，但 Shell_NotifyIcon
 * 失败时不一定设置 last error）。 */
int Tray_Add(HWND hwnd, HICON icon, const WCHAR* tip);

/* 更新提示文本（图标已在托盘上才生效）。返回 1 = 成功。 */
int Tray_UpdateTip(const WCHAR* tip);

/* 更新图标（例如运行中切换状态图标）。返回 1 = 成功。 */
int Tray_UpdateIcon(HICON icon);

/* 删除托盘图标。幂等：未添加时直接返回 1。 */
int Tray_Remove(void);

/* 重建托盘图标（先删后加）：用于收到 TaskbarCreated 广播后恢复图标。 */
int Tray_Rebuild(HWND hwnd, HICON icon, const WCHAR* tip);

/* 当前是否已添加。 */
BOOL Tray_IsAdded(void);

#endif /* HWC_TRAY_H */
