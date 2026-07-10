using System;
using System.Threading.Tasks;
using System.Windows.Input;
using HuaweiWmiControl.Services;

namespace HuaweiWmiControl.ViewModels
{
    /// <summary>
    /// 智能充电标签页 ViewModel。
    /// </summary>
    public sealed class SmartChargeViewModel : DomainViewModelBase
    {
        private ISmartChargeService _service = null!;

        private double _mode;
        public double Mode { get => _mode; set => SetProperty(ref _mode, value); }

        private double _unknown;
        public double Unknown { get => _unknown; set => SetProperty(ref _unknown, value); }

        private double _start;
        public double Start { get => _start; set => SetProperty(ref _start, value); }

        private double _end = 100;
        public double End { get => _end; set => SetProperty(ref _end, value); }

        private double _param;
        public double Param { get => _param; set => SetProperty(ref _param, value); }

        public ICommand ReadCommand { get; }
        public ICommand ApplyCommand { get; }

        public SmartChargeViewModel(ILogService log) : base(log)
        {
            ReadCommand = CreateCommand(ReadAsync, "智能充电读取");
            ApplyCommand = CreateCommand(ApplyAsync, "智能充电设置");
        }

        public void Inject(ISmartChargeService service) => _service = service;

        internal async Task ReadAsync()
        {
            MarkReadStarting();
            var vTask = _service.GetModeAsync();
            var pTask = _service.GetParamAsync();
            await Task.WhenAll(vTask, pTask);

            var v = await vTask;
            if (v == null) { MarkReadFailed(); return; }
            Mode = v.Value.mode;
            Unknown = v.Value.unknown;
            Start = v.Value.start;
            End = v.Value.end;

            var p = await pTask;
            if (p != null) Param = p.Value;
            Log($"智能充电：mode={v.Value.mode} unknown={v.Value.unknown} " +
                $"{v.Value.start}-{v.Value.end} param={p}");
        }

        internal async Task ApplyAsync()
        {
            var ok = await _service.SetModeAsync((int)Mode, (int)Unknown, (int)Start, (int)End);
            var okp = await _service.SetParamAsync((int)Param);
            LogSuccess((ok ? "智能充电：已应用" : "智能充电：失败" + FormatLastError()) +
                "；" + (okp ? "参数：已应用" : "参数：失败" + FormatLastError()));
        }

        private string FormatLastError()
        {
            var ex = _service.LastError;
            return ex == null ? "（不支持）" : $"：{ex.GetType().Name} — {ex.Message}";
        }
    }
}
