# 数据列表模块可见性 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (recommended) or superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 仅在“地图成果管理”和“地图智能综合”显示工作台左侧“数据列表”页签，在“地图成果质检”和“地图服务管理”隐藏该页签，同时保留“图层管理”页签和已有数据目录状态。

**Architecture:** 保持 `LTZK` 工作台中的单一 `QTabWidget` 与 `DataCatalogWidget` 实例；新增按业务模块标题判断可见性的 helper 和统一更新方法，在模块激活时调用 `QTabWidget::setTabVisible`。隐藏当前数据页签前切换到“图层管理”，避免当前页变成不可见页面。

**Tech Stack:** C++/Qt 5.15.3、现有 `LTZK.h`/`LTZK.cpp`、PowerShell 静态验证脚本、MSBuild x64 Release。

---

## Task 1: Add a failing regression test for module-specific tab visibility

**Files:** `tests/verify_workspace_data_catalog.ps1`

- [x] Add assertions that the source declares `updateDataCatalogVisibility`, defines an allow-list containing exactly “地图成果管理” and “地图智能综合”, calls `setTabVisible`, locates the data tab with `indexOf(m_dataCatalogWidget)`, and switches to the layer-management widget before hiding an active data tab.
- [x] Add assertions that the four module titles remain present so the test covers the requested module mapping.
- [x] Run `.\tests\verify_workspace_data_catalog.ps1` and confirm it fails because the production visibility helper/call do not exist yet.

## Task 2: Implement the visibility policy in `LTZK`

**Files:** `LTZK.h`, `LTZK.cpp`

- [x] Declare `void updateDataCatalogVisibility(const QString& moduleTitle);` in the private `LTZK` interface near the workspace/module activation methods.
- [x] Add `isDataCatalogVisibleModuleTitle(const QString&)` in the anonymous namespace; return `true` only for “地图成果管理” and “地图智能综合”.
- [x] Implement `LTZK::updateDataCatalogVisibility` with null/index guards, active-data-tab fallback to `m_layerTreePanelWidget` or `m_layerTreeView`, and `setTabVisible(dataCatalogTabIndex, visible)`.
- [x] Set the side-tab position to `QTabWidget::South` for visible modules and `QTabWidget::North` for hidden modules so “图层管理” is at the top in quality/service modules.
- [x] Call the method after `m_currentBusinessModuleTitle = moduleTitle` in `activateBusinessModule` so all module entry paths use one policy.
- [x] Apply the initial empty-title policy after the workspace tabs are created so the data tab is hidden until an allowed module is active.

## Task 3: Verify and package the Release change

**Files:** `x64/Release/LTZK.exe`, `x64/Release/LTZK_catalog_visibility_fix.exe`

- [x] Re-run the regression script and the relevant Release/deployment environment checks.
- [x] Build `LTZK.vcxproj` for `x64|Release` with isolated temporary `OutDir`/`IntDir` values.
- [x] Copy the verified binary to a uniquely named Release artifact and update the default `x64/Release/LTZK.exe` after preserving the previous binary as a backup when needed.
- [x] Start the default Release executable with the QGIS runtime environment, verify it remains alive during the startup smoke interval, then stop only that smoke-test process.
- [x] Record the final binary hash/size and mark all plan checkboxes complete.

## Acceptance criteria

- “地图成果管理”和“地图智能综合”进入工作台后显示“数据列表”。
- “地图成果质检”和“地图服务管理”进入工作台后隐藏“数据列表”，仍显示“图层管理”。
- 质检和服务管理中“图层管理”页签栏位于容器顶部，允许显示数据列表的模块保持原底部页签栏。
- 在数据列表页签上已有的数据库连接、本地文件夹连接和目录内容不会因模块切换被清空或销毁。
- Static regression, x64 Release build, and startup smoke verification pass.
