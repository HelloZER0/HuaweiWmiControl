using System.Threading;
using System.Threading.Tasks;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// 传感器（温度 + 风扇）服务接口。
    /// </summary>
    public interface ISensorService
    {
        /// <summary>最近一次错误（P2-5）。</summary>
        System.Exception? LastError { get; }

        /// <summary>获取指定风扇转速（RPM）。</summary>
        int? GetFanSpeed(int num);

        /// <summary>异步获取风扇转速。</summary>
        Task<int?> GetFanSpeedAsync(int num, CancellationToken ct = default);

        /// <summary>获取指定温度传感器读数（°C）。</summary>
        int? GetTemp(int num);

        /// <summary>异步获取温度传感器读数。</summary>
        Task<int?> GetTempAsync(int num, CancellationToken ct = default);
    }
}
