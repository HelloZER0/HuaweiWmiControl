using System;
using System.Threading.Tasks;
using HuaweiWmiControl.Services;
using HuaweiWmiControl.Wmi;

namespace HuaweiWmiControl
{
    /// <summary>
    /// 华为笔记本 WMI 控制层（向后兼容 Facade）。
    /// 内部委托至按功能域拆分的服务类（BatteryService / FnLockService / KeyboardService /
    /// SensorService / SmartChargeService / PowerUnlockService / MicMuteLedService）。
    /// </summary>
    /// <remarks>
    /// 推荐直接使用 <see cref="Services"/> 下的接口属性代替旧转发方法。
    /// </remarks>
    public sealed class HwWmi : IDisposable
    {
        private readonly WmiConnectionManager _connection = new();

        /// <summary>WMI 连接是否可用。</summary>
        public bool Available => _connection.Available;

        /// <summary>连接失败原因。</summary>
        public string NotAvailableReason => _connection.NotAvailableReason;

        /// <summary>上一次读取键盘背光时的原始返回字节。</summary>
        public byte[]? LastKbdLightRawBytes => _connection.LastKbdLightRawBytes;

        /// <summary>是否检测到 MACH-WX9 等反转映射机型。</summary>
        public bool KbdLightQuirkDetected => _connection.KbdLightQuirkDetected;

        /// <summary>温度传感器区域列表。</summary>
        public static readonly (string name, int idx)[] TempZones = WmiConstants.TempZones;

        // ---- 领域服务（直接暴露）----

        /// <summary>电池充电阈值服务。</summary>
        public IBatteryService? Battery { get; private set; }

        /// <summary>Fn-Lock 服务。</summary>
        public IFnLockService? FnLock { get; private set; }

        /// <summary>键盘背光服务。</summary>
        public IKeyboardService? Keyboard { get; private set; }

        /// <summary>传感器服务。</summary>
        public ISensorService? Sensor { get; private set; }

        /// <summary>智能充电服务。</summary>
        public ISmartChargeService? SmartCharge { get; private set; }

        /// <summary>电源解锁服务。</summary>
        public IPowerUnlockService? PowerUnlock { get; private set; }

        /// <summary>麦克风静音 LED 服务。</summary>
        public IMicMuteLedService? MicMuteLed { get; private set; }

        /// <inheritdoc cref="WmiConnectionManager.Connect"/>
        public void Connect()
        {
            _connection.Connect();
            if (_connection.Available)
            {
                var p = _connection.Protocol;
                var c = _connection;
                Battery = new BatteryService(p, c);
                FnLock = new FnLockService(p, c);
                Keyboard = new KeyboardService(p, c);
                Sensor = new SensorService(p, c);
                SmartCharge = new SmartChargeService(p, c);
                PowerUnlock = new PowerUnlockService(p, c);
                MicMuteLed = new MicMuteLedService(p, c);
            }
        }

        private bool _disposed;

        /// <summary>释放所有 WMI COM 资源。</summary>
        public void Dispose()
        {
            if (_disposed) return;
            _disposed = true;
            _connection.Dispose();
            GC.SuppressFinalize(this);
        }
    }
}
