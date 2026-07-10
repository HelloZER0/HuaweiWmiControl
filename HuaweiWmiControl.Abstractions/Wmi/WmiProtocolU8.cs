using System.Threading;
using System.Threading.Tasks;
using Microsoft.Management.Infrastructure;

namespace HuaweiWmiControl.Wmi
{
    /// <summary>
    /// U8 协议（CIM 版）——老版固件，通过 u8Input 参数传递 64 字节 SAFEARRAY。
    /// 使用 <see cref="CimSession.InvokeMethodAsync"/> 真正异步 API。
    /// </summary>
    public sealed class WmiProtocolU8 : IWmiProtocol
    {
        public string ProtocolName => "U8";

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
            var arr = new byte[WmiConstants.U8_BUFFER_LENGTH];
            for (int i = 0; i < WmiConstants.U8_CMD_BYTES; i++)
                arr[i] = (byte)(cmd >> (8 * i));
            return new CimMethodParametersCollection
            {
                CimMethodParameter.Create("u8Input", arr, CimType.UInt8Array, CimFlags.In)
            };
        }
    }
}
