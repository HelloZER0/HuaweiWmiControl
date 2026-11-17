/* ============================================================================
 * proto.c — proto.h 中 extern 数据的唯一定义处
 * ========================================================================== */
#include "proto.h"

/* 与 C# WmiConstants.TempZones 逐项一致（显示名, 固件索引），顺序即显示顺序。 */
const HwTempZone kHwTempZones[] = {
    { L"CPU",  0x00 },
    { L"TP01", 0x01 },
    { L"TSLO", 0x05 },
    { L"TP06", 0x06 },
    { L"TNTC", 0x07 },
    { L"CNTC", 0x08 },
    { L"DNTC", 0x0B },
    { L"电池", 0x0E },
    { L"TP0C", 0x0F },
    { L"TP07", 0x15 },
    { L"TP04", 0x16 },
};

const int kHwTempZoneCount =
    (int)(sizeof(kHwTempZones) / sizeof(kHwTempZones[0]));
