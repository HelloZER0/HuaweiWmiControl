using System;
using System.Threading.Tasks;
using System.Windows.Input;
using HuaweiWmiControl.Services;

namespace HuaweiWmiControl.ViewModels
{
    /// <summary>
    /// 电源解锁标签页 ViewModel。
    /// </summary>
    public sealed class PowerUnlockViewModel : DomainViewModelBase
    {
        private IPowerUnlockService _service = null!;

        private bool _state;
        public bool State { get => _state; set => SetProperty(ref _state, value); }

        public ICommand ReadCommand { get; }
        public ICommand ApplyCommand { get; }

        public PowerUnlockViewModel(ILogService log) : base(log)
        {
            ReadCommand = CreateCommand(ReadAsync, "电源解锁读取");
            ApplyCommand = CreateCommand(ApplyAsync, "电源解锁设置");
        }

        public void Inject(IPowerUnlockService service) => _service = service;

        internal async Task ReadAsync()
        {
            MarkReadStarting();
            var v = await _service.GetStateAsync();
            if (v == null) { MarkReadFailed(); return; }
            State = v.Value;
            Log("电源解锁：" + (v.Value ? "开" : "关"));
        }

        internal async Task ApplyAsync()
        {
            var ok = await _service.SetStateAsync(State);
            LogSuccess(ok ? "电源解锁：已应用" : "电源解锁：设置失败" + FormatLastError());
        }

        private string FormatLastError()
        {
            var ex = _service.LastError;
            return ex == null ? "（不支持）" : $"：{ex.GetType().Name} — {ex.Message}";
        }
    }
}
