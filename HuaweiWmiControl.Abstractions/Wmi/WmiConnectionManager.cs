using System;
using System.Linq;
using System.Security.Principal;
using Microsoft.Management.Infrastructure;

namespace HuaweiWmiControl.Wmi
{
    /// <summary>
    /// WMI 连接管理器（CIM 版）——使用 Microsoft.Management.Infrastructure
    /// （WS-Man/CIM 协议）替代 System.Management（DCOM 协议）。
    /// </summary>
    public class WmiConnectionManager : IDisposable
    {
        private CimSession? _session;
        private CimInstance? _instance;
        private IWmiProtocol? _protocol;

        private volatile bool _available;
        /// <summary>WMI 连接是否可用。volatile 确保多线程可见性。</summary>
        public virtual bool Available
        {
            get => _available;
            protected set => _available = value;
        }

        private string _notAvailableReason = "";
        /// <summary>连接不可用的原因说明。</summary>
        public virtual string NotAvailableReason
        {
            get => _notAvailableReason;
            protected set => _notAvailableReason = value;
        }

        public virtual IWmiProtocol Protocol =>
            _protocol ?? throw new InvalidOperationException("WMI 未连接");

        public virtual CimSession Session =>
            _session ?? throw new InvalidOperationException("WMI 未连接");

        public virtual CimInstance Instance =>
            _instance ?? throw new InvalidOperationException("WMI 未连接");

        /// <summary>供测试子类使用的无参构造，不初始化 WMI 连接字段。</summary>
        protected WmiConnectionManager() { }

        /// <summary>供生产代码使用的默认构造。</summary>
        public WmiConnectionManager(bool _ = true) { }

        public KbdLightEncoding KbdEncoding { get; protected set; } = KbdLightEncoding.Unknown;

        /// <summary>最近一次键盘背光读取的原始字节（由 KeyboardService 写入）。</summary>
        public byte[]? LastKbdLightRawBytes { get; set; }
        public bool KbdLightQuirkDetected => KbdEncoding == KbdLightEncoding.BitmaskInverted;

        /// <summary>
        /// 连接到本地 WMI 并探测协议与键盘编码。
        /// 必须在 STA 线程上调用（UI 线程）。
        /// </summary>
        public void Connect()
        {
            if (!IsRunningAsAdmin())
            {
                Available = false;
                NotAvailableReason = "需要管理员权限。请右键 exe → 以管理员身份运行。";
                return;
            }

            try
            {
                _session = CimSession.Create(null);

                // 查找 HWMI 实例
                _instance = FindHwmiInstance(_session);
                if (_instance == null) return;

                // 探测协议（U64 vs U8）
                _protocol = DetectProtocol(_session);

                Available = true;
                DetectKbdLightEncoding();
            }
            catch (CimException ex)
            {
                Available = false;
                NotAvailableReason = $"CIM 连接失败: {ex.Message}。\n" +
                    "请确认华为电脑管家已安装且以管理员身份运行。";
            }
        }

        private bool _disposed;

        public void Dispose()
        {
            if (_disposed) return;
            _disposed = true;
            _instance?.Dispose();
            _instance = null;
            _session?.Dispose();
            _session = null;
            GC.SuppressFinalize(this);
        }

        // ================================================================
        // 步骤分解
        // ================================================================

        private CimInstance? FindHwmiInstance(CimSession session)
        {
            var instances = session.EnumerateInstances(
                WmiConstants.WmiNamespace, WmiConstants.WmiClassName);
            var list = instances.ToList();

            var hwmi = list.FirstOrDefault(o =>
            {
                var name = o.CimInstanceProperties[WmiConstants.WmiInstanceNameProp]?.Value as string;
                return name != null && name.Contains(WmiConstants.WmiInstanceNameFilter);
            }) ?? list.FirstOrDefault();

            if (hwmi == null)
            {
                Available = false;
                NotAvailableReason =
                    $"未找到 OemWMIMethod 实例（共 {list.Count} 个，均不含 HWMI）";
            }
            return hwmi;
        }

        private static IWmiProtocol DetectProtocol(CimSession session)
        {
            using var testClass = session.GetClass(
                WmiConstants.WmiNamespace, WmiConstants.WmiClassName);
            var method = testClass.CimClassMethods[WmiConstants.WmiMethodName];
            return method.Parameters.Any(p => p.Name == "u64Input")
                ? new WmiProtocolU64()
                : new WmiProtocolU8();
        }

        // ================================================================
        // 键盘背光编码检测
        // ================================================================

        private void DetectKbdLightEncoding()
        {
            if (_protocol == null || _instance == null || _session == null) return;

            try
            {
                var cmd = _protocol.EncodeCommand(WmiConstants.C_KBDLIGHT_GET);
                byte[] b;
                try
                {
                    b = InvokeInternal(cmd);
                }
                catch (CimException)
                {
                    // 固件不支持背光读取，编码未知
                    KbdEncoding = KbdLightEncoding.Unknown;
                    return;
                }

                LastKbdLightRawBytes = b;
                KbdEncoding = ClassifyKbdEncoding(b);
            }
            catch (CimException)
            {
                // 固件不支持背光读取，编码未知
                KbdEncoding = KbdLightEncoding.Unknown;
            }
        }

        private static KbdLightEncoding ClassifyKbdEncoding(byte[] b)
        {
            // 百分比编码：b[1]=0x01 且 b[2] 取值为 {0x00, 0x32, 0x64}
            if (b[1] == WmiConstants.KBD_PERCENT_FLAG &&
                (b[2] == 0x00 || b[2] == 0x32 || b[2] == 0x64))
                return KbdLightEncoding.Percent;

            // 位掩码编码：b[2] 非零
            if (b[2] != 0)
                return b[1] == WmiConstants.KBD_INVERTED_FLAG
                    ? KbdLightEncoding.BitmaskInverted
                    : KbdLightEncoding.BitmaskNormal;

            return KbdLightEncoding.Unknown;
        }

        private byte[] InvokeInternal(ulong cmd)
        {
            if (_protocol == null || _session == null || _instance == null)
                throw new InvalidOperationException("WMI 未连接。");

            for (int attempt = 0; attempt < 2; attempt++)
            {
                var buf = _protocol.Invoke(cmd, _session, _instance);
                byte status = (buf is { Length: > 0 }) ? buf[0] : WmiConstants.STATUS_DEFAULT_ERROR;
                if (status == WmiConstants.STATUS_SUCCESS) return buf;
                if (attempt == 1)
                    throw new CimException($"命令 0x{cmd:X} 返回状态 0x{status:X2}");
            }
            throw new CimException("不可达");
        }

        private static bool IsRunningAsAdmin()
        {
            using var identity = WindowsIdentity.GetCurrent();
            return new WindowsPrincipal(identity).IsInRole(WindowsBuiltInRole.Administrator);
        }
    }
}
