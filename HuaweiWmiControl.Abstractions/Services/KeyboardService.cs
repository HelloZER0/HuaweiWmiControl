using System;
using System.Threading;
using System.Threading.Tasks;
using HuaweiWmiControl.Wmi;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// 键盘背光服务——支持位掩码和百分比两种固件编码，
    /// 编码类型在 <see cref="WmiConnectionManager.Connect"/> 时自动检测。
    /// </summary>
    public sealed class KeyboardService : WmiServiceBase, IKeyboardService
    {
        public KeyboardService(IWmiProtocol protocol, WmiConnectionManager connection)
            : base(protocol, connection) { }

        /// <inheritdoc/>
        public KbdLightEncoding Encoding => Connection.KbdEncoding;

        /// <inheritdoc/>
        public byte[]? LastRawBytes => Connection.LastKbdLightRawBytes;

        // ---- 背光级别 ----

        /// <inheritdoc/>
        public int? GetLevel()
        {
            // 需要保存原始字节，不使用 InvokeGet 模板
            try
            {
                var b = Call(Protocol.EncodeCommand(WmiConstants.C_KBDLIGHT_GET));
                Connection.LastKbdLightRawBytes = b;
                LastError = null;
                return DecodeLevel(b);
            }
            catch (WmiNotSupportedException ex) { LastError = ex; return null; }
        }

        /// <inheritdoc/>
        public async Task<int?> GetLevelAsync(CancellationToken ct = default)
        {
            try
            {
                var b = await CallAsync(
                    Protocol.EncodeCommand(WmiConstants.C_KBDLIGHT_GET), ct)
                    .ConfigureAwait(false);
                Connection.LastKbdLightRawBytes = b;
                LastError = null;
                return DecodeLevel(b);
            }
            catch (WmiNotSupportedException ex) { LastError = ex; return null; }
        }

        /// <inheritdoc/>
        public bool SetLevel(int level)
            => InvokeSet(WmiConstants.C_KBDLIGHT_SET, EncodeSetValue(level));

        /// <inheritdoc/>
        public Task<bool> SetLevelAsync(int level, CancellationToken ct = default)
            => InvokeSetAsync(WmiConstants.C_KBDLIGHT_SET, new byte[] { EncodeSetValue(level) }, ct);

        // ---- 背光超时 ----

        /// <inheritdoc/>
        public int? GetTimeout()
            => InvokeGet<int>(WmiConstants.C_KBDLIGHT_TIMEOUT_GET, b => b[1] | (b[2] << 8));

        /// <inheritdoc/>
        public Task<int?> GetTimeoutAsync(CancellationToken ct = default)
            => InvokeGetAsync<int>(WmiConstants.C_KBDLIGHT_TIMEOUT_GET, b => b[1] | (b[2] << 8), ct: ct);

        /// <inheritdoc/>
        public bool SetTimeout(int sec)
            => InvokeSet(WmiConstants.C_KBDLIGHT_TIMEOUT_SET, (byte)(sec & 0xff), (byte)(sec >> 8));

        /// <inheritdoc/>
        public Task<bool> SetTimeoutAsync(int sec, CancellationToken ct = default)
            => InvokeSetAsync(WmiConstants.C_KBDLIGHT_TIMEOUT_SET,
                new byte[] { (byte)(sec & 0xff), (byte)(sec >> 8) }, ct);

        // ---- 编码/解码 ----

        /// <summary>根据已检测的编码类型从缓冲区解码背光级别。</summary>
        private int? DecodeLevel(byte[] b)
        {
            switch (Connection.KbdEncoding)
            {
                case KbdLightEncoding.Percent:
                    if (b[2] == 0) return 0;
                    if (b[2] <= 0x32) return 1;
                    return 2;
                case KbdLightEncoding.BitmaskNormal:
                case KbdLightEncoding.BitmaskInverted:
                    int level = 0;
                    byte v = b[2];
                    while ((v >>= 1) != 0) level++;
                    return Connection.KbdLightQuirkDetected
                        ? level
                        : level - WmiConstants.KBD_BITMASK_OFFSET;
                default:
                    return null;
            }
        }

        /// <summary>根据编码类型编码设置值。</summary>
        private byte EncodeSetValue(int level)
        {
            switch (Connection.KbdEncoding)
            {
                case KbdLightEncoding.Percent:
                    return level < WmiConstants.KbdPercentValues.Length
                        ? WmiConstants.KbdPercentValues[level]
                        : WmiConstants.KbdPercentValues[^1];
                case KbdLightEncoding.BitmaskInverted:
                    return (byte)(1 << level);
                default: // BitmaskNormal
                    return (byte)(1 << (level + WmiConstants.KBD_BITMASK_OFFSET));
            }
        }
    }
}
