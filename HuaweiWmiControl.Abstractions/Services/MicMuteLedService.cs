using System.Threading;
using System.Threading.Tasks;
using HuaweiWmiControl.Wmi;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// 麦克风静音 LED 服务——控制硬件麦克风静音指示灯。
    /// </summary>
    public sealed class MicMuteLedService : WmiServiceBase, IMicMuteLedService
    {
        public MicMuteLedService(IWmiProtocol protocol, WmiConnectionManager connection)
            : base(protocol, connection) { }

        /// <inheritdoc/>
        public bool SetState(bool on)
            => InvokeSet(WmiConstants.C_MICMUTE_LED_SET, (byte)(on ? 1 : 0));

        /// <inheritdoc/>
        public Task<bool> SetStateAsync(bool on, CancellationToken ct = default)
            => InvokeSetAsync(WmiConstants.C_MICMUTE_LED_SET, new byte[] { (byte)(on ? 1 : 0) }, ct);
    }
}
