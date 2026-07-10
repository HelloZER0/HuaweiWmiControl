using System.Threading;
using System.Threading.Tasks;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// 麦克风静音 LED 服务接口。
    /// </summary>
    public interface IMicMuteLedService
    {
        /// <summary>最近一次错误（P2-5）。</summary>
        System.Exception? LastError { get; }

        /// <summary>设置麦克风静音指示灯状态。</summary>
        bool SetState(bool on);

        /// <summary>异步设置麦克风静音指示灯状态。</summary>
        Task<bool> SetStateAsync(bool on, CancellationToken ct = default);
    }
}
