using System;
using System.Threading;
using System.Threading.Tasks;
using HuaweiWmiControl.Wmi;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// 电池充电阈值服务——读取与设置充电起止百分比，
    /// 包含 MACH-WX9 兼容路径（清零后延时 1 秒）。
    /// </summary>
    public sealed class BatteryService : WmiServiceBase, IBatteryService
    {
        public BatteryService(IWmiProtocol protocol, WmiConnectionManager connection)
            : base(protocol, connection) { }

        /// <inheritdoc/>
        public (int start, int end)? GetThreshold()
            => InvokeGet<(int start, int end)>(WmiConstants.C_BATTERY_THRESH_GET, ParseBatteryThreshold);

        /// <inheritdoc/>
        public Task<(int start, int end)?> GetThresholdAsync(CancellationToken ct = default)
            => InvokeGetAsync<(int start, int end)>(WmiConstants.C_BATTERY_THRESH_GET, ParseBatteryThreshold, ct: ct);

        /// <inheritdoc/>
        public bool SetThreshold(int start, int end, bool resetBeforeDisable)
        {
            try
            {
                if (resetBeforeDisable && start == 0 && end == 100)
                {
                    // MACH-WX9 兼容：关闭保护前先清零阈值并延时 1 秒
                    Protocol.Invoke(
                        Protocol.EncodeCommand(WmiConstants.C_BATTERY_THRESH_SET, 0, 0),
                        Connection.Session, Connection.Instance);
                    Thread.Sleep(1000);
                }
                Call(Protocol.EncodeCommand(WmiConstants.C_BATTERY_THRESH_SET, (byte)start, (byte)end));
                LastError = null;
                return true;
            }
            catch (WmiNotSupportedException ex) { LastError = ex; return false; }
        }

        /// <inheritdoc/>
        public async Task<bool> SetThresholdAsync(int start, int end, bool resetBeforeDisable,
            CancellationToken ct = default)
        {
            try
            {
                if (resetBeforeDisable && start == 0 && end == 100)
                {
                    await Protocol.InvokeAsync(
                        Protocol.EncodeCommand(WmiConstants.C_BATTERY_THRESH_SET, 0, 0),
                        Connection.Session, Connection.Instance, ct).ConfigureAwait(false);
                    await Task.Delay(1000, ct).ConfigureAwait(false);
                }
                await CallAsync(Protocol.EncodeCommand(
                    WmiConstants.C_BATTERY_THRESH_SET, (byte)start, (byte)end), ct)
                    .ConfigureAwait(false);
                LastError = null;
                return true;
            }
            catch (WmiNotSupportedException ex) { LastError = ex; return false; }
        }

        /// <summary>解析电池阈值响应：从尾部向前查找非零字节。</summary>
        private static (int, int) ParseBatteryThreshold(byte[] b)
        {
            for (int i = b.Length - 1; i >= WmiConstants.BATTERY_THRESH_DATA_OFFSET; i--)
                if (b[i] != 0) return (b[i - 1], b[i]);
            return (0, 0);
        }
    }
}
