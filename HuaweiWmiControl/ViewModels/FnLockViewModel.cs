using System;
using System.Threading.Tasks;
using System.Windows.Input;
using HuaweiWmiControl.Services;

namespace HuaweiWmiControl.ViewModels
{
    /// <summary>
    /// Fn-Lock 标签页 ViewModel。
    /// </summary>
    public sealed class FnLockViewModel : DomainViewModelBase
    {
        private IFnLockService _service = null!;

        private bool _state;
        public bool State { get => _state; set => SetProperty(ref _state, value); }

        public ICommand ReadCommand { get; }
        public ICommand ApplyCommand { get; }

        public FnLockViewModel(ILogService log) : base(log)
        {
            ReadCommand = CreateCommand(ReadAsync, "Fn-Lock 读取");
            ApplyCommand = CreateCommand(ApplyAsync, "Fn-Lock 设置");
        }

        public void Inject(IFnLockService service) => _service = service;

        internal async Task ReadAsync()
        {
            MarkReadStarting();
            var v = await _service.GetStateAsync();
            if (v == null) { MarkReadFailed(); return; }
            State = v.Value;
            Log("Fn-Lock：" + (v.Value ? "开" : "关"));
        }

        internal async Task ApplyAsync()
        {
            var ok = await _service.SetStateAsync(State);
            LogSuccess(ok ? "Fn-Lock：已应用" : "Fn-Lock：设置失败" + FormatLastError());
        }

        private string FormatLastError()
        {
            var ex = _service.LastError;
            return ex == null ? "（不支持）" : $"：{ex.GetType().Name} — {ex.Message}";
        }
    }
}
