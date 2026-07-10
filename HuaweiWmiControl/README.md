# 华为/荣耀笔记本调控工具 (Huawei/Honor WMI Control)

基于华为笔记本 Linux 驱动 `huawei-wmi` 移植的 **Windows 调控工具**，使用 **WinUI 3 (Windows App SDK)** 界面。
通过 WMI 调用固件的 `OemWMIMethod` / `OemWMIfun` 接口，控制以下华为/荣耀笔记本功能：

- 电池充电阈值（充电保护）
- Fn-Lock
- 键盘背光及自动熄灭超时
- 智能充电（华为私有模式）
- 电源解锁（Fn+P）
- 麦克风静音指示灯
- 风扇转速 / 温度传感器（只读）

## 工作原理

Windows 侧与 Linux 驱动共用同一套固件命令（WMI GUID、命令码完全一致），仅传输层不同：

- **WMI 命名空间**：`ROOT\wmi`
- **WMI 类**：`OemWMIMethod`
- **方法**：`OemWMIfun`
- **输入参数**：新版固件用 `u64Input`（一个 uint64）；老版用 `u8Input`（64 字节 SAFEARRAY）。代码自动适配。
- **命令编码**：小端 uint64，命令号在低 32 位，参数依次填入字节 2/3…，移植自 Linux 驱动的 `union hwmi_arg`。
- **返回**：`u8Output` 缓冲区，第 0 字节为状态（0=成功），数据从后续字节读取。

传输层实现见 `HwWmi.cs`，与界面完全解耦。

## 构建与运行

### 前置条件
- Windows 10 17763+ / Windows 11
- [.NET 8 SDK](https://dotnet.microsoft.com/download) 或 Visual Studio 2022（含“使用 .NET 的桌面开发”与 **Windows App SDK** 工作负载）
- 已安装 **华为电脑管家** 或 **荣耀电脑管家**（提供 ACPI-WMI 驱动，使 `OemWMIMethod` 可用）
- 以 **管理员身份** 运行（WMI 调用需要权限；`app.manifest` 已声明 `requireAdministrator`）

### 编译
用 Visual Studio 2022 打开 `HuaweiWmiControl.csproj` 直接 F5 运行，或：

```sh
dotnet build -c Release
```

运行生成目录下的 `HuaweiWmiControl.exe`（需用管理员运行）。
若提示缺少 Windows App SDK 运行时，请安装
[Windows App SDK 运行时](https://learn.microsoft.com/zh-cn/windows/apps/windows-app-sdk/downloads)
或改用自包含发布：

```sh
dotnet publish -c Release -r win-x64 --self-contained true -p:WindowsAppSDKSelfContained=true
```

## 使用

打开程序后**自动连接并刷新当前状态**（界面底部日志区可见过程）。各页签的「读取」会拉取当前值，「应用」会写入固件。

- 连接失败（如未装电脑管家、未以管理员运行）只会在底部日志区提示，**不会弹窗**；荣耀笔记本没有「华为电脑管家」，请装「荣耀电脑管家」。
- 灰色/“不支持”的项是该机型固件未开放的接口，属正常。
- 智能充电为华为私有模式，`mode`/`unknow` 含义未完全公开，建议先「读取」再微调。

## 工程结构

```
HuaweiWmiControl/
├─ HuaweiWmiControl.csproj   # WinUI 3 解包工程
├─ App.xaml / App.xaml.cs    # WinUI Application 引导
├─ Program.cs                # 解包 WinUI 3 COM 引导 (ComWrappersSupport)
├─ MainWindow.xaml / .cs     # 主界面 (TabView 七个功能页)
├─ HwWmi.cs                  # WMI 传输层 + 命令编解码（与界面无关，可独立复用）
├─ app.manifest              # 声明 requireAdministrator
└─ README.md
```

## 免责声明

本工具为社区逆向成果，命令编码参照 Linux 主线驱动。不同机型固件可能存在差异，
请在你的实际设备上验证各项功能；使用风险自负。
