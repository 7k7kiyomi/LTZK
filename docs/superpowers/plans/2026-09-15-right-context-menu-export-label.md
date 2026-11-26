# 右键菜单“按制图导出”显示名称 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将成果目录相关右键菜单中的“按主区裁切导出”显示为“按制图导出”，同时保持原有按主区裁切导出行为不变。

**Architecture:** 只修改四处 `QMenu::addAction()` 的用户可见字符串；动作变量、信号连接、`main-area` 分发标识和导出实现全部保持不变。现有 PowerShell 静态验证脚本增加菜单标签数量及旧标签菜单定义检查，覆盖平台目录菜单、平台产品菜单、插件目录菜单和插件产品菜单。

**Tech Stack:** Qt/C++、PowerShell 静态验证脚本、Visual Studio 工程。

---

### Task 1: 为新菜单标签建立失败回归检查

**Files:**
- Modify: `tests/verify_data_catalog_product_tree.ps1:70-80`，将右键菜单期望文字切换为新名称。
- Modify: `tests/verify_data_catalog_product_tree.ps1`，在读取源文件后增加四个菜单定义的精确检查。

- [x] **Step 1: 将现有菜单期望值改为新名称**

把 required 字符串数组中的：

```powershell
'按主区裁切导出',
```

改为：

```powershell
'按制图导出',
```

- [x] **Step 2: 增加菜单定义数量和行为标识检查**

在现有 `$pluginDialogText = ...` 读取完成后增加：

```powershell
$menuSourcesText = "$sourceText`n$pluginDialogText"
$newMenuLabelCount = [regex]::Matches(
    $menuSourcesText,
    'addAction\(QStringLiteral\("按制图导出"\)\)'
).Count
if ($newMenuLabelCount -ne 4) {
    throw "Expected four right-click menu labels named 按制图导出, found: $newMenuLabelCount"
}

$oldMenuLabelCount = [regex]::Matches(
    $menuSourcesText,
    'addAction\(QStringLiteral\("按主区裁切导出"\)\)'
).Count
if ($oldMenuLabelCount -ne 0) {
    throw "Legacy right-click menu label remains in $oldMenuLabelCount definition(s)."
}

if (-not $menuSourcesText.Contains('QStringLiteral("main-area")')) {
    throw 'Main-area export operation identifier must remain unchanged.'
}
```

- [x] **Step 3: Run the test before changing production code**

Run:

```powershell
.\tests\verify_data_catalog_product_tree.ps1
```

Expected: FAIL with `Expected four right-click menu labels named 按制图导出, found: 0` (the current source still defines the old menu label).

### Task 2: 修改四个右键菜单显示字符串

**Files:**
- Modify: `DataCatalogWidget.cpp:2557`，目录节点菜单显示文字。
- Modify: `DataCatalogWidget.cpp:2677`，产品节点菜单显示文字。
- Modify: `map_product_manager_tools/ui_class/product_storage_dialog.cpp:1215`，目录节点菜单显示文字。
- Modify: `map_product_manager_tools/ui_class/product_storage_dialog.cpp:1256`，产品节点菜单显示文字。

- [x] **Step 1: 替换四处菜单显示字符串**

仅将以下表达式中的字符串替换：

```cpp
menu.addAction(QStringLiteral("按主区裁切导出"))
```

或：

```cpp
menu.addAction(QStringLiteral("按主区裁切导出"));
```

替换为：

```cpp
menu.addAction(QStringLiteral("按制图导出"))
```

或对应带分号形式。不得修改相邻的 `mainAreaExport`、`actMainArea`、`main-area` 或 `onExportByMainAreaClip()`。

- [x] **Step 2: 运行回归检查**

Run:

```powershell
.\tests\verify_data_catalog_product_tree.ps1
```

Expected: `Data catalog product directory verification passed.`，退出码为 0。

- [x] **Step 3: 检查旧文字仅保留在非菜单业务语境**

Run:

```powershell
rg -n -S '按主区裁切导出|按制图导出|main-area|onExportByMainAreaClip' `
  '.\DataCatalogWidget.cpp' `
  '.\map_product_manager_tools\ui_class\product_storage_dialog.cpp' `
  '.\map_product_manager_tools\ui_class\main_area_clip.ui'
```

Expected: 四个 `addAction(QStringLiteral("按制图导出"))` 菜单定义存在；旧名称仍可出现在对话框标题、提示、注释或导出完成摘要中；`main-area` 和 `onExportByMainAreaClip()` 保持存在。

- [x] **Step 4: 查看最终差异，确认没有越界改动**

Run:

```powershell
git diff -- DataCatalogWidget.cpp map_product_manager_tools/ui_class/product_storage_dialog.cpp tests/verify_data_catalog_product_tree.ps1
```

Expected: 若当前目录仍不是 Git 仓库，则命令提示不是 Git repository；此时改用 `rg` 和文件内容检查确认仅有四处菜单字符串及验证脚本对应断言变化。不得执行提交、重置或覆盖用户已有改动。
