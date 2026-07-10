using System.Threading;
using System.Threading.Tasks;
using HuaweiWmiControl.Wmi;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// Fn-Lock 服务——控制 F1-F12 键的功能键/多媒体模式切换。
    /// </summary>
    public sealed class FnLockService : WmiServiceBase, IFnLockService
    {
        public FnLockService(IWmiProtocol protocol, WmiConnectionManager connection)
            : base(protocol, connection) { }

        /// <inheritdoc/>
        public bool? GetState()
            => InvokeGet<bool>(WmiConstants.C_FN_LOCK_GET, ParseFnLockState);

        /// <inheritdoc/>
        public Task<bool?> GetStateAsync(CancellationToken ct = default)
            => InvokeGetAsync<bool>(WmiConstants.C_FN_LOCK_GET, ParseFnLockState, ct: ct);

        /// <inheritdoc/>
        public bool SetState(bool on)
            => InvokeSet(WmiConstants.C_FN_LOCK_SET, (byte)(on ? 2 : 1));

        /// <inheritdoc/>
        public Task<bool> SetStateAsync(bool on, CancellationToken ct = default)
            => InvokeSetAsync(WmiConstants.C_FN_LOCK_SET, new byte[] { (byte)(on ? 2 : 1) }, ct);

        /// <summary>从响应缓冲区解析 Fn-Lock 状态。</summary>
        private static bool ParseFnLockState(byte[] b)
        {
            for (int i = 1; i < b.Length; i++)
                if (b[i] != 0) return (b[i] - 1) == 1;
            return false;
        }
    }
}
