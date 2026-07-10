using System;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using HuaweiWmiControl.Services;
using HuaweiWmiControl.ViewModels;
using HuaweiWmiControl.Wmi;

namespace HuaweiWmiControl
{
    /// <summary>
    /// WinUI 3 应用程序入口。使用解包（unpackaged）模式，直接启动主窗口。
    /// 同时作为简易 DI 容器宿主——<see cref="Services"/> 在首次访问时初始化，
    /// 注册所有 Service 和 ViewModel，消除手动构造链。
    /// </summary>
    public partial class App : Application
    {
        private Window? _window;

        /// <summary>
        /// 应用程序级服务提供者。
        /// 所有 Service 和 ViewModel 通过此容器解析，避免手动 new 散落在各处。
        /// </summary>
        public static IServiceProvider Services { get; } = ConfigureServices();

        private static IServiceProvider ConfigureServices()
        {
            var services = new ServiceCollection();

            // ---- 基础设施 ----
            services.AddSingleton<WmiConnectionManager>();
            // IWmiProtocol 在 Connect() 之后才可用——注册工厂使 DI 能延迟解析
            services.AddSingleton<IWmiProtocol>(sp =>
                sp.GetRequiredService<WmiConnectionManager>().Protocol);
            // 日志服务——应用级单例，写入内存缓冲区 + 滚动日志文件
            services.AddSingleton<ILogService, LogService>();

            // ---- 服务层 ----
            services.AddSingleton<IBatteryService, BatteryService>();
            services.AddSingleton<IFnLockService, FnLockService>();
            services.AddSingleton<IKeyboardService, KeyboardService>();
            services.AddSingleton<ISmartChargeService, SmartChargeService>();
            services.AddSingleton<IPowerUnlockService, PowerUnlockService>();
            services.AddSingleton<IMicMuteLedService, MicMuteLedService>();
            services.AddSingleton<ISensorService, SensorService>();

            // ---- ViewModel 层 ----
            services.AddSingleton<MainViewModel>();

            return services.BuildServiceProvider();
        }

        public App()
        {
            this.InitializeComponent();

            // 全局未处理异常兜底——防止 async void 漏捕导致进程静默崩溃。
            // 注意：WinUI 3 的 UnhandledException 仅能捕获 UI 线程上的异常，
            // 后台线程 async void 的异常仍可能逃逸，此处作为最后一道防线。
            this.UnhandledException += (sender, e) =>
            {
                System.Diagnostics.Trace.TraceError(
                    $"[HuaweiWmiControl] 全局未处理异常: {e.Exception}");
                e.Handled = true; // 阻止进程崩溃，允许继续运行
            };
        }

        /// <summary>
        /// 应用程序启动时创建并激活主窗口。
        /// </summary>
        protected override void OnLaunched(LaunchActivatedEventArgs args)
        {
            try
            {
                _window = new MainWindow();

                // 窗口关闭时切断引用，防止内存泄漏。
                _window.Closed += (s, e) => _window = null;

                _window.Activate();
            }
            catch (Exception ex)
            {
                // 启动阶段异常无法通过 UI 展示，写入系统事件日志以便诊断。
                System.Diagnostics.Trace.TraceError(
                    $"[HuaweiWmiControl] 启动失败: {ex}");
                throw;
            }
        }
    }
}
