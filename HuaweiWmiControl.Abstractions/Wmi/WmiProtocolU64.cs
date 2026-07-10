using System.Threading;
using System.Threading.Tasks;
using Microsoft.Management.Infrastructure;

namespace HuaweiWmiControl.Wmi
{
    /// <summary>
    /// U64 协议（CIM 版）——新版固件，通过 u64Input 参数传递 64 位命令值。
    /// 使用 <see cref="CimSession.InvokeMethodAsync"/> 真正异步 API，
    /// 通过 <see cref="CimAsyncExtensions.AsTask{T}"/> 桥接为 Task。
    /// </summary>
    public sealed class WmiProtocolU64 : IWmiProtocol
    {
        public string ProtocolName => "U64";

        public byte[] Invoke(ulong cmd, CimSession session, CimInstance instance)
        {
            var parameters = CreateInputParameters(cmd);
            var result = session.InvokeMethod(
                instance, WmiConstants.WmiMethodName, parameters);
            return (byte[])result.OutParameters[WmiConstants.WmiOutputParam].Value;
        }

        public async Task<byte[]> InvokeAsync(ulong cmd, CimSession session, CimInstance instance,
            CancellationToken ct = default)
        {
            var parameters = CreateInputParameters(cmd);
            var asyncResult = session.InvokeMethodAsync(
                WmiConstants.WmiNamespace, instance, WmiConstants.WmiMethodName, parameters);
            var result = await asyncResult.AsTask(ct).ConfigureAwait(false);
            return (byte[])result.OutParameters[WmiConstants.WmiOutputParam].Value;
        }

        public CimMethodParametersCollection CreateInputParameters(ulong cmd)
        {
            return new CimMethodParametersCollection
            {
                CimMethodParameter.Create("u64Input", cmd, CimType.UInt64, CimFlags.In)
            };
        }
    }
}
