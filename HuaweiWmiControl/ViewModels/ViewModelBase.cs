using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace HuaweiWmiControl.ViewModels
{
    /// <summary>
    /// ViewModel 基类，提供 <see cref="INotifyPropertyChanged"/> 实现和属性设置辅助方法。
    /// </summary>
    public abstract class ViewModelBase : INotifyPropertyChanged
    {
        /// <inheritdoc/>
        public event PropertyChangedEventHandler? PropertyChanged;

        /// <summary>
        /// 触发 <see cref="PropertyChanged"/> 事件。
        /// </summary>
        /// <param name="propertyName">属性名称（由 CallerMemberName 自动填充）。</param>
        protected void OnPropertyChanged([CallerMemberName] string? propertyName = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));

        /// <summary>
        /// 设置属性值并在值变化时触发通知。
        /// </summary>
        /// <typeparam name="T">属性类型。</typeparam>
        /// <param name="field">字段引用。</param>
        /// <param name="value">新值。</param>
        /// <param name="propertyName">属性名称（自动填充）。</param>
        /// <returns>值是否已更改。</returns>
        protected bool SetProperty<T>(ref T field, T value, [CallerMemberName] string? propertyName = null)
        {
            if (EqualityComparer<T>.Default.Equals(field, value))
                return false;
            field = value;
            OnPropertyChanged(propertyName);
            return true;
        }
    }
}
