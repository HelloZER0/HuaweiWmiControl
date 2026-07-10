using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace HuaweiWmiControl
{
    /// <summary>
    /// 应用程序入口——解包模式下通过 Windows App SDK Bootstrap API 注册框架包，
    /// 解决 SelfContained=false 时的 REGDB_E_CLASSNOTREG 问题。
    /// </summary>
    public static class Program
    {
        /// <summary>
        /// 应用程序主入口。先注册 Windows App SDK 框架包，再启动 WinUI 3 应用。
        /// </summary>
        [STAThread]
        static void Main(string[] args)
        {
            if (!TryInitializeWindowsAppSdk())
            {
                ShowBootstrapError();
                return;
            }

            WinRT.ComWrappersSupport.InitializeComWrappers();
            Microsoft.UI.Xaml.Application.Start((p) =>
            {
                var context = new Microsoft.UI.Dispatching.DispatcherQueueSynchronizationContext(
                    Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread());
                System.Threading.SynchronizationContext.SetSynchronizationContext(context);
#pragma warning disable S1848 // WinUI 3: Application.Start 内部会保持 App 实例的引用
                new App();
#pragma warning restore S1848
            });
        }

        /// <summary>
        /// 通过 P/Invoke 调用 Microsoft.WindowsAppRuntime.Bootstrap.dll 的
        /// MddBootstrapInitialize 注册框架包。
        /// </summary>
        private static bool TryInitializeWindowsAppSdk()
        {
            try
            {
                // 1. 尝试查找已安装的框架包并注册
                int hr = MddBootstrapInitialize(
                    majorMinorVersion: 0x00010007,  // 1.7
                    versionTag: null,                // 不限制 tag
                    minVersion: MddBootstrapInitializeOptions.None
                );

                if (hr >= 0)
                    return true; // S_OK 或 S_FALSE (已注册过)

                // 2. 框架包未安装（HRESULT 0x80073CFB = APPX_E_PACKAGE_NOT_FOUND）
                // 尝试以当前用户身份注册已解包的框架包
                if (hr == unchecked((int)0x80073CFB))
                {
                    hr = MddBootstrapInitialize(
                        majorMinorVersion: 0x00010007,
                        versionTag: null,
                        minVersion: MddBootstrapInitializeOptions.OnNoMatch_ShowUI);
                    return hr >= 0;
                }

                return false;
            }
            catch (DllNotFoundException)
            {
                return false;
            }
        }

        private static void ShowBootstrapError()
        {
            Debug.WriteLine("[HuaweiWmiControl] Windows App SDK Runtime 1.7 未安装。");
            Debug.WriteLine("请下载安装: https://learn.microsoft.com/windows/apps/windows-app-sdk/downloads");
            _ = MessageBoxW(
                IntPtr.Zero,
                "需要安装 Windows App SDK Runtime 1.7 才能运行此程序。\n\n" +
                "下载地址：\nhttps://aka.ms/windowsappsdk/1.7/1.7.250310001/" +
                "WindowsAppRuntimeInstall-x64.exe\n\n" +
                "或使用 SelfContained=true 重新编译以捆绑运行时。",
                "华为笔记本调控工具 — 缺少运行时",
                0x00000010);
        }

        // ---- P/Invoke: Microsoft.WindowsAppRuntime.Bootstrap.dll (WinAppSDK 1.2+) ----

        /// <summary>MddBootstrapInitialize 的标志。</summary>
        [Flags]
        private enum MddBootstrapInitializeOptions : uint
        {
            None = 0,
            OnNoMatch_ShowUI = 1,
            OnPackageIdentity_NOOP = 2,
        }

        [DllImport("Microsoft.WindowsAppRuntime.Bootstrap.dll", ExactSpelling = true, CharSet = CharSet.Unicode)]
        private static extern int MddBootstrapInitialize(
            uint majorMinorVersion,
            [MarshalAs(UnmanagedType.LPWStr)] string? versionTag,
            MddBootstrapInitializeOptions minVersion);

        [DllImport("user32.dll", ExactSpelling = true, CharSet = CharSet.Unicode)]
        private static extern int MessageBoxW(IntPtr hWnd, string text, string caption, uint type);
    }
}
