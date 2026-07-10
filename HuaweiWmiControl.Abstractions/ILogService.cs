namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// 日志级别。
    /// </summary>
    public enum LogLevel
    {
        /// <summary>一般信息。</summary>
        Info,
        /// <summary>警告——不影响功能但值得注意。</summary>
        Warning,
        /// <summary>错误——功能失败需要关注。</summary>
        Error,
    }

    /// <summary>
    /// 应用日志服务——支持分级日志、内存缓冲区（供 UI 展示）和文件持久化。
    /// </summary>
    public interface ILogService
    {
        /// <summary>记录信息级日志。</summary>
        void Info(string message);

        /// <summary>记录警告级日志。</summary>
        void Warn(string message);

        /// <summary>记录错误级日志（含可选异常信息）。</summary>
        void Error(string message, Exception? ex = null);

        /// <summary>获取当前内存缓冲区中的全部日志文本（供 UI 绑定）。</summary>
        string GetLogText();

        /// <summary>清空内存缓冲区。</summary>
        void Clear();
    }
}
