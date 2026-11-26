# 图层关系配置下拉框文字显示修复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 保证宿主程序重新润色图层关系配置下拉框后，原有的非空当前文字仍显示在内部 `QLineEdit` 中。

**Architecture:** 在 `LTZK.cpp` 的 `polishDrawingLayoutTreeEmbeddedCombos` 内，仅增加一次显示文字快照和一次恢复。现有主题样式、箭头、单元格文字抑制及插件数据匹配逻辑保持不变。用 PowerShell 源码级回归脚本验证操作顺序。

**Tech Stack:** C++/Qt 5/6 API、PowerShell 回归检查、Visual Studio/MSBuild（若环境可用）。

---

### Task 1: 建立回归检查的失败基线

**Files:**
- Test: `E:\AAALTZK\tests\verify_drawing_layout_combo_display_preserved.ps1`

- [ ] **Step 1: 检查回归脚本断言内容**

确认脚本定位 `polishDrawingLayoutTreeEmbeddedCombos` 函数体，并要求：

```powershell
const QString preservedComboDisplayText = combo->currentText();
combo->lineEdit()->setText(preservedComboDisplayText);
```

同时要求快照位于 `combo->setEditable(true)` 之前，恢复位于 `combo->lineEdit()->setPalette(lineEditPalette)` 之后。

- [ ] **Step 2: 运行脚本并确认当前实现失败**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_drawing_layout_combo_display_preserved.ps1
```

Expected: FAIL，错误信息应指出缺少 `preservedComboDisplayText` 快照；这证明回归检查确实捕获当前缺陷。

### Task 2: 在宿主下拉框润色流程中恢复显示文字

**Files:**
- Modify: `E:\AAALTZK\LTZK.cpp:549-651`

- [ ] **Step 1: 在重新配置下拉框前保存当前文字**

在 `polishCombo` lambda 中、现有主题键检查通过后，`combo->setEditable(true)` 之前加入：

```cpp
const QString preservedComboDisplayText = combo->currentText();
```

保留现有的 `ltzkComboPolishedTheme` 防重复处理逻辑，不改变快照来源。

- [ ] **Step 2: 在内部编辑框样式处理完成后恢复文字**

在现有代码：

```cpp
lineEditPalette.setColor(QPalette::WindowText, comboTextPaletteColor);
combo->lineEdit()->setPalette(lineEditPalette);
```

之后加入：

```cpp
if (!preservedComboDisplayText.isEmpty())
{
    combo->lineEdit()->setText(preservedComboDisplayText);
}
```

该代码放在 `if (combo->lineEdit())` 保护块内，确保只有存在内部编辑框时才恢复，且不把原本为空的值改成非空值。

- [ ] **Step 3: 运行回归检查确认修复通过**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_drawing_layout_combo_display_preserved.ps1
```

Expected: exit code 0，并输出：

```text
Drawing-layout combo display-text preservation verified.
```

### Task 3: 运行相关回归与构建验证

**Files:**
- Verify: `E:\AAALTZK\tests\verify_drawing_layout_combo_display_preserved.ps1`
- Verify: `E:\AAALTZK\tests\verify_drawing_layout_combo_text_preserved.ps1`
- Verify: `E:\AAALTZK\tests\verify_plugin_table_text_suppression_lifecycle.ps1`
- Verify: `E:\AAALTZK\tests\verify_table_embedded_controls_all_themes.ps1`

- [ ] **Step 1: 运行下拉框和表格嵌入控件回归检查**

Run each command separately:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_drawing_layout_combo_display_preserved.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_drawing_layout_combo_text_preserved.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_plugin_table_text_suppression_lifecycle.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_table_embedded_controls_all_themes.ps1
```

Expected: 每个脚本 exit code 0，无未处理异常；若某个脚本本身依赖不存在的历史文件，记录该环境限制并继续完成可执行检查。

- [ ] **Step 2: 检查工程构建入口**

确认 `E:\AAALTZK\LTZK.sln` 和 `E:\AAALTZK\LTZK.vcxproj` 存在；若 `msbuild` 可用，运行：

```powershell
msbuild .\LTZK.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m
```

Expected: exit code 0；若构建工具、Qt 或 QGIS 依赖不可用，保留完整错误输出，不把源码回归通过误报为构建通过。

- [ ] **Step 3: 复核改动范围**

检查 `LTZK.cpp` 与回归脚本的差异，确认没有修改 `Drawing_Layout` 插件、三套主题 QSS、CSV 过滤规则或单元格背景文字抑制逻辑。

### Task 4: 记录交付状态

- [ ] **Step 1: 记录 Git 环境限制**

尝试 `git status --short`；如果仍返回 `not a git repository`，说明当前工程没有可用 Git 元数据，因此不执行 commit，并在交付说明中明确记录。

- [ ] **Step 2: 交付修改文件和验证结果**

交付 `E:\AAALTZK\LTZK.cpp`、回归脚本及设计/计划文档的绝对路径，并只报告实际运行命令的结果。
