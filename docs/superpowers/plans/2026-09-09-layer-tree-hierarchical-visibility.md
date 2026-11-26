# 图层管理父子勾选联动 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让图层管理中的父组勾选状态递归控制所有子组/子图层，并让子节点状态反映到父组，同时保持地图画布刷新逻辑不变。

**Architecture:** 复用 QGIS `QgsLayerTreeModel` 已有的 `ActionHierarchical` 行为，并由应用侧 `LTZKLayerTreeModel::setData()` 对组节点做一次有条件的递归兜底。基类先处理标准 QGIS 语义，只有当复选框操作目标是组且子树状态仍不一致时才调用 `setItemVisibilityCheckedRecursive()`；子图层单独操作不会进入组节点分支。现有 `dataChanged` 回调继续调用 `refreshMapCanvasLayerStack()`，负责把树状态同步到地图。

**Tech Stack:** C++17、Qt 5、QGIS 3.28、PowerShell 静态回归脚本、Visual Studio/MSBuild。

---

### Task 1: Add a failing regression check for hierarchical layer-tree visibility

**Files:**
- Create: `E:\AAALTZK\tests\verify_layer_tree_hierarchical_visibility.ps1`
- Reference: `E:\AAALTZK\LTZK.cpp:8880-8905`

- [ ] **Step 1: Write the failing test**

Create a PowerShell check that reads only the `LTZK::initLayerTreeView()` body, extracts the model-flag section before `setAutoCollapseLegendNodes`, and requires both flags:

```powershell
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing source file: $sourcePath"
}

$source = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8
$initStart = $source.IndexOf('void LTZK::initLayerTreeView()')
if ($initStart -lt 0) {
    throw 'Could not locate LTZK::initLayerTreeView().' 
}

$flagStart = $source.IndexOf('model->setFlag(', $initStart)
$flagEnd = $source.IndexOf('model->setAutoCollapseLegendNodes', $flagStart)
if ($flagStart -lt 0 -or $flagEnd -le $flagStart) {
    throw 'Could not locate the QgsLayerTreeModel flag configuration.'
}

$flagText = $source.Substring($flagStart, $flagEnd - $flagStart)
foreach ($requiredFlag in @(
        'QgsLayerTreeModel::AllowNodeChangeVisibility',
        'QgsLayerTreeModel::ActionHierarchical')) {
    if (-not $flagText.Contains($requiredFlag)) {
        throw "Layer-tree model must enable $requiredFlag."
    }
}

Write-Host 'Hierarchical layer-tree visibility configuration verified.'
```

- [ ] **Step 2: Run the test to verify it fails**

Run from `E:\AAALTZK`:

```powershell
.\tests\verify_layer_tree_hierarchical_visibility.ps1
```

Expected result before the production change: exit code 1 with `Layer-tree model must enable QgsLayerTreeModel::ActionHierarchical.`

### Task 2: Enable QGIS hierarchical visibility actions and guard the group click path

**Files:**
- Modify: `E:\AAALTZK\LTZK.cpp` (layer-tree model helper and `initLayerTreeView()`)
- Test: `E:\AAALTZK\tests\verify_layer_tree_hierarchical_visibility.ps1`
- Test: `E:\AAALTZK\tests\verify_layer_tree_group_visibility_guard.ps1`

- [ ] **Step 1: Add the model flag and group guard**

Immediately after the existing `AllowNodeChangeVisibility` flag in `LTZK::initLayerTreeView()`, add:

```cpp
    model->setFlag(
        QgsLayerTreeModel::ActionHierarchical);
```

Create `LTZKLayerTreeModel` as a local model subclass and construct it in `initLayerTreeView()`. Its `setData()` override must call the base implementation first, identify only group nodes for `Qt::CheckStateRole`, and conditionally call `setItemVisibilityCheckedRecursive()` when the descendant state is not yet consistent. Do not add `dataChanged` recursion, new state members, or changes to result-panel code.

- [ ] **Step 2: Run the regression check**

Run:

```powershell
.\tests\verify_layer_tree_hierarchical_visibility.ps1
.\tests\verify_layer_tree_group_visibility_guard.ps1
```

Expected result: exit code 0 and `Hierarchical layer-tree visibility configuration verified.`

### Task 3: Verify source compatibility and existing result behavior

**Files:**
- Test: `E:\AAALTZK\tests\verify_layer_tree_hierarchical_visibility.ps1`
- Existing checks: `E:\AAALTZK\tests\verify_quality_result_dual_display.ps1`, `E:\AAALTZK\tests\verify_quality_result_board_registry_layers.ps1`, `E:\AAALTZK\tests\verify_quality_result_layers_hidden_on_module_switch.ps1`

- [ ] **Step 1: Run the focused and related regression checks**

Run:

```powershell
.\tests\verify_layer_tree_hierarchical_visibility.ps1
.\tests\verify_quality_result_dual_display.ps1
.\tests\verify_quality_result_board_registry_layers.ps1
.\tests\verify_quality_result_layers_hidden_on_module_switch.ps1
```

Expected result: all four scripts exit with code 0. The checks must continue to find the existing result SHP registration, result-panel display, and module-switch hiding behavior.

- [ ] **Step 2: Run a syntax-only compile for the changed translation unit**

Use the configured MSVC toolchain and existing project include paths through the normal project build rather than changing source-specific compiler options. The required source-level verification is that `LTZK.cpp` compiles without a C++ error after the new enum flag is added.

### Task 4: Build an isolated Release artifact

**Files:**
- Input: `E:\AAALTZK\LTZK.vcxproj`
- Output: `C:\Users\孟子腾\.codex\visualizations\2026\09\09\01a0849a-4faf-7532-b4b7-9bde400db65a\ltzk-layer-tree-build\out\LTZK.exe`

- [ ] **Step 1: Build without touching the running application output**

Run from `E:\AAALTZK`:

```powershell
$buildRoot = 'C:\Users\孟子腾\.codex\visualizations\2026\09\09\01a0849a-4faf-7532-b4b7-9bde400db65a\ltzk-layer-tree-build'
New-Item -ItemType Directory -Force -Path $buildRoot, (Join-Path $buildRoot 'int'), (Join-Path $buildRoot 'out') | Out-Null
$env:TMP = $buildRoot
$env:TEMP = $buildRoot
& 'E:\VS 2019\MSBuild\Current\Bin\MSBuild.exe' 'LTZK.vcxproj' /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:IntDir="$buildRoot\int\" /p:OutDir="$buildRoot\out\" /m /v:minimal
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
```

Expected result: MSBuild exits with code 0 and produces `LTZK.exe` in the isolated output directory. Existing encoding warnings may remain, but no new C++ errors or C1041 errors are acceptable.

- [ ] **Step 2: Confirm the artifact exists**

Run:

```powershell
Get-Item 'C:\Users\孟子腾\.codex\visualizations\2026\09\09\01a0849a-4faf-7532-b4b7-9bde400db65a\ltzk-layer-tree-build\out\LTZK.exe'
```

Expected result: the executable exists and the current `E:\AAALTZK\x64\Release\LTZK.exe` remains untouched.

### Task 5: Handoff

- [ ] Report the exact source/test files changed, the focused test result, and the isolated build result.
- [ ] Tell the user to reload the project and rebuild after stopping any current build task; do not terminate the user's running application or Visual Studio process automatically.

The workspace is not a Git repository, so no commit step is performed.
