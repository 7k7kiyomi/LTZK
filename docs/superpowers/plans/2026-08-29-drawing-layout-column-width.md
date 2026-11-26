# 图层关系配置列宽与横向滚动修复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让质检任务管理的图层关系配置表在首次加载时显示 gdb 图层、实际图层和实际注记图层的已选内容，并在窗口不足时通过横向滚动访问完整表格。

**Architecture:** 在宿主 `LTZK.cpp` 中增加一个幂等的 `treeWidget_csv` 列宽整理辅助函数，只识别六列的目标 `QTreeWidget`。该函数关闭最后一列强制拉伸、将列设为可交互调整、设置六列最小宽度并启用按需横向滚动；从现有的图层关系下拉框润色入口调用，保留插件数据和已有嵌入控件处理。

**Tech Stack:** C++、Qt `QTreeWidget`/`QHeaderView`/`QScrollBar`、PowerShell 源码级回归检查、MSBuild/Visual Studio（若环境可用）。

---

### Task 1: 建立列宽行为的失败回归检查

**Files:**
- Create: `E:\AAALTZK\tests\verify_drawing_layout_column_widths.ps1`

- [ ] **Step 1: 编写源码级回归脚本**

脚本读取 `LTZK.cpp`，截取 `polishDrawingLayoutTreeEmbeddedCombos` 前的匿名辅助函数区域与该函数体，要求源码包含目标表识别、六列宽度、交互式表头和横向滚动策略：

```powershell
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path $sourcePath)) { throw "Missing required file: $sourcePath" }

$source = Get-Content -Raw -Encoding UTF8 $sourcePath

$polishStart = $source.IndexOf('void polishDrawingLayoutTreeEmbeddedCombos(')
if ($polishStart -lt 0) { throw 'Could not locate drawing-layout combo polish helper.' }

$helperBody = $source.Substring(0, $polishStart)
$polishEnd = $source.IndexOf('void suppressPluginTableEmbeddedWidgetTexts(', $polishStart)
if ($polishEnd -lt 0) { throw 'Could not locate end of drawing-layout combo polish helper.' }
$polishBody = $source.Substring($polishStart, $polishEnd - $polishStart)

foreach ($required in @(
    'void ensureDrawingLayoutTreeColumnWidths(QWidget* widget)',
    'tree->objectName() != QStringLiteral("treeWidget_csv")',
    'tree->columnCount() != 6',
    'header->setStretchLastSection(false)',
    'header->setSectionResizeMode(column, QHeaderView::Interactive)',
    'tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded)',
    'const int minimumColumnWidths[] = { 105, 150, 180, 245, 120, 160 }',
    'qMax(tree->columnWidth(column), minimumColumnWidths[column])',
    'ensureDrawingLayoutTreeColumnWidths(widget)'
)) {
    if (-not ($helperBody.Contains($required) -or $polishBody.Contains($required))) {
        throw "Drawing-layout table column-width behavior missing: $required"
    }
}

Write-Host 'Drawing-layout table column widths and horizontal scrolling verified.'
```

- [ ] **Step 2: 运行失败基线**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_drawing_layout_column_widths.ps1
```

Expected: FAIL，错误应指出缺少 `ensureDrawingLayoutTreeColumnWidths(QWidget* widget)` 或其列宽行为；这证明检查能够捕获当前缺陷。

### Task 2: 增加目标表格列宽整理并接入加载流程

**Files:**
- Modify: `E:\AAALTZK\LTZK.cpp:243-250`，增加匿名辅助函数
- Modify: `E:\AAALTZK\LTZK.cpp:492-705`，在现有图层关系润色流程中调用辅助函数

- [ ] **Step 1: 添加幂等列宽整理辅助函数**

在 `isDrawingLayoutTreeEmbeddedColumn` 后加入以下函数：

```cpp
void ensureDrawingLayoutTreeColumnWidths(QWidget* widget)
{
    if (!widget)
    {
        return;
    }

    const QList<QTreeWidget*> trees = widget->findChildren<QTreeWidget*>();
    for (QTreeWidget* tree : trees)
    {
        if (!tree
            || tree->objectName() != QStringLiteral("treeWidget_csv")
            || tree->columnCount() != 6)
        {
            continue;
        }

        QHeaderView* header = tree->header();
        if (!header)
        {
            continue;
        }

        header->setStretchLastSection(false);
        for (int column = 0; column < tree->columnCount(); ++column)
        {
            header->setSectionResizeMode(column, QHeaderView::Interactive);
        }

        tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        const int minimumColumnWidths[] = { 105, 150, 180, 245, 120, 160 };
        for (int column = 0; column < tree->columnCount(); ++column)
        {
            tree->setColumnWidth(column, qMax(tree->columnWidth(column), minimumColumnWidths[column]));
        }

        tree->updateGeometry();
        if (tree->viewport())
        {
            tree->viewport()->update();
        }
    }
}
```

- [ ] **Step 2: 在图层关系下拉框润色入口调用**

在 `polishDrawingLayoutTreeEmbeddedCombos` 完成组合框遍历后、现有 `const QList<QAbstractItemView*> views = ...` 之前加入：

```cpp
        ensureDrawingLayoutTreeColumnWidths(widget);
```

保持该调用在 `onlyCombo` 提前返回之前，使首次窗口润色和后续新增组合框润色都能整理目标表格；不修改已有组合框文字恢复、箭头、代理或单元格文字抑制逻辑。

- [ ] **Step 3: 检查幂等性和目标边界**

确认 `qMax` 保留用户已拖出的更宽列，函数只作用于对象名为 `treeWidget_csv` 且正好六列的表格；确认 `setHorizontalScrollBarPolicy` 使用 `Qt::ScrollBarAsNeeded`，没有隐藏滚动条或修改 CSV 数据。

### Task 3: 验证回归、相关检查和构建

**Files:**
- Verify: `E:\AAALTZK\tests\verify_drawing_layout_column_widths.ps1`
- Verify: `E:\AAALTZK\tests\verify_drawing_layout_combo_display_preserved.ps1`
- Verify: `E:\AAALTZK\tests\verify_drawing_layout_combo_text_preserved.ps1`
- Verify: `E:\AAALTZK\tests\verify_plugin_table_embedded_control_layout.ps1`
- Verify: `E:\AAALTZK\tests\verify_plugin_table_text_suppression_lifecycle.ps1`
- Verify: `E:\AAALTZK\tests\verify_quality_window_groupbox_margin_override.ps1`

- [ ] **Step 1: 运行列宽回归检查并确认通过**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_drawing_layout_column_widths.ps1
```

Expected: exit code 0，并输出 `Drawing-layout table column widths and horizontal scrolling verified.`。

- [ ] **Step 2: 运行已有相关回归检查**

逐个运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_drawing_layout_combo_display_preserved.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_drawing_layout_combo_text_preserved.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_plugin_table_embedded_control_layout.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_plugin_table_text_suppression_lifecycle.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_quality_window_groupbox_margin_override.ps1
```

Expected: 每个脚本 exit code 0；若某个脚本因现有工程状态失败，记录实际错误并判断是否与本次改动有关。

- [ ] **Step 3: 执行 x64 Release 构建（若工具链可用）**

先检查 `msbuild` 是否可用；可用时运行：

```powershell
msbuild .\LTZK.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m
```

Expected: exit code 0。若 Qt/QGIS 或 MSBuild 依赖缺失，只报告完整错误，不将源码检查通过表述为构建通过。

- [ ] **Step 4: 复核最终差异和部署影响**

确认只修改宿主源码和回归脚本，不修改 `plugin_map_quality_check_tools.dll`、CSV 文件、主题 QSS 或现有的下拉框文字处理逻辑；记录当前目录没有 Git 元数据，无法提交。
