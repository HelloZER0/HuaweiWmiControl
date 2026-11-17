/* ============================================================================
 * test_proto.c — proto.h 纯逻辑测试（零依赖，不碰 Windows.h / COM / 硬件）
 * ----------------------------------------------------------------------------
 * 为什么这个测试值得存在：
 *   services.cpp / wmi.cpp 需要真实华为固件才能验证，CI 与开发机都没有。
 *   但「命令编码」和「响应解析」是纯函数，且是全部风险所在——编码错一个字节，
 *   写进固件的值就是错的。
 *
 * 尤其 hw_cmd_encode 有外部证据可交叉验证：
 *   docs/reverse-engineering.md 用 x64dbg 抓到 PC Manager 实际下发的 4 个
 *   十进制值，换算成十六进制后必须被 hw_cmd_encode 精确复现：
 *       1677725699 = 0x64001003  (关闭 0-100)
 *       1514541059 = 0x5A461003  (70-90)
 *       1177030659 = 0x46281003  (40-70)
 *       1683951619 = 0x645F1003  (95-100)
 *   这条对应下面的 test_encode_matches_captured_firmware_values()。
 *
 * 编译运行：
 *   MSVC : cl /utf-8 /W4 /Fe:test_proto.exe test_proto.c ..\src\proto.c
 *   MinGW: gcc -std=c11 -Wall -Wextra -o test_proto test_proto.c ../src/proto.c
 *   返回码 0 = 全部通过。
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "../src/proto.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (cond) {                                                           \
            ++g_pass;                                                         \
        } else {                                                              \
            ++g_fail;                                                         \
            printf("  FAIL  %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                     \
    } while (0)

#define CHECK_EQ_U64(actual, expected)                                        \
    do {                                                                      \
        unsigned long long a_ = (unsigned long long)(actual);                 \
        unsigned long long e_ = (unsigned long long)(expected);               \
        if (a_ == e_) {                                                       \
            ++g_pass;                                                         \
        } else {                                                              \
            ++g_fail;                                                         \
            printf("  FAIL  %s:%d  %s == 0x%016llX, 期望 0x%016llX\n",         \
                   __FILE__, __LINE__, #actual, a_, e_);                      \
        }                                                                     \
    } while (0)

#define CHECK_EQ_INT(actual, expected)                                        \
    do {                                                                      \
        long a_ = (long)(actual);                                             \
        long e_ = (long)(expected);                                           \
        if (a_ == e_) {                                                       \
            ++g_pass;                                                         \
        } else {                                                              \
            ++g_fail;                                                         \
            printf("  FAIL  %s:%d  %s == %ld, 期望 %ld\n",                     \
                   __FILE__, __LINE__, #actual, a_, e_);                      \
        }                                                                     \
    } while (0)

/* ---------------------------------------------------------------- 命令编码 */

/* 与逆向文档抓到的固件实际值逐位对齐——这是本项目最重要的单条断言。 */
static void test_encode_matches_captured_firmware_values(void)
{
    uint8_t a2[2];

    /* 关闭充电控制：start=0, end=100 */
    a2[0] = 0; a2[1] = 100;
    CHECK_EQ_U64(hw_cmd_encode(HW_C_BATTERY_THRESH_SET, a2, 2), 0x64001003ull);
    CHECK_EQ_U64(hw_cmd2(HW_C_BATTERY_THRESH_SET, 0, 100), 0x64001003ull);

    /* 70% - 90% */
    CHECK_EQ_U64(hw_cmd2(HW_C_BATTERY_THRESH_SET, 70, 90), 0x5A461003ull);

    /* 40% - 70% */
    CHECK_EQ_U64(hw_cmd2(HW_C_BATTERY_THRESH_SET, 40, 70), 0x46281003ull);

    /* 95% - 100% */
    CHECK_EQ_U64(hw_cmd2(HW_C_BATTERY_THRESH_SET, 95, 100), 0x645F1003ull);
}

static void test_encode_basics(void)
{
    uint8_t args[2];

    /* 无参数：原样返回命令号 */
    CHECK_EQ_U64(hw_cmd_encode(HW_C_TEMP_GET, NULL, 0), HW_C_TEMP_GET);
    CHECK_EQ_U64(hw_cmd_simple(HW_C_FN_LOCK_GET), HW_C_FN_LOCK_GET);

    /* 单参数落在第 2 字节 */
    CHECK_EQ_U64(hw_cmd1(HW_C_FAN_SPEED_GET, 0), 0x00000802ull);
    CHECK_EQ_U64(hw_cmd1(HW_C_FAN_SPEED_GET, 1), 0x00010802ull);
    CHECK_EQ_U64(hw_cmd1(HW_C_TEMP_GET, 0x15), 0x00150202ull);

    /* 4 参数落在第 2..5 字节：mode,unknown,start,end
     * 字节序：b0=0x03 b1=0x15 b2=1 b3=0 b4=70 b5=90 → 0x5A4600011503 */
    CHECK_EQ_U64(hw_cmd4(HW_C_BATTERY_CHARGE_MODE_SET, 1, 0, 70, 90),
                 0x5A4600011503ull);

    /* 参数偏移必须从第 2 字节开始（HW_CMD_ARG_OFFSET == 2）：
     * b2=0xAA b3=0xBB → 0xBBAA0000 */
    args[0] = 0xAA; args[1] = 0xBB;
    CHECK_EQ_U64(hw_cmd_encode(0x00ull, args, 2), 0xBBAA0000ull);
}

/* ------------------------------------------------------------ 响应解析 */

static void test_parse_battery_threshold(void)
{
    int st = -1, en = -1;
    uint8_t buf[5];

    /* 状态码 0，随后 start=70 end=90，尾部补零 */
    buf[0] = 0; buf[1] = 70; buf[2] = 90; buf[3] = 0; buf[4] = 0;
    CHECK_EQ_INT(hw_parse_battery_threshold(buf, 5, &st, &en), 1);
    CHECK_EQ_INT(st, 70);
    CHECK_EQ_INT(en, 90);

    /* 紧贴状态码的有效数据 */
    buf[0] = 0; buf[1] = 0; buf[2] = 0x46; buf[3] = 0x5A;
    CHECK_EQ_INT(hw_parse_battery_threshold(buf, 4, &st, &en), 1);
    CHECK_EQ_INT(st, 0x46);
    CHECK_EQ_INT(en, 0x5A);

    /* 全零 → (0,0)，属正常值而非失败 */
    buf[0] = 0; buf[1] = 0; buf[2] = 0;
    CHECK_EQ_INT(hw_parse_battery_threshold(buf, 3, &st, &en), 1);
    CHECK_EQ_INT(st, 0);
    CHECK_EQ_INT(en, 0);

    /* 缓冲区过短必须报错，不能越界读 */
    CHECK_EQ_INT(hw_parse_battery_threshold(buf, 2, &st, &en), 0);
    CHECK_EQ_INT(hw_parse_battery_threshold(NULL, 5, &st, &en), 0);
}

static void test_parse_fnlock(void)
{
    int on = -1;
    uint8_t b1[2] = { 0, 2 };      /* 2-1 == 1 → 开 */
    uint8_t b2[2] = { 0, 1 };      /* 1-1 == 0 → 关 */
    uint8_t b3[3] = { 0, 0, 2 };   /* 跳过前导零 */
    uint8_t b4[2] = { 0, 0 };      /* 无非零 → 关 */

    CHECK_EQ_INT(hw_parse_fnlock(b1, 2, &on), 1); CHECK_EQ_INT(on, 1);
    CHECK_EQ_INT(hw_parse_fnlock(b2, 2, &on), 1); CHECK_EQ_INT(on, 0);
    CHECK_EQ_INT(hw_parse_fnlock(b3, 3, &on), 1); CHECK_EQ_INT(on, 1);
    CHECK_EQ_INT(hw_parse_fnlock(b4, 2, &on), 1); CHECK_EQ_INT(on, 0);
    CHECK_EQ_INT(hw_parse_fnlock(b1, 1, &on), 0);
}

static void test_parse_powerunlock(void)
{
    int on = -1;
    uint8_t yes[2] = { 0, 1 };
    uint8_t no[2] = { 0, 0 };

    CHECK_EQ_INT(hw_parse_powerunlock(yes, 2, &on), 1); CHECK_EQ_INT(on, 1);
    CHECK_EQ_INT(hw_parse_powerunlock(no, 2, &on), 1);  CHECK_EQ_INT(on, 0);
    CHECK_EQ_INT(hw_parse_powerunlock(yes, 1, &on), 0);
}

static void test_parse_sensor(void)
{
    int v = -1;
    /* 风扇 3200 RPM = 0x0C80，小端 b[1]=0x80 b[2]=0x0C */
    uint8_t fan[3] = { 0, 0x80, 0x0C };
    CHECK_EQ_INT(hw_parse_u16le(fan, 3, &v), 1);
    CHECK_EQ_INT(v, 3200);

    /* 温度取 b[2]，十进制直读 */
    {
        uint8_t t[3] = { 0, 0xFF, 45 };
        CHECK_EQ_INT(hw_parse_temp(t, 3, &v), 1);
        CHECK_EQ_INT(v, 45);
    }

    /* 边界：长度不足 */
    CHECK_EQ_INT(hw_parse_u16le(fan, 2, &v), 0);
    CHECK_EQ_INT(hw_parse_temp(fan, 2, &v), 0);
}

static void test_parse_kbd_timeout(void)
{
    int sec = -1;
    /* 60 秒 = 0x003C，小端 b[1]=0x3C b[2]=0x00 */
    uint8_t b[3] = { 0, 0x3C, 0x00 };
    CHECK_EQ_INT(hw_parse_kbd_timeout(b, 3, &sec), 1);
    CHECK_EQ_INT(sec, 60);

    /* 300 秒 = 0x012C */
    b[1] = 0x2C; b[2] = 0x01;
    CHECK_EQ_INT(hw_parse_kbd_timeout(b, 3, &sec), 1);
    CHECK_EQ_INT(sec, 300);

    CHECK_EQ_INT(hw_parse_kbd_timeout(b, 2, &sec), 0);
}

static void test_parse_smart_charge(void)
{
    int mode = -1, unk = -1, st = -1, en = -1, p = -1;
    uint8_t m[5] = { 0, 1, 0, 70, 90 };
    uint8_t pm[2] = { 0, 1 };

    CHECK_EQ_INT(hw_parse_charge_mode(m, 5, &mode, &unk, &st, &en), 1);
    CHECK_EQ_INT(mode, 1);
    CHECK_EQ_INT(unk, 0);
    CHECK_EQ_INT(st, 70);
    CHECK_EQ_INT(en, 90);
    CHECK_EQ_INT(hw_parse_charge_mode(m, 4, &mode, &unk, &st, &en), 0);

    CHECK_EQ_INT(hw_parse_charge_param(pm, 2, &p), 1);
    CHECK_EQ_INT(p, 1);
    CHECK_EQ_INT(hw_parse_charge_param(pm, 1, &p), 0);
}

/* ---------------------------------------------------------- 键盘背光编码 */

static void test_kbd_classify(void)
{
    uint8_t percent[3]  = { 0, 0x01, 0x32 };   /* b[1]=0x01, b[2]=50% */
    uint8_t percent0[3] = { 0, 0x01, 0x00 };
    uint8_t percent100[3] = { 0, 0x01, 0x64 };
    uint8_t inverted[3] = { 0, 0xFF, 0x01 };   /* MACH-WX9 */
    uint8_t normal[3]   = { 0, 0x00, 0x04 };
    uint8_t unknown[3]  = { 0, 0x00, 0x00 };

    CHECK_EQ_INT(hw_kbd_classify(percent, 3),    HW_KBD_PERCENT);
    CHECK_EQ_INT(hw_kbd_classify(percent0, 3),   HW_KBD_PERCENT);
    CHECK_EQ_INT(hw_kbd_classify(percent100, 3), HW_KBD_PERCENT);
    CHECK_EQ_INT(hw_kbd_classify(inverted, 3),   HW_KBD_BITMASK_INVERTED);
    CHECK_EQ_INT(hw_kbd_classify(normal, 3),     HW_KBD_BITMASK_NORMAL);
    CHECK_EQ_INT(hw_kbd_classify(unknown, 3),    HW_KBD_UNKNOWN);
    CHECK_EQ_INT(hw_kbd_classify(percent, 2),    HW_KBD_UNKNOWN);
}

static void test_kbd_decode_level(void)
{
    int lv = -1;
    uint8_t b[3];

    /* 百分比：0x00→0, 0x32→1, 0x64→2 */
    b[1] = 0x01;
    b[2] = 0x00; CHECK_EQ_INT(hw_kbd_decode_level(HW_KBD_PERCENT, b, 3, &lv), 1);
    CHECK_EQ_INT(lv, 0);
    b[2] = 0x32; hw_kbd_decode_level(HW_KBD_PERCENT, b, 3, &lv);
    CHECK_EQ_INT(lv, 1);
    b[2] = 0x64; hw_kbd_decode_level(HW_KBD_PERCENT, b, 3, &lv);
    CHECK_EQ_INT(lv, 2);
    /* 中间值按 C# 的分档规则落进 1 */
    b[2] = 0x20; hw_kbd_decode_level(HW_KBD_PERCENT, b, 3, &lv);
    CHECK_EQ_INT(lv, 1);

    /* 位掩码反转（MACH-WX9）：bit0=最亮 0x01→0, 0x02→1, 0x04→2 */
    b[1] = 0xFF;
    b[2] = 0x01; hw_kbd_decode_level(HW_KBD_BITMASK_INVERTED, b, 3, &lv);
    CHECK_EQ_INT(lv, 0);
    b[2] = 0x02; hw_kbd_decode_level(HW_KBD_BITMASK_INVERTED, b, 3, &lv);
    CHECK_EQ_INT(lv, 1);
    b[2] = 0x04; hw_kbd_decode_level(HW_KBD_BITMASK_INVERTED, b, 3, &lv);
    CHECK_EQ_INT(lv, 2);

    /* 位掩码正序：bit 位置 - 2  0x04→0, 0x08→1, 0x10→2 */
    b[1] = 0x00;
    b[2] = 0x04; hw_kbd_decode_level(HW_KBD_BITMASK_NORMAL, b, 3, &lv);
    CHECK_EQ_INT(lv, 0);
    b[2] = 0x08; hw_kbd_decode_level(HW_KBD_BITMASK_NORMAL, b, 3, &lv);
    CHECK_EQ_INT(lv, 1);
    b[2] = 0x10; hw_kbd_decode_level(HW_KBD_BITMASK_NORMAL, b, 3, &lv);
    CHECK_EQ_INT(lv, 2);

    /* 未知编码必须失败而不是瞎猜 */
    CHECK_EQ_INT(hw_kbd_decode_level(HW_KBD_UNKNOWN, b, 3, &lv), 0);
    CHECK_EQ_INT(hw_kbd_decode_level(HW_KBD_PERCENT, b, 2, &lv), 0);
}

/* 编码 → 解码往返一致性：三种编码、三个级别全部覆盖。 */
static void test_kbd_roundtrip(void)
{
    static const HwKbdEncoding encs[3] = {
        HW_KBD_PERCENT, HW_KBD_BITMASK_NORMAL, HW_KBD_BITMASK_INVERTED
    };
    int i, lv, out;
    uint8_t b[3];

    for (i = 0; i < 3; ++i) {
        int level;
        for (level = 0; level <= 2; ++level) {
            b[0] = 0;
            b[1] = (encs[i] == HW_KBD_PERCENT) ? 0x01
                 : (encs[i] == HW_KBD_BITMASK_INVERTED) ? 0xFF : 0x00;
            b[2] = hw_kbd_encode_set(encs[i], level);

            lv = -1;
            CHECK_EQ_INT(hw_kbd_decode_level(encs[i], b, 3, &lv), 1);
            out = lv;
            if (out != level) {
                ++g_fail;
                printf("  FAIL  往返不一致 enc=%d level=%d 得到 %d\n",
                       (int)encs[i], level, out);
            } else {
                ++g_pass;
            }
        }
    }

    /* 越界级别必须夹紧而不是移位越界 */
    CHECK_EQ_INT(hw_kbd_encode_set(HW_KBD_PERCENT, -1), 0x00);
    CHECK_EQ_INT(hw_kbd_encode_set(HW_KBD_PERCENT, 9), 0x64);
    CHECK_EQ_INT(hw_kbd_encode_set(HW_KBD_BITMASK_INVERTED, 9), 0x04);
}

/* ------------------------------------------------------------ 常量表 */

static void test_temp_zones(void)
{
    int i;

    CHECK_EQ_INT(kHwTempZoneCount, 11);
    if (kHwTempZoneCount == 11) {
        CHECK_EQ_INT(kHwTempZones[0].idx, 0x00);
        if (kHwTempZones[0].label)
            CHECK(wcscmp(kHwTempZones[0].label, L"CPU") == 0);
        CHECK_EQ_INT(kHwTempZones[7].idx, 0x0E);   /* 电池 */
        if (kHwTempZones[7].label)
            CHECK(wcscmp(kHwTempZones[7].label, L"电池") == 0);
        CHECK_EQ_INT(kHwTempZones[10].idx, 0x16);
    }

    /* 索引不得重复（重复意味着表格抄错） */
    for (i = 1; i < kHwTempZoneCount; ++i) {
        int j;
        for (j = 0; j < i; ++j)
            CHECK(kHwTempZones[i].idx != kHwTempZones[j].idx);
    }
}

static void test_constants(void)
{
    /* 命令号必须与 C# WmiConstants.cs 完全一致 */
    CHECK_EQ_U64(HW_C_BATTERY_THRESH_GET,            0x1103ull);
    CHECK_EQ_U64(HW_C_BATTERY_THRESH_SET,            0x1003ull);
    CHECK_EQ_U64(HW_C_FN_LOCK_GET,                   0x0604ull);
    CHECK_EQ_U64(HW_C_FN_LOCK_SET,                   0x0704ull);
    CHECK_EQ_U64(HW_C_KBDLIGHT_GET,                  0x0602ull);
    CHECK_EQ_U64(HW_C_KBDLIGHT_SET,                  0x0702ull);
    CHECK_EQ_U64(HW_C_MICMUTE_LED_SET,               0x0B04ull);
    CHECK_EQ_U64(HW_C_KBDLIGHT_TIMEOUT_SET,          0x1106ull);
    CHECK_EQ_U64(HW_C_KBDLIGHT_TIMEOUT_GET,          0x1206ull);
    CHECK_EQ_U64(HW_C_POWER_UNLOCK_SET,              0x0F04ull);
    CHECK_EQ_U64(HW_C_POWER_UNLOCK_GET,              0x0E04ull);
    CHECK_EQ_U64(HW_C_FAN_SPEED_GET,                 0x0802ull);
    CHECK_EQ_U64(HW_C_TEMP_GET,                      0x0202ull);
    CHECK_EQ_U64(HW_C_BATTERY_CHARGE_MODE_GET,       0x1603ull);
    CHECK_EQ_U64(HW_C_BATTERY_CHARGE_MODE_SET,       0x1503ull);
    CHECK_EQ_U64(HW_C_BATTERY_CHARGE_MODE_PARAM_GET, 0x1303ull);
    CHECK_EQ_U64(HW_C_BATTERY_CHARGE_MODE_PARAM_SET, 0x1203ull);

    /* 协议常量 */
    CHECK_EQ_INT(HW_STATUS_SUCCESS, 0);
    CHECK_EQ_INT(HW_U8_BUFFER_LENGTH, 64);
    CHECK_EQ_INT(HW_U8_CMD_BYTES, 8);
    CHECK_EQ_INT(HW_CMD_ARG_OFFSET, 2);
    CHECK_EQ_INT(HW_OUT_MAX, 256);

    /* U8 缓冲必须装得下 8 字节命令值 */
    CHECK(HW_U8_CMD_BYTES <= HW_U8_BUFFER_LENGTH);
    /* 6 个参数最大落在第 7 字节，仍在 8 字节内 */
    CHECK(HW_CMD_ARG_OFFSET + 6 <= HW_U8_CMD_BYTES);
}

/* ------------------------------------------------------------------ main */

int main(void)
{
    printf("=== proto.h 测试 ===\n");

    test_encode_matches_captured_firmware_values();
    test_encode_basics();
    test_constants();
    test_temp_zones();

    test_parse_battery_threshold();
    test_parse_fnlock();
    test_parse_powerunlock();
    test_parse_sensor();
    test_parse_kbd_timeout();
    test_parse_smart_charge();

    test_kbd_classify();
    test_kbd_decode_level();
    test_kbd_roundtrip();

    printf("---\n通过 %d，失败 %d\n", g_pass, g_fail);
    if (g_fail != 0) {
        printf("结果：失败\n");
        return 1;
    }
    printf("结果：全部通过\n");
    return 0;
}
