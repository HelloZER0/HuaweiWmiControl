using System;
using System.Threading.Tasks;
using System.Windows.Input;
using HuaweiWmiControl.Services;
using Microsoft.UI.Xaml;

namespace HuaweiWmiControl.ViewModels
{
    /// <summary>
    /// 键盘背光标签页 ViewModel。
    /// 背光亮度每 500ms 自动刷新（只读），仅超时可手动写入。
    /// </summary>
    public sealed class KeyboardViewModel : DomainViewModelBase, IDisposable
    {
        private IKeyboardService _service = null!;
        private readonly DispatcherTimer _timer = new();

        private int _level;
        /// <summary>当前背光级别（0=关, 1=低, 2=高），只读。</summary>
        public int Level
        {
            get => _level;
            private set
            {
                if (SetProperty(ref _level, value))
                    OnPropertyChanged(nameof(BrightnessText));
            }
        }

        private string _brightnessText = "—";
        /// <summary>背光级别的中文显示文本。</summary>
        public string BrightnessText
        {
            get => _brightnessText;
            private set => SetProperty(ref _brightnessText, value);
        }

        private double _timeout;
        /// <summary>背光熄灭超时（秒, 0=常亮）。</summary>
        public double Timeout
        {
            get => _timeout;
            set => SetProperty(ref _timeout, value);
        }

        public ICommand ApplyCommand { get; }

        public KeyboardViewModel(ILogService log) : base(log)
        {
            ApplyCommand = CreateCommand(ApplyAsync, "键盘背光超时设置");
            _timer.Interval = TimeSpan.FromMilliseconds(500);
            _timer.Tick += async (_, _) => await RefreshLevelAsync();
        }

        public void Inject(IKeyboardService service)
        {
            _service = service;
        }

        /// <summary>就绪时启动自动刷新。</summary>
        public new void MarkReady()
        {
            base.MarkReady();
            _ = RefreshLevelAsync();
            _timer.Start();
        }

        public void Dispose()
        {
            _timer.Stop();
        }

        /// <summary>仅刷新背光级别（异步，不阻塞 UI）。</summary>
        private async Task RefreshLevelAsync()
        {
            if (!IsReady) return;
            try
            {
                var lvl = await _service.GetLevelAsync();
                if (lvl != null)
                {
                    var clamped = Math.Max(0, Math.Min(2, lvl.Value));
                    Level = clamped;
                    BrightnessText = clamped switch
                    {
                        0 => "关",
                        1 => "低 (1 级)",
                        2 => "高 (2 级)",
                        _ => $"级 {clamped}",
                    };
                }
            }
            catch
            {
                // 静默忽略——自动刷新不把异常抛到 UI
            }
        }

        internal async Task ApplyAsync()
        {
            var ok = await _service.SetTimeoutAsync((int)Timeout);
            LogSuccess(ok
                ? $"键盘背光超时：已设为 {Timeout} 秒"
                : "键盘背光超时：设置失败" + FormatLastError());
        }

        private string FormatLastError()
        {
            var ex = _service.LastError;
            return ex == null ? "（不支持）" : $"：{ex.GetType().Name} — {ex.Message}";
        }
    }
}
