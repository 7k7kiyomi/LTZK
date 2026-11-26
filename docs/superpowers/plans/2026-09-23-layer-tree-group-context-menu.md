# 图层管理组右键操作 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让图层管理组节点支持右键重命名、删除，并支持多选顶层组/节点的一次性移除，同时不触碰源数据。

**Architecture:** 复用现有 `QgsLayerTreeView`、`QgsLayerTreeGroup` 和 `LTZK::removeLayer()`。右键菜单先根据点击节点同步选择状态，再由 `selectedNodes(true)` 得到去重后的顶层选择；组重命名走输入框和 `setName()`，移除组走父节点 `removeChildNode()`。

**Tech Stack:** C++, Qt 5.15, QGIS 3.28 layer tree API, PowerShell static regression checks, MSBuild.

---

### Task 1: Add the failing group-menu contract test

**Files:**
- Create: `E:\AAALTZK\tests\verify_layer_tree_group_context_menu.ps1`

- [ ] **Step 1: Write the failing test**

  检查 `LTZK.cpp`/`LTZK.h` 必须包含 `ExtendedSelection`、`selectedNodes(true)`、组右键菜单、组重命名 helper、`removeChildNode`；同时拒绝在组功能实现附近出现 `QFile::remove`、`QDir::removeRecursively`、`DROP TABLE` 等源数据删除调用。

- [ ] **Step 2: Run the test to verify it fails**

  Run:

  ```powershell
  pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_layer_tree_group_context_menu.ps1
  ```

  Expected: FAIL because the current layer tree context menu exits when no current layer exists and `removeLayer()` only handles one current node.

### Task 2: Implement group selection and context-menu entry

**Files:**
- Modify: `E:\AAALTZK\LTZK.cpp`
- Modify: `E:\AAALTZK\LTZK.h`

- [ ] **Step 1: Enable extended selection**

  In `LTZK::initLayerTreeView()`, call:

  ```cpp
  m_layerTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  ```

- [ ] **Step 2: Preserve multi-selection on right-click**

  For the clicked index, keep the current selection if it is already selected; otherwise clear the selection and select only that row. Resolve the clicked `QgsLayerTreeNode` before checking `currentLayer()` so groups can open a menu.

- [ ] **Step 3: Add the group menu**

  Add `重命名` for a single selected group and `删除组` for one or more selected top-level groups. Keep the existing layer actions for layer nodes and connect removal to `removeLayer()`.

### Task 3: Implement group rename and multi-node removal

**Files:**
- Modify: `E:\AAALTZK\LTZK.cpp`
- Modify: `E:\AAALTZK\LTZK.h`

- [ ] **Step 1: Implement group rename**

  Add `renameLayerTreeGroup(QgsLayerTreeGroup*)`, use `QInputDialog::getText()`, reject empty/newline names, and call `groupNode->setName(newName)`.

- [ ] **Step 2: Extend `removeLayer()` to selected top-level nodes**

  Use `m_layerTreeView->selectedNodes(true)`. For each selected group remember nested quality-result folder paths, then remove it from its parent with `removeChildNode()`. For each selected layer call the existing `QgsProject::removeMapLayer()` path and remove unresolved tree-only nodes through their parent. Update `m_curMapLayer`, refresh the canvas, and log all removed names.

- [ ] **Step 3: Prevent automatic result-group recreation**

  Add a session set of suppressed quality-result folder paths. `ensureQualityResultLayerGroup()` returns null for a suppressed folder or descendant; clear the set whenever a new result-monitoring session starts.

- [ ] **Step 4: Run the contract test**

  Run the same PowerShell test from Task 1.

  Expected: PASS with `Layer tree group context-menu contract passed.`

### Task 4: Verify integration and build

**Files:**
- Test: `E:\AAALTZK\tests\verify_layer_tree_group_context_menu.ps1`
- Existing checks: `E:\AAALTZK\tests\verify_workspace_data_catalog.ps1`, `E:\AAALTZK\tests\verify_data_catalog_product_tree.ps1`

- [ ] **Step 1: Run all relevant static checks**

  ```powershell
  pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_layer_tree_group_context_menu.ps1
  pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_workspace_data_catalog.ps1
  pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_data_catalog_product_tree.ps1
  ```

- [ ] **Step 2: Build Release**

  ```powershell
  & 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\LTZK.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m /v:minimal
  ```

  Expected: `LTZK.vcxproj -> E:\AAALTZK\x64\Release\LTZK.exe` and exit code 0.
