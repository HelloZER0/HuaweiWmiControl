using System;
using System.Threading.Tasks;
using System.Windows.Input;
using HuaweiWmiControl.Services;
using Microsoft.UI.Xaml.Controls;

namespace HuaweiWmiControl.ViewModels
{
    /// <summary>
    /// 领域 ViewModel 基类——共享 fire-and-forget 异步安全包装与日志输出。
    /// 服务通过 <see cref="MarkReady"/> 延迟注入，创建时不要求服务已就绪。
    /// 日志通过 <see cref="ILogService"/> 写入，支持级别分类和文件持久化。
    /// 可通过 <see cref="SetStatusCallback"/> 将状态推送到主窗口 InfoBar。
    /// </summary>
    public abstract class DomainViewModelBase : ViewModelBase
    {
        private readonly ILogService _log;
        private volatile bool _isReady;
        private Action<string, InfoBarSeverity>? _statusCallback;

        /// <summary>是否已就绪（服务已注入）。</summary>
        protected bool IsReady => _isReady;

        /// <summary>初次读取失败——主界面将自动隐藏此选项卡。</summary>
        public bool ReadFailed { get; private set; }

        /// <summary>初始化领域 ViewModel。</summary>
        /// <param name="log">日志服务。</param>
        protected DomainViewModelBase(ILogService log)
        {
            _log = log ?? throw new ArgumentNullException(nameof(log));
        }

        /// <summary>设置状态回调，用于将操作结果推送到全局 InfoBar。</summary>
        public void SetStatusCallback(Action<string, InfoBarSeverity> callback)
            => _statusCallback = callback;

        /// <summary>标记服务已就绪，允许执行命令。</summary>
        public void MarkReady() => _isReady = true;

        /// <summary>读取前调用，重置失败标记。</summary>
        public void MarkReadStarting() => ReadFailed = false;

        /// <summary>读取失败时调用，标记此选项卡应自动隐藏。</summary>
        public void MarkReadFailed()
        {
            ReadFailed = true;
            LogWarn("读取失败，此选项卡将被自动隐藏");
        }

        /// <summary>追加一条信息级日志。</summary>
        protected void Log(string s) => _log.Info(s);

        /// <summary>追加一条警告级日志，并显示在 InfoBar 上。</summary>
        protected void LogWarn(string s)
        {
            _log.Warn(s);
            _statusCallback?.Invoke(s, InfoBarSeverity.Warning);
        }

        /// <summary>追加一条错误级日志（含异常信息），并显示在 InfoBar 上。</summary>
        protected void LogError(string s, Exception? ex = null)
        {
            _log.Error(s, ex);
            _statusCallback?.Invoke(s, InfoBarSeverity.Error);
        }

        /// <summary>追加一条成功/信息级日志，并显示在 InfoBar 上。</summary>
        protected void LogSuccess(string s)
        {
            _log.Info(s);
            _statusCallback?.Invoke(s, InfoBarSeverity.Success);
        }

        /// <summary>
        /// 安全 fire-and-forget：捕获异常并写入日志。
        /// 若服务未就绪则提示等待。
        /// </summary>
        /// <remarks>
        /// ⚠ async void 是故意的——因需满足 ICommand.Execute 的同步签名，无法返回 Task。
        /// 异常由内部 try-catch 捕获，并借助 <see cref="App.UnhandledException"/>
        /// 作为最后防线防止进程崩溃。
        /// </remarks>
#pragma warning disable S3168 // async void 是 ICommand.Execute 签名强制要求
        protected async void SafeFireAndForget(Func<Task> handler, string label)
#pragma warning restore S3168
        {
            if (!_isReady)
            {
                Log("⚠ 服务未就绪，请等待连接完成后再试。");
                return;
            }
            try
            {
                await handler().ConfigureAwait(false);
            }
            catch (OperationCanceledException)
            {
                Log($"[{label}] 操作已取消");
            }
            catch (Exception ex)
            {
                Log($"[{label}] {ex.GetType().Name}: {ex.Message}");
            }
        }

        /// <summary>
        /// 创建 RelayCommand，自动包装为安全 fire-and-forget。
        /// </summary>
        protected ICommand CreateCommand(Func<Task> handler, string label)
            => new RelayCommand(() => SafeFireAndForget(handler, label));
    }
}
