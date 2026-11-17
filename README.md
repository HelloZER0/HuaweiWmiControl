# 华为/荣耀笔记本调控工具（原生 Win32 托盘版）

[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C)](https://isocpp.org)
[![Win32](https://img.shields.io/badge/Win32-tray%20only-0078D7)](https://learn.microsoft.com/windows/win32)
[![.NET](https://img.shields.io/badge/.NET-none-success)](.)
[![License](https://img.shields.io/badge/License-GPL_2.0-blue.svg)](LICENSE)

> 告别电脑管家，轻量调控你的华为/荣耀笔记本硬件。

单文件 exe，**无 .NET、无 WinUI、无第三方库**。没有主窗口，只有一个任务栏托盘图标；
左键或右键弹出菜单，直接经 COM/`IWbemLocator` 调用 `ROOT\WMI` 的固件接口。

> **状态（2026-11-18）：已完成真机验证。** 在 **HONOR BRN-HXX（BIOS 1.15）** 上，
> WMI 连接、协议识别、全部读取、4 项写入均验证通过。验证过程中发现并修复了
> 3 个真实问题（菜单置灰反转、背光亮度只读、Fn-Lock 缓存过期），详见 §7。
>
> 构建验证：MSVC 19.52（VS 2026）+ Windows SDK 10.0.26100，`cl` 与 CMake 双路径通过，
> `proto.h` 纯逻辑测试 183 断言全绿。

---

## 1. 项目沿革

本仓库最初是 **C# / .NET 8 / WinUI 3** 实现（带完整窗口界面）。为了彻底去掉
.NET 与 Windows App SDK 运行时依赖、把体积和常驻开销压到最低，已全部重写为
原生 C++17 + Win32：

| | 旧版（已移除） | 现版本 |
|---|---|---|
| 运行时 | .NET 8 + Windows App SDK 1.7 | 无（静态 CRT，单 exe） |
| UI | WinUI 3，900×720 窗口 + NavigationView | 托盘图标 + 弹出菜单 |
| 依赖 | 需安装 Windows App SDK Runtime | 仅 Windows 系统 DLL |
| WMI 通道 | CIM / WS-Man | COM / `IWbemLocator` |
| 产物 | 数十 MB + 运行时 | **约 193 KB 单文件** |

旧的 C# 实现仍可在 git 历史中找回（`3fc7912` 及其之前）。**协议、命令号、字节编码、
机型兼容路径全部原样继承**，只有传输层与表现层改变。

## 2. 机型兼容性

### 2.1 实测机型：HONOR BRN-HXX（BIOS 1.15）

用 `build_diag.bat` 生成的诊断程序逐项取证，非推测：

| 功能 | 结果 | 实测依据 |
|---|:---:|---|
| 传感器 | ✅ | 风扇1 **3252 RPM**；11 路温度中 **7 路可用**：CPU 67℃、TSLO 49℃、TNTC 40℃、CNTC 52℃、DNTC 47℃、电池 32℃、TP0C 26℃（TP01/TP06/TP07/TP04 固件不支持） |
| 电池保护 | ✅ | 读 `70-90`；写回 `(70,90)` 成功 |
| Fn-Lock | ✅ | `on=1`；写入后回读一致，物理 F1-F12 行为正确切换 |
| 键盘背光 | ⚠️ | 亮度**只读**：读 `level=0` 正常，写亮度被固件拒绝（`fw=0x01`）；**熄灭超时可写** |
| 智能充电 | ❌ | `C_BATTERY_CHARGE_MODE_GET` 返回 `fw=0x01`，该机型不支持 |
| 电源解锁 | ✅ | `on=0`；写回成功 |
| 麦克风 LED | ✅ | 点亮 / 熄灭均成功（该功能只有 SET，无回读接口） |

- 协议：**U8**（该机型 `OemWMIMethod` 只有 `u8Input`，无 `u64Input`），程序自动识别
- 键盘背光编码：**百分比**
- WMI 实例：`ACPI\PNP0C14\HWMI_0`

### 2.2 其它机型历史报告

以下报告来自旧 .NET 版；两者使用完全相同的固件命令，结论可直接参考：

| 机型 | 系统 | 传感器 | 电池保护 | Fn-Lock | 键盘背光 | 智能充电 | 电源解锁 | 麦克风 LED | 报告人 |
|------|------|:------:|:--------:|:-------:|:---------:|:--------:|:--------:|:-----------:|--------|
| MateBook X Pro 2024 (MACH-WX9) | Windows 11 | ✅ | ✅¹ | ✅ | ✅² | ✅ | ✅ | ✅ | @HelloZER0 |
| MateBook 14 2020 | Windows 11 | ➖ | ✅ | ➖ | ➖ | ➖ | ➖ | ➖ | @AceDroidX |
| MateBook D 14 (BIOS 1.17) | Windows 11 | ➖ | ✅³ | ➖ | ➖ | ➖ | ➖ | ➖ | @AceDroidX |
| MateBook E GO | Windows 11 | ➖ | ✅ | ➖ | ➖ | ➖ | ➖ | ➖ | @AceDroidX |
| 荣耀 MagicBook (BRN-H76) | Windows 11 | ✅ | ✅ | ✅ | ⚠️⁴ | ❌⁵ | ✅ | ✅ | @HelloZER0 |
| 荣耀 Hunter V700 | Windows 11 | ➖ | ✅ | ➖ | ➖ | ➖ | ➖ | ➖ | @AceDroidX |

> ¹ 电池保护含 MACH-WX9 兼容路径（关闭前先清零阈值并延时 1 秒）
> ² 键盘背光编码为位掩码反转模式（bit 0 = 最亮）
> ³ 需在 BIOS 1.17 下使用 `--new` 参数运行
> ⁴ 键盘背光亮度只读，仅可设置熄灭超时
> ⁵ 机器固件不支持智能充电功能

➖ = 未测。**欢迎在本机跑一遍 `build_diag.bat` 后提 Issue 补充你的机型。**

## 3. 构建

### 前置

- **Visual Studio 2022 或 2026**，安装时勾选「**使用 C++ 的桌面开发**」
  （带上 MSVC 工具集 + Windows SDK；只装 VS 本体不够）
- Windows 10/11 SDK（10.0.19041 或更新；已在 10.0.26100 上验证）
- **不需要** .NET SDK；CMake 可选

### 方式 A：一键构建（推荐）

在**普通命令提示符**里 `cd` 到仓库根目录后运行：

```bat
build_all.bat
```

它会用 `vswhere` 自动定位 Visual Studio 并调用 `vcvars64.bat`，无需先开
「开发者命令提示符」。流程：编测试 → 跑测试 → 编主程序。

产物：`build\HuaweiWmiControl.exe`

### 方式 B：CMake

```bat
cmake -S . -B build-cmake -G "Visual Studio 17 2022" -A x64
cmake --build build-cmake --config Release
```

（VS 2026 用 `-G "Visual Studio 18 2026"`。）

### 清理

```bat
build_clean.bat        :: 清 *.obj/*.res/*.pdb 等中间件，保留 build\*.exe
build_clean.bat all    :: 连 build\ 目录一起删
```

### 关键编译选项

| 选项 | 原因 |
|---|---|
| `/utf-8` | 源码里直接写中文宽字符串字面量，缺了会乱码 |
| `/MT` | 静态 CRT，目标机无需 VC++ 运行时 |
| `/MANIFESTINPUT:app.manifest` | 嵌入 `requireAdministrator` + `PerMonitorV2` |
| `/MANIFESTUAC:NO` | **必需**。否则链接器追加的 `asInvoker` 片段会与清单冲突（`c1010001`） |
| `/SUBSYSTEM:WINDOWS` | 无控制台窗口 |

> ⚠️ **批处理脚本一律保持纯 ASCII。** cmd.exe 用 OEM 码页（简中 936）解析 `.bat`，
> UTF-8 中文会把命令行切碎（报 `'sual' is not recognized` 这类怪错）。中文说明只放本 README。

## 4. 使用

1. 双击 `HuaweiWmiControl.exe`（清单要求提权，会弹 UAC）
2. 托盘出现绿色电池图标；**左键或右键**都弹出菜单

| 菜单 | 说明 |
|---|---|
| 传感器 ▸ | 风扇转速 + 11 路温度，只读，来自后台缓存（30 秒刷新） |
| 电池保护 ▸ | 关闭充电控制 / 原厂预设 40-70、70-90、95-100 / MACH-WX9 清零兼容开关 |
| Fn-Lock | 复选项，勾选即切换 F1-F12 模式 |
| 键盘背光 ▸ | 关 / 低 / 高（单勾选）+ 熄灭超时（0/15/30/60/300 秒） |
| 智能充电 ▸ | 模式 0-3、参数 0-2 子菜单 + 「应用当前值」 |
| 电源解锁 | 复选项，解除 Fn+P 性能限制 |
| 麦克风 LED ▸ | 点亮 / 熄灭（固件无回读，故不显示勾选状态） |
| 刷新状态 | 重读所有功能，更新缓存与 tooltip |
| 打开日志 | 用默认程序打开 `%LOCALAPPDATA%\HuaweiWmiControl\hwc.log` |
| 退出 | 移除托盘图标并退出 |

- 机型不支持的功能自动**置灰**，不弹错误框，结果只写日志。
- 菜单渲染**零 WMI 调用**（只读状态缓存），所以弹出是瞬时的。

## 5. 目录结构

```
HuaweiWmiControl/
├── CMakeLists.txt
├── app.manifest            # requireAdministrator + PerMonitorV2 + UTF-8 ACP
├── app.rc                  # 版本信息（清单由链接器嵌入，不放 RT_MANIFEST）
├── build_all.bat           # 一键构建：定位 VS → 编测试 → 跑测试 → 编主程序
├── build_test_proto.bat    # 只编 + 跑纯逻辑测试
├── build_diag.bat          # 编三个真机诊断工具（需管理员运行）
├── build_clean.bat         # 清理编译中间件
├── docs/
│   └── reverse-engineering.md   # 原始逆向分析（命令号来源）
├── src/
│   ├── proto.h proto.c     # ★冻结契约：命令号/常量/纯编解码函数/温度区表
│   ├── hwapi.h             # ★冻结契约：对外 C API + HwStatus + HwDevice
│   ├── wmi.cpp             # COM/WMI 连接、协议探测、命令调用与重试
│   ├── services.cpp        # 电池/Fn-Lock/键盘/智能充电/电源解锁/MIC/传感器
│   ├── log.cpp             # UTF-16 滚动日志（512 KB 轮转）
│   ├── icon.cpp            # 运行时 GDI 生成 16×16 托盘图标
│   ├── tray.cpp            # Shell_NotifyIcon 封装
│   ├── menu.cpp            # 弹出菜单构建与命令分发
│   └── main.cpp            # WinMain、隐藏窗口、消息循环、状态缓存
└── tests/
    ├── test_proto.c        # 183 个断言，无需硬件、无需管理员
    ├── diag_live.cpp       # 真机诊断：逐项 HwStatus + 写回测试
    ├── diag_fnlock.cpp     # Fn-Lock 原始字节 + 有效值映射
    └── diag_fnwatch.cpp    # 轮询 Fn-Lock 值，与物理 Fn+Esc 对照
```

**分层**：`hwapi.h` / `proto.h` 是冻结契约，核心层（wmi/services/log）与外壳层
（main/menu/tray/icon）各自实现，彼此只通过契约通信。全部协议常量与字节编解码
收在 `proto.h` 的纯函数里，因此可以脱离硬件测试。

## 6. 测试与诊断

### 6.1 纯逻辑测试（无需硬件）

```
build_test_proto.bat
→ 通过 183，失败 0
```

覆盖：命令编码（含用 `docs/reverse-engineering.md` 里 x64dbg 抓到的 4 个原厂
固件值 `0x64001003`/`0x5A461003`/`0x46281003`/`0x645F1003` 反推校验）、各响应
解析器的边界（含缓冲区过短不得越界）、键盘背光三种编码的解码/反编码往返、
17 条命令号与 5 个协议常量。

### 6.2 真机诊断（需管理员）

```
build_diag.bat
```

| 程序 | 输出 | 用途 |
|---|---|---|
| `diag_live.exe` | `%TEMP%\hwc_diag.txt` | 每个 getter 的 `HwStatus` + 固件状态码 + 写回测试 |
| `diag_fnlock.exe` | `%TEMP%\hwc_fnlock.txt` | Fn-Lock 原始返回字节、有效值扫描（0..3） |
| `diag_fnwatch.exe` | `%TEMP%\hwc_fnwatch.txt` | 轮询 Fn-Lock 值，判断物理键与 WMI 标志是否同源 |

它的价值在于**区分「机型真的不支持」和「客户端代码有错」**——本次真机验证中
那个菜单置灰反转 bug 就是靠它与像素比对一起定位的。

## 7. 已知风险与限制

### 7.1 真机验证中发现并修复的问题

| # | 问题 | 根因 | 修复 |
|---|---|---|---|
| 1 | 菜单置灰逻辑整体反转（严重）：可用的功能被灰掉、唯一不支持的反而不灰 | `FlagsIf(avail, MF_GRAYED)` 漏写取反，共 4 处 | 引入 `GrayUnavailable(available)` 替换全部 7 处用法，根除同类反转 |
| 2 | 键盘背光亮度在 BRN-HXX 只读，但菜单仍可点、点了必失败 | 固件读得到、写被拒（`fw=0x01`），探测阶段无法区分 | `AppState.kbd_level_rw`：首次写失败后置灰亮度三项 |
| 3 | Fn-Lock「点了没反应」 | `ToggleFnLock` 用缓存推导目标，缓存过期时重写同一个值（成功但状态不变） | 改为**写前实测 + 写后回读**，第一次点击必定产生真实翻转 |

### 7.2 仍未验证 / 已知限制

| # | 限制 | 说明 |
|---|---|---|
| R1 | 仅在一台机型完成完整实测 | BRN-HXX 之外的行为仍是历史报告，未经本版复测 |
| R2 | 回到 DCOM | 旧版特意迁到 CIM/WS-Man，本版为脱离 .NET 又用 `IWbemLocator`。本机连接已验证正常 |
| R3 | `wmi.cpp` 职责未拆 | 仍同时管连接、协议探测、键盘编码探测。有意先保正确性 |
| R4 | 托盘图标运行时绘制 | 若 AND 掩码处理异常会带黑底；已内置 `LoadIconW` 降级路径 |
| R5 | 菜单无滑杆 | 电池阈值只能选原厂预设，任意值需配置文件或对话框 |
| R6 | 智能充电语义未文档化 | `mode/unknown/start/end/param` 原样暴露，无官方语义说明 |
| R7 | 传感器定时器占用 UI 线程 | WMI 同步调用；菜单弹出期间已跳过，极慢固件仍可能感到卡顿 |
| R8 | 自动化无法点击本程序菜单 | 程序以管理员（高完整性）运行，普通权限进程的 `SendInput` 被 UIPI 拦截；真实鼠标不受影响 |
| R9 | `avail_micled` 乐观置 1 | 麦克风 LED 无回读接口，无法探测，只能试写 |
| R10 | 物理 Fn+Esc 可能不更新 WMI 值 | 实测 Fn+Esc 期间该值无变化，EC 与 WMI 标志可能不同源；不影响菜单写入 |

## 8. 背景

华为、荣耀笔记本电脑出厂自带的「电脑管家」虽然功能齐全，但后台常驻四五个进程、
时不时弹窗，还有从睡眠恢复后 CPU 异常高占用等问题，体验并不好。许多用户只是想用
其中的**电池保护（智能充电）**、**键盘背光调节**、**Fn-Lock** 等硬件调控功能，
却被迫接受整套臃肿的软件包。

Linux 社区早已通过 [`huawei-wmi`](https://github.com/aymanbagabas/Huawei-WMI)
内核驱动实现了对华为笔记本的硬件控制，但 Windows 上一直缺少一个轻量的替代品。

本项目正是为此而生——基于逆向分析，通过 WMI 直接调用固件接口，实现与电脑管家
相同的硬件调控能力，**无需电脑管家后台常驻**。

## 9. 相关项目与鸣谢

相关项目：

- [huawei-wmi](https://github.com/aymanbagabas/Huawei-WMI) — 华为笔记本 Linux 内核驱动，本项目的命令编码基础
- [AceDroidX/HuaweiBatteryControl](https://github.com/AceDroidX/HuaweiBatteryControl) — 基于 C++ 的华为电池控制工具，本项目的逆向分析起点

特别感谢：

- **[AceDroidX/HuaweiBatteryControl](https://github.com/AceDroidX/HuaweiBatteryControl)** — [@AceDroidX](https://github.com/AceDroidX) 的逆向分析为项目奠定了坚实基础
- **[huawei-wmi](https://github.com/aymanbagabas/Huawei-WMI)** — [@aymanbagabas](https://github.com/aymanbagabas) 的 Linux 内核驱动提供了完整的命令编码参考

同时感谢以下 Linux 驱动社区成员在不同机型上的测试验证：

- Daniel Vogelbacher [@cytrinox](https://github.com/cytrinox) 和 Jan Baer [@janbaer](https://github.com/janbaer) — MateBook X (2017)
- [@nekr0z](https://github.com/nekr0z) — MateBook 13 (2019) `WRT-WX9`，以及其项目 [matebook-applet](https://github.com/nekr0z/matebook-applet)
- [@wasakakero](https://github.com/wasakakero) — MateBook D 14-AMD `KPL-W0X`

## 10. 参与贡献

[![](https://img.shields.io/badge/申请-Pull%20Request-orange)](https://github.com/HelloZER0/HuaweiWmiControl/pulls)

欢迎各种形式的贡献：

- 🐛 **报告 Bug** — 提 [Issue](https://github.com/HelloZER0/HuaweiWmiControl/issues/new)，注明笔记本型号、系统版本，并附上 `%LOCALAPPDATA%\HuaweiWmiControl\hwc.log`
- 🧪 **机型兼容性** — 跑 `build_diag.bat`，把 `%TEMP%\hwc_diag.txt` 的结果填进 §2 的表格
- ✨ **新功能** — 先开 Issue 讨论再 PR
- 🔬 **逆向新命令** — 补充到 `docs/`

提交 PR 前请确保：

```bat
build_test_proto.bat      :: 183 个断言必须全绿
build_all.bat             :: 必须编译通过
```

详细参与指南见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 11. 开源协议

[GNU General Public License v2.0](LICENSE) © HelloZER0

主要项目负责人：[@HelloZER0](https://github.com/HelloZER0)

---

> **免责声明**：本工具为社区逆向成果，命令编码参照 Linux 主线驱动。不同机型固件
> 可能存在差异，请在实际设备上验证各项功能；使用风险自负。
