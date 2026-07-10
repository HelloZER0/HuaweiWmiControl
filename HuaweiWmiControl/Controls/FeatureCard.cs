using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace HuaweiWmiControl.Controls
{
    /// <summary>
    /// 功能卡片控件——统一 7 个功能页的卡片外观（标题 + 描述 + 圆角卡片容器）。
    /// 使用方式：
    /// <code>
    /// &lt;controls:FeatureCard Title="电池保护"
    ///                       Description="通过限制充电区间来延长电池寿命"&gt;
    ///     &lt;StackPanel Spacing="16" Padding="24"&gt;
    ///         &lt;!-- 功能特有控件 --&gt;
    ///     &lt;/StackPanel&gt;
    /// &lt;/controls:FeatureCard&gt;
    /// </code>
    /// </summary>
    public sealed class FeatureCard : ContentControl
    {
        /// <summary>卡片标题。</summary>
        public string Title
        {
            get => (string)GetValue(TitleProperty);
            set => SetValue(TitleProperty, value);
        }

        public static readonly DependencyProperty TitleProperty =
            DependencyProperty.Register(nameof(Title), typeof(string),
                typeof(FeatureCard), new PropertyMetadata(""));

        /// <summary>卡片副标题/描述。</summary>
        public string Description
        {
            get => (string)GetValue(DescriptionProperty);
            set => SetValue(DescriptionProperty, value);
        }

        public static readonly DependencyProperty DescriptionProperty =
            DependencyProperty.Register(nameof(Description), typeof(string),
                typeof(FeatureCard), new PropertyMetadata(""));

        /// <summary>内部内容间距（默认 24）。</summary>
        public Thickness InnerPadding
        {
            get => (Thickness)GetValue(InnerPaddingProperty);
            set => SetValue(InnerPaddingProperty, value);
        }

        public static readonly DependencyProperty InnerPaddingProperty =
            DependencyProperty.Register(nameof(InnerPadding), typeof(Thickness),
                typeof(FeatureCard), new PropertyMetadata(new Thickness(24)));

        public FeatureCard()
        {
            this.DefaultStyleKey = typeof(FeatureCard);
        }
    }
}
