# 贡献指南

感谢你对本项目的兴趣！🎉

无论你是开发者、测试者、文档写手还是普通用户，都有很多方式可以参与贡献。

## 目录

- [行为准则](#行为准则)
- [如何参与](#如何参与)
- [开发流程](#开发流程)
- [代码规范](#代码规范)
- [提交 Issue](#提交-issue)
- [提交 Pull Request](#提交-pull-request)

## 行为准则

本项目采用 [Contributor Covenant](https://www.contributor-covenant.org/) 行为准则。参与即表示你同意遵守其条款。

## 如何参与

### 🐛 报告 Bug

1. 先搜索 [已有 Issue](https://github.com/HelloZER0/HuaweiWmiControl/issues)，避免重复
2. 提 Bug 时请包含：
   - 笔记本型号及系统版本
   - 操作步骤
   - 期望行为与实际行为
   - 截图或错误日志

### ✨ 提交新功能

1. 先开 [Issue](https://github.com/HelloZER0/HuaweiWmiControl/issues/new) 讨论设计方案
2. 确认后再开始编码

### 📖 完善文档

- 修正错别字、翻译错误
- 补充使用教程或常见问题
- 添加新机型的兼容性报告

### 🔬 逆向新固件命令

- 发现新的固件接口后，在 `docs/` 目录下新增说明文档
- 同时在代码中添加对应的命令定义和测试

### 🧪 兼容性测试

在不同型号的华为/荣耀笔记本上测试各项功能，将结果反馈在 [兼容性讨论](https://github.com/HelloZER0/HuaweiWmiControl/discussions) 中。

## 开发流程

```mermaid
flowchart LR
    A[Fork 项目] --> B[创建分支]
    B --> C[开发 & 测试]
    C --> D[提交 PR]
    D --> E[Code Review]
    E --> F[合并到 main]
```

## 代码规范

- 遵循项目已有的代码风格（`.editorconfig` 已配置；缩进 4 空格、UTF-8 **无 BOM**）
- 源文件直接写中文宽字符串字面量（`L"中文"`），构建脚本已带 `/utf-8`
- **批处理脚本保持纯 ASCII**：cmd.exe 按 OEM 码页（简中 936）解析 `.bat`，
  UTF-8 中文会把命令行切碎，报出 `'sual' is not recognized` 这类怪错
- 保持分层：`src/proto.h` 与 `src/hwapi.h` 是**冻结契约**；
  外壳层（`main` / `menu` / `tray` / `icon`）只允许调用 `hwapi.h`，
  不得直接接触 COM / WMI
- 新增 WMI 命令时：
  1. 常量加进 `src/proto.h`
  2. 解析器写成**带长度校验的纯函数**（不得直接下标访问 `u8Output`）
  3. 在 `tests/test_proto.c` 补断言
- 涉及硬件写入的改动**必须在真机上验证**，PR 里贴上
  `%LOCALAPPDATA%\HuaweiWmiControl\hwc.log` 的相关片段

## 提交 Issue

请使用对应的 Issue 模板：

- [报告 Bug](https://github.com/HelloZER0/HuaweiWmiControl/issues/new?template=bug_report.yml)
- [请求新功能](https://github.com/HelloZER0/HuaweiWmiControl/issues/new?template=feature_request.yml)

## 提交 Pull Request

1. 确保你的 Fork 是基于最新的 `main` 分支
2. 创建功能分支：`git checkout -b feat/your-feature-name`
3. 提交你的改动
4. 确保所有测试通过：

   ```bat
   build_test_proto.bat    :: 纯逻辑测试，无需硬件，必须全绿
   build_all.bat           :: 必须编译通过
   ```

5. 发起 PR，描述清楚改动内容和测试情况
6. 等待 Code Review

---

再次感谢你的贡献！🙌
