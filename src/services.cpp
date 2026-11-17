/* ============================================================================
 * services.cpp — hwapi.h 的全部业务函数 + 状态/名称字符串
 * ----------------------------------------------------------------------------
 * 只做三件事：
 *   1. 参数校验（越界 → HW_ERR_ARG）；
 *   2. 把命令编码交给 proto.h（hw_cmd_*），把响应解析交给 proto.h
 *      （hw_parse_* / hw_kbd_*，它们内部已做长度校验）；
 *   3. 映射成 HwStatus。
 *
 * 本文件绝不直接下标访问固件缓冲区；所有字节语义都在 proto.h。
 * ========================================================================== */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>

#include <stdint.h>

#include "hwapi.h"

/* wmi.cpp 提供的内部入口（签名必须与 wmi.cpp 中的定义一致）。 */
extern "C" HwStatus hw_wmi_call(HwDevice* dev, uint64_t cmd,
                                uint8_t* out, int* out_len);

/* ------------------------------------------------------------------ 内部工具 */

/* 设备是否已连接。 */
static int hwc_ready(const HwDevice* dev)
{
    return dev != NULL && dev->available != 0 && dev->_p != NULL;
}

/* 一个 0/1 布尔入参的校验。 */
static int hwc_bool_arg(int on)
{
    return (on == 0 || on == 1) ? 1 : 0;
}

/* 电池阈值 0..100。 */
static int hwc_percent_arg(int v)
{
    return (v >= 0 && v <= 100) ? 1 : 0;
}

/* ==========================================================================
 * 生命周期（hw_device_init / hw_open / hw_close 在 wmi.cpp 中实现）
 * ========================================================================== */

/* ==========================================================================
 * 字符串
 * ========================================================================== */

const WCHAR* hw_protocol_name(const HwDevice* dev)
{
    if (!dev) return L"未知";
    switch (dev->protocol) {
    case HW_PROTO_U64:
        return L"U64";
    case HW_PROTO_U8:
        return L"U8";
    default:
        return L"未知";
    }
}

const WCHAR* hw_kbd_encoding_name(const HwDevice* dev)
{
    if (!dev) return L"未知";
    switch (dev->kbd_encoding) {
    case HW_KBD_PERCENT:
        return L"百分比";
    case HW_KBD_BITMASK_NORMAL:
        return L"位掩码";
    case HW_KBD_BITMASK_INVERTED:
        return L"位掩码(反转)";
    default:
        return L"未知";
    }
}

const WCHAR* hw_status_text(HwStatus st)
{
    switch (st) {
    case HW_OK:
        return L"成功";
    case HW_ERR_NOT_ADMIN:
        return L"需要管理员权限";
    case HW_ERR_NO_DEVICE:
        return L"未找到 HWMI 设备实例";
    case HW_ERR_NOT_SUPPORTED:
        return L"当前机型不支持该功能";
    case HW_ERR_COM:
        return L"WMI 调用失败";
    case HW_ERR_BUFFER:
        return L"固件响应异常";
    case HW_ERR_ARG:
        return L"参数越界";
    case HW_ERR_UNKNOWN:
    default:
        return L"未知错误";
    }
}

/* ==========================================================================
 * 电池保护
 * ========================================================================== */

HwStatus hw_battery_get(HwDevice* dev, int* start, int* end)
{
    uint8_t buf[HW_OUT_MAX];
    int n = 0;
    int s = 0;
    int e = 0;
    HwStatus st;

    if (!dev) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    st = hw_wmi_call(dev, hw_cmd_simple(HW_C_BATTERY_THRESH_GET), buf, &n);
    if (st != HW_OK) return st;

    if (!hw_parse_battery_threshold(buf, n, &s, &e)) return HW_ERR_BUFFER;

    if (start) *start = s;
    if (end)   *end = e;
    return HW_OK;
}

HwStatus hw_battery_set(HwDevice* dev, int start, int end, int reset_before_disable)
{
    if (!dev) return HW_ERR_ARG;
    if (!hwc_percent_arg(start) || !hwc_percent_arg(end)) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    /* MACH-WX9 兼容路径：关闭保护前先写 (0,0) 并等待 1 秒。
     * 与 C# 版一致：这一步的结果不参与成败判定。 */
    if (reset_before_disable && start == 0 && end == 100) {
        (void)hw_wmi_call(dev, hw_cmd2(HW_C_BATTERY_THRESH_SET, 0, 0),
                          NULL, NULL);
        Sleep(1000);
    }

    return hw_wmi_call(dev,
                       hw_cmd2(HW_C_BATTERY_THRESH_SET,
                               (uint8_t)start, (uint8_t)end),
                       NULL, NULL);
}

/* ==========================================================================
 * Fn-Lock
 * ========================================================================== */

HwStatus hw_fnlock_get(HwDevice* dev, int* on)
{
    uint8_t buf[HW_OUT_MAX];
    int n = 0;
    int state = 0;
    HwStatus st;

    if (!dev) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    st = hw_wmi_call(dev, hw_cmd_simple(HW_C_FN_LOCK_GET), buf, &n);
    if (st != HW_OK) return st;

    if (!hw_parse_fnlock(buf, n, &state)) return HW_ERR_BUFFER;

    if (on) *on = state;
    return HW_OK;
}

HwStatus hw_fnlock_set(HwDevice* dev, int on)
{
    if (!dev) return HW_ERR_ARG;
    if (!hwc_bool_arg(on)) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    return hw_wmi_call(dev, hw_cmd1(HW_C_FN_LOCK_SET, (uint8_t)(on ? 2 : 1)),
                       NULL, NULL);
}

/* ==========================================================================
 * 键盘背光
 * ========================================================================== */

HwStatus hw_kbd_get_level(HwDevice* dev, int* level)
{
    uint8_t buf[HW_OUT_MAX];
    int n = 0;
    int lv = 0;
    HwStatus st;

    if (!dev) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    st = hw_wmi_call(dev, hw_cmd_simple(HW_C_KBDLIGHT_GET), buf, &n);
    if (st != HW_OK) return st;

    /* 读取时顺带补上编码探测（若 hw_open 时仍未知）。 */
    if (dev->kbd_encoding == HW_KBD_UNKNOWN)
        dev->kbd_encoding = hw_kbd_classify(buf, n);

    if (!hw_kbd_decode_level(dev->kbd_encoding, buf, n, &lv))
        return HW_ERR_NOT_SUPPORTED;

    /* 位掩码正序在 b[2]==1 时会算出负数，夹到合法档位。 */
    if (lv < 0) lv = 0;
    if (lv > 2) lv = 2;

    if (level) *level = lv;
    return HW_OK;
}

HwStatus hw_kbd_set_level(HwDevice* dev, int level)
{
    HwKbdEncoding enc;

    if (!dev) return HW_ERR_ARG;
    if (level < 0 || level > 2) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    /* 编码未知时 hw_kbd_encode_set 按位掩码正序处理（与 C# 默认分支一致）。 */
    enc = dev->kbd_encoding;
    return hw_wmi_call(dev,
                       hw_cmd1(HW_C_KBDLIGHT_SET, hw_kbd_encode_set(enc, level)),
                       NULL, NULL);
}

HwStatus hw_kbd_get_timeout(HwDevice* dev, int* sec)
{
    uint8_t buf[HW_OUT_MAX];
    int n = 0;
    int value = 0;
    HwStatus st;

    if (!dev) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    st = hw_wmi_call(dev, hw_cmd_simple(HW_C_KBDLIGHT_TIMEOUT_GET), buf, &n);
    if (st != HW_OK) return st;

    if (!hw_parse_kbd_timeout(buf, n, &value)) return HW_ERR_BUFFER;

    if (sec) *sec = value;
    return HW_OK;
}

HwStatus hw_kbd_set_timeout(HwDevice* dev, int sec)
{
    if (!dev) return HW_ERR_ARG;
    if (sec < 0 || sec > 65535) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    return hw_wmi_call(dev,
                       hw_cmd2(HW_C_KBDLIGHT_TIMEOUT_SET,
                               (uint8_t)(sec & 0xFF),
                               (uint8_t)((sec >> 8) & 0xFF)),
                       NULL, NULL);
}

/* ==========================================================================
 * 智能充电（华为私有）
 * ========================================================================== */

HwStatus hw_smartcharge_get(HwDevice* dev, int* mode, int* unknown,
                            int* start, int* end)
{
    uint8_t buf[HW_OUT_MAX];
    int n = 0;
    int m = 0;
    int u = 0;
    int s = 0;
    int e = 0;
    HwStatus st;

    if (!dev) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    st = hw_wmi_call(dev, hw_cmd_simple(HW_C_BATTERY_CHARGE_MODE_GET), buf, &n);
    if (st != HW_OK) return st;

    if (!hw_parse_charge_mode(buf, n, &m, &u, &s, &e)) return HW_ERR_BUFFER;

    if (mode)    *mode = m;
    if (unknown) *unknown = u;
    if (start)   *start = s;
    if (end)     *end = e;
    return HW_OK;
}

HwStatus hw_smartcharge_set(HwDevice* dev, int mode, int unknown,
                            int start, int end)
{
    if (!dev) return HW_ERR_ARG;
    if (mode < 0 || mode > 3) return HW_ERR_ARG;
    if (unknown < 0 || unknown > 255) return HW_ERR_ARG;
    if (!hwc_percent_arg(start) || !hwc_percent_arg(end)) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    return hw_wmi_call(dev,
                       hw_cmd4(HW_C_BATTERY_CHARGE_MODE_SET,
                               (uint8_t)mode, (uint8_t)unknown,
                               (uint8_t)start, (uint8_t)end),
                       NULL, NULL);
}

HwStatus hw_smartcharge_get_param(HwDevice* dev, int* value)
{
    uint8_t buf[HW_OUT_MAX];
    int n = 0;
    int v = 0;
    HwStatus st;

    if (!dev) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    st = hw_wmi_call(dev, hw_cmd_simple(HW_C_BATTERY_CHARGE_MODE_PARAM_GET),
                     buf, &n);
    if (st != HW_OK) return st;

    if (!hw_parse_charge_param(buf, n, &v)) return HW_ERR_BUFFER;

    if (value) *value = v;
    return HW_OK;
}

HwStatus hw_smartcharge_set_param(HwDevice* dev, int value)
{
    if (!dev) return HW_ERR_ARG;
    if (value < 0 || value > 2) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    return hw_wmi_call(dev,
                       hw_cmd1(HW_C_BATTERY_CHARGE_MODE_PARAM_SET,
                               (uint8_t)value),
                       NULL, NULL);
}

/* ==========================================================================
 * 电源解锁（Fn+P）
 * ========================================================================== */

HwStatus hw_powerunlock_get(HwDevice* dev, int* on)
{
    uint8_t buf[HW_OUT_MAX];
    int n = 0;
    int state = 0;
    HwStatus st;

    if (!dev) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    st = hw_wmi_call(dev, hw_cmd_simple(HW_C_POWER_UNLOCK_GET), buf, &n);
    if (st != HW_OK) return st;

    if (!hw_parse_powerunlock(buf, n, &state)) return HW_ERR_BUFFER;

    if (on) *on = state;
    return HW_OK;
}

HwStatus hw_powerunlock_set(HwDevice* dev, int on)
{
    if (!dev) return HW_ERR_ARG;
    if (!hwc_bool_arg(on)) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    return hw_wmi_call(dev,
                       hw_cmd1(HW_C_POWER_UNLOCK_SET, (uint8_t)(on ? 1 : 0)),
                       NULL, NULL);
}

/* ==========================================================================
 * 麦克风静音 LED（只有 SET，无法回读）
 * ========================================================================== */

HwStatus hw_micled_set(HwDevice* dev, int on)
{
    if (!dev) return HW_ERR_ARG;
    if (!hwc_bool_arg(on)) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    return hw_wmi_call(dev,
                       hw_cmd1(HW_C_MICMUTE_LED_SET, (uint8_t)(on ? 1 : 0)),
                       NULL, NULL);
}

/* ==========================================================================
 * 传感器
 * ========================================================================== */

HwStatus hw_fan_speed(HwDevice* dev, int num, int* rpm)
{
    uint8_t buf[HW_OUT_MAX];
    int n = 0;
    int value = 0;
    HwStatus st;

    if (!dev) return HW_ERR_ARG;
    if (num != 0 && num != 1) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    st = hw_wmi_call(dev, hw_cmd1(HW_C_FAN_SPEED_GET, (uint8_t)num), buf, &n);
    if (st != HW_OK) return st;

    if (!hw_parse_u16le(buf, n, &value)) return HW_ERR_BUFFER;

    if (rpm) *rpm = value;
    return HW_OK;
}

HwStatus hw_temp(HwDevice* dev, int idx, int* celsius)
{
    uint8_t buf[HW_OUT_MAX];
    int n = 0;
    int value = 0;
    HwStatus st;

    if (!dev) return HW_ERR_ARG;
    if (idx < 0 || idx > 255) return HW_ERR_ARG;
    if (!hwc_ready(dev)) return HW_ERR_UNKNOWN;

    st = hw_wmi_call(dev, hw_cmd1(HW_C_TEMP_GET, (uint8_t)idx), buf, &n);
    if (st != HW_OK) return st;

    if (!hw_parse_temp(buf, n, &value)) return HW_ERR_BUFFER;

    if (celsius) *celsius = value;
    return HW_OK;
}
