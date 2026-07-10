using System;
using System.Threading.Tasks;
using System.Windows.Input;
using HuaweiWmiControl.Services;

namespace HuaweiWmiControl.ViewModels
{
    /// <summary>
    /// 电池保护标签页 ViewModel。
    /// </summary>
    public sealed class BatteryViewModel : DomainViewModelBase
    {
        private IBatteryService _service = null!;

        // ---- 属性 ----
        private double _start;
        public double Start { get => _start; set => SetProperty(ref _start, value); }

        private double _end = 100;
        public double End { get => _end; set => SetProperty(ref _end, value); }

        private bool _reset;
        public bool Reset { get => _reset; set => SetProperty(ref _reset, value); }

        // ---- 命令 ----
        public ICommand ReadCommand { get; }
        public ICommand ApplyCommand { get; }
        /// <summary>关闭充电控制——一键重置为 0-100%（满范围充电）。</summary>
        public ICommand DisableCommand { get; }

        public BatteryViewModel(ILogService log) : base(log)
        {
            ReadCommand = CreateCommand(ReadAsync, "电池保护读取");
            ApplyCommand = CreateCommand(ApplyAsync, "电池保护设置");
            DisableCommand = CreateCommand(DisableAsync, "关闭充电控制");
        }

        /// <summary>连接后注入服务实例。</summary>
        public void Inject(IBatteryService service) => _service = service;

        internal async Task ReadAsync()
        {
            MarkReadStarting();
            var v = await _service.GetThresholdAsync();
            if (v == null)
            {
                MarkReadFailed();
                return;
            }
            Start = v.Value.start;
            End = v.Value.end;
            Log($"电池保护：当前 {v.Value.start}-{v.Value.end}");
        }

        internal async Task ApplyAsync()
        {
            var ok = await _service.SetThresholdAsync((int)Start, (int)End, Reset);
            Log(ok ? $"电池保护：已设为 {(int)Start}-{(int)End}" : "电池保护：设置失败" + FormatLastError());
        }

        /// <summary>关闭充电控制：重置阈值 0-100（满范围充电），启用 MACH-WX9 兼容路径。</summary>
        internal async Task DisableAsync()
        {
            var ok = await _service.SetThresholdAsync(0, 100, resetBeforeDisable: true);
            if (ok)
            {
                Start = 0;
                End = 100;
                LogSuccess("充电控制：已关闭（阈值 0-100）");
            }
            else
            {
                LogWarn("充电控制：关闭失败" + FormatLastError());
            }
        }

        private string FormatLastError()
        {
            var ex = _service.LastError;
            return ex == null ? "（不支持）" : $"：{ex.GetType().Name} — {ex.Message}";
        }
    }
}
