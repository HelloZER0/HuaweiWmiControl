using System.Threading;
using System.Threading.Tasks;
using HuaweiWmiControl.Wmi;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// 传感器服务——提供风扇转速和温度传感器读取功能。
    /// </summary>
    public sealed class SensorService : WmiServiceBase, ISensorService
    {
        public SensorService(IWmiProtocol protocol, WmiConnectionManager connection)
            : base(protocol, connection) { }

        /// <inheritdoc/>
        public int? GetFanSpeed(int num)
            => InvokeGet<int>(WmiConstants.C_FAN_SPEED_GET, ParseU16Le, (byte)num);

        /// <inheritdoc/>
        public Task<int?> GetFanSpeedAsync(int num, CancellationToken ct = default)
            => InvokeGetAsync<int>(WmiConstants.C_FAN_SPEED_GET, ParseU16Le, new byte[] { (byte)num }, ct);

        /// <inheritdoc/>
        public int? GetTemp(int num)
            => InvokeGet<int>(WmiConstants.C_TEMP_GET, b => (int)b[2], (byte)num);

        /// <inheritdoc/>
        public Task<int?> GetTempAsync(int num, CancellationToken ct = default)
            => InvokeGetAsync<int>(WmiConstants.C_TEMP_GET, b => (int)b[2], new byte[] { (byte)num }, ct);

        /// <summary>将 b[1..2] 解析为小端 16 位无符号整数。</summary>
        private static int ParseU16Le(byte[] b) => b[1] | (b[2] << 8);
    }
}
