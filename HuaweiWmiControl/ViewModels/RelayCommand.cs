using System;
using System.Windows.Input;

namespace HuaweiWmiControl.ViewModels
{
    /// <summary>
    /// 通用的 <see cref="ICommand"/> 实现，将命令执行委托给指定的 Action。
    /// 支持异步执行（fire-and-forget）和可选的 CanExecute 判断。
    /// </summary>
    public sealed class RelayCommand : ICommand
    {
        private readonly Action _execute;
        private readonly Func<bool>? _canExecute;
        private bool _isExecuting;

        /// <summary>创建同步 RelayCommand。</summary>
        /// <param name="execute">执行委托。</param>
        /// <param name="canExecute">可选的可执行性判断委托。</param>
        public RelayCommand(Action execute, Func<bool>? canExecute = null)
        {
            _execute = execute ?? throw new ArgumentNullException(nameof(execute));
            _canExecute = canExecute;
        }

        /// <inheritdoc/>
        public event EventHandler? CanExecuteChanged;

        /// <inheritdoc/>
        public bool CanExecute(object? parameter) => !_isExecuting && (_canExecute?.Invoke() ?? true);

        /// <inheritdoc/>
        public void Execute(object? parameter)
        {
            if (!CanExecute(parameter)) return;
            _isExecuting = true;
            try
            {
                CanExecuteChanged?.Invoke(this, EventArgs.Empty);
                _execute();
            }
            finally
            {
                _isExecuting = false;
                CanExecuteChanged?.Invoke(this, EventArgs.Empty);
            }
        }

        /// <summary>通知命令可执行性已变更。</summary>
        public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
    }

    /// <summary>
    /// 泛型 <see cref="ICommand"/> 实现，支持参数传递。
    /// </summary>
    /// <typeparam name="T">命令参数类型。</typeparam>
    public sealed class RelayCommand<T> : ICommand
    {
        private readonly Action<T?> _execute;
        private readonly Func<T?, bool>? _canExecute;
        private bool _isExecuting;

        /// <summary>创建泛型 RelayCommand。</summary>
        public RelayCommand(Action<T?> execute, Func<T?, bool>? canExecute = null)
        {
            _execute = execute ?? throw new ArgumentNullException(nameof(execute));
            _canExecute = canExecute;
        }

        /// <inheritdoc/>
        public event EventHandler? CanExecuteChanged;

        /// <inheritdoc/>
        public bool CanExecute(object? parameter) => !_isExecuting && (_canExecute?.Invoke((T?)parameter) ?? true);

        /// <inheritdoc/>
        public void Execute(object? parameter)
        {
            if (!CanExecute(parameter)) return;
            _isExecuting = true;
            try
            {
                CanExecuteChanged?.Invoke(this, EventArgs.Empty);
                _execute((T?)parameter);
            }
            finally
            {
                _isExecuting = false;
                CanExecuteChanged?.Invoke(this, EventArgs.Empty);
            }
        }

        /// <summary>通知命令可执行性已变更。</summary>
        public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
    }
}
