using System.Threading;
using System.Threading.Tasks;
using HuaweiWmiControl.Wmi;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// 电源解锁服务——控制 Fn+P 电源解锁功能。
    /// </summary>
    public sealed class PowerUnlockService : WmiServiceBase, IPowerUnlockService
    {
        public PowerUnlockService(IWmiProtocol protocol, WmiConnectionManager connection)
            : base(protocol, connection) { }

        /// <inheritdoc/>
        public bool? GetState()
            => InvokeGet<bool>(WmiConstants.C_POWER_UNLOCK_GET, b => b[1] == 1);

        /// <inheritdoc/>
        public Task<bool?> GetStateAsync(CancellationToken ct = default)
            => InvokeGetAsync<bool>(WmiConstants.C_POWER_UNLOCK_GET, b => b[1] == 1, ct: ct);

        /// <inheritdoc/>
        public bool SetState(bool on)
            => InvokeSet(WmiConstants.C_POWER_UNLOCK_SET, (byte)(on ? 1 : 0));

        /// <inheritdoc/>
        public Task<bool> SetStateAsync(bool on, CancellationToken ct = default)
            => InvokeSetAsync(WmiConstants.C_POWER_UNLOCK_SET, new byte[] { (byte)(on ? 1 : 0) }, ct);
    }
}
