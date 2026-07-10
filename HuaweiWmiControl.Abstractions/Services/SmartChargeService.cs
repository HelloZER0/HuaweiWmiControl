using System.Threading;
using System.Threading.Tasks;
using HuaweiWmiControl.Wmi;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// 智能充电服务——华为私有充电模式管理。
    /// </summary>
    public sealed class SmartChargeService : WmiServiceBase, ISmartChargeService
    {
        public SmartChargeService(IWmiProtocol protocol, WmiConnectionManager connection)
            : base(protocol, connection) { }

        /// <inheritdoc/>
        public (int mode, int unknown, int start, int end)? GetMode()
            => InvokeGet<(int mode, int unknown, int start, int end)>(WmiConstants.C_BATTERY_CHARGE_MODE_GET, ParseChargeMode);

        /// <inheritdoc/>
        public Task<(int mode, int unknown, int start, int end)?> GetModeAsync(CancellationToken ct = default)
            => InvokeGetAsync<(int mode, int unknown, int start, int end)>(WmiConstants.C_BATTERY_CHARGE_MODE_GET, ParseChargeMode, ct: ct);

        /// <inheritdoc/>
        public bool SetMode(int mode, int unknown, int start, int end)
            => InvokeSet(WmiConstants.C_BATTERY_CHARGE_MODE_SET,
                (byte)mode, (byte)unknown, (byte)start, (byte)end);

        /// <inheritdoc/>
        public Task<bool> SetModeAsync(int mode, int unknown, int start, int end,
            CancellationToken ct = default)
            => InvokeSetAsync(WmiConstants.C_BATTERY_CHARGE_MODE_SET,
                new byte[] { (byte)mode, (byte)unknown, (byte)start, (byte)end }, ct);

        /// <inheritdoc/>
        public int? GetParam()
            => InvokeGet<int>(WmiConstants.C_BATTERY_CHARGE_MODE_PARAM_GET, b => (int)b[1]);

        /// <inheritdoc/>
        public Task<int?> GetParamAsync(CancellationToken ct = default)
            => InvokeGetAsync<int>(WmiConstants.C_BATTERY_CHARGE_MODE_PARAM_GET, b => (int)b[1], ct: ct);

        /// <inheritdoc/>
        public bool SetParam(int value)
            => InvokeSet(WmiConstants.C_BATTERY_CHARGE_MODE_PARAM_SET, (byte)value);

        /// <inheritdoc/>
        public Task<bool> SetParamAsync(int value, CancellationToken ct = default)
            => InvokeSetAsync(WmiConstants.C_BATTERY_CHARGE_MODE_PARAM_SET, new byte[] { (byte)value }, ct);

        private static (int, int, int, int) ParseChargeMode(byte[] b)
            => (b[1], b[2], b[3], b[4]);
    }
}
