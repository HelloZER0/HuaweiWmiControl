# Senior Developer — 团队技术能力提升实践指南

> 审查日期: 2026-07-10  
> 项目: HuaweiWmiControl (WinUI 3 / .NET 8 / C#)  
> 作者: Senior Developer（高级开发工程师）

---

## 目录

1. [项目现状速览](#1-项目现状速览)
2. [3个最该立刻修的问题](#2-3个最该立刻修的问题)
3. [一轮Code Review要查什么](#3-一轮code-review要查什么)
4. [团队评审清单](#4-团队评审清单)
5. [代码风格统一](#5-代码风格统一)
6. [技术深度提升](#6-技术深度提升)
7. [长期文化建议](#7-长期文化建议)
8. [动手改一把](#8-动手改一把)

---

## 1. 项目现状速览

先说好的：这个项目**架构分层清晰**（策略模式+模板方法），**文档质量优秀**（XML注释、设计决策记录完整），性能优化也做了不少（StringBuilder、传感器预填充、CIM替代DCOM）。团队底子是有的。

但仔细看代码，能发现**三个共性短板**：异常处理粗糙、异步滥用 `Task.Run`、大量 Copy-Paste 样板代码。这些都是工程习惯问题，改起来不难，但回报很大。

| 维度 | 评分 | 核心问题 |
|------|------|----------|
| 架构设计 | ★★★★☆ | 好，但缺少DI容器 |
| 可读性 | ★★★☆☆ | 命名规范，但大量重复 |
| 错误处理 | ★★☆☆☆ | **异常吞噬是最大隐患** |
| 代码复用 | ★☆☆☆☆ | 7份几乎一样的XAML |
| 可测试性 | ★★☆☆☆ | 接口抽象好但零测试 |

---

## 2. 3个最该立刻修的问题

### 🔴 问题1：异常不该被沉默

```csharp
// ❌ 当前做法 — 所有异常一律吞掉，调用方无法区分场景
catch (Exception ex) { LastError = ex; return default; }

// ✅ 正确做法 — 区分"预期失败"和"系统故障"
catch (OperationCanceledException) { throw; }  // 放行取消
catch (CimException ex) when (IsTransient(ex)) { /* 重试逻辑 */ }
catch (Exception ex) { LastError = ex; return default; }  // 仅吞预期失败
```

**团队原则**：每一层只处理自己能处理的异常。不能处理的——往上抛，不要默默吞掉。用户端看到"读取失败"但不知道是"设备不支持"还是"驱动崩了"，排查问题要多花半小时。

### 🔴 问题2：Task.Run 不是真正的异步

```csharp
// ❌ 当前做法 — 浪费一个线程池线程
public Task<byte[]> InvokeAsync(...) => Task.Run(() => Invoke(...), ct);

// ✅ 正确做法 — CIM 本身有异步 API
public async Task<byte[]> InvokeAsync(...) 
{
    var asyncResult = session.InvokeMethodAsync(ns, instance, method, params);
    var result = await asyncResult.AsTask(ct).ConfigureAwait(false);
    return (byte[])result.OutParameters[WmiConstants.WmiOutputParam].Value;
}
```

**团队原则**：异步不是 `Task.Run` 的包装器。如果底层 API 有异步版本，永远用它。`Task.Run` 是 CPU 密集任务的工具，不是 IO 操作的补丁。这个已修复完成，值得学习。

### 🔴 问题3：XAML 不要 Copy-Paste

项目原来 330 行 XAML 里 7 个功能卡片几乎一模一样。提取为 `FeatureCard` 自定义控件后，添加新功能页从 **40 行样板降到 5 行**。

```xml
<!-- ✅ 提取后的用法 -->
<controls:FeatureCard Title="电池保护" Description="通过限制充电区间来延长电池寿命">
    <StackPanel Spacing="16">
        <!-- 只有这里放特有控件 -->
    </StackPanel>
</controls:FeatureCard>
```

**团队原则**：如果复制粘贴超过 2 次，就应该提取公共组件。XAML 的 `ContentControl` 和 `DependencyProperty` 就是为此设计的。

---

## 3. 一轮 Code Review 要查什么

每次提 PR 前，Reviewer 按这个清单逐条过：

### 必查项（5分钟）

- [ ] **异常**：新加的 try-catch 是吞了还是处理了？有没有 `catch (Exception)` 却不 rethrow 的？
- [ ] **async**：有没有 `async void`（除了 ICommand）？有没有 `Task.Run` 包装同步 API？有没有忘了 `ConfigureAwait(false)`？
- [ ] **线程安全**：静态/共享字段有没有竞态？是不是该用 `volatile` 或 `lock`？
- [ ] **重复**：这段代码是不是在别的地方见过？该不该提取公共方法/控件？
- [ ] **命名**：方法名是不是动词？属性名是不是名词？布尔属性是不是 `Is`/`Has`/`Can` 开头？

### 进阶项（有经验后追加）

- [ ] 新功能页有没有对应的单元测试？
- [ ] 错误信息是不是对用户友好（而不是直接吐异常消息）？
- [ ] WMI 调用是否通过基类模板方法（InvokeGet/InvokeSet），而不是直接调 Protocol？

---

## 4. 团队评审清单

我已经把 Checklist 写入 `.editorconfig` 和 Roslyn Analyzer 配置，提交代码前运行 `dotnet build` 会自动检查：

```
# 当前启用的关键规则
dotnet_diagnostic.SA0001 = error        # XML 注释规范
dotnet_diagnostic.CA1031 = warning      # 不要捕获一般异常类型
dotnet_diagnostic.CA2007 = error        # 必须使用 ConfigureAwait
dotnet_diagnostic.S3168 = warning       # async void 需显式 suppress
dotnet_diagnostic.S112 = warning        # 不要抛一般异常类型
dotnet_diagnostic.S907 = warning        # 避免 goto 语句
dotnet_diagnostic.S1854 = warning       # 未使用的赋值
```

目前项目已经是 **Debug + Release 双模式 0 warnings, 0 errors**，请保持这个标准。

---

## 5. 代码风格统一

### 命名规范（.editorconfig 已配置）

| 类型 | 规则 | 示例 |
|------|------|------|
| 类/方法/属性 | PascalCase | `GetThresholdAsync`, `LastError` |
| 私有字段 | `_camelCase` | `_connection`, `isReady` |
| 参数/局部变量 | `camelCase` | `cmd`, `parser` |
| 接口 | `I` 前缀 | `IWmiProtocol`, `IBatteryService` |
| 布尔属性 | `Is`/`Has`/`Can` 前缀 | `IsReady`, `Available`, `IsConnected` |

### 代码组织（每个文件的结构）

```
1. using 语句（System → NuGet → 内部命名空间）
2. namespace 声明
3. XML 注释说明类/方法的用途
4. 公共属性 → 构造函数 → 公共方法 → 内部方法 → 私有方法
```

---

## 6. 技术深度提升

### 6.1 单元测试入门

项目架构设计良好（接口抽象 + 构造函数注入），天然适合测试。

```csharp
// 测试 WmiServiceBase 的 InvokeGet 模板方法
[Test]
public void InvokeGet_WhenProtocolSucceeds_ReturnsParsedValue()
{
    var mockProtocol = new Mock<IWmiProtocol>();
    var mockConnection = new Mock<WmiConnectionManager>();
    
    mockProtocol.Setup(p => p.Invoke(It.IsAny<ulong>(), ...))
                .Returns(new byte[] { 0x00, 0x50, 0x00 });
    
    var service = new TestService(mockProtocol.Object, mockConnection.Object);
    var result = service.TestGet();
    
    Assert.That(result, Is.EqualTo(80)); // 0x50 = 80
}
```

**优先覆盖的目标**：
1. `WmiServiceBase` 的 Call/CallAsync 重试逻辑
2. 各 Service 的 ParseXxx 解析方法（纯函数，最容易测）
3. `WmiConnectionManager.DetectProtocol` / `ClassifyKbdEncoding`

### 6.2 DI 容器引入 ✅ 已实现

当前 `App.xaml.cs` 持有 `IServiceProvider`，所有 Service 通过 DI 容器注册和解析：

```csharp
// App.xaml.cs — DI 容器配置
public static IServiceProvider Services { get; } = ConfigureServices();

private static IServiceProvider ConfigureServices()
{
    var services = new ServiceCollection();
    services.AddSingleton<WmiConnectionManager>();
    services.AddSingleton<IWmiProtocol>(sp =>
        sp.GetRequiredService<WmiConnectionManager>().Protocol);
    services.AddSingleton<IBatteryService, BatteryService>();
    // ...7 个 Service
    services.AddSingleton<MainViewModel>();
    return services.BuildServiceProvider();
}
```

`WireServices()` 从手动 `new` 简化为 DI 解析：

```csharp
// ✅ 现在
Battery.Inject(App.Services.GetRequiredService<IBatteryService>());

// ❌ 之前
Battery.Inject(new BatteryService(p, c));
```

**好处**：添加新服务只需要注册一行+接口实现，无需改 `MainViewModel`。单元测试时可以替换为 Mock 实现。

### 6.3 日志框架升级（待办）

```csharp
// 引入 Microsoft.Extensions.Logging
// 只需注入 ILogger<T>，不需要开发者自己关心输出到哪里
public class BatteryService : WmiServiceBase, IBatteryService
{
    private readonly ILogger<BatteryService> _logger;
    
    public BatteryService(ILogger<BatteryService> logger, IWmiProtocol protocol, ...)
        : base(protocol, connection)
    {
        _logger = logger;
    }
}
```

---

## 7. 长期文化建议

### 每周 Code Review 轮值

- 每周 2 次，每次 30 分钟
- 轮值 Reviewer 负责过所有 PR
- Reviewer 用 Checklist 评审（上面第 3 节）
- 发现重复/可提取的模式，当场建 Issue

### 技术分享主题建议

| 周次 | 主题 | 谁来讲 |
|------|------|--------|
| 1 | 异常处理最佳实践（结合本项目真实案例） | (轮值) |
| 2 | async/await 深度——不要 Task.Run 包装 IO | (轮值) |
| 3 | WinUI 3 自定义控件和模板化 | (轮值) |
| 4 | 单元测试入门——先给 Service 层写测试 | (轮值) |
| 5 | DI 容器原理和在本项目的应用 | (轮值) |
| 6 | 代码复用技巧——从复制粘贴到抽象 | (轮值) |

### 性能文化

- 每次提交前思考：有没有不必要的分配？有没有不必要的 `Task.Run`？
- 考虑引入 BenchmarkDotNet 做性能回归
- 传感器 3 秒自动刷新的场景下，确保 60fps 不卡顿

---

## 8. 动手改一把

### 如果今天只有 1 小时

> **修完这些，代码质量立竿见影**

1. **检查所有 `catch (Exception)` 的代码**：是不是每个都能改成捕获特定异常？
2. **确认所有 async 方法**：有没有漏 `ConfigureAwait(false)`？
3. **看 XAML 里还有没有重复结构**：设置页的 4 个 Border 卡片还能不能提取？

### 如果今天有 3 小时

> **可以做一次完整的代码架构体检**

1. 逐个文件过一遍，用 Checklist 记录问题
2. 挑出最严重的 3 个问题当场修复
3. 把修复经验写成简短的 ADR 记录

### 如果今天有 1 天

> **可以做一次 Sprint 级别的质量冲刺**

1. 修复所有 P0/P1 问题（参考 `overview.md`）
2. 给 `WmiServiceBase.Call` / `CallAsync` 写单元测试
3. 引入 DI 容器，消除手动构造
4. 引入日志框架替代 `Action<string>` 回调
5. 更新 `.editorconfig` 确保后续代码风格一致

---

> **最后说一句**：代码质量不是一次性重构出来的，是每次提交时多花 5 分钟 Review 出来的。我已经把大部分基础设施做好了（editorconfig、Analyzer、FeatureCard），剩下的就是**坚持**。
>
> 需要我带你们现场改某一块代码，随时说。
