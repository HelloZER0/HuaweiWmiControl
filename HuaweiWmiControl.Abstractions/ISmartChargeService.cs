using System.Threading;
using System.Threading.Tasks;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// 智能充电服务接口。
    /// </summary>
    public interface ISmartChargeService
    {
        /// <summary>最近一次错误（P2-5）。</summary>
        System.Exception? LastError { get; }

        /// <summary>获取智能充电参数。</summary>
        (int mode, int unknown, int start, int end)? GetMode();

        /// <summary>异步获取智能充电参数。</summary>
        Task<(int mode, int unknown, int start, int end)?> GetModeAsync(CancellationToken ct = default);

        /// <summary>设置智能充电参数。</summary>
        bool SetMode(int mode, int unknown, int start, int end);

        /// <summary>异步设置智能充电参数。</summary>
        Task<bool> SetModeAsync(int mode, int unknown, int start, int end, CancellationToken ct = default);

        /// <summary>获取智能充电附加参数。</summary>
        int? GetParam();

        /// <summary>异步获取智能充电附加参数。</summary>
        Task<int?> GetParamAsync(CancellationToken ct = default);

        /// <summary>设置智能充电附加参数。</summary>
        bool SetParam(int value);

        /// <summary>异步设置智能充电附加参数。</summary>
        Task<bool> SetParamAsync(int value, CancellationToken ct = default);
    }
}
