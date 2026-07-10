using System;
using System.Threading.Tasks;
using System.Windows.Input;
using HuaweiWmiControl.Services;

namespace HuaweiWmiControl.ViewModels
{
    /// <summary>
    /// 麦克风静音 LED 标签页 ViewModel。
    /// </summary>
    public sealed class MicMuteLedViewModel : DomainViewModelBase
    {
        private IMicMuteLedService _service = null!;

        private bool _state;
        public bool State { get => _state; set => SetProperty(ref _state, value); }

        public ICommand ApplyCommand { get; }

        public MicMuteLedViewModel(ILogService log) : base(log)
        {
            ApplyCommand = CreateCommand(ApplyAsync, "麦克风 LED 设置");
        }

        public void Inject(IMicMuteLedService service) => _service = service;

        internal async Task ApplyAsync()
        {
            var ok = await _service.SetStateAsync(State);
            LogSuccess(ok ? "麦克风 LED：已应用" : "麦克风 LED：设置失败" + FormatLastError());
        }

        private string FormatLastError()
        {
            var ex = _service.LastError;
            return ex == null ? "（不支持）" : $"：{ex.GetType().Name} — {ex.Message}";
        }
    }
}
