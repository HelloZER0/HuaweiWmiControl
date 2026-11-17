# HuaweiWmiControl Native — Win32 托盘版

华为/荣耀笔记本硬件调控工具的**原生重写版**：无 .NET、无 WinUI、无第三方库，
只有一个任务栏托盘图标 + 右键/左键弹出菜单，直接通过 COM/`IWbemLocator`
调用 `ROOT\WMI` 的固件方法。

> **状态（2026-11-18）：已完成真机验证。** 在 HONOR BRN-HXX（BIOS 1.15）上，
> WMI 连接、协议识别、全部读取、4 项写入均验证通过；过程中发现并修复了
> **1 个严重 UI 逻辑 bug**（见 §6.1）与 **1 个机型特有问题**（见 §6.2）。
>
> 编译验证：MSVC 19.52 + Windows SDK 10.0.26100，`cl` 与 CMake 两条路径均通过，
> `proto.h` 纯逻辑测试 183 断言全绿。
>
> **人工操作仍未覆盖**：托盘菜单项的实际点击（自动化受 UIPI 限制，见 §6.3）。
> 首次移植时开发机无任何 C/C++ 工具链，代码未经编译；随后在
> **MSVC 19.52（VS 2026 Community）+ Windows SDK 10.0.26100** 上完成首次构建，
> 修掉 3 个问题后 `cl` 与 `CMake` 两条路径均构建成功：
>
> - `proto.h` 纯逻辑测试：**183 个断言全部通过**（含 4 个逆向抓包值交叉校验）
> - 产物 `HuaweiWmiControl.exe`：197 KB、`Subsystem=2`（GUI）、
>   `requireAdministrator` 已嵌入清单
>
> **仍未验证的部分**：全部 `hw_*` 与硬件交互的行为（需华为真机 + 管理员权限），
> 详见文末「已知风险」。

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

- **Visual Studio 2022 或 2026**，安装时勾选「**使用 C++ 的桌面开发**」工作负载
  （它会带上 MSVC 工具集 + Windows SDK；只装 VS 本体是不够的）
- Windows 10/11 SDK（10.0.19041 或更新；已在 10.0.26100 上验证）
- 不需要 .NET SDK、不需要 CMake（可选）

### 方式 A：一键构建（推荐）

在**普通命令提示符**里、`cd` 到本目录后运行：

```bat
build_all.bat
```

它会用 `vswhere` 自动定位 Visual Studio 并调用 `vcvars64.bat`，不需要你先开
「开发者命令提示符」。流程是：编测试 → 跑测试 → 编主程序。

只想跑纯逻辑测试（不需要管理员、不需要华为硬件）：

```bat
build_test_proto.bat
```

真机诊断（需管理员运行；输出 `build\diag_live.exe`，结果写到 `%TEMP%\hwc_diag.txt`）：

```bat
build_diag.bat
```

`diag_live.exe` 会逐项打印每个 getter 的 `HwStatus` 与固件状态码，并把当前值
原样写回（不改变状态）。排查「某功能为什么置灰」时先跑它——它能区分
「机型真的不支持」和「客户端代码有错」。

清理编译中间件（`.obj` / `.res` / `.pdb` 等），保留 `build\*.exe`：

```bat
build_clean.bat        :: 只清中间件
build_clean.bat all    :: 连 build\ 目录一起删
```

构建脚本已用 `/Fo:build\` 把目标文件统一收进 `build\`，源码根目录不会留下
`.obj`；这个脚本用于回收磁盘或交付前清理。

产物：`build\HuaweiWmiControl.exe`

### 方式 B：CMake

```bat
cmake -S . -B build-cmake -G "Visual Studio 17 2022" -A x64
cmake --build build-cmake --config Release
```

（VS 2026 用 `-G "Visual Studio 18 2026"`。）

产物：`build-cmake\Release\HuaweiWmiControl.exe`，另带 `test_proto.exe`。

> ⚠️ **批处理脚本一律保持纯 ASCII。** cmd.exe 用 OEM 码页（简中为 936）解析
> `.bat`，UTF-8 中文会把命令行切碎，报出 `'sual' is not recognized` 这类怪错。
> 中文说明只放在本 README 里。

### 关键编译选项

| 选项 | 原因 |
|---|---|
| `/utf-8` | 源码里直接写中文宽字符串字面量，缺了会变乱码 |
| `/MT` | 静态 CRT，目标机无需 VC++ 运行时 |
| `/MANIFESTINPUT:app.manifest` | 嵌入 `requireAdministrator` + `PerMonitorV2`，不用 .rc 的 `RT_MANIFEST`（避免双清单） |
| `/MANIFESTUAC:NO` | **必需**。否则链接器仍会追加一份 `level="asInvoker"` 的 UAC 片段，与清单里的 `requireAdministrator` 冲突，报 `manifest authoring error c1010001: Values of attribute level not equal` |
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
├── build_all.bat           # 一键构建：定位 VS → 编测试 → 跑测试 → 编主程序
├── build_test_proto.bat    # 只编 + 跑纯逻辑测试
├── build_diag.bat          # 编真机诊断工具（需管理员运行）
├── build_clean.bat         # 清理编译中间件（保留 build\*.exe）
├── tests/
│   ├── test_proto.c        # 183 个断言，无需硬件
│   ├── diag_live.cpp       # 真机诊断：逐项打印 HwStatus + 写回测试
│   ├── diag_fnlock.cpp     # Fn-Lock 原始字节 + 有效值映射(0..3)
│   └── diag_fnwatch.cpp    # 轮询 Fn-Lock 值，用于和物理 Fn+Esc 对照
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

本版把**全部纯逻辑**（命令编码、`u8Output` 解析、键盘背光编码判定/解码/反编码）
都收敛进了 `proto.h` 的 `static inline` 函数，**不依赖 Windows.h、不依赖 COM**，
因此可以由 `tests/test_proto.c` 直接编译运行：

```
build_test_proto.bat
→ 通过 183，失败 0
```

覆盖点：

- **4 个逆向抓包值的交叉校验**（最强证据）：用 `docs/reverse-engineering.md`
  里 x64dbg 抓到的 PC Manager 实际下发值反推编码器是否正确
  （`0x64001003` / `0x5A461003` / `0x46281003` / `0x645F1003`）。
- 17 条命令号与 5 个协议常量逐一比对
- 各响应解析器的正常值、全零、**缓冲区过短**（不得越界读）
- 键盘背光三种编码的级别解码 + 编码→解码往返一致性 + 越界夹紧
- 温度区表 11 项、索引不重复

`services.cpp` / `wmi.cpp` 需要真实固件，只能真机回归——这是本项目剩余的唯一盲区。

---

## 6. 已知风险

### 6.1 真机验证结果（HONOR BRN-HXX / BIOS 1.15）

用 `build_diag.bat` 生成的 `diag_live.exe` 逐项取证：

| 项目 | 结果 | 实测数据 |
|---|---|---|
| WMI 连接 | ✅ | 实例 `ACPI\PNP0C14\HWMI_0` |
| 协议探测 | ✅ 自动识别 **U8** | 该类只有 `u8Input`（无 `u64Input`） |
| 键盘背光编码 | ✅ 识别为 **百分比** | — |
| 电池保护 读 | ✅ | `start=70 end=90` |
| 电池保护 写 | ✅ | `set(70,90)` 成功——**字节序在真机确认正确** |
| Fn-Lock 读/写 | ✅ | `on=1` → `set(1)` 成功 |
| 键盘背光 亮度读 | ✅ | `level=0` |
| 键盘背光 亮度写 | ❌ 固件拒绝 | 见 §6.2 |
| 键盘背光 超时写 | ✅ | `set(0)` 成功 |
| 智能充电 | ❌ 机型不支持 | `fw=0x01` |
| 电源解锁 读/写 | ✅ | `on=0` → `set(0)` 成功 |
| 风扇转速 | ✅ | 风扇1 = 3252 RPM |
| 温度 | ✅ 7/11 路 | CPU 67℃、TSLO 49℃、TNTC 40℃、CNTC 52℃、DNTC 47℃、电池 32℃、TP0C 26℃ |
| 单实例 | ✅ | 二次启动正确通知旧实例并退出 |
| 托盘图标 / 菜单 / 日志 | ✅ | 图标可显示、菜单 11 项结构完整、日志落盘 |

### 6.2 真机验证发现并修复的 2 个问题

**① 菜单置灰逻辑整体反转（严重，已修复）**

`menu.cpp` 里 4 处写成 `FlagsIf(st->avail_xxx, MF_GRAYED)`，而
`FlagsIf(cond, flag)` 的语义是 `cond ? flag : 0`——漏了取反，导致
**「功能可用」的功能被灰掉、「唯一不支持」的反而不灰**：

| 菜单项 | 修复前 | 修复后 |
|---|---|---|
| 电池保护 | 灰（错） | 可用 |
| 键盘背光 | 灰（错） | 可用 |
| 麦克风 LED | 灰（错） | 可用 |
| 智能充电 | 可用（错，点了必然失败） | 灰 |

修复方式不是补 `!`，而是引入语义明确的 `GrayUnavailable(available)`
（`available ? 0 : MF_GRAYED`），从根上消除这类反转。像素级验证：置灰项
文字灰度 159、可用项 0。

**② 键盘背光亮度在此机型只读（已按机型自适应）**

真机确认：亮度**读得到**（`level=0`）但**写被固件拒绝**（`fw=0x01`），
这台机器只允许改「熄灭超时」。原先菜单把「关/低/高」三项显示为可点，
点了必然失败。

修复：`AppState` 增加 `kbd_level_rw`，探测时乐观置 1，首次写入收到
`HW_ERR_NOT_SUPPORTED` 就置 0 并写日志，菜单下次弹出即灰掉这三项。
（探测阶段无法区分只读/可写——只能试写一次才知道，因此采用了这种自适应策略。）

**③ Fn-Lock「点了没反应」——缓存过期导致第一次点击是空操作（已修复）**

真机反馈「Fn-Lock 无法控制」。逐层取证后排除了编码问题：

```
GET          -> 00 01 ...        1 = 关
SET arg=1    -> 成功, GET 仍 01  （已是关，写入无效但合法）
SET arg=2    -> 成功, GET 变 02  （2 = 开，写入生效且可回读）
SET arg=0/3  -> 当前机型不支持    （有效值集合就是 {1,2}）
```

即固件层完全正常，与 C# 原版语义（on→2、off→1）一致。真正的问题是
`ToggleFnLock` **用缓存值推导目标**：

```cpp
int target = st->fnlock_on ? 0 : 1;   // 缓存若已过期，就会重写同一个值
```

只要缓存与实际不符（程序启动后用 Fn+Esc 或电脑管家改过、或上一次点击的
结果没被采纳），第一次点击就会把**同一个值**再写一遍——返回成功、状态不变，
表现就是「点了没反应」。用户连点三次才偶然生效。

修复：改为**写前实测 + 写后回读**

1. 写前先 `hw_fnlock_get` 取实测值，与缓存不一致就以实测为准重算目标；
2. 写入后立刻回读，只有读回值等于目标才更新缓存，否则记日志「固件未采纳」
   并以实际读回值为准。

真机日志已验证：`Fn-Lock：写入 1（开），回读 1 —— 已生效`，物理 F1-F12 行为正确切换。

### 6.3 首次构建时修掉的 3 个问题（已修复，记录以备回溯）

| 现象 | 原因 | 修复 |
|---|---|---|
| `'sual' is not recognized`、命令行被切碎 | `.bat` 里写了 UTF-8 中文，而 cmd.exe 按 OEM 码页（936）解析 `.bat` | 所有 `.bat` 改为纯 ASCII，中文说明移入本 README |
| `log.cpp(330): error C3861: "ShellExecuteW": 找不到标识符` | 缺 `#include <shellapi.h>` | `log.cpp` 补上该头 |
| `LNK1327` + `manifest authoring error c1010001: Values of attribute "level" not equal` | 链接器默认 UAC 片段（`asInvoker`）与清单里的 `requireAdministrator` 合并冲突 | 链接加 `/MANIFESTUAC:NO` |

### 6.4 仍未验证 / 已知限制

| # | 风险 | 说明 / 处理 |
|---|---|---|
| R1 | **硬件行为全部未验证** | 编译与纯逻辑测试通过 ≠ 固件调用正确。7 项功能需在华为/荣耀真机上逐项回归 |
| R2 | **回到 DCOM** | 旧版特意从 `System.Management`/DCOM 迁到 CIM/WS-Man；本版因要脱离 .NET 又回到 `IWbemLocator`。本机连接通常没问题，但 `CoInitializeSecurity` 时机、`CoSetProxyBlanket` 认证级别是经典坑位，需真机验证 |
| R3 | **STA 线程约束未验证** | 旧版注释声称 `Connect()` 必须在 STA，但无依据。本版在 `wWinMain` 所在线程（OLE STA）里完成全部 WMI 调用，行为与旧版一致，但不代表这是必要条件 |
| R4 | 职责未拆分 | `wmi.cpp` 仍同时管连接、协议探测、键盘编码探测。这是**有意保留**：先保证移植正确，再谈重划边界（参见 `docs/refactor-plan.md`） |
| R5 | 托盘图标是运行时画的 | 若 `CreateIconIndirect` 的 AND 掩码处理有误，图标会带黑底。回退方案：`LoadIconW(NULL, IDI_APPLICATION)`（代码已内置该降级路径） |
| R6 | 菜单无滑杆 | 旧版的 0-100 连续阈值在新 UI 里变成原厂预设（40-70 / 70-90 / 95-100）。要任意值需加配置或对话框 |
| R7 | 智能充电语义未文档化 | 旧版也是把 `mode/unknown/start/end/param` 原样暴露，没有任何机型语义说明。本版保持原样 |
| R8 | 传感器定时器阻塞消息循环 | WMI 调用是同步的，30 秒定时器刷新 13 个传感器时会占用 UI 线程。菜单弹出期间已用 `g_menuOpen` 跳过，但极端慢固件下仍可能感到卡顿 |
| R9 | 托盘回调版本 | 本程序不调 `NIM_SETVERSION`，Shell 按 v1 语义回调（`lParam`=鼠标消息）。若未来要 v4 坐标，必须同步改 `WndProc` 的分支 |
| R11 | **自动化无法点击本程序菜单** | 程序以管理员（高完整性）运行，普通权限进程的 `SendInput` 被 Windows UIPI 拦截；真实鼠标点击不受影响。故「菜单项点击」只能人工验证 |
| R10 | `avail_micled` 乐观置 1 | 麦克风 LED 只有 SET 没有 GET，无法探测。刷新状态后会重新点亮为「可用」，需再点一次才会重新置灰 |

---

## 7. 许可

与上游一致：GPL-2.0。
