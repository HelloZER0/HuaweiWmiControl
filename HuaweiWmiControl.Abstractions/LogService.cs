using System.Text;

namespace HuaweiWmiControl.Services
{
    /// <summary>
    /// 日志服务实现——同时写入内存缓冲区（供 UI 展示）和滚动日志文件。
    /// 日志文件路径：%LOCALAPPDATA%/HuaweiWmiControl/logs/
    /// </summary>
    public sealed class LogService : ILogService, IDisposable
    {
        private readonly StringBuilder _buffer = new();
        private readonly string _logFilePath;
        private readonly int _maxBufferLines;
        private const string TimeFormat = "HH:mm:ss";
        private const int MaxBufferLinesDefault = 400;
        private const int MaxLogFileSize = 1024 * 512; // 512KB

        /// <summary>
        /// 初始化日志服务。
        /// </summary>
        /// <param name="maxBufferLines">内存缓冲区最大行数，超限时截断。</param>
        public LogService(int maxBufferLines = MaxBufferLinesDefault)
        {
            _maxBufferLines = maxBufferLines;

            var appData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            var logDir = Path.Combine(appData, "HuaweiWmiControl", "logs");
            Directory.CreateDirectory(logDir);
            _logFilePath = Path.Combine(logDir, "app.log");

            // 启动时写入分隔线
            var separator = $"=== 会话开始 {DateTime.Now:yyyy-MM-dd HH:mm:ss} ===";
            AppendToBuffer(separator);
            AppendToFile(separator);
        }

        public void Info(string message) => Write(LogLevel.Info, message);
        public void Warn(string message) => Write(LogLevel.Warning, message);

        public void Error(string message, Exception? ex = null)
        {
            var msg = ex == null ? message : $"{message} | {ex.GetType().Name}: {ex.Message}";
            Write(LogLevel.Error, msg);
        }

        public string GetLogText() => _buffer.ToString();

        public void Clear()
        {
            _buffer.Clear();
            Info("日志已清空");
        }

        public void Dispose()
        {
            var separator = $"=== 会话结束 {DateTime.Now:yyyy-MM-dd HH:mm:ss} ===";
            AppendToBuffer(separator);
            AppendToFile(separator);
        }

        // ---- 内部 ----

        private static readonly Dictionary<LogLevel, string> LevelTags = new()
        {
            [LogLevel.Info] = "INF",
            [LogLevel.Warning] = "WRN",
            [LogLevel.Error] = "ERR",
        };

        private void Write(LogLevel level, string message)
        {
            var line = $"[{DateTime.Now.ToString(TimeFormat)}][{LevelTags[level]}] {message}";
            lock (_buffer)
            {
                _buffer.AppendLine(line);
                TrimBuffer();
            }
            AppendToFile(line);
        }

        private void AppendToBuffer(string line)
        {
            lock (_buffer)
            {
                _buffer.AppendLine(line);
                TrimBuffer();
            }
        }

        private void TrimBuffer()
        {
            int lines = 0, pos = _buffer.Length;
            while (pos > 0 && lines < _maxBufferLines)
            {
                pos--;
                if (_buffer[pos] == '\n') lines++;
            }
            if (pos > 0)
            {
                var tail = _buffer.ToString(pos + 1, _buffer.Length - pos - 1);
                _buffer.Clear();
                _buffer.Append(tail);
            }
        }

        private void AppendToFile(string line)
        {
            try
            {
                // 文件超限时轮转
                var fileInfo = new FileInfo(_logFilePath);
                if (fileInfo.Exists && fileInfo.Length > MaxLogFileSize)
                {
                    var rotated = _logFilePath + ".old";
                    if (File.Exists(rotated)) File.Delete(rotated);
                    File.Move(_logFilePath, rotated);
                }

                File.AppendAllText(_logFilePath, line + Environment.NewLine);
            }
            catch
            {
                // 文件写入失败不向上抛——日志服务不能成为主流程的故障点
            }
        }
    }
}
