# 图层树复选框批量删除 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让图层树右键删除覆盖整棵树内所有已勾选节点，并安全处理半选组、显式多选和父子重复目标。

**Architecture:** 在现有 `LTZKLayerTreeModel` 中提供组复选框三态；在 `LTZK.cpp` 匿名命名空间中把“收集目标”和“执行删除”分开。目标收集合并可见性勾选、视图选择和右键上下文节点，再以祖先节点过滤为顶层删除集合；执行阶段显示确认框并批量处理组与图层。

**Tech Stack:** C++17, Qt 5, QGIS 3.28 layer-tree API, PowerShell 静态回归检查, MSBuild。

---

### Task 1: Add the failing checked-delete regression contract

**Files:**
- Create: `E:\AAALTZK\tests\verify_layer_tree_checked_delete.ps1`

- [x] **Step 1: Write the failing test**

  检查 `LTZK.cpp` 中必须出现三态组状态、递归扫描根节点、复选框状态读取、显式节点合并、祖先去重、确认框和分阶段删除；当前代码缺少这些实现，因此测试应失败。

- [x] **Step 2: Run the test to verify it fails**

  Run:

  ```powershell
  pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_layer_tree_checked_delete.ps1
  ```

  Expected: FAIL with a message identifying the first missing checked-delete contract.

### Task 2: Implement model state and target collection

**Files:**
- Modify: `E:\AAALTZK\LTZK.cpp`

- [x] **Step 1: Add the model's three-state data override**

  Override `LTZKLayerTreeModel::data()` for group `Qt::CheckStateRole`. Return `Qt::Checked` when `isItemVisibilityCheckedRecursive()` is true, `Qt::Unchecked` when `isItemVisibilityUncheckedRecursive()` is true, and `Qt::PartiallyChecked` otherwise. Delegate all other roles and non-group nodes to `QgsLayerTreeModel::data()`.

- [x] **Step 2: Add recursive checked-node collection**

  Traverse every child of `QgsProject::instance()->layerTreeRoot()`. Use the model's `Qt::CheckStateRole`: a fully checked group is a single target, a partial group is traversed, and checked layer nodes are targets. Do not add the root node.

- [x] **Step 3: Add target merging and ancestor filtering**

  Merge checked nodes, `selectedNodes(true)`, and the context node. Deduplicate pointers and remove every candidate whose ancestor is also a candidate, so the final list contains only safe top-level targets.

- [x] **Step 4: Run the test and confirm the target contracts now pass**

  Run the Task 1 PowerShell command. Expected: it still fails only for deletion/confirmation contracts that have not been implemented yet.

### Task 3: Implement confirmation, menu snapshot, and safe deletion

**Files:**
- Modify: `E:\AAALTZK\LTZK.cpp`

- [x] **Step 1: Add a deletion summary and confirmation dialog**

  Count group and layer targets, show the total and a clear statement that only the current project tree changes. Use `QMessageBox::question()` with No as the default; return without mutation when the user cancels.

- [x] **Step 2: Refactor `removeLayer()` into collection plus execution**

  Keep the toolbar slot collecting checked and selected nodes. Add a helper that executes a supplied, already normalized target snapshot. Before mutation, record names, group suppression paths, valid layer IDs, and unresolved layer nodes.

- [x] **Step 3: Update both right-click branches**

  Build one target snapshot when the context menu is created. Add a delete action whose label includes the target count, enable it only when the snapshot is non-empty, and capture the snapshot in a lambda. Group and layer context menus must use the same action semantics.

- [x] **Step 4: Remove nodes in a safe order**

  Remove top-level groups through their checked parent with `removeChildNode()`, then call `QgsProject::removeMapLayers()` once for deduplicated valid layer IDs, then remove unresolved tree-only layer nodes through their parents. Refresh the map canvas and log the snapshot names after mutation.

- [x] **Step 5: Run the checked-delete contract**

  Run:

  ```powershell
  pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_layer_tree_checked_delete.ps1
  ```

  Expected: `Layer tree checked-delete contract passed.`

### Task 4: Verify regression coverage and build

**Files:**
- Test: `E:\AAALTZK\tests\verify_layer_tree_checked_delete.ps1`
- Existing checks: `E:\AAALTZK\tests\verify_layer_tree_group_context_menu.ps1`, `E:\AAALTZK\tests\verify_layer_tree_node_cast_guards.ps1`, `E:\AAALTZK\tests\verify_layer_tree_group_visibility_guard.ps1`

- [x] **Step 1: Run all related static checks**

  ```powershell
  pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_layer_tree_checked_delete.ps1
  pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_layer_tree_group_context_menu.ps1
  pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_layer_tree_node_cast_guards.ps1
  pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_layer_tree_group_visibility_guard.ps1
  ```

  Expected: all four scripts exit 0.

- [x] **Step 2: Build Release x64**

  ```powershell
  & 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\LTZK.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m /v:minimal
  ```

  Expected: exit code 0 and a refreshed `E:\AAALTZK\x64\Release\LTZK.exe`.
