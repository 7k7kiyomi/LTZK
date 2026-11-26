# 质检方案真实表头匹配修复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让宿主正确识别插件 `QTreeWidget` 的“质检规则数值”表头，使名称、描述、数值三列的 250 像素设置实际执行，并让单位列填满剩余表头宽度。

**Architecture:** 保留现有 `normalizeQualityWindowTableColumnWidths(QWidget*)` 和 `QTreeView` 支持，修正错误的表头文字与局部变量命名，并将真实的单位列设为 `QHeaderView::Stretch`。通过 PowerShell 源码回归检查验证真实插件表头和剩余宽度填充，不修改插件 DLL 或其他布局逻辑。

**Tech Stack:** C++17、Qt 5 `QTreeWidget`/`QTreeView`/`QHeaderView`、PowerShell、MSBuild x64 Release。

---

### Task 1: 建立真实表头回归检查

**Files:**
- Create: `E:\AAALTZK\tests\verify_quality_scheme_value_header_match.ps1`

- [x] **Step 1: 编写失败检查**

检查 `normalizeQualityWindowTableColumnWidths` 必须包含 `QStringLiteral("质检规则数值")`、`qualityValueColumn` 和三列数组，并禁止继续出现 `QStringLiteral("质检规则数量")`。

- [x] **Step 2: 运行检查确认红灯**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_quality_scheme_value_header_match.ps1`

Expected: FAIL，提示缺少真实表头“质检规则数值”。

### Task 2: 实施单点修复

**Files:**
- Modify: `E:\AAALTZK\LTZK.cpp:5694`

- [x] **Step 1: 修正表头和局部变量**

将 `qualityCountColumn` 改为 `qualityValueColumn`，并将查找字符串从“质检规则数量”改为“质检规则数值”。三列数组使用 `nameColumn`、`descriptionColumn`、`valueColumn`，宽度保持 `{ 250, 250, 250 }`。

- [x] **Step 2: 运行新增检查确认绿灯**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_quality_scheme_value_header_match.ps1`

Expected: PASS，输出真实数值表头匹配已验证。

### Task 3: 回归与构建

**Files:**
- Test: `E:\AAALTZK\tests\verify_quality_scheme_value_header_match.ps1`
- Test: `E:\AAALTZK\tests\verify_quality_scheme_direct_column_widths.ps1`
- Test: `E:\AAALTZK\tests\verify_quality_scheme_table_header_width_lifecycle.ps1`

- [x] **Step 1: 运行质检表头相关检查**

Run the three scripts above and require exit code 0 for each.

- [x] **Step 2: 构建 Release**

Run: `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\LTZK.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m`

Expected: build exits 0 with no compile errors.

### Task 4: 填充表头剩余宽度

**Files:**
- Create: `E:\AAALTZK\tests\verify_quality_scheme_header_fills_row.ps1`
- Modify: `E:\AAALTZK\LTZK.cpp:5764`

- [x] **Step 1: 编写失败检查并确认红灯**

检查质检方案规范化逻辑必须解析 `unitColumn`，并将其 resize mode 设置为 `QHeaderView::Stretch`。

- [x] **Step 2: 设置单位列拉伸**

在三列固定宽度设置之后，将“质检规则单位”列设置为 `Stretch`，由 Qt 自动填充表格剩余宽度。

- [x] **Step 3: 回归检查并重新构建 Release**

新增检查、既有质检方案检查和 x64 Release 构建均通过。
