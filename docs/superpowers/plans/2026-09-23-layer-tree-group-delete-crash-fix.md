# Layer Tree Group Delete Crash Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate the access violation caused by treating layer-tree group nodes as layer nodes during context-menu handling and removal.

**Architecture:** Keep the existing layer-tree operations, but make every downcast conditional on the node's declared QGIS type. Let the QGIS model own selection updates during removal and retain the existing distinction between removing a tree group and removing a project layer.

**Tech Stack:** C++17, Qt 5.15, QGIS 3.28, PowerShell contract tests, MSBuild.

---

### Task 1: Add cast-safety regression coverage

**Files:**
- Modify: `tests/verify_layer_tree_group_context_menu.ps1`
- Create: `tests/verify_layer_tree_node_cast_guards.ps1`

- [ ] **Step 1: Write the failing test**

Require `removeLayer()` and the context-menu handler to use `isLayer()` / `isGroup()` before downcasting, reject direct pointer-condition casts, and reject the previous `selectionModel->clear()` workaround.

- [ ] **Step 2: Run tests to verify they fail**

Run:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_layer_tree_group_context_menu.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_layer_tree_node_cast_guards.ps1
```

Expected: failure identifying unguarded `toLayer()` / `toGroup()` calls.

### Task 2: Guard every affected node downcast

**Files:**
- Modify: `LTZK.cpp:5694`
- Modify: `LTZK.cpp:5960`
- Modify: `LTZK.cpp:9466`
- Modify: `LTZK.cpp:9679`

- [ ] **Step 1: Implement the minimal production fix**

Use this pattern at every type-dispatch site:

```cpp
if (QgsLayerTree::isLayer(node))
{
    QgsLayerTreeLayer* layerNode = QgsLayerTree::toLayer(node);
}
else if (QgsLayerTree::isGroup(node))
{
    QgsLayerTreeGroup* groupNode = QgsLayerTree::toGroup(node);
}
```

In recursive group traversal, skip non-group children before calling `toGroup()`. In the context menu, initialize each typed pointer using a guarded ternary expression. Remove the pre-delete `selectionModel()->clear()` call.

- [ ] **Step 2: Run focused tests to verify they pass**

Run the two commands from Task 1. Expected: PASS.

### Task 3: Build and regression verification

**Files:**
- Verify: `LTZK.vcxproj`

- [ ] **Step 1: Build Release x64**

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\LTZK.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m /v:minimal
```

Expected: exit code 0 and `x64\Release\LTZK.exe` produced.

- [ ] **Step 2: Run related regression checks**

Run the group context-menu, node-cast, group visibility, hierarchical visibility, layer-tree bridge, workspace data-catalog, and product-tree scripts. Expected: all exit with code 0.

## Self-review

- The plan covers every unsafe type-dispatch occurrence found in the deletion path and matching quality-result traversals.
- No placeholders or unrelated refactoring are included.
- Runtime UI reproduction remains a manual verification because the repository has no automated native-GUI harness for this interaction.

