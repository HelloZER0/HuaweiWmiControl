using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace HuaweiWmiControl.Models
{
    /// <summary>
    /// 传感器数据项，用于 ListView 的 {x:Bind} 数据绑定。
    /// 实现 <see cref="INotifyPropertyChanged"/> 以支持运行时更新。
    /// </summary>
    public sealed class SensorItem : INotifyPropertyChanged
    {
        private string _name = "";
        private string _value = "";

        /// <summary>传感器名称（如 "CPU"、"风扇 1"）。</summary>
        public string Name
        {
            get => _name;
            set { if (_name != value) { _name = value; OnPropertyChanged(); } }
        }

        /// <summary>传感器数值（如 "45 °C"、"3200RPM"、"不支持"）。</summary>
        public string Value
        {
            get => _value;
            set { if (_value != value) { _value = value; OnPropertyChanged(); } }
        }

        /// <inheritdoc/>
        public event PropertyChangedEventHandler? PropertyChanged;

        /// <summary>触发属性变更通知。</summary>
        private void OnPropertyChanged([CallerMemberName] string? propertyName = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
}
