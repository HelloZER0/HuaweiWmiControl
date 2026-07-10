using System.Threading;
using System.Threading.Tasks;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// 电池充电阈值服务接口。
    /// </summary>
    public interface IBatteryService
    {
        /// <summary>最近一次错误（P2-5）。</summary>
        System.Exception? LastError { get; }

        /// <summary>获取电池充电起止百分比。</summary>
        (int start, int end)? GetThreshold();

        /// <summary>异步获取电池充电起止百分比。</summary>
        Task<(int start, int end)?> GetThresholdAsync(CancellationToken ct = default);

        /// <summary>设置电池充电起止百分比。</summary>
        /// <param name="start">起始百分比。</param>
        /// <param name="end">上限百分比。</param>
        /// <param name="resetBeforeDisable">MACH-WX9 兼容：关闭保护前先清零阈值并延时 1 秒。</param>
        bool SetThreshold(int start, int end, bool resetBeforeDisable);

        /// <summary>异步设置电池充电起止百分比。</summary>
        Task<bool> SetThresholdAsync(int start, int end, bool resetBeforeDisable, CancellationToken ct = default);
    }
}
