namespace HuaweiWmiControl.Wmi
{
    /// <summary>
    /// 键盘背光固件编码类型。
    /// </summary>
    public enum KbdLightEncoding
    {
        /// <summary>未检测。</summary>
        Unknown,
        /// <summary>位掩码正序：b[2] 的位位置 - 2 = 背光级别。</summary>
        BitmaskNormal,
        /// <summary>位掩码倒序：MACH-WX9 等机型，bit 0 = 最亮。</summary>
        BitmaskInverted,
        /// <summary>百分比编码：b[2]=0x00/0x32/0x64 对应 0%/50%/100%。</summary>
        Percent,
    }
}
