/* ============================================================================
 * proto.h — 华为 ACPI-WMI 固件协议：常量 + 纯编解码函数
 * ----------------------------------------------------------------------------
 * 本文件是「冻结契约」的一部分，由 Lead 维护。
 * 内容逐字移植自 C# 版的 WmiConstants.cs / IWmiProtocol.EncodeCommand /
 * 各 Service 的 Parse* 方法，数值必须与 Linux huawei-wmi 驱动保持一致。
 *
 * 约束：
 *   - 只允许 header-only 的 static inline 纯函数，不得引入任何全局状态。
 *   - 不得 include <windows.h>，保持可在任意平台做单元测试。
 *   - 所有解析函数必须自行做长度校验（C# 版靠抛异常，C 版必须返回值报错）。
 * ========================================================================== */
#ifndef HWC_PROTO_H
#define HWC_PROTO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------- WMI 路径 */
/* 命名空间用反斜杠形式（IWbemLocator::ConnectServer 接受 "ROOT\\WMI"）。 */
#define HW_WMI_NAMESPACE      L"ROOT\\WMI"
#define HW_WMI_CLASS          L"OemWMIMethod"
#define HW_WMI_METHOD         L"OemWMIfun"
#define HW_WMI_OUT_PARAM      L"u8Output"
#define HW_WMI_IN_U64         L"u64Input"
#define HW_WMI_IN_U8          L"u8Input"
#define HW_WMI_INST_PROP      L"InstanceName"
/* 优先选择 InstanceName 含此子串的实例，否则退化为列表中第一个。 */
#define HW_WMI_INST_FILTER    L"HWMI"

/* -------------------------------------------------------- 协议/编码枚举 */
typedef enum HwProtocol {
    HW_PROTO_UNKNOWN = 0,
    HW_PROTO_U64     = 1,   /* 新固件：u64Input（VT_UI8）            */
    HW_PROTO_U8      = 2    /* 老固件：u8Input（64 字节 VT_UI1 数组） */
} HwProtocol;

typedef enum HwKbdEncoding {
    HW_KBD_UNKNOWN           = 0,  /* 固件不支持背光读取 */
    HW_KBD_BITMASK_NORMAL    = 1,  /* 位掩码正序：bit 位置 - 2 = 级别 */
    HW_KBD_BITMASK_INVERTED  = 2,  /* MACH-WX9 等：bit 0 = 最亮 */
    HW_KBD_PERCENT           = 3   /* b[2] = 0x00/0x32/0x64 */
} HwKbdEncoding;

/* ------------------------------------------------------------- 命令号 */
/* 低 16 位为命令号；参数由 hw_cmd_encode 填到第 2 字节起。 */
#define HW_C_BATTERY_THRESH_GET              0x00001103ull
#define HW_C_BATTERY_THRESH_SET              0x00001003ull
#define HW_C_FN_LOCK_GET                     0x00000604ull
#define HW_C_FN_LOCK_SET                     0x00000704ull
#define HW_C_KBDLIGHT_GET                    0x00000602ull
#define HW_C_KBDLIGHT_SET                    0x00000702ull
#define HW_C_MICMUTE_LED_SET                 0x00000b04ull
#define HW_C_KBDLIGHT_TIMEOUT_SET            0x00001106ull
#define HW_C_KBDLIGHT_TIMEOUT_GET            0x00001206ull
#define HW_C_POWER_UNLOCK_SET                0x00000F04ull
#define HW_C_POWER_UNLOCK_GET                0x00000E04ull
#define HW_C_FAN_SPEED_GET                   0x00000802ull
#define HW_C_TEMP_GET                        0x00000202ull
#define HW_C_BATTERY_CHARGE_MODE_GET         0x00001603ull
#define HW_C_BATTERY_CHARGE_MODE_SET         0x00001503ull
#define HW_C_BATTERY_CHARGE_MODE_PARAM_GET   0x00001303ull
#define HW_C_BATTERY_CHARGE_MODE_PARAM_SET   0x00001203ull

/* ------------------------------------------------------------- 协议常量 */
#define HW_STATUS_SUCCESS        0u   /* u8Output[0] 为该值表示成功 */
#define HW_STATUS_DEFAULT_ERROR  1u   /* 缓冲区为空时的默认错误码   */
#define HW_U8_BUFFER_LENGTH      64   /* 老固件 u8Input 数组长度     */
#define HW_U8_CMD_BYTES          8    /* 命令值占用的前 8 字节       */
#define HW_CMD_ARG_OFFSET        2    /* 参数起始字节偏移            */
#define HW_OUT_MAX               256  /* u8Output 最大长度（MOF MAX(256)） */

/* 键盘背光编码常量 */
#define HW_KBD_PERCENT_FLAG      0x01u
#define HW_KBD_INVERTED_FLAG     0xFFu
#define HW_KBD_BITMASK_OFFSET    2

/* 电池阈值数据起始偏移 */
#define HW_BATTERY_THRESH_DATA_OFFSET  2

/* ------------------------------------------------------- 命令编码（核心） */
/* 与 C# 版 IWmiProtocol.EncodeCommand 逐字等价：
 *   v = cmd; for (i) v |= args[i] << (8 * (2 + i))
 * 例：cmd=0x1003, args={0,100} → 0x64001003 */
static inline uint64_t hw_cmd_encode(uint64_t cmd, const uint8_t* args, int nargs)
{
    uint64_t v = cmd;
    int i;
    if (args == NULL || nargs <= 0) return v;
    /* 最多容纳 6 个参数（2..7 字节），超出部分按 C# 行为自然溢出丢弃。 */
    if (nargs > 6) nargs = 6;
    for (i = 0; i < nargs; ++i)
        v |= ((uint64_t)args[i]) << (8 * (HW_CMD_ARG_OFFSET + i));
    return v;
}

/* 便捷封装：不带参数 */
static inline uint64_t hw_cmd_simple(uint64_t cmd) { return cmd; }

/* 1 参数 */
static inline uint64_t hw_cmd1(uint64_t cmd, uint8_t a0)
{
    return hw_cmd_encode(cmd, &a0, 1);
}

/* 2 参数 */
static inline uint64_t hw_cmd2(uint64_t cmd, uint8_t a0, uint8_t a1)
{
    uint8_t a[2]; a[0] = a0; a[1] = a1;
    return hw_cmd_encode(cmd, a, 2);
}

/* 4 参数 */
static inline uint64_t hw_cmd4(uint64_t cmd, uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3)
{
    uint8_t a[4]; a[0] = a0; a[1] = a1; a[2] = a2; a[3] = a3;
    return hw_cmd_encode(cmd, a, 4);
}

/* ==========================================================================
 * 响应缓冲区解析
 * buf/buf_len 为 u8Output；buf[0] 是状态码，解析时跳过。
 * 每个函数返回 false 表示缓冲区长度不足（协议违例），true 表示解析成功。
 * ========================================================================== */

/* 电池阈值：从尾部向前找第一个非零字节 i，结果为 (b[i-1], b[i])。
 * 全零时返回 (0,0)——与 C# 版一致，属正常值而非失败。 */
static inline int hw_parse_battery_threshold(const uint8_t* b, int len,
                                             int* start, int* end)
{
    int i;
    if (start) *start = 0;
    if (end)   *end   = 0;
    if (b == NULL || len < HW_BATTERY_THRESH_DATA_OFFSET + 1) return 0;
    for (i = len - 1; i >= HW_BATTERY_THRESH_DATA_OFFSET; --i) {
        if (b[i] != 0) {
            if (start) *start = b[i - 1];
            if (end)   *end   = b[i];
            return 1;
        }
    }
    return 1;
}

/* Fn-Lock：第一个非零 b[i]，状态 = ((b[i]-1) == 1)。 */
static inline int hw_parse_fnlock(const uint8_t* b, int len, int* on)
{
    int i;
    if (on) *on = 0;
    if (b == NULL || len < 2) return 0;
    for (i = 1; i < len; ++i) {
        if (b[i] != 0) {
            if (on) *on = ((b[i] - 1) == 1) ? 1 : 0;
            return 1;
        }
    }
    return 1;
}

/* 电源解锁：b[1] == 1 */
static inline int hw_parse_powerunlock(const uint8_t* b, int len, int* on)
{
    if (on) *on = 0;
    if (b == NULL || len < 2) return 0;
    if (on) *on = (b[1] == 1) ? 1 : 0;
    return 1;
}

/* 风扇转速：b[1] | (b[2] << 8) */
static inline int hw_parse_u16le(const uint8_t* b, int len, int* value)
{
    if (value) *value = 0;
    if (b == NULL || len < 3) return 0;
    if (value) *value = (int)(b[1] | ((int)b[2] << 8));
    return 1;
}

/* 温度：b[2]（摄氏度） */
static inline int hw_parse_temp(const uint8_t* b, int len, int* celsius)
{
    if (celsius) *celsius = 0;
    if (b == NULL || len < 3) return 0;
    if (celsius) *celsius = (int)b[2];
    return 1;
}

/* 键盘背光超时（秒）：b[1] | (b[2] << 8) */
static inline int hw_parse_kbd_timeout(const uint8_t* b, int len, int* sec)
{
    if (sec) *sec = 0;
    if (b == NULL || len < 3) return 0;
    if (sec) *sec = (int)(b[1] | ((int)b[2] << 8));
    return 1;
}

/* 智能充电模式：(mode, unknown, start, end) = b[1..4] */
static inline int hw_parse_charge_mode(const uint8_t* b, int len,
                                       int* mode, int* unknown, int* start, int* end)
{
    if (mode)    *mode    = 0;
    if (unknown) *unknown = 0;
    if (start)   *start   = 0;
    if (end)     *end     = 0;
    if (b == NULL || len < 5) return 0;
    if (mode)    *mode    = (int)b[1];
    if (unknown) *unknown = (int)b[2];
    if (start)   *start   = (int)b[3];
    if (end)     *end     = (int)b[4];
    return 1;
}

/* 智能充电附加参数：b[1] */
static inline int hw_parse_charge_param(const uint8_t* b, int len, int* value)
{
    if (value) *value = 0;
    if (b == NULL || len < 2) return 0;
    if (value) *value = (int)b[1];
    return 1;
}

/* ==========================================================================
 * 键盘背光编码判定 / 级别解码 / 设置值编码
 * ========================================================================== */

/* 由一次 C_KBDLIGHT_GET 的响应判定编码类型。 */
static inline HwKbdEncoding hw_kbd_classify(const uint8_t* b, int len)
{
    if (b == NULL || len < 3) return HW_KBD_UNKNOWN;

    /* 百分比编码：b[1] == 0x01 且 b[2] ∈ {0x00, 0x32, 0x64} */
    if (b[1] == HW_KBD_PERCENT_FLAG &&
        (b[2] == 0x00 || b[2] == 0x32 || b[2] == 0x64))
        return HW_KBD_PERCENT;

    /* 位掩码编码：b[2] 非零，靠 b[1] 区分正序/倒序 */
    if (b[2] != 0)
        return (b[1] == HW_KBD_INVERTED_FLAG) ? HW_KBD_BITMASK_INVERTED
                                              : HW_KBD_BITMASK_NORMAL;

    return HW_KBD_UNKNOWN;
}

/* 解码背光级别（0=关 1=低 2=高）。返回 false 表示编码未知或缓冲区不足。 */
static inline int hw_kbd_decode_level(HwKbdEncoding enc, const uint8_t* b, int len, int* level)
{
    if (level) *level = 0;
    if (b == NULL || len < 3) return 0;
    switch (enc) {
    case HW_KBD_PERCENT:
        if (b[2] == 0x00)      *level = 0;
        else if (b[2] <= 0x32) *level = 1;
        else                   *level = 2;
        return 1;

    case HW_KBD_BITMASK_NORMAL:
    case HW_KBD_BITMASK_INVERTED: {
        int lv = 0;
        uint8_t v = b[2];
        while ((v >>= 1) != 0) ++lv;
        *level = (enc == HW_KBD_BITMASK_INVERTED) ? lv : (lv - HW_KBD_BITMASK_OFFSET);
        return 1;
    }
    default:
        return 0;
    }
}

/* 把 0..2 的级别编成写入 C_KBDLIGHT_SET 的参数字节。 */
static inline uint8_t hw_kbd_encode_set(HwKbdEncoding enc, int level)
{
    static const uint8_t kPercent[3] = { 0x00, 0x32, 0x64 };

    if (level < 0) level = 0;
    if (level > 2) level = 2;

    switch (enc) {
    case HW_KBD_PERCENT:
        return kPercent[level];
    case HW_KBD_BITMASK_INVERTED:
        return (uint8_t)(1u << level);
    case HW_KBD_BITMASK_NORMAL:
    default:
        return (uint8_t)(1u << (level + HW_KBD_BITMASK_OFFSET));
    }
}

/* ==========================================================================
 * 温度传感器区域表
 * 与 C# WmiConstants.TempZones 一致，顺序即 UI 显示顺序。
 * ========================================================================== */
typedef struct HwTempZone {
    const wchar_t* label;   /* 显示名 */
    int            idx;     /* 固件索引 */
} HwTempZone;

/* 定义放在 proto.c 中（避免每个 TU 各存一份）。 */
extern const HwTempZone kHwTempZones[];
extern const int       kHwTempZoneCount;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* HWC_PROTO_H */
