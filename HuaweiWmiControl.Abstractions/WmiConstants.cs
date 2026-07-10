namespace HuaweiWmiControl.Wmi
{
    /// <summary>
    /// 华为 ACPI-WMI 固件通信的命令号与协议常量。
    /// 所有值均与 Linux huawei-wmi 驱动保持一致。
    /// </summary>
    public static class WmiConstants
    {
        // ---- WMI 路径 ----
        /// <summary>WMI 根命名空间路径（DCOM 用）。</summary>
        public const string WmiScope = @"\\.\root\wmi";
        /// <summary>CIM 命名空间。</summary>
        public const string WmiNamespace = @"root/wmi";
        /// <summary>WMI 管理类名称。</summary>
        public const string WmiClassName = "OemWMIMethod";
        /// <summary>WMI 方法名称。</summary>
        public const string WmiMethodName = "OemWMIfun";
        /// <summary>输出参数名。</summary>
        public const string WmiOutputParam = "u8Output";
        /// <summary>实例名称属性。</summary>
        public const string WmiInstanceNameProp = "InstanceName";
        /// <summary>实例名称需包含此子串。</summary>
        public const string WmiInstanceNameFilter = "HWMI";

        // ---- 命令号（64 位参数的低 32 位）----
        public const ulong C_BATTERY_THRESH_GET = 0x00001103;
        public const ulong C_BATTERY_THRESH_SET = 0x00001003;
        public const ulong C_FN_LOCK_GET = 0x00000604;
        public const ulong C_FN_LOCK_SET = 0x00000704;
        public const ulong C_KBDLIGHT_GET = 0x00000602;
        public const ulong C_KBDLIGHT_SET = 0x00000702;
        public const ulong C_MICMUTE_LED_SET = 0x00000b04;
        public const ulong C_KBDLIGHT_TIMEOUT_SET = 0x00001106;
        public const ulong C_KBDLIGHT_TIMEOUT_GET = 0x00001206;
        public const ulong C_POWER_UNLOCK_SET = 0x00000F04;
        public const ulong C_POWER_UNLOCK_GET = 0x00000E04;
        public const ulong C_FAN_SPEED_GET = 0x00000802;
        public const ulong C_TEMP_GET = 0x00000202;
        public const ulong C_BATTERY_CHARGE_MODE_GET = 0x00001603;
        public const ulong C_BATTERY_CHARGE_MODE_SET = 0x00001503;
        public const ulong C_BATTERY_CHARGE_MODE_PARAM_GET = 0x00001303;
        public const ulong C_BATTERY_CHARGE_MODE_PARAM_SET = 0x00001203;

        // ---- 协议常量 ----
        /// <summary>WMI 返回缓冲区第 0 字节为此值时表示操作成功。</summary>
        public const byte STATUS_SUCCESS = 0;
        /// <summary>当缓冲区为空时的默认错误状态。</summary>
        public const byte STATUS_DEFAULT_ERROR = 1;
        /// <summary>U8 协议缓冲区总字节数（老固件 SAFEARRAY 长度）。</summary>
        public const int U8_BUFFER_LENGTH = 64;
        /// <summary>U8 缓冲区中存放小端命令值的前 8 字节。</summary>
        public const int U8_CMD_BYTES = 8;
        /// <summary>命令编码时参数的起始字节偏移。</summary>
        public const int CMD_ARG_OFFSET = 2;

        // ---- 键盘背光编码常量 ----
        /// <summary>百分比编码的标志位：b[1] 为 0x01。</summary>
        public const byte KBD_PERCENT_FLAG = 0x01;
        /// <summary>位掩码反转标志：b[1] 为 0xFF 表示 MACH-WX9 等机型。</summary>
        public const byte KBD_INVERTED_FLAG = 0xFF;
        /// <summary>位掩码普通编码的基准偏移。</summary>
        public const int KBD_BITMASK_OFFSET = 2;
        /// <summary>百分比编码各档位对应值。</summary>
        public static readonly byte[] KbdPercentValues = { 0x00, 0x32, 0x64 };

        // ---- 电池阈值常量 ----
        /// <summary>电池阈值缓冲区数据起始偏移。</summary>
        public const int BATTERY_THRESH_DATA_OFFSET = 2;

        // ---- 温度传感器区域 ----
        /// <summary>温度传感器区域列表（显示名, 固件索引）。</summary>
        public static readonly (string name, int idx)[] TempZones =
        {
            ("CPU", 0x00), ("TP01", 0x01), ("TSLO", 0x05), ("TP06", 0x06),
            ("TNTC", 0x07), ("CNTC", 0x08), ("DNTC", 0x0B), ("电池", 0x0E),
            ("TP0C", 0x0F), ("TP07", 0x15), ("TP04", 0x16),
        };
    }
}
