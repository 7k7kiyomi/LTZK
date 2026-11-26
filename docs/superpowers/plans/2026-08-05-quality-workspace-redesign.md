# Quality Workspace Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign the second-page quality workspace so input shp layers are managed on the left, quality result shp outputs are managed in a hideable bottom result board, and all module/map actions live on one top row.

**Architecture:** Keep the existing Qt/QGIS desktop host and reuse `QgsLayerTreeView` for input layer management. Load quality result shp files into the QGIS registry without adding them to the visible layer tree, then let the result board toggle whether each result layer participates in the map canvas layer stack.

**Tech Stack:** C++/Qt Widgets, QGIS 3.28 APIs, PowerShell static verification scripts.

---

### Task 1: Lock Expected Workspace Structure

**Files:**
- Create: `tests/verify_quality_workspace_result_board_layout.ps1`
- Create: `tests/verify_quality_result_board_registry_layers.ps1`

- [ ] **Step 1: Write the failing layout verification**

Create a script that checks the workspace title, left input layer panel, bottom result board, one-row top actions, and absence of the old feature rail.

- [ ] **Step 2: Write the failing result-layer verification**

Create a script that checks quality result shp files are added with `QgsProject::instance()->addMapLayer(layer, false)`, are tracked in a map, and are shown/hidden from the result board rather than inserted into the visible layer tree.

- [ ] **Step 3: Run both scripts and confirm failure**

Run:

```powershell
.\tests\verify_quality_workspace_result_board_layout.ps1
.\tests\verify_quality_result_board_registry_layers.ps1
```

Expected: both fail against the current implementation because the old right result panel and old left feature rail are still present.

### Task 2: Add Result Board State

**Files:**
- Modify: `LTZK.h`
- Modify: `LTZK.cpp`

- [ ] **Step 1: Add members and helper declarations**

Add result-layer state for `m_qualityResultLayers`, visible result paths, and bottom-board widgets. Add helper declarations for reading result fields, toggling result visibility, refreshing map canvas overlays, and locating result features.

- [ ] **Step 2: Compile-check headers**

Run the relevant build or static verification after header changes.

### Task 3: Rebuild Workspace Layout

**Files:**
- Modify: `LTZK.cpp`

- [ ] **Step 1: Replace the old three-column body**

Change `buildSciFiWorkspacePage()` from left feature rail + map + right result panel into:

```text
top one-row bar
left input layer manager | right vertical splitter
                         | map canvas
                         | bottom result board
```

- [ ] **Step 2: Move actions to the top right**

Use icon-only `QToolButton`s for plugin actions, map operation actions, result board toggle, and return home. Keep the title on the left as `陇图智控平台——<模块名>子平台`.

- [ ] **Step 3: Keep the existing layer tree visible only in the left panel**

Move `m_layerTreeView` into the left panel titled `图层管理器`, and remove the old feature button container.

### Task 4: Load Results Into Bottom Board

**Files:**
- Modify: `LTZK.cpp`

- [ ] **Step 1: Change result shp loading**

In `syncQualityResultShpLayers()`, add result layers to the project registry with `addMapLayer(layer, false)` and track them in `m_qualityResultLayers` by canonical path.

- [ ] **Step 2: Populate result board**

In `refreshQualityResultPanel()`, create one top-level item per result shp with a checkbox for map visibility. Add child rows for feature details showing `id`, `错误类型`, and `定位`.

- [ ] **Step 3: Add result board interactions**

On top-level item check changes, update the visible-result path set and rebuild the map canvas layer stack. On child activation, locate and select the referenced feature.

### Task 5: Verify

**Files:**
- Run: `tests/verify_quality_workspace_result_board_layout.ps1`
- Run: `tests/verify_quality_result_board_registry_layers.ps1`
- Run: existing related workspace/result scripts

- [ ] **Step 1: Run targeted scripts**

Run:

```powershell
.\tests\verify_quality_workspace_result_board_layout.ps1
.\tests\verify_quality_result_board_registry_layers.ps1
.\tests\verify_layer_context_zoom_to_layer.ps1
```

- [ ] **Step 2: Build if the configured compiler is available**

Run the existing Visual Studio/MSBuild command for `LTZK.sln` and fix compile errors.

- [ ] **Step 3: Manual smoke check**

Launch the app, enter the map quality workspace, run a check, confirm input shp stays in the left manager and result shp appears only in the bottom board.
