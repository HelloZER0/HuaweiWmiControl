using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using System;
using System.Collections.Generic;
using Windows.Graphics;
using HuaweiWmiControl.ViewModels;

namespace HuaweiWmiControl
{
    /// <summary>
    /// 主窗口——Windows 11 Fluent Design 规范界面。
    /// NavigationView 导航 / InfoBar 状态通知 / 主题选项。
    /// 兼容：亮/暗/系统/高对比度模式、屏幕阅读器、键盘导航。
    /// </summary>
    public sealed partial class MainWindow : Window
    {
        /// <summary>主 ViewModel，通过 {x:Bind} 绑定到 XAML。</summary>
        public MainViewModel ViewModel { get; } = new();

        private static readonly SizeInt32 DefaultWindowSize = new(900, 720);

        private IReadOnlyDictionary<string, UIElement>? _sections;

        // ComboBox 选项索引到 ElementTheme 的映射
        private static readonly ElementTheme[] ThemeIndexMap =
            { ElementTheme.Default, ElementTheme.Light, ElementTheme.Dark };

        public MainWindow()
        {
            this.InitializeComponent();

            // Acrylic 背景材质（节能模式/低端硬件自动退为纯色）
            this.SystemBackdrop = new DesktopAcrylicBackdrop();

            // 设置默认窗口尺寸；AppWindow.Resize 在解包模式下可能抛异常，忽略即可
            try { AppWindow.Resize(DefaultWindowSize); } catch { /* 解包模式限制 */ }

            // 响应式：最小窗口尺寸
            AppWindow.ResizeClient(DefaultWindowSize);
            var presenter = AppWindow.Presenter as Microsoft.UI.Windowing.OverlappedPresenter;
            if (presenter != null)
            {
                presenter.IsResizable = true;
                presenter.IsMinimizable = true;
                presenter.IsMaximizable = true;
            }

            // 标题栏交互：设为可拖动区域（仅代码，XAML 中会触发编译器错误）
            try
            {
                this.ExtendsContentIntoTitleBar = true;
                this.SetTitleBar(AppTitleBar);
            }
            catch { /* 解包模式限制 */ }

            this.Closed += OnClosed;
        }

        private void OnClosed(object sender, WindowEventArgs args)
        {
            NavView.SizeChanged -= OnNavViewSizeChanged;
            ViewModel.Dispose();
        }

        /// <summary>窗口加载后初始化节映射并连接 WMI。</summary>
        private async void RootGrid_Loaded(object sender, RoutedEventArgs e)
        {
            _sections = new Dictionary<string, UIElement>
            {
                ["sensors"]     = SectionSensors,
                ["battery"]     = SectionBattery,
                ["fnlock"]      = SectionFnLock,
                ["keyboard"]    = SectionKeyboard,
                ["smartcharge"] = SectionSmartCharge,
                ["powerunlock"] = SectionPowerUnlock,
                ["micled"]      = SectionMicLed,
                ["settings"]    = SectionSettings,
            };

            // 响应式：窗口过窄时切换为顶部导航
            NavView.SizeChanged += OnNavViewSizeChanged;

            NavView.SelectedItem = NavView.MenuItems[0]; // 首页为传感器
            await ViewModel.InitializeAsync();

            // 同步主题选择器到当前主题
            SyncThemeSelector();
        }

        /// <summary>NavigationView 选择变更——切换对应功能页面。</summary>
        private void NavView_SelectionChanged(NavigationView sender,
            NavigationViewSelectionChangedEventArgs args)
        {
            if (_sections == null) return;

            foreach (var section in _sections.Values)
                section.Visibility = Visibility.Collapsed;

            if (args.IsSettingsSelected)
            {
                _sections["settings"].Visibility = Visibility.Visible;
            }
            else if (args.SelectedItem is NavigationViewItem { Tag: string tag }
                     && _sections.TryGetValue(tag, out var target))
            {
                target.Visibility = Visibility.Visible;
            }

            ContentScroll.ChangeView(null, 0, null);
        }

        private void OnNavViewSizeChanged(object sender, SizeChangedEventArgs args)
        {
            NavView.PaneDisplayMode = NavView.ActualWidth < 640
                ? NavigationViewPaneDisplayMode.Top
                : NavigationViewPaneDisplayMode.LeftCompact;
        }

        /// <summary>主题选择器变更——将 ComboBox 选中项同步到 RootGrid。</summary>
        private void ThemeSelector_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (ThemeSelector.SelectedIndex < 0
                || ThemeSelector.SelectedIndex >= ThemeIndexMap.Length)
                return;
            RootGrid.RequestedTheme = ThemeIndexMap[ThemeSelector.SelectedIndex];
        }

        /// <summary>将 ThemeSelector 同步为当前主题对应的索引。</summary>
        private void SyncThemeSelector()
        {
            for (int i = 0; i < ThemeIndexMap.Length; i++)
            {
                if (ThemeIndexMap[i] == RootGrid.RequestedTheme)
                {
                    ThemeSelector.SelectedIndex = i;
                    return;
                }
            }
        }

        private void ClearLog_Click(object sender, RoutedEventArgs e)
        {
            ViewModel.ClearLog();
        }
    }
}
