using System.Threading;
using System.Threading.Tasks;
using HuaweiWmiControl.Wmi;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// 键盘背光服务接口。
    /// </summary>
    public interface IKeyboardService
    {
        /// <summary>最近一次错误（P2-5）。</summary>
        System.Exception? LastError { get; }

        /// <summary>键盘背光编码类型（连接时已检测）。</summary>
        KbdLightEncoding Encoding { get; }

        /// <summary>上一次读取的原始字节（调试用）。</summary>
        byte[]? LastRawBytes { get; }

        /// <summary>获取键盘背光级别（0-2）。</summary>
        int? GetLevel();

        /// <summary>异步获取键盘背光级别。</summary>
        Task<int?> GetLevelAsync(CancellationToken ct = default);

        /// <summary>设置键盘背光级别（0=关,1=低,2=高）。</summary>
        bool SetLevel(int level);

        /// <summary>异步设置键盘背光级别。</summary>
        Task<bool> SetLevelAsync(int level, CancellationToken ct = default);

        /// <summary>获取键盘背光超时秒数。</summary>
        int? GetTimeout();

        /// <summary>异步获取键盘背光超时。</summary>
        Task<int?> GetTimeoutAsync(CancellationToken ct = default);

        /// <summary>设置键盘背光超时秒数（0=常亮）。</summary>
        bool SetTimeout(int sec);

        /// <summary>异步设置键盘背光超时。</summary>
        Task<bool> SetTimeoutAsync(int sec, CancellationToken ct = default);
    }
}
