using System;
using System.Threading;
using System.Threading.Tasks;
using HuaweiWmiControl.Wmi;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// WMI 领域服务的抽象基类，提供协议访问、带重试的同步/异步调用、
    /// 以及 <see cref="LastError"/> 错误追踪。
    /// </summary>
    public abstract class WmiServiceBase
    {
        /// <summary>WMI 协议实现。</summary>
        protected IWmiProtocol Protocol { get; }

        /// <summary>WMI 连接管理器。</summary>
        protected WmiConnectionManager Connection { get; }

        /// <summary>
        /// 最近一次操作发生的异常。
        /// 业务方法返回 null/false 时可通过此属性获取详细错误信息。
        /// </summary>
        public Exception? LastError { get; protected set; }

        /// <summary>
        /// 初始化 WMI 服务基类。
        /// </summary>
        /// <param name="protocol">WMI 协议实现。</param>
        /// <param name="connection">WMI 连接管理器。</param>
        protected WmiServiceBase(IWmiProtocol protocol, WmiConnectionManager connection)
        {
            Protocol = protocol ?? throw new ArgumentNullException(nameof(protocol));
            Connection = connection ?? throw new ArgumentNullException(nameof(connection));
        }

        /// <summary>
        /// 确保 WMI 连接已建立，否则抛出异常。
        /// </summary>
        protected void EnsureConnected()
        {
            if (!Connection.Available)
                throw new InvalidOperationException("WMI 未连接，请先调用 Connect()。");
        }

        // ================================================================
        // 底层调用（含重试）
        // ================================================================

        /// <summary>
        /// 同步调用 WMI 命令，内置一次重试。
        /// 失败时设置 <see cref="LastError"/> 并抛出 <see cref="WmiNotSupportedException"/>。
        /// 系统级异常（连接断开等）原样向上传播。
        /// </summary>
        protected byte[] Call(ulong cmd)
        {
            EnsureConnected();
            for (int attempt = 0; attempt < 2; attempt++)
            {
                var buf = Protocol.Invoke(cmd, Connection.Session, Connection.Instance);
                byte status = (buf is { Length: > 0 }) ? buf[0] : WmiConstants.STATUS_DEFAULT_ERROR;
                if (status == WmiConstants.STATUS_SUCCESS)
                {
                    LastError = null;
                    return buf;
                }
                if (attempt == 1)
                {
                    var ex = new WmiNotSupportedException(cmd, status);
                    LastError = ex;
                    throw ex;
                }
            }
            throw new WmiNotSupportedException(cmd, WmiConstants.STATUS_DEFAULT_ERROR);
        }

        /// <summary>
        /// 异步调用 WMI 命令，内置一次重试。
        /// 失败时设置 <see cref="LastError"/> 并抛出 <see cref="WmiNotSupportedException"/>。
        /// 系统级异常（连接断开等）原样向上传播。
        /// </summary>
        protected async Task<byte[]> CallAsync(ulong cmd, CancellationToken ct = default)
        {
            EnsureConnected();
            for (int attempt = 0; attempt < 2; attempt++)
            {
                var buf = await Protocol.InvokeAsync(cmd, Connection.Session, Connection.Instance, ct)
                    .ConfigureAwait(false);
                byte status = (buf is { Length: > 0 }) ? buf[0] : WmiConstants.STATUS_DEFAULT_ERROR;
                if (status == WmiConstants.STATUS_SUCCESS)
                {
                    LastError = null;
                    return buf;
                }
                if (attempt == 1)
                {
                    var ex = new WmiNotSupportedException(cmd, status);
                    LastError = ex;
                    throw ex;
                }
            }
            throw new WmiNotSupportedException(cmd, WmiConstants.STATUS_DEFAULT_ERROR);
        }

        // ================================================================
        // 模板方法 — 消除服务层 try-catch 重复
        // ================================================================

        /// <summary>
        /// 同步调用 WMI 命令并解析返回值（Get 操作通用模板）。
        /// 仅吞噬 <see cref="WmiNotSupportedException"/>（预期行为，如设备不支持），
        /// 其他系统级异常（连接断开等）向上传播。
        /// </summary>
        /// <typeparam name="T">返回值类型（必须是 struct）。</typeparam>
        /// <param name="cmd">命令号。</param>
        /// <param name="parser">缓冲区解析函数。</param>
        /// <param name="args">可选的命令参数。</param>
        /// <returns>解析结果，设备不支持时返回 null。</returns>
        protected T? InvokeGet<T>(ulong cmd, Func<byte[], T> parser, params byte[] args) where T : struct
        {
            try
            {
                var buf = Call(Protocol.EncodeCommand(cmd, args));
                LastError = null;
                return parser(buf);
            }
            catch (WmiNotSupportedException ex) { LastError = ex; return default; }
        }

        /// <summary>
        /// 异步调用 WMI 命令并解析返回值（Get 操作通用模板）。
        /// 吞噬 <see cref="WmiNotSupportedException"/>（预期行为，如设备不支持），
        /// 其他异常（包括 <see cref="OperationCanceledException"/>）自然向上传播。
        /// </summary>
        /// <typeparam name="T">返回值类型（必须是 struct）。</typeparam>
        /// <param name="cmd">命令号。</param>
        /// <param name="parser">缓冲区解析函数。</param>
        /// <param name="args">可选的命令参数（null=无参数）。</param>
        /// <param name="ct">取消令牌。</param>
        /// <returns>解析结果，设备不支持时返回 null。</returns>
        protected async Task<T?> InvokeGetAsync<T>(ulong cmd, Func<byte[], T> parser,
            byte[]? args = null, CancellationToken ct = default) where T : struct
        {
            try
            {
                var encoded = args is { Length: > 0 }
                    ? Protocol.EncodeCommand(cmd, args)
                    : cmd;
                var buf = await CallAsync(encoded, ct)
                    .ConfigureAwait(false);
                LastError = null;
                return parser(buf);
            }
            catch (WmiNotSupportedException ex) { LastError = ex; return default; }
        }

        /// <summary>
        /// 同步调用 WMI 设置命令（Set 操作通用模板）。
        /// 仅吞噬 <see cref="WmiNotSupportedException"/>（预期行为），
        /// 其他系统级异常向上传播。
        /// </summary>
        /// <param name="cmd">命令号。</param>
        /// <param name="args">可选的命令参数。</param>
        /// <returns>成功返回 true，设备不支持时返回 false。</returns>
        protected bool InvokeSet(ulong cmd, params byte[] args)
        {
            try
            {
                Call(Protocol.EncodeCommand(cmd, args));
                LastError = null;
                return true;
            }
            catch (WmiNotSupportedException ex) { LastError = ex; return false; }
        }

        /// <summary>
        /// 异步调用 WMI 设置命令（Set 操作通用模板）。
        /// 吞噬 <see cref="WmiNotSupportedException"/>（预期行为），
        /// 其他异常（包括 <see cref="OperationCanceledException"/>）自然向上传播。
        /// </summary>
        /// <param name="cmd">命令号。</param>
        /// <param name="args">可选的命令参数。</param>
        /// <param name="ct">取消令牌。</param>
        /// <returns>成功返回 true，设备不支持时返回 false。</returns>
        protected async Task<bool> InvokeSetAsync(ulong cmd,
            byte[]? args = null, CancellationToken ct = default)
        {
            try
            {
                var encoded = args is { Length: > 0 }
                    ? Protocol.EncodeCommand(cmd, args)
                    : cmd;
                await CallAsync(encoded, ct)
                    .ConfigureAwait(false);
                LastError = null;
                return true;
            }
            catch (WmiNotSupportedException ex) { LastError = ex; return false; }
        }
    }
}
