using System;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// WMI 操作异常——WMI 固件返回非成功状态时抛出。
    /// 调用方可通过此类区分"设备/固件不支持"（预期行为）和"系统异常"（需要修复）。
    /// </summary>
    public class WmiException : Exception
    {
        /// <summary>引发异常的命令号。</summary>
        public ulong Command { get; }

        /// <summary>固件返回的状态码。</summary>
        public byte Status { get; }

        /// <summary>
        /// 初始化 WMI 异常。
        /// </summary>
        /// <param name="cmd">命令号。</param>
        /// <param name="status">固件状态码。</param>
        /// <param name="message">可选的消息文本。</param>
        /// <param name="innerException">内部异常。</param>
        public WmiException(ulong cmd, byte status, string? message = null, Exception? innerException = null)
            : base(message ?? $"命令 0x{cmd:X} 返回状态 0x{status:X2}", innerException)
        {
            Command = cmd;
            Status = status;
        }
    }

    /// <summary>
    /// 设备/固件不支持异常——预期行为，非系统故障。
    /// 当某个 WMI 命令在当前硬件上不被支持时抛出此异常，
    /// 调用方可安心吞掉（如返回 null 或显示"不支持"）。
    /// </summary>
    public class WmiNotSupportedException : WmiException
    {
        /// <summary>
        /// 初始化不支持异常。
        /// </summary>
        /// <param name="cmd">命令号。</param>
        /// <param name="status">固件状态码。</param>
        public WmiNotSupportedException(ulong cmd, byte status)
            : base(cmd, status, $"命令 0x{cmd:X} 当前固件不支持（状态 0x{status:X2}）")
        {
        }
    }
}
