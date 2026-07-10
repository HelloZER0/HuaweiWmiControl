using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Input;
using HuaweiWmiControl.Models;
using HuaweiWmiControl.Services;
using HuaweiWmiControl.Wmi;
using Microsoft.UI.Xaml;

namespace HuaweiWmiControl.ViewModels
{
    public sealed class SensorViewModel : DomainViewModelBase
    {
        private ISensorService _service = null!;
        private DispatcherTimer? _autoRefreshTimer;

        public ObservableCollection<SensorItem> Items { get; }

        public ICommand RefreshCommand { get; }
        public ICommand ToggleAutoRefreshCommand { get; }

        private bool _isAutoRefresh;
        public bool IsAutoRefresh
        {
            get => _isAutoRefresh;
            set
            {
                if (SetProperty(ref _isAutoRefresh, value))
                    OnPropertyChanged(nameof(AutoRefreshLabel));
            }
        }

        /// <summary>自动刷新按钮文字。</summary>
        public string AutoRefreshLabel => IsAutoRefresh ? "自动刷新开" : "自动刷新关";

        public SensorViewModel(ILogService log) : base(log)
        {
            RefreshCommand = CreateCommand(RefreshAsync, "传感器刷新");
            ToggleAutoRefreshCommand = new RelayCommand(ToggleAutoRefresh);

            // 预填充固定传感器项，后续只更新值
            var items = new ObservableCollection<SensorItem>();
            items.Add(new SensorItem { Name = "风扇 1 (RPM)", Value = "—" });
            items.Add(new SensorItem { Name = "风扇 2 (RPM)", Value = "—" });
            foreach (var z in WmiConstants.TempZones)
                items.Add(new SensorItem { Name = "温度 " + z.name, Value = "—" });
            Items = items;
        }

        public void Inject(ISensorService service) => _service = service;

        internal async Task RefreshAsync()
        {
            var fan1Task = _service.GetFanSpeedAsync(0);
            var fan2Task = _service.GetFanSpeedAsync(1);
            var tempTasks = WmiConstants.TempZones
                .Select(z => _service.GetTempAsync(z.idx))
                .ToArray();

            await Task.WhenAll(fan1Task, fan2Task);
            await Task.WhenAll(tempTasks);

            // 就地更新，不新建对象
            UpdateItem(0, await fan1Task, "");
            UpdateItem(1, await fan2Task, "");

            for (int i = 0; i < WmiConstants.TempZones.Length; i++)
                UpdateItem(2 + i, await tempTasks[i], " °C");

            Log("传感器：已刷新");
        }

        private void ToggleAutoRefresh()
        {
            if (!IsReady) return;

            if (_autoRefreshTimer == null)
            {
                _autoRefreshTimer = new DispatcherTimer();
                _autoRefreshTimer.Interval = TimeSpan.FromSeconds(3);
                _autoRefreshTimer.Tick += async (s, e) =>
                {
                    if (!IsReady) return;
                    await RefreshAsync();
                };
            }

            if (_autoRefreshTimer.IsEnabled)
            {
                _autoRefreshTimer.Stop();
                IsAutoRefresh = false;
                Log("传感器：自动刷新关");
            }
            else
            {
                _autoRefreshTimer.Start();
                IsAutoRefresh = true;
                Log("传感器：自动刷新开（每 3 秒）");
            }
        }

        private void UpdateItem(int index, int? val, string unit)
        {
            var item = Items[index];
            var newValue = val == null ? "不支持" : val.Value + unit;
            if (item.Value != newValue)
                item.Value = newValue;
        }
    }
}
