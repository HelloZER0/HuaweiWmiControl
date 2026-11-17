/* ============================================================================
 * menu.cpp — 弹出菜单构建与命令分发
 * ----------------------------------------------------------------------------
 * 约束：
 *   1. 菜单渲染路径只读 AppState 缓存，绝不调用 hw_*（保证弹出即时）。
 *   2. 每次弹出都重新构建菜单，因此勾选状态永远是实时的。
 *   3. TrackPopupMenu 的标准套路集中在 BuildAppMenu()：
 *        SetForegroundWindow -> TrackPopupMenu(TPM_RETURNCMD) ->
 *        PostMessage(WM_NULL) -> DestroyMenu
 *      少任何一步都会出现「菜单不消失 / 点菜单外不响应」的经典 bug。
 * ========================================================================== */
#include <windows.h>
#include <strsafe.h>

#include "menu.h"
#include "hwapi.h"

namespace {

/* 简易 RAII：作用域结束自动 DestroyMenu，保证任何 return 都不泄漏。
 * 一旦子菜单成功挂到父菜单上就 Release()，此后由父菜单负责销毁。 */
struct MenuGuard {
    HMENU h;
    explicit MenuGuard(HMENU m = NULL) : h(m) {}
    ~MenuGuard() { if (h != NULL) DestroyMenu(h); }
    HMENU Get(void) const { return h; }
    void  Release(void) { h = NULL; }
    MenuGuard(const MenuGuard&) = delete;
    MenuGuard& operator=(const MenuGuard&) = delete;
};

/* 条件成立时追加 flag（用于 MF_GRAYED / MF_CHECKED）。 */
inline UINT FlagsIf(int cond, UINT flag)
{
    return cond ? flag : 0u;
}

/* 把 cmd 映射成熄灭超时秒数；不属于超时命令时 *ok = 0。 */
static int KbdTimeoutSeconds(int cmd, int* ok)
{
    *ok = 1;
    switch (cmd) {
    case CMD_KBD_TIMEOUT_0:   return 0;
    case CMD_KBD_TIMEOUT_15:  return 15;
    case CMD_KBD_TIMEOUT_30:  return 30;
    case CMD_KBD_TIMEOUT_60:  return 60;
    case CMD_KBD_TIMEOUT_300: return 300;
    default:
        *ok = 0;
        return 0;
    }
}

/* ============================================================================
 * 构建菜单树（只读缓存）
 * ========================================================================== */
static HMENU BuildMenuTree(const AppState* st)
{
    WCHAR buf[256];

    MenuGuard main(CreatePopupMenu());
    if (main.Get() == NULL) return NULL;

    /* ---------------------------------------------------------- 传感器 --- */
    {
        MenuGuard sensors(CreatePopupMenu());
        int i;
        if (sensors.Get() == NULL) return NULL;

        for (i = 0; i < APP_FAN_COUNT; ++i) {
            if (st->fan_ok[i])
                StringCchPrintfW(buf, ARRAYSIZE(buf), L"风扇 %d: %d RPM",
                                 i + 1, st->fan_rpm[i]);
            else
                StringCchPrintfW(buf, ARRAYSIZE(buf), L"风扇 %d: -- RPM", i + 1);
            AppendMenuW(sensors.Get(), MF_STRING | MF_GRAYED,
                        (UINT_PTR)CMD_SENSORS, buf);
        }

        for (i = 0; i < st->temp_count && i < APP_MAX_TEMP_ZONES; ++i) {
            if (st->temp_ok[i])
                StringCchPrintfW(buf, ARRAYSIZE(buf), L"%s: %d °C",
                                 kHwTempZones[i].label, st->temp_celsius[i]);
            else
                StringCchPrintfW(buf, ARRAYSIZE(buf), L"%s: -- °C",
                                 kHwTempZones[i].label);
            AppendMenuW(sensors.Get(), MF_STRING | MF_GRAYED,
                        (UINT_PTR)CMD_SENSORS, buf);
        }

        if (!AppendMenuW(main.Get(), MF_POPUP, (UINT_PTR)sensors.Get(), L"传感器"))
            return NULL;
        sensors.Release();
    }

    AppendMenuW(main.Get(), MF_SEPARATOR, 0, NULL);

    /* -------------------------------------------------------- 电池保护 --- */
    {
        MenuGuard bat(CreatePopupMenu());
        UINT g = FlagsIf(st->avail_battery, MF_GRAYED);
        if (bat.Get() == NULL) return NULL;

        AppendMenuW(bat.Get(), MF_STRING | g, (UINT_PTR)CMD_BATTERY_OFF,
                    L"关闭充电控制 (0-100)");
        AppendMenuW(bat.Get(), MF_SEPARATOR, 0, NULL);
        AppendMenuW(bat.Get(), MF_STRING | g, (UINT_PTR)CMD_BATTERY_P40_70,
                    L"40% - 70%");
        AppendMenuW(bat.Get(), MF_STRING | g, (UINT_PTR)CMD_BATTERY_P70_90,
                    L"70% - 90%");
        AppendMenuW(bat.Get(), MF_STRING | g, (UINT_PTR)CMD_BATTERY_P95_100,
                    L"95% - 100%");
        AppendMenuW(bat.Get(), MF_SEPARATOR, 0, NULL);
        AppendMenuW(bat.Get(),
                    MF_STRING | g | FlagsIf(st->reset_before_disable, MF_CHECKED),
                    (UINT_PTR)CMD_BATTERY_TOGGLE_RESET,
                    L"关闭前先清零阈值 (MACH-WX9 兼容)");
        /* 只读状态行：让用户能看到当前阈值（来自缓存，零成本）。 */
        AppendMenuW(bat.Get(), MF_SEPARATOR, 0, NULL);
        if (st->avail_battery)
            StringCchPrintfW(buf, ARRAYSIZE(buf), L"当前: %d%% - %d%%",
                             st->battery_start, st->battery_end);
        else
            StringCchCopyW(buf, ARRAYSIZE(buf), L"当前: 不可用");
        AppendMenuW(bat.Get(), MF_STRING | MF_GRAYED, (UINT_PTR)CMD_NONE, buf);

        if (!AppendMenuW(main.Get(), MF_POPUP | g, (UINT_PTR)bat.Get(), L"电池保护"))
            return NULL;
        bat.Release();
    }

    /* ----------------------------------------------------------- Fn-Lock --- */
    AppendMenuW(main.Get(),
                MF_STRING | FlagsIf(!st->avail_fnlock, MF_GRAYED) |
                    FlagsIf(st->fnlock_on, MF_CHECKED),
                (UINT_PTR)CMD_FNLOCK_TOGGLE, L"Fn-Lock");

    /* -------------------------------------------------------- 键盘背光 --- */
    {
        MenuGuard kbd(CreatePopupMenu());
        UINT g = FlagsIf(st->avail_kbd, MF_GRAYED);
        UINT tg = FlagsIf(st->avail_kbd && st->kbd_timeout_ok, MF_GRAYED);
        if (kbd.Get() == NULL) return NULL;

        AppendMenuW(kbd.Get(), MF_STRING | g |
                        FlagsIf(st->kbd_level == 0, MF_CHECKED),
                    (UINT_PTR)CMD_KBD_OFF, L"关");
        AppendMenuW(kbd.Get(), MF_STRING | g |
                        FlagsIf(st->kbd_level == 1, MF_CHECKED),
                    (UINT_PTR)CMD_KBD_LOW, L"低");
        AppendMenuW(kbd.Get(), MF_STRING | g |
                        FlagsIf(st->kbd_level == 2, MF_CHECKED),
                    (UINT_PTR)CMD_KBD_HIGH, L"高");
        AppendMenuW(kbd.Get(), MF_SEPARATOR, 0, NULL);

        {
            MenuGuard kbdTo(CreatePopupMenu());
            static const struct { int sec; int id; const WCHAR* text; } kItems[] = {
                { 0,   CMD_KBD_TIMEOUT_0,   L"0 秒（常亮）" },
                { 15,  CMD_KBD_TIMEOUT_15,  L"15 秒" },
                { 30,  CMD_KBD_TIMEOUT_30,  L"30 秒" },
                { 60,  CMD_KBD_TIMEOUT_60,  L"60 秒" },
                { 300, CMD_KBD_TIMEOUT_300, L"300 秒" }
            };
            size_t i;
            if (kbdTo.Get() == NULL) return NULL;

            for (i = 0; i < ARRAYSIZE(kItems); ++i) {
                UINT flags = MF_STRING | tg;
                if (st->kbd_timeout_ok && st->kbd_timeout == kItems[i].sec)
                    flags |= MF_CHECKED;
                AppendMenuW(kbdTo.Get(), flags, (UINT_PTR)kItems[i].id,
                            kItems[i].text);
            }
            if (!AppendMenuW(kbd.Get(), MF_POPUP | tg, (UINT_PTR)kbdTo.Get(),
                             L"熄灭超时"))
                return NULL;
            kbdTo.Release();
        }

        AppendMenuW(kbd.Get(), MF_SEPARATOR, 0, NULL);
        StringCchPrintfW(buf, ARRAYSIZE(buf), L"编码: %s",
                         hw_kbd_encoding_name(&st->dev));
        AppendMenuW(kbd.Get(), MF_STRING | MF_GRAYED, (UINT_PTR)CMD_NONE, buf);

        if (!AppendMenuW(main.Get(), MF_POPUP | g, (UINT_PTR)kbd.Get(), L"键盘背光"))
            return NULL;
        kbd.Release();
    }

    /* -------------------------------------------------------- 智能充电 --- */
    {
        MenuGuard sc(CreatePopupMenu());
        UINT g = FlagsIf(st->avail_smartcharge, MF_GRAYED);
        if (sc.Get() == NULL) return NULL;

        {
            MenuGuard modes(CreatePopupMenu());
            int m;
            if (modes.Get() == NULL) return NULL;
            for (m = 0; m < 4; ++m) {
                StringCchPrintfW(buf, ARRAYSIZE(buf), L"%d", m);
                AppendMenuW(modes.Get(),
                            MF_STRING | g |
                                FlagsIf(st->charge_mode == m, MF_CHECKED),
                            (UINT_PTR)(CMD_SC_MODE_0 + m), buf);
            }
            if (!AppendMenuW(sc.Get(), MF_POPUP | g, (UINT_PTR)modes.Get(), L"模式"))
                return NULL;
            modes.Release();
        }

        {
            MenuGuard params(CreatePopupMenu());
            int p;
            if (params.Get() == NULL) return NULL;
            for (p = 0; p < 3; ++p) {
                StringCchPrintfW(buf, ARRAYSIZE(buf), L"%d", p);
                AppendMenuW(params.Get(),
                            MF_STRING | g |
                                FlagsIf(st->charge_param == p, MF_CHECKED),
                            (UINT_PTR)(CMD_SC_PARAM_0 + p), buf);
            }
            if (!AppendMenuW(sc.Get(), MF_POPUP | g, (UINT_PTR)params.Get(), L"参数"))
                return NULL;
            params.Release();
        }

        AppendMenuW(sc.Get(), MF_SEPARATOR, 0, NULL);
        AppendMenuW(sc.Get(), MF_STRING | g, (UINT_PTR)CMD_SC_APPLY, L"应用当前值");
        AppendMenuW(sc.Get(), MF_SEPARATOR, 0, NULL);
        if (st->avail_smartcharge)
            StringCchPrintfW(buf, ARRAYSIZE(buf), L"当前: mode=%d %d-%d param=%d",
                             st->charge_mode, st->charge_start,
                             st->charge_end, st->charge_param);
        else
            StringCchCopyW(buf, ARRAYSIZE(buf), L"当前: 不可用");
        AppendMenuW(sc.Get(), MF_STRING | MF_GRAYED, (UINT_PTR)CMD_NONE, buf);

        if (!AppendMenuW(main.Get(), MF_POPUP | g, (UINT_PTR)sc.Get(), L"智能充电"))
            return NULL;
        sc.Release();
    }

    /* -------------------------------------------------------- 电源解锁 --- */
    AppendMenuW(main.Get(),
                MF_STRING | FlagsIf(!st->avail_powerunlock, MF_GRAYED) |
                    FlagsIf(st->powerunlock_on, MF_CHECKED),
                (UINT_PTR)CMD_POWERUNLOCK_TOGGLE, L"电源解锁");

    /* ------------------------------------------------------ 麦克风 LED --- */
    {
        MenuGuard mic(CreatePopupMenu());
        UINT g = FlagsIf(st->avail_micled, MF_GRAYED);
        if (mic.Get() == NULL) return NULL;
        /* 无回读：只给动作，不给勾选状态。 */
        AppendMenuW(mic.Get(), MF_STRING | g, (UINT_PTR)CMD_MICLED_ON, L"点亮");
        AppendMenuW(mic.Get(), MF_STRING | g, (UINT_PTR)CMD_MICLED_OFF, L"熄灭");
        if (!AppendMenuW(main.Get(), MF_POPUP | g, (UINT_PTR)mic.Get(), L"麦克风 LED"))
            return NULL;
        mic.Release();
    }

    /* -------------------------------------------------------- 顶层命令 --- */
    AppendMenuW(main.Get(), MF_SEPARATOR, 0, NULL);
    AppendMenuW(main.Get(), MF_STRING, (UINT_PTR)CMD_REFRESH, L"刷新状态");
    AppendMenuW(main.Get(), MF_STRING, (UINT_PTR)CMD_OPEN_LOG, L"打开日志");
    AppendMenuW(main.Get(), MF_STRING, (UINT_PTR)CMD_ABOUT, L"关于");
    AppendMenuW(main.Get(), MF_STRING, (UINT_PTR)CMD_EXIT, L"退出");

    {
        HMENU h = main.Get();
        main.Release();
        return h;
    }
}

} /* namespace */

/* ============================================================================
 * BuildAppMenu — 构建 + 弹出 + 销毁，返回所选命令
 * ----------------------------------------------------------------------------
 * 坐标说明：本程序使用托盘回调的「默认版本」（不调用 NIM_SETVERSION v4），
 * 因此回调 lParam 是鼠标消息而不是坐标（v4 才是 MAKELPARAM(x,y)）。
 * 所以这里用 GetCursorPos() 取位置——既符合默认版本的语义，也避免用
 * GET_X_LPARAM(lParam) 解出垃圾坐标。
 * ========================================================================== */
int BuildAppMenu(HWND hwnd, const AppState* st)
{
    HMENU hMenu;
    POINT pt;
    int sel;

    if (hwnd == NULL || st == NULL) return CMD_NONE;

    hMenu = BuildMenuTree(st);
    if (hMenu == NULL) {
        hw_log(L"菜单构建失败（资源不足？）");
        return CMD_NONE;
    }

    if (!GetCursorPos(&pt)) {
        pt.x = 0;
        pt.y = 0;
    }

    SetForegroundWindow(hwnd);   /* 必须：否则菜单在点击别处时不消失 */
    sel = (int)TrackPopupMenu(hMenu,
                              TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN,
                              pt.x, pt.y, 0, hwnd, NULL);
    PostMessageW(hwnd, WM_NULL, 0, 0);   /* 必须：清掉菜单的隐藏状态 */
    DestroyMenu(hMenu);

    return (sel > 0) ? sel : CMD_NONE;
}

/* ============================================================================
 * 命令执行
 * ----------------------------------------------------------------------------
 * 统一策略：set 成功 -> 就地更新缓存（不整轮重读）；
 *           set 失败 -> 只写日志 + 立刻定点重读该项（不用 balloon 打断用户）。
 * ========================================================================== */
namespace {

static void ApplyBattery(AppState* st, int start, int end)
{
    HwStatus s;

    hw_log(L"电池保护：请求 %d%% - %d%%（关闭前清零=%s）",
           start, end, st->reset_before_disable ? L"开" : L"关");
    s = hw_battery_set(&st->dev, start, end,
                       st->reset_before_disable ? 1 : 0);
    hw_log_result(L"电池保护", s);
    if (s == HW_OK) {
        st->battery_start = start;
        st->battery_end = end;
    } else {
        AppProbeBattery(st);
    }
}

static void ToggleFnLock(AppState* st)
{
    int target = st->fnlock_on ? 0 : 1;
    HwStatus s = hw_fnlock_set(&st->dev, target);

    hw_log_result(L"Fn-Lock", s);
    if (s == HW_OK) st->fnlock_on = target;
    else            AppProbeFnLock(st);
}

static void SetKbdLevel(AppState* st, int level)
{
    HwStatus s = hw_kbd_set_level(&st->dev, level);

    hw_log_result(L"键盘背光", s);
    if (s == HW_OK) st->kbd_level = level;
    else            AppProbeKbd(st);
}

static void SetKbdTimeout(AppState* st, int sec)
{
    HwStatus s = hw_kbd_set_timeout(&st->dev, sec);

    hw_log_result(L"键盘背光熄灭超时", s);
    if (s == HW_OK) {
        st->kbd_timeout = sec;
        st->kbd_timeout_ok = 1;
    } else {
        AppProbeKbdTimeout(st);
    }
}

static void SetChargeMode(AppState* st, int mode)
{
    HwStatus s = hw_smartcharge_set(&st->dev, mode, st->charge_unknown,
                                    st->charge_start, st->charge_end);

    hw_log_result(L"智能充电模式", s);
    if (s == HW_OK) st->charge_mode = mode;
    else            AppProbeSmartCharge(st);
}

static void SetChargeParam(AppState* st, int param)
{
    HwStatus s = hw_smartcharge_set_param(&st->dev, param);

    hw_log_result(L"智能充电参数", s);
    if (s == HW_OK) st->charge_param = param;
    else            AppProbeSmartCharge(st);
}

static void ApplySmartCharge(AppState* st)
{
    HwStatus s = hw_smartcharge_set(&st->dev, st->charge_mode,
                                    st->charge_unknown, st->charge_start,
                                    st->charge_end);

    hw_log_result(L"智能充电（应用当前值）", s);
    if (s != HW_OK) {
        AppProbeSmartCharge(st);
        return;
    }
    s = hw_smartcharge_set_param(&st->dev, st->charge_param);
    hw_log_result(L"智能充电参数（应用当前值）", s);
    if (s != HW_OK) AppProbeSmartCharge(st);
}

static void TogglePowerUnlock(AppState* st)
{
    int target = st->powerunlock_on ? 0 : 1;
    HwStatus s = hw_powerunlock_set(&st->dev, target);

    hw_log_result(L"电源解锁", s);
    if (s == HW_OK) st->powerunlock_on = target;
    else            AppProbePowerUnlock(st);
}

static void SetMicLed(AppState* st, int on)
{
    HwStatus s = hw_micled_set(&st->dev, on);

    hw_log_result(on ? L"麦克风 LED（点亮）" : L"麦克风 LED（熄灭）", s);
    if (s == HW_ERR_NOT_SUPPORTED) {
        /* 无 GET 可回读：固件明确说不支持时，直接把菜单项置灰。
         * 设备未连接等临时错误不改变可用性（下次还可重试）。 */
        st->avail_micled = 0;
    }
}

} /* namespace */

void HandleMenuCommand(HWND hwnd, int cmd, AppState* st)
{
    UNREFERENCED_PARAMETER(hwnd);   /* 失败只写日志，不弹框 */

    if (st == NULL) return;

    switch (cmd) {
    /* ---- 电池保护 ---- */
    case CMD_BATTERY_OFF:      if (st->avail_battery) ApplyBattery(st, 0, 100);   break;
    case CMD_BATTERY_P40_70:   if (st->avail_battery) ApplyBattery(st, 40, 70);   break;
    case CMD_BATTERY_P70_90:   if (st->avail_battery) ApplyBattery(st, 70, 90);   break;
    case CMD_BATTERY_P95_100:  if (st->avail_battery) ApplyBattery(st, 95, 100);  break;
    case CMD_BATTERY_TOGGLE_RESET:
        st->reset_before_disable = st->reset_before_disable ? 0 : 1;
        hw_log(L"兼容选项：关闭前先清零阈值 = %s",
               st->reset_before_disable ? L"开" : L"关");
        break;

    /* ---- Fn-Lock ---- */
    case CMD_FNLOCK_TOGGLE:
        if (st->avail_fnlock) ToggleFnLock(st);
        break;

    /* ---- 键盘背光 ---- */
    case CMD_KBD_OFF:  if (st->avail_kbd) SetKbdLevel(st, 0); break;
    case CMD_KBD_LOW:  if (st->avail_kbd) SetKbdLevel(st, 1); break;
    case CMD_KBD_HIGH: if (st->avail_kbd) SetKbdLevel(st, 2); break;
    case CMD_KBD_TIMEOUT_0:
    case CMD_KBD_TIMEOUT_15:
    case CMD_KBD_TIMEOUT_30:
    case CMD_KBD_TIMEOUT_60:
    case CMD_KBD_TIMEOUT_300: {
        int ok = 0;
        int sec = KbdTimeoutSeconds(cmd, &ok);
        if (ok && st->avail_kbd) SetKbdTimeout(st, sec);
        break;
    }

    /* ---- 智能充电 ---- */
    case CMD_SC_MODE_0:
    case CMD_SC_MODE_1:
    case CMD_SC_MODE_2:
    case CMD_SC_MODE_3:
        if (st->avail_smartcharge)
            SetChargeMode(st, cmd - CMD_SC_MODE_0);
        break;
    case CMD_SC_PARAM_0:
    case CMD_SC_PARAM_1:
    case CMD_SC_PARAM_2:
        if (st->avail_smartcharge)
            SetChargeParam(st, cmd - CMD_SC_PARAM_0);
        break;
    case CMD_SC_APPLY:
        if (st->avail_smartcharge) ApplySmartCharge(st);
        break;

    /* ---- 电源解锁 ---- */
    case CMD_POWERUNLOCK_TOGGLE:
        if (st->avail_powerunlock) TogglePowerUnlock(st);
        break;

    /* ---- 麦克风 LED ---- */
    case CMD_MICLED_ON:  if (st->avail_micled) SetMicLed(st, 1); break;
    case CMD_MICLED_OFF: if (st->avail_micled) SetMicLed(st, 0); break;

    /* ---- 顶层命令由 main.cpp 统一分发，这里只兜底 ---- */
    case CMD_NONE:
    case CMD_SENSORS:
    case CMD_REFRESH:
    case CMD_OPEN_LOG:
    case CMD_ABOUT:
    case CMD_EXIT:
    default:
        hw_log(L"未处理的菜单命令：%d", cmd);
        break;
    }
}
