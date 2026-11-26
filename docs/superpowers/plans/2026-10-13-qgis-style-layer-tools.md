# QGIS Style Layer Tools Implementation Plan

> **For agentic workers:** Use this plan task-by-task. The user requested inline execution. Steps use checkbox syntax for tracking.

**Goal:** Add manageable QGIS-style floating map toolbars, layer-specific context actions, and layer panel toolbar features to the Qt/QGIS 3.28 application.

**Architecture:** Keep the current `LTZK` main window and `QgsLayerTreeView`. Host native movable `QToolBar` instances in a nested `QMainWindow` inside the map panel, above its canvas; use a map-panel menu button because the platform menu bar is hidden. Explicitly suspend the host and its floating bars outside the authenticated map page, and persist toolbar visibility preferences separately from the host's saved geometry. Use `QgsLayerTreeViewDefaultActions` and `QgsMapThemeCollection` for QGIS-style tree actions and themes, while keeping selected-node and checked-node removal scopes separate.

**Tech Stack:** C++17, Qt 5.15, QGIS 3.28.15, MSBuild.

---

### Task 1: Replace the canvas toolbox button with manageable map toolbars

**Files:**
- Modify: `LTZK.cpp`
- Modify: `LTZK.h`

- [x] Add named project/data, map navigation, query/measure, and selection `QToolBar` instances owned by the map-panel toolbar host.
- [x] Move existing project/data/navigation actions from the canvas popup into appropriate toolbars; preserve their signal behavior.
- [x] Add a persistent map-panel toolbar manager and toolbar context menu using each toolbar's `toggleViewAction()`.
- [x] Enable movable, floatable toolbars with stable object names and restore/save the map-host state through `QSettings`.
- [x] Remove the obsolete floating single-button overlay from the workspace canvas, including its resize path and member pointer.
- [x] Add working identify, layer/selection extent, previous/next canvas history when supported by existing APIs, and style-panel actions. Leave unsupported edit operations hidden.

### Task 2: Expand layer context menus while preserving safe removal scopes

**Files:**
- Modify: `LTZK.cpp`
- Modify: `LTZK.h`

- [x] Keep the existing right-click selection behavior for groups and multi-selection.
- [x] Own a `QgsLayerTreeViewDefaultActions` factory and reuse its QGIS 3.28 actions for zoom, overview, feature count, move, grouping, and mutually exclusive groups.
- [x] Add type-aware vector actions for duplicate, filter, export, and the existing attribute-table entry; show raster style/stretch/export actions only for raster nodes.
- [x] Keep the current group rename and quality-result removal safeguards.
- [x] Add explicit separate remove-selected and remove-checked actions; ensure multi-selection acts on deduplicated top-level nodes.

### Task 3: Add layer panel toolbar filters and map-theme controls

**Files:**
- Modify: `LTZK.cpp`
- Modify: `LTZK.h`

- [x] Retain create group, expand, collapse, and remove; add the layer styling toggle and layer-name filter widget.
- [x] Add a checkable map-content legend filter using the current map canvas settings and an expression-filter entry backed by QGIS legend filter settings.
- [x] Add visibility batch actions and map-theme create/apply/update/rename/remove actions backed by the project map theme collection.
- [x] Keep name filtering, legend filtering, and feature filtering distinct, with a clear action for each active filter.

### Task 4: Keep map toolbars inside the authenticated map workspace

**Files:**
- Modify: `LTZK.cpp`
- Modify: `LTZK.h`
- Modify: `WorkspaceLayerTools.cpp`

- [x] Replace the map panel's direct canvas child with a nested `QMainWindow` whose central widget is `m_mapCanvas`; place a compact toolbar manager row immediately above it.
- [x] Create all four toolbars with the nested host as their parent and add them to its top toolbar area, preserving native dragging and floating.
- [x] Put each toolbar's `toggleViewAction()` and a reset-layout action in the persistent map-panel manager button; remove the hidden main-menu-only toolbar entry.
- [x] Connect `m_workspaceStack::currentChanged` to a workspace-activity handler that requires both the map page and an active authenticated session, and explicitly hides every toolbar and floating window otherwise.
- [x] Store each toolbar's user visibility choice separately from the host's `saveState(2)` data; save the host layout before hiding it on page exit and when closing while the map page is active.
- [x] Restore the nested host state and visibility choices at startup, then force the host hidden until the authenticated map page is active. Default project/data, navigation, and query/measure bars to visible; keep feature selection hidden by default.

### Task 5: Build the application

**Files:**
- Build: `LTZK.vcxproj`

- [x] Build Release x64 with the Visual Studio 2022 MSBuild installation at `C:\Program Files\Microsoft Visual Studio\2022\Community\Msbuild\Current\Bin\MSBuild.exe`.
- [x] Resolve compile diagnostics from APIs unavailable in the installed QGIS 3.28.15 headers before reporting the result.
