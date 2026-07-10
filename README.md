# 华为/荣耀笔记本调控工具

[![.NET](https://img.shields.io/badge/.NET-8.0-512BD4)](https://dotnet.microsoft.com)
[![WinUI](https://img.shields.io/badge/WinUI-3.0-0078D7)](https://learn.microsoft.com/windows/apps/winui)
[![License](https://img.shields.io/badge/License-GPL_3.0-blue.svg)](LICENSE)

基于华为笔记本 Linux 驱动 [`huawei-wmi`](https://github.com/aymanbagabas/Huawei-WMI) 移植的 **Windows 调控工具**。
通过 WMI 调用固件 `OemWMIMethod` / `OemWMIfun` 接口，控制华为/荣耀笔记本硬件功能。

![screenshot](docs/screenshot.png)

## 功能

| 选项卡 | 功能 | 备注 |
|--------|------|------|
| 传感器 | CPU/电池/风扇温度、风扇转速 | 只读，首页 |
| 电池保护 | 充电起始/上限阈值 | MACH-WX9 兼容 |
| Fn-Lock | F1-F12 键模式 | 开关 |
| 键盘背光 | 背光亮度（自动刷新）+ 熄灭超时 | 亮度只读 |
| 智能充电 | 华为私有充电模式 | 含附加参数 |
| 电源解锁 | 解除 Fn+P 性能限制 | 开关 |
| 麦克风 LED | 静音指示灯 | 开关 |

## 工作原理

Windows 侧与 Linux 驱动共用同一套固件命令，仅传输层不同：

- **WMI 命名空间**：`ROOT\wmi`
- **WMI 类**：`OemWMIMethod` → 方法 `OemWMIfun`
- **输入**：新版固件 `u64Input`（uint64）；老版 `u8Input`（64 字节 SAFEARRAY）— 自动适配
- **命令编码**：小端 64 位，命令号低 32 位，参数依次填入字节 2/3，移植自 Linux 驱动的 `union hwmi_arg`
- **返回**：`u8Output` 缓冲区，第 0 字节状态码

> 详细逆向过程参见 [index.md](docs/reverse-engineering.md)

## 构建

### 前置条件

- Windows 10 17763+ / Windows 11
- [.NET 8 SDK](https://dotnet.microsoft.com/download)
- [Windows App SDK 运行时](https://learn.microsoft.com/windows/apps/windows-app-sdk/downloads)
- **华为电脑管家** 或 **荣耀电脑管家**（提供 ACPI-WMI 驱动）
- **管理员权限**（`app.manifest` 已声明 `requireAdministrator`）

### 编译

```sh
dotnet build -c Release
```

自包含发布：

```sh
dotnet publish -c Release -r win-x64 --self-contained -p:WindowsAppSDKSelfContained=true
```

运行 `bin\Release\HuaweiWmiControl.exe`（管理员权限）。

## 工程结构

```
HuaweiWmiControl/
├── HuaweiWmiControl.csproj              # WinUI 3 主项目（UI 层）
├── HuaweiWmiControl.Abstractions.csproj  # 抽象层（纯逻辑，无 WinUI 依赖）
├── HuaweiWmiControl.Tests.csproj         # xUnit 单元测试
├── HuaweiWmiControl/                    # 主项目源码
│   ├── MainWindow.xaml / .cs            # 主界面 + 导航
│   ├── App.xaml / .cs                   # WinUI Application + DI
│   ├── ViewModels/                      # 8 个 ViewModel
│   └── Controls/                        # 自定义控件
├── HuaweiWmiControl.Abstractions/       # 抽象层源码
│   ├── Services/                        # WMI 服务接口 + 实现
│   ├── Wmi/                             # WMI 协议实现
│   └── *.cs                             # 接口/模型/常量
├── HuaweiWmiControl.Tests/              # 测试项目
│   └── WmiServiceBaseTests.cs           # 9 个单元测试
├── docs/
│   └── reverse-engineering.md           # 原始逆向文档
├── README.md
└── .gitignore
```

## 测试

```sh
dotnet test HuaweiWmiControl.Tests
```

当前覆盖：`WmiServiceBase` 的全部 Call/InvokeGet/InvokeSet 路径。

## 技术栈

| 技术 | 用途 |
|------|------|
| WinUI 3 / Windows App SDK 1.7 | 桌面 UI |
| .NET 8 | 运行时 |
| Microsoft.Management.Infrastructure | CIM/WS-Man WMI 协议 |
| Microsoft.Extensions.DependencyInjection | DI 容器 |
| xUnit + Moq | 单元测试 |

## 免责声明

本工具为社区逆向成果，命令编码参照 Linux 主线驱动。不同机型固件可能存在差异，
请在实际设备上验证各项功能；使用风险自负。

---

基于 [AceDroidX/HuaweiBatteryControl](https://github.com/AceDroidX/HuaweiBatteryControl) 的逆向分析开发。
文章使用 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) 许可。
