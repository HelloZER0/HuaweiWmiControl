/* ============================================================================
 * hwapi.h — 华为笔记本硬件调控对外 API（冻结契约）
 * ----------------------------------------------------------------------------
 * 本文件是「冻结契约」的主体，由 Lead 维护、两侧共同遵守：
 *   - 核心层（wmi.cpp / services.cpp）负责实现；
 *   - Win32 外壳（main.cpp / menu.cpp / tray.cpp）只允许调用这里声明的函数。
 *
 * 设计要点：
 *   1. 单设备模型：整个进程只操作一块本机的 ACPI-WMI 接口，因此用
 *      HwDevice 值类型 + 调用方持有，不引入全局单例状态。
 *   2. 全部函数返回 HwStatus，不抛异常、不 terminate。
 *   3. 所有字符串为 UTF-16（宽字符），不含格式化占位符，直接可用于菜单。
 *   4. HwDevice 的 _p 为不透明实现指针，外壳层不得解引用。
 * ========================================================================== */
#ifndef HWC_HWAPI_H
#define HWC_HWAPI_H

#include <windows.h>
#include "proto.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------- 状态码 */
typedef enum HwStatus {
    HW_OK              = 0,   /* 成功                                     */
    HW_ERR_NOT_ADMIN   = 1,   /* 未以管理员身份运行                       */
    HW_ERR_NO_DEVICE   = 2,   /* 找不到 OemWMIMethod / 含 HWMI 的实例     */
    HW_ERR_NOT_SUPPORTED = 3, /* 固件返回非零状态（机型不支持，属预期）   */
    HW_ERR_COM         = 4,   /* COM/WMI 调用本身失败（连接断开等）       */
    HW_ERR_BUFFER      = 5,   /* 响应缓冲区长度不足（协议违例）           */
    HW_ERR_ARG         = 6,   /* 参数越界                                 */
    HW_ERR_UNKNOWN     = 7    /* 未初始化或其他                                */
} HwStatus;

/* ------------------------------------------------------------- 设备句柄 */
typedef struct HwDevice {
    void*          _p;            /* 不透明实现指针，外壳层勿动               */
    HwProtocol     protocol;      /* 探测到的协议                             */
    HwKbdEncoding  kbd_encoding;  /* 探测到的键盘背光编码                     */
    int            available;     /* 1 = 已连接可用                           */
    int            last_fw_status;/* 最近一次固件状态字节（调试用）           */
    WCHAR          reason[256];   /* 失败原因（中文，可直接显示）             */
    WCHAR          instance[128]; /* InstanceName                             */
} HwDevice;

/* --------------------------------------------------- 生命周期 */

/* 清零结构体。必须先调用（或 memset 0）再 hw_open。 */
void hw_device_init(HwDevice* dev);

/* 建立 COM/WMI 连接、定位 HWMI 实例、探测协议与键盘背光编码。
 * 幂等：重复调用是安全的（内部先 hw_close）。
 * 失败时返回对应 HwStatus，dev->reason 填入中文原因。 */
HwStatus hw_open(HwDevice* dev);

/* 释放 COM 对象。可对未打开的设备调用。 */
void hw_close(HwDevice* dev);

/* 协议名（"U64"/"U8"/"未知"），可直接显示。 */
const WCHAR* hw_protocol_name(const HwDevice* dev);

/* 键盘背光编码名（"百分比"/"位掩码"/"位掩码(反转)"/"未知"）。 */
const WCHAR* hw_kbd_encoding_name(const HwDevice* dev);

/* 状态码的中文说明，静态字符串，勿释放。 */
const WCHAR* hw_status_text(HwStatus st);

/* --------------------------------------------------- 电池保护 */

/* 读取充电阈值。*start/*end 为 0..100。 */
HwStatus hw_battery_get(HwDevice* dev, int* start, int* end);

/* 设置充电阈值。
 * reset_before_disable=true 且 (start==0 && end==100) 时，先写一次 (0,0)
 * 并等待 1 秒——MACH-WX9 固件的兼容路径，与 C# 版行为一致。 */
HwStatus hw_battery_set(HwDevice* dev, int start, int end, int reset_before_disable);

/* --------------------------------------------------- Fn-Lock */

HwStatus hw_fnlock_get(HwDevice* dev, int* on);   /* 0/1 */
HwStatus hw_fnlock_set(HwDevice* dev, int on);

/* --------------------------------------------------- 键盘背光 */

/* 级别 0=关 1=低 2=高。读取时会顺带更新 dev->kbd_encoding（若仍未知）。 */
HwStatus hw_kbd_get_level(HwDevice* dev, int* level);
HwStatus hw_kbd_set_level(HwDevice* dev, int level);

/* 熄灭超时（秒，0=常亮，上限 65535）。 */
HwStatus hw_kbd_get_timeout(HwDevice* dev, int* sec);
HwStatus hw_kbd_set_timeout(HwDevice* dev, int sec);

/* --------------------------------------------------- 智能充电（华为私有） */

HwStatus hw_smartcharge_get(HwDevice* dev, int* mode, int* unknown, int* start, int* end);
HwStatus hw_smartcharge_set(HwDevice* dev, int mode, int unknown, int start, int end);

HwStatus hw_smartcharge_get_param(HwDevice* dev, int* value);
HwStatus hw_smartcharge_set_param(HwDevice* dev, int value);

/* --------------------------------------------------- 电源解锁（Fn+P） */

HwStatus hw_powerunlock_get(HwDevice* dev, int* on);
HwStatus hw_powerunlock_set(HwDevice* dev, int on);

/* --------------------------------------------------- 麦克风静音 LED */

/* 注意：该功能只有 SET，没有 GET，无法回读当前状态。 */
HwStatus hw_micled_set(HwDevice* dev, int on);

/* --------------------------------------------------- 传感器 */

/* 风扇转速（RPM）。num: 0 或 1。 */
HwStatus hw_fan_speed(HwDevice* dev, int num, int* rpm);

/* 温度（摄氏度）。idx 取 kHwTempZones[].idx。 */
HwStatus hw_temp(HwDevice* dev, int idx, int* celsius);

/* --------------------------------------------------- 日志 */

/* 打开/滚动日志文件：%LOCALAPPDATA%\HuaweiWmiControl\hwc.log
 * 失败时静默降级为「仅调试输出」。返回日志文件绝对路径（静态缓冲）。 */
const WCHAR* hw_log_init(void);
void        hw_log_shutdown(void);

/* 写一条日志（UTF-16 格式化，自带时间戳与换行）。 */
void hw_log(const WCHAR* fmt, ...);

/* 把最近一次 HwStatus + 附加说明拼成一句可显示的日志文本并写入日志。
 * 便于外壳层统一处理："电池保护：已设为 70-80" / "电池保护：失败（固件不支持）" */
void hw_log_result(const WCHAR* action, HwStatus st);

/* 用系统默认程序打开日志文件。返回是否成功。 */
int hw_log_open_in_shell(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* HWC_HWAPI_H */
