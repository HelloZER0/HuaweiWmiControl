using System.Threading;
using System.Threading.Tasks;
using Microsoft.Management.Infrastructure;

namespace HuaweiWmiControl.Wmi
{
    /// <summary>
    /// WMI 协议抽象（CIM 版本）——支持 U64 和 U8 两种命令编码格式。
    /// </summary>
    public interface IWmiProtocol
    {
        /// <summary>协议标识名称。</summary>
        string ProtocolName { get; }

        /// <summary>同步调用 OemWMIfun，返回 u8Output 缓冲区。</summary>
        byte[] Invoke(ulong cmd, CimSession session, CimInstance instance);

        /// <summary>异步调用 OemWMIfun。</summary>
        Task<byte[]> InvokeAsync(ulong cmd, CimSession session, CimInstance instance, CancellationToken ct = default);

        /// <summary>创建输入参数（U64 或 U8 格式）。</summary>
        CimMethodParametersCollection CreateInputParameters(ulong cmd);

        /// <summary>
        /// 编码命令号与参数——将命令号和可选参数打包为 64 位值。
        /// 命令号占低 16 位，参数从第 3 字节起依次左移。
        /// </summary>
        /// <param name="cmd">命令号（低 16 位有效）。</param>
        /// <param name="args">可选参数。</param>
        ulong EncodeCommand(ulong cmd, params byte[] args)
        {
            ulong v = cmd;
            for (int i = 0; i < args.Length; i++)
                v |= (ulong)args[i] << (8 * (WmiConstants.CMD_ARG_OFFSET + i));
            return v;
        }
    }
}
