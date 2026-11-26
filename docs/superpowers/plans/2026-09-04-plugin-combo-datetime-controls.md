# 插件下拉框与日期时间控件修复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 限制宿主程序的嵌入式下拉框润色范围，并为日期时间控件补齐主题箭头样式。

**Architecture:** 在 `LTZK.cpp` 中增加基于祖先 `QTreeWidget#treeWidget_csv` 的范围判断，使现有嵌入式控件处理只作用于图层关系树；在五套平台 QSS 中为 `QDateEdit`/`QDateTimeEdit` 声明主题箭头资源。通过 PowerShell 源码级回归脚本锁定范围和样式规则。

**Tech Stack:** C++17、Qt 5 Widgets/QSS、PowerShell、MSBuild（若可用）。

---

### Task 1: 建立失败回归检查

**Files:**
- Create: `E:/AAALTZK/tests/verify_plugin_combo_datetime_controls.ps1`

- [ ] **Step 1: 编写检查目标**

检查 `LTZK.cpp` 的组合框润色逻辑必须包含 `treeWidget_csv` 祖先范围判断；检查五套主题 QSS 都必须同时声明 `QDateEdit::down-arrow` 和 `QDateTimeEdit::down-arrow`，并使用匹配的箭头 SVG。

- [ ] **Step 2: 运行当前代码并确认失败**

运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_plugin_combo_datetime_controls.ps1
```

预期：失败，因为当前组合框润色函数没有 `treeWidget_csv` 范围判断，日期控件也没有 `::down-arrow` 规则。

### Task 2: 限制嵌入式组合框润色范围

**Files:**
- Modify: `E:/AAALTZK/LTZK.cpp` 的 `polishDrawingLayoutTreeEmbeddedCombos()` 辅助函数

- [ ] **Step 1: 增加祖先树识别**

新增只读判断逻辑，沿 `combo->parentWidget()` 向上查找 `QTreeWidget`，仅当对象名为 `treeWidget_csv` 时返回真。

- [ ] **Step 2: 在组合框润色入口过滤普通插件组合框**

在 `polishCombo` lambda 中，执行 `setEditable(true)` 前跳过不属于 `treeWidget_csv` 的组合框；保留现有图层关系树组合框的自绘箭头、字体、调色板和只读内部编辑框逻辑。

- [ ] **Step 3: 运行回归检查**

重新运行 `verify_plugin_combo_datetime_controls.ps1`，此时仅日期箭头断言仍失败，范围断言应通过。

### Task 3: 补齐五套主题日期箭头

**Files:**
- Modify: `E:/AAALTZK/Resources/styles/ltzk_sci_fi.qss`
- Modify: `E:/AAALTZK/Resources/styles/ltzk_black.qss`
- Modify: `E:/AAALTZK/Resources/styles/ltzk_white.qss`
- Modify: `E:/AAALTZK/Resources/styles/ltzk_field_notes.qss`
- Modify: `E:/AAALTZK/Resources/styles/ltzk_steel_warm_white.qss`

- [ ] **Step 1: 添加深色主题箭头规则**

在 sci-fi 和 black 主题的日期下拉区域规则后添加：

```css
QDateEdit::down-arrow, QDateTimeEdit::down-arrow {
    image: url(:/LTZK/Resources/combo_arrow_dark.svg);
    width: 14px;
    height: 14px;
}
```

- [ ] **Step 2: 添加浅色主题箭头规则**

在 white、field-notes 和 steel-warm-white 主题添加相同选择器，并使用 `combo_arrow_light.svg`。

- [ ] **Step 3: 运行回归检查**

运行 `verify_plugin_combo_datetime_controls.ps1`，预期输出成功信息并返回退出码 0。

### Task 4: 复核并构建

**Files:**
- Verify: `E:/AAALTZK/LTZK.cpp`
- Verify: `E:/AAALTZK/Resources/styles/*.qss`
- Verify: `E:/AAALTZK/tests/verify_plugin_combo_datetime_controls.ps1`

- [ ] **Step 1: 运行相关源码回归检查**

运行新增检查及既有嵌入式控件检查，确认没有破坏自绘组合框约定。

- [ ] **Step 2: 检查工程构建入口并尝试 Release x64 构建**

若 `msbuild`、Qt 和 QGIS 依赖可用，运行：

```powershell
msbuild .\LTZK.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m
```

若环境依赖不可用，记录实际错误，不将源码检查结果表述为构建通过。

- [ ] **Step 3: 复核改动范围**

确认只修改宿主组合框范围判断、五套主题日期箭头规则及对应回归脚本，没有改动插件业务数据处理。
