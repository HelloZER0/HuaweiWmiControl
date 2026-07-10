using System.Threading;
using System.Threading.Tasks;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// 电源解锁（Fn+P）服务接口。
    /// </summary>
    public interface IPowerUnlockService
    {
        /// <summary>最近一次错误（P2-5）。</summary>
        System.Exception? LastError { get; }

        /// <summary>获取电源解锁状态。</summary>
        bool? GetState();

        /// <summary>异步获取电源解锁状态。</summary>
        Task<bool?> GetStateAsync(CancellationToken ct = default);

        /// <summary>设置电源解锁状态。</summary>
        bool SetState(bool on);

        /// <summary>异步设置电源解锁状态。</summary>
        Task<bool> SetStateAsync(bool on, CancellationToken ct = default);
    }
}
