using System.Threading;
using System.Threading.Tasks;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// Fn-Lock 切换服务接口。
    /// </summary>
    public interface IFnLockService
    {
        /// <summary>最近一次错误（P2-5）。</summary>
        System.Exception? LastError { get; }

        /// <summary>获取 Fn-Lock 状态（true=功能键模式）。</summary>
        bool? GetState();

        /// <summary>异步获取 Fn-Lock 状态。</summary>
        Task<bool?> GetStateAsync(CancellationToken ct = default);

        /// <summary>设置 Fn-Lock 状态。</summary>
        bool SetState(bool on);

        /// <summary>异步设置 Fn-Lock 状态。</summary>
        Task<bool> SetStateAsync(bool on, CancellationToken ct = default);
    }
}
