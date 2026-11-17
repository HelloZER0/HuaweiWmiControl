/* ============================================================================
 * menu.h — 弹出菜单构建、菜单命令 ID 与状态缓存
 * ----------------------------------------------------------------------------
 * 为什么 AppState 放在这里：menu.cpp 渲染菜单需要读缓存，main.cpp 负责
 * 填充缓存，两边必须看到同一个结构体定义。外壳层只有这几个文件，故不另建
 * state.h。
 *
 * 约束：
 *   - 本文件不 include 任何 COM/WMI 头，只依赖 windows.h + hwapi.h。
 *   - 菜单渲染路径只能读 AppState，绝不能调用 hw_* 之外的任何东西。
 * ========================================================================== */
#ifndef HWC_MENU_H
#define HWC_MENU_H

#include <windows.h>
#include "hwapi.h"

/* 传感器缓存上限。
 * 注意：kHwTempZoneCount 是 extern const int，不是常量表达式，
 * 不能直接用作数组维度，因此这里用固定上限（proto.c 当前是 11 路）。 */
#define APP_MAX_TEMP_ZONES 16
#define APP_FAN_COUNT      2

/* ============================================================================
 * AppState — 进程内唯一的状态缓存
 * ----------------------------------------------------------------------------
 * 所有字段都由 main.cpp 的 AppProbe* 填充 / 就地更新。
 * menu.cpp 只读（BuildAppMenu 取 const AppState*），保证弹出菜单零 WMI 调用。
 * ========================================================================== */
struct AppState {
    HwDevice dev;               /* 设备句柄（hwapi.h；_p 字段外壳层勿动） */

    /* ---- 功能可用性：1 = 可用，0 = 固件不支持 / 设备未连接 ---- */
    int avail_battery;
    int avail_fnlock;
    int avail_kbd;
    int avail_smartcharge;
    int avail_powerunlock;
    int avail_micled;           /* 只有 SET 没有 GET，无法探测，故乐观置 1 */

    /* ---- 当前值缓存（hw_*_get 的结果） ---- */
    int battery_start;          /* 0..100 */
    int battery_end;            /* 0..100 */
    int fnlock_on;              /* 0/1 */
    int kbd_level;              /* 0=关 1=低 2=高 */
    int kbd_timeout;            /* 秒，0=常亮 */
    int kbd_timeout_ok;         /* 超时读取是否成功（固件可能单独不支持） */
    int charge_mode;            /* 智能充电 mode */
    int charge_unknown;         /* 智能充电 unknown 字节 */
    int charge_start;           /* 智能充电 start */
    int charge_end;             /* 智能充电 end */
    int charge_param;           /* 智能充电 param */
    int powerunlock_on;         /* 0/1 */
    int reset_before_disable;   /* 复选框：关闭前先清零阈值（MACH-WX9 兼容） */

    /* ---- 传感器缓存 ---- */
    int fan_ok[APP_FAN_COUNT];      /* 1 = 本次读取成功 */
    int fan_rpm[APP_FAN_COUNT];
    int temp_count;                 /* 有效温度区数量（<= APP_MAX_TEMP_ZONES） */
    int temp_ok[APP_MAX_TEMP_ZONES];
    int temp_celsius[APP_MAX_TEMP_ZONES];
};

/* ============================================================================
 * 菜单命令 ID
 * ----------------------------------------------------------------------------
 * 分段编号，便于日志与排错：
 *   1xx 电池保护   2xx Fn-Lock    3xx 键盘背光
 *   4xx 智能充电   5xx 电源解锁   6xx 麦克风 LED
 *   9xx 顶层命令
 * ========================================================================== */
enum MenuCmd {
    CMD_NONE = 0,

    /* 只读占位（传感器项不可点，仅用于标识） */
    CMD_SENSORS = 1,

    /* ---- 电池保护 ---- */
    CMD_BATTERY_OFF         = 100,  /* 关闭充电控制 (0-100) */
    CMD_BATTERY_P40_70      = 101,  /* 40% - 70%          */
    CMD_BATTERY_P70_90      = 102,  /* 70% - 90%          */
    CMD_BATTERY_P95_100     = 103,  /* 95% - 100%         */
    CMD_BATTERY_TOGGLE_RESET = 104, /* 关闭前先清零阈值复选框 */

    /* ---- Fn-Lock ---- */
    CMD_FNLOCK_TOGGLE       = 200,

    /* ---- 键盘背光 ---- */
    CMD_KBD_OFF             = 300,
    CMD_KBD_LOW             = 301,
    CMD_KBD_HIGH            = 302,
    CMD_KBD_TIMEOUT_0       = 320,  /* 0 秒（常亮） */
    CMD_KBD_TIMEOUT_15      = 321,
    CMD_KBD_TIMEOUT_30      = 322,
    CMD_KBD_TIMEOUT_60      = 323,
    CMD_KBD_TIMEOUT_300     = 324,

    /* ---- 智能充电 ---- */
    CMD_SC_MODE_0           = 400,
    CMD_SC_MODE_1           = 401,
    CMD_SC_MODE_2           = 402,
    CMD_SC_MODE_3           = 403,
    CMD_SC_PARAM_0          = 410,
    CMD_SC_PARAM_1          = 411,
    CMD_SC_PARAM_2          = 412,
    CMD_SC_APPLY            = 420,  /* 应用当前值 */

    /* ---- 电源解锁 ---- */
    CMD_POWERUNLOCK_TOGGLE  = 500,

    /* ---- 麦克风 LED（无回读，不显示勾选） ---- */
    CMD_MICLED_ON           = 600,
    CMD_MICLED_OFF          = 601,

    /* ---- 顶层命令 ---- */
    CMD_REFRESH             = 900,
    CMD_OPEN_LOG            = 901,
    CMD_ABOUT               = 902,
    CMD_EXIT                = 903
};

/* 构建并弹出菜单，返回用户选择的命令 ID（取消返回 CMD_NONE）。
 * 内部完成 SetForegroundWindow -> TrackPopupMenu -> PostMessage(WM_NULL)
 * -> DestroyMenu 的标准套路，坐标取当前鼠标位置。 */
int BuildAppMenu(HWND hwnd, const AppState* st);

/* 执行一条菜单命令。hwnd 仅用于可能弹出的提示框。 */
void HandleMenuCommand(HWND hwnd, int cmd, AppState* st);

/* ============================================================================
 * 缓存刷新（由 main.cpp 实现）
 * ----------------------------------------------------------------------------
 * ProbeAll = 全部功能 + 传感器；单项 Probe* 供 menu.cpp 在 set 失败后定点重读。
 * 约定：返回 HW_ERR_NOT_SUPPORTED 置 avail_* = 0；其他错误只写日志、保留旧值。
 * ========================================================================== */
void AppProbeAll(AppState* st);
void AppProbeBattery(AppState* st);
void AppProbeFnLock(AppState* st);
void AppProbeKbd(AppState* st);
void AppProbeKbdTimeout(AppState* st);
void AppProbeSmartCharge(AppState* st);
void AppProbePowerUnlock(AppState* st);
void AppProbeSensors(AppState* st);

#endif /* HWC_MENU_H */
