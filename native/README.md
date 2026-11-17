# HuaweiWmiControl Native — Win32 托盘版

华为/荣耀笔记本硬件调控工具的**原生重写版**：无 .NET、无 WinUI、无第三方库，
只有一个任务栏托盘图标 + 右键/左键弹出菜单，直接通过 COM/`IWbemLocator`
调用 `ROOT\WMI` 的固件方法。

> 状态：**源码已完成，未经编译器验证。** 开发机未安装任何 C/C++ 工具链
> （无 MSVC / MinGW / clang，也无 Windows SDK），因此本目录下的代码**一次都没有编译过**。
> 首次构建时请预期需要修若干编译错误，详见文末「已知风险」。

---

## 1. 与旧版（C#/.NET/WinUI）的关系

| | 旧版 | 本版 |
|---|---|---|
| 运行时 | .NET 8 + Windows App SDK 1.7 | 无（静态 CRT，单 exe） |
| UI | WinUI 3，900×720 窗口 + NavigationView | 无窗口，仅托盘图标 + 弹出菜单 |
| 体积依赖 | 需装 Windows App SDK Runtime | 仅 Windows 系统 DLL |
| WMI 通道 | CIM / WS-Man (`Microsoft.Management.Infrastructure`) | COM / `IWbemLocator` + `IWbemServices` |
| 命令编码 | 相同（逐字移植） | 相同 |

**协议、命令号、字节编码、机型兼容路径全部与旧版一致**，唯一变化的是传输层与表现层。

---

## 2. 构建

### 前置

- Visual Studio 2022（或 Build Tools 2022），勾选「使用 C++ 的桌面开发」
- Windows 10 SDK（10.0.19041 或更新）
- 不需要 .NET SDK、不需要 CMake（可选）

### 方式 A：直接用 cl.exe（推荐，零额外依赖）

打开 **「x64 Native Tools Command Prompt for VS 2022」**，`cd` 到本目录：

```bat
build_msvc.bat
```

产物：`build\HuaweiWmiControl.exe`

### 方式 B：CMake

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

产物：`build\Release\HuaweiWmiControl.exe`

### 关键编译选项

| 选项 | 原因 |
|---|---|
| `/utf-8` | 源码里直接写中文宽字符串字面量，缺了会变乱码 |
| `/MT` | 静态 CRT，目标机无需 VC++ 运行时 |
| `/MANIFESTINPUT:app.manifest` | 嵌入 `requireAdministrator` + `PerMonitorV2`，不用 .rc 的 `RT_MANIFEST`（避免双清单） |
| `/SUBSYSTEM:WINDOWS` | 无控制台窗口 |

---

## 3. 使用

1. 双击 `HuaweiWmiControl.exe`（清单要求提权，会弹 UAC）
2. 托盘出现图标；**左键或右键**都弹出菜单
3. 菜单项说明：

| 菜单 | 说明 |
|---|---|
| 传感器 ▸ | 风扇转速 + 11 路温度，只读，来自后台缓存 |
| 电池保护 ▸ | 关闭充电控制 / 原厂预设 40-70、70-90、95-100 / MACH-WX9 清零兼容开关 |
| Fn-Lock | 复选项，勾选即切换 F1-F12 模式 |
| 键盘背光 ▸ | 关 / 低 / 高（单勾选）+ 熄灭超时子菜单（0/15/30/60/300 秒） |
| 智能充电 ▸ | 模式 0-3、参数 0-2 子菜单 + 「应用当前值」 |
| 电源解锁 | 复选项，解除 Fn+P 性能限制 |
| 麦克风 LED ▸ | 点亮 / 熄灭（固件**无回读**，故不显示勾选状态） |
| 刷新状态 | 重读所有功能，更新缓存与 tooltip |
| 打开日志 | 用默认程序打开 `%LOCALAPPDATA%\HuaweiWmiControl\hwc.log` |
| 退出 | 移除托盘图标并退出 |

- 机型不支持的功能会自动**置灰**，不弹错误框。
- 所有操作结果写日志文件，不打断使用。

---

## 4. 目录结构与移植对照

```
native/
├── app.manifest            # requireAdministrator + PerMonitorV2 + UTF-8 ACP
├── app.rc                  # 版本信息（不含清单）
├── CMakeLists.txt
├── build_msvc.bat          # 直接 cl.exe 构建
└── src/
    ├── proto.h  proto.c    # ★冻结契约：命令号/常量/纯编解码函数/温度区表
    ├── hwapi.h             # ★冻结契约：对外 C API + HwStatus + HwDevice
    ├── wmi.cpp             # COM/WMI 连接、协议探测、命令调用与重试
    ├── services.cpp        # 业务：电池/Fn-Lock/键盘/智能充电/电源解锁/MIC LED/传感器
    ├── log.cpp             # 滚动日志
    ├── icon.cpp            # 运行时 GDI 生成 16x16 托盘图标
    ├── tray.cpp            # Shell_NotifyIcon 封装
    ├── menu.cpp            # 弹出菜单构建与命令分发
    └── main.cpp            # WinMain、隐藏窗口、消息循环、状态缓存
```

| 旧版文件（C#） | 本版文件（C++） | 说明 |
|---|---|---|
| `Program.cs` | `main.cpp` | 入口。不再需要 Windows App SDK Bootstrap |
| `App.xaml.cs` | `main.cpp` | DI 容器整体删除，改为全局 `AppState` 值类型 |
| `MainWindow.xaml` / `.cs`（334 行 XAML） | `menu.cpp` + `tray.cpp` | NavigationView + 8 个 Visibility 页面 → 弹出菜单 |
| `Controls/FeatureCard.cs` | — | 无 UI，删除 |
| `ViewModels/*.cs`（8 个） | `main.cpp` | ViewModel 层消失，状态收敛进 `AppState` |
| `Wmi/WmiConnectionManager.cs` | `wmi.cpp` | CIM/WS-Man → `IWbemLocator`/DCOM；三职责仍在一处（见风险 R4） |
| `Wmi/WmiProtocolU64.cs` / `WmiProtocolU8.cs` | `wmi.cpp` | 合并为一个 `invoke()` 内的协议分支 |
| `IWmiProtocol.cs` 的 `EncodeCommand` | `proto.h` `hw_cmd_encode` | 提成纯函数，可独立测试 |
| `WmiConstants.cs` | `proto.h` | 全部常量逐字搬运 |
| `Services/WmiServiceBase.cs` | `wmi.cpp` | 模板方法 + 重试 → 一个内部 `invoke()` |
| `BatteryService.cs` 等 7 个服务 | `services.cpp` | 一一对应 |
| `WmiException.cs` | `hwapi.h` `HwStatus` | 异常 → 状态码，不再靠 throw 区分 |
| `LogService.cs` | `log.cpp` | 内存缓冲 + 文件 → 仅文件（无 UI 面板了） |
| `KbdLightEncoding.cs` | `proto.h` `HwKbdEncoding` | |
| `SensorItem.cs` | — | 无 UI 数据模型 |
| `HuaweiWmiControl.Tests/` | — | 见下节 |

---

## 5. 测试现状

旧版唯一有价值的单测是 `WmiServiceBaseTests`（打桩协议，验证重试与状态码判定）。

本版把**全部纯逻辑**（命令编码、u8Output 解析、键盘编码判定/解码/反编码）都收敛进了
`proto.h` 的 `static inline` 函数，**不依赖 Windows.h、不依赖 COM**，因此可以直接用
任意 C 测试框架独立编译测试。建议下一步补一个 `tests/test_proto.c`，
把旧版的 9 个用例翻译过来——这是本项目唯一能脱离真机验证的部分。

`services.cpp` / `wmi.cpp` 需要真实固件，只能真机回归。

---

## 6. 已知风险（首次构建必读）

| # | 风险 | 说明 / 处理 |
|---|---|---|
| R1 | **完全未经编译** | 本机无工具链。首次 `build_msvc.bat` 大概率报若干编译错误（缺 include、API 签名细节）。请逐个修，不要怀疑整体设计 |
| R2 | **回到 DCOM** | 旧版特意从 `System.Management`/DCOM 迁到 CIM/WS-Man；本版因要脱离 .NET 又回到 `IWbemLocator`。本机连接通常没问题，但 `CoInitializeSecurity` 时机、`CoSetProxyBlanket` 的认证级别是经典坑位，需真机验证 |
| R3 | **STA 线程约束未验证** | 旧版注释声称 `Connect()` 必须在 STA，但无依据。本版在 `wWinMain` 所在线程（OLE STA）里完成全部 WMI 调用，行为与旧版一致，但不代表这是必要条件 |
| R4 | 职责未拆分 | `wmi.cpp` 仍同时管连接、协议探测、键盘编码探测。这是**有意保留**：先保证移植正确，再谈重划边界（参见 `docs/refactor-plan.md`） |
| R5 | 托盘图标是运行时画的 | 若 `CreateIconIndirect` 的 AND 掩码处理有误，图标会带黑底。回退方案：换 `LoadIconW(NULL, IDI_APPLICATION)` |
| R6 | 菜单无滑杆 | 旧版的 0-100 连续阈值在新 UI 里变成原厂预设（40-70 / 70-90 / 95-100）。要任意值需加配置或对话框 |
| R7 | 智能充电语义未文档化 | 旧版也是把 `mode/unknown/start/end/param` 原样暴露，没有任何机型语义说明。本版保持原样 |

---

## 7. 许可

与上游一致：GPL-2.0。
