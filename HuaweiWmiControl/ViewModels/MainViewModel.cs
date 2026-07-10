using System;
using System.Threading.Tasks;
using System.Windows.Input;
using Microsoft.Extensions.DependencyInjection;
using HuaweiWmiControl.Services;
using HuaweiWmiControl.Wmi;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace HuaweiWmiControl.ViewModels
{
    /// <summary>
    /// 主窗口 ViewModel——协调 7 个领域标签页、WMI 连接与日志。
    /// 各标签页的业务逻辑已提取至 <see cref="BatteryViewModel"/> 等子 ViewModel。
    /// 日志通过 <see cref="ILogService"/> 的 <see cref="LogService"/> 实现写入内存和文件。
    /// </summary>
    public sealed class MainViewModel : ViewModelBase, IDisposable
    {
        private readonly WmiConnectionManager _connection;
        private readonly ILogService _logService;

        public BatteryViewModel Battery { get; }
        public FnLockViewModel FnLock { get; }
        public KeyboardViewModel Keyboard { get; }
        public SmartChargeViewModel SmartCharge { get; }
        public PowerUnlockViewModel PowerUnlock { get; }
        public MicMuteLedViewModel MicMuteLed { get; }
        public SensorViewModel Sensors { get; }

        // ---- 选项卡可见性（所有选项卡默认可见，传感器始终可见）----
        private static readonly string[] TabTags = { "battery", "fnlock", "keyboard", "smartcharge", "powerunlock", "micled", "sensors" };
        private static readonly string[] TabLabels = { "电池保护", "Fn-Lock", "键盘背光", "智能充电", "电源解锁", "麦克风 LED", "传感器" };

        private bool _batteryVisible = true;
        public bool BatteryVisible
        {
            get => _batteryVisible;
            set { if (SetProperty(ref _batteryVisible, value)) OnTabVisibleChanged("battery"); }
        }
        public Visibility BatteryVisibility => BatteryVisible ? Visibility.Visible : Visibility.Collapsed;

        private bool _fnLockVisible = true;
        public bool FnLockVisible
        {
            get => _fnLockVisible;
            set { if (SetProperty(ref _fnLockVisible, value)) OnTabVisibleChanged("fnlock"); }
        }
        public Visibility FnLockVisibility => FnLockVisible ? Visibility.Visible : Visibility.Collapsed;

        private bool _keyboardVisible = true;
        public bool KeyboardVisible
        {
            get => _keyboardVisible;
            set { if (SetProperty(ref _keyboardVisible, value)) OnTabVisibleChanged("keyboard"); }
        }
        public Visibility KeyboardVisibility => KeyboardVisible ? Visibility.Visible : Visibility.Collapsed;

        private bool _smartChargeVisible = true;
        public bool SmartChargeVisible
        {
            get => _smartChargeVisible;
            set { if (SetProperty(ref _smartChargeVisible, value)) OnTabVisibleChanged("smartcharge"); }
        }
        public Visibility SmartChargeVisibility => SmartChargeVisible ? Visibility.Visible : Visibility.Collapsed;

        private bool _powerUnlockVisible = true;
        public bool PowerUnlockVisible
        {
            get => _powerUnlockVisible;
            set { if (SetProperty(ref _powerUnlockVisible, value)) OnTabVisibleChanged("powerunlock"); }
        }
        public Visibility PowerUnlockVisibility => PowerUnlockVisible ? Visibility.Visible : Visibility.Collapsed;

        private bool _micLedVisible = true;
        public bool MicLedVisible
        {
            get => _micLedVisible;
            set { if (SetProperty(ref _micLedVisible, value)) OnTabVisibleChanged("micled"); }
        }
        public Visibility MicLedVisibility => MicLedVisible ? Visibility.Visible : Visibility.Collapsed;

        /// <summary>传感器始终可见，不可关闭。</summary>
        public static bool SensorsVisible => true;
        public static Visibility SensorsVisibility => Visibility.Visible;

        /// <summary>选项卡管理：批量标签与可见性状态。</summary>
        public (string tag, string label, bool visible)[] TabVisibilityItems
        {
            get
            {
                var states = new[] { BatteryVisible, FnLockVisible, KeyboardVisible,
                    SmartChargeVisible, PowerUnlockVisible, MicLedVisible, true };
                var items = new (string, string, bool)[TabTags.Length];
                for (int i = 0; i < TabTags.Length; i++)
                    items[i] = (TabTags[i], TabLabels[i], states[i]);
                return items;
            }
        }

        /// <summary>关闭所有选项卡（保留传感器）。</summary>
        public ICommand HideAllExceptSensorsCommand { get; }

        // ---- 连接状态 ----
        private string _statusMessage = "正在连接…";
        public string StatusMessage
        {
            get => _statusMessage;
            set => SetProperty(ref _statusMessage, value);
        }

        private bool _isConnected;
        public bool IsConnected
        {
            get => _isConnected;
            set => SetProperty(ref _isConnected, value);
        }

        private bool _isLoading;
        public bool IsLoading
        {
            get => _isLoading;
            set => SetProperty(ref _isLoading, value);
        }

        private InfoBarSeverity _statusSeverity = InfoBarSeverity.Informational;
        public InfoBarSeverity StatusSeverity
        {
            get => _statusSeverity;
            set => SetProperty(ref _statusSeverity, value);
        }

        private bool _isStatusOpen;
        public bool IsStatusOpen
        {
            get => _isStatusOpen;
            set => SetProperty(ref _isStatusOpen, value);
        }

        private string _protocolName = "";
        public string ProtocolName
        {
            get => _protocolName;
            set => SetProperty(ref _protocolName, value);
        }

        // ---- 日志 ----
        private string _logText = "";
        /// <summary>日志文本（由 <see cref="ILogService"/> 提供，每 500ms 刷新）。</summary>
        public string LogText
        {
            get => _logText;
            private set => SetProperty(ref _logText, value);
        }

        public MainViewModel()
        {
            _connection = App.Services.GetRequiredService<WmiConnectionManager>();
            _logService = App.Services.GetRequiredService<ILogService>();

            Battery = new BatteryViewModel(_logService);
            FnLock = new FnLockViewModel(_logService);
            Keyboard = new KeyboardViewModel(_logService);
            SmartCharge = new SmartChargeViewModel(_logService);
            PowerUnlock = new PowerUnlockViewModel(_logService);
            MicMuteLed = new MicMuteLedViewModel(_logService);
            Sensors = new SensorViewModel(_logService);

            WireStatusCallbacks();

            HideAllExceptSensorsCommand = new RelayCommand(() =>
            {
                BatteryVisible = false;
                FnLockVisible = false;
                KeyboardVisible = false;
                SmartChargeVisible = false;
                PowerUnlockVisible = false;
                MicLedVisible = false;
                Log("已隐藏除传感器外的所有选项卡");
            });
        }

        public async Task InitializeAsync()
        {
            try
            {
                IsLoading = true;
                Log("初始化开始…");

                _connection.Connect();
                Log($"Connect 完成: Available={_connection.Available}");
                if (!string.IsNullOrEmpty(_connection.NotAvailableReason))
                    Log($"原因: {_connection.NotAvailableReason}");

                IsConnected = _connection.Available;
                if (!_connection.Available)
                {
                    StatusMessage = "未连接到设备管理接口：" + _connection.NotAvailableReason;
                    StatusSeverity = InfoBarSeverity.Warning;
                    IsStatusOpen = true;
                    IsLoading = false;
                    return;
                }

                WireServices();
                ProtocolName = _connection.Protocol.ProtocolName;
                Log($"已连接（协议: {ProtocolName}），读取状态…");
                StatusMessage = $"已连接（{ProtocolName}），读取状态…";
                StatusSeverity = InfoBarSeverity.Informational;

                await RefreshAllAsync();
                Log("所有功能就绪");
                StatusMessage = "所有功能就绪";
                StatusSeverity = InfoBarSeverity.Success;
                IsLoading = false;
            }
            catch (Exception ex)
            {
                Log($"启动异常: {ex.GetType().Name}: {ex.Message}");
                StatusMessage = $"启动错误: {ex.Message}";
                StatusSeverity = InfoBarSeverity.Error;
                IsStatusOpen = true;
                IsLoading = false;
            }
        }

        public void Dispose()
        {
            LogText = _logService.GetLogText(); // 刷新最后一帧日志
            if (_logService is IDisposable d) d.Dispose();
            _connection.Dispose();
        }

        /// <summary>选项卡可见性变更后的附加通知（Visibility 属性、TabVisibilityItems）。</summary>
        private void OnTabVisibleChanged(string tag)
        {
            if (tag == "sensors") return;
            OnPropertyChanged(tag switch
            {
                "battery" => nameof(BatteryVisibility),
                "fnlock" => nameof(FnLockVisibility),
                "keyboard" => nameof(KeyboardVisibility),
                "smartcharge" => nameof(SmartChargeVisibility),
                "powerunlock" => nameof(PowerUnlockVisibility),
                "micled" => nameof(MicLedVisibility),
                _ => "",
            });
            OnPropertyChanged(nameof(TabVisibilityItems));
        }

        /// <summary>追加一条信息级日志，并刷新 UI 绑定。</summary>
        private void Log(string s)
        {
            _logService.Info(s);
            LogText = _logService.GetLogText();
        }

        /// <summary>为所有子 ViewModel 设置状态回调，操作结果自动显示在 InfoBar 上。</summary>
        private void WireStatusCallbacks()
        {
            Action<DomainViewModelBase> wire = vm => vm.SetStatusCallback((msg, sev) =>
            {
                StatusMessage = msg;
                StatusSeverity = sev;
                IsStatusOpen = true;
            });
            wire(Battery);
            wire(FnLock);
            wire(Keyboard);
            wire(SmartCharge);
            wire(PowerUnlock);
            wire(MicMuteLed);
            wire(Sensors);
        }

        public void ClearLog()
        {
            _logService.Clear();
            LogText = _logService.GetLogText();
        }

        private void WireServices()
        {
            // 通过 DI 容器解析所有服务实例
            Battery.Inject(App.Services.GetRequiredService<IBatteryService>());
            FnLock.Inject(App.Services.GetRequiredService<IFnLockService>());
            Keyboard.Inject(App.Services.GetRequiredService<IKeyboardService>());
            SmartCharge.Inject(App.Services.GetRequiredService<ISmartChargeService>());
            PowerUnlock.Inject(App.Services.GetRequiredService<IPowerUnlockService>());
            MicMuteLed.Inject(App.Services.GetRequiredService<IMicMuteLedService>());
            Sensors.Inject(App.Services.GetRequiredService<ISensorService>());

            Battery.MarkReady();
            FnLock.MarkReady();
            Keyboard.MarkReady();
            SmartCharge.MarkReady();
            PowerUnlock.MarkReady();
            MicMuteLed.MarkReady();
            Sensors.MarkReady();
        }

        private async Task RefreshAllAsync()
        {
            IsLoading = true;
            await Task.WhenAll(
                Battery.ReadAsync(),
                FnLock.ReadAsync(),
                SmartCharge.ReadAsync(),
                PowerUnlock.ReadAsync(),
                Sensors.RefreshAsync()
            );
            // 初次读取失败的选项卡自动隐藏（设备不支持的功能不展示）
            if (Battery.ReadFailed) BatteryVisible = false;
            if (FnLock.ReadFailed) FnLockVisible = false;
            if (SmartCharge.ReadFailed) SmartChargeVisible = false;
            if (PowerUnlock.ReadFailed) PowerUnlockVisible = false;
            IsLoading = false;
        }
    }
}
