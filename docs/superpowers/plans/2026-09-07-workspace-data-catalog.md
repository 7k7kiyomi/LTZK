# Workspace Data Catalog Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a bottom-tabbed workspace data catalog which browses the connected PostGIS database and user-connected local folders, and loads selected sources into the existing QGIS project.

**Architecture:** Keep the existing layer tree as the single “图层管理” page, and add a focused `DataCatalogWidget` for database and local sources. The database tree queries metadata through the already initialized named Qt SQL connection, while the local tree uses `QgsBrowserGuiModel`; both produce `QgsMimeDataUtils::Uri` values consumed by one loader in `LTZK`.

**Tech Stack:** C++17, Qt 5 Widgets/SQL item models, QGIS 3.28 browser and layer APIs, PowerShell regression checks, MSBuild.

---

### Task 1: Lock the sidebar and catalog contract with a failing regression check

**Files:**
- Create: `tests/verify_workspace_data_catalog.ps1`

- [ ] **Step 1: Write the failing source-level regression test**

Require `DataCatalogWidget.h/.cpp`, a south-positioned `QTabWidget` with “数据列表” and “图层管理”, a `QgsBrowserGuiModel`, folder add/remove/refresh actions, database metadata filtering, a unified URI loader, and project-file entries for the new source files.

- [ ] **Step 2: Run the check and verify failure**

Run: `powershell -ExecutionPolicy Bypass -File tests\verify_workspace_data_catalog.ps1`

Expected: FAIL because `DataCatalogWidget` and the bottom workspace tabs do not exist.

### Task 2: Add the focused data catalog widget

**Files:**
- Create: `DataCatalogWidget.h`
- Create: `DataCatalogWidget.cpp`
- Modify: `LTZK.vcxproj`
- Modify: `LTZK.vcxproj.filters`

- [ ] **Step 1: Define `DataCatalogWidget`**

Expose `QgsBrowserGuiModel *browserModel() const`, `void refreshDatabaseData()`, and a `std::function<void(const QgsMimeDataUtils::Uri &)>` load callback. Keep all owned tree views, item models, tool buttons, and browser objects private.

- [ ] **Step 2: Build the database section**

Use a `QStandardItemModel` with database, Schema, and table nodes. Call `LTZKAuthManager::ensureConnectedAndInitialized`, reuse the `LTZKAuthPostgisConnection` connection, and query `information_schema.tables` plus `geometry_columns`. Store the provider URI only on loadable leaf nodes and show an inline error node on failure.

- [ ] **Step 3: Build the local section**

Initialize `QgsBrowserGuiModel`, find its `QgsFavoritesItem` root, and show that root through `QgsBrowserTreeView`. “连接文件夹” calls `addFavoriteDirectory`, “断开连接” calls `removeFavorite` only for selected favorite roots, and “刷新” refreshes the favorites root.

- [ ] **Step 4: Emit unified sources**

On database-tree double click, construct a Postgres vector URI. On local-tree double click, read `QgsDataItem::mimeUris()` and pass vector/raster URIs to the callback.

- [ ] **Step 5: Add the files to both Visual Studio item groups**

Add `DataCatalogWidget.cpp` to `ClCompile`, `DataCatalogWidget.h` to `ClInclude`, and corresponding Source/Header filters. The widget does not declare Qt signals, so it must not require a separate `QtMoc` item.

- [ ] **Step 6: Run the focused check**

Expected: the component portion passes while workspace integration assertions still fail.

### Task 3: Integrate bottom tabs and unified layer loading

**Files:**
- Modify: `LTZK.h`
- Modify: `LTZK.cpp`
- Modify: `LTZKQgisInterface.cpp`

- [ ] **Step 1: Add integration members and methods**

Add `DataCatalogWidget *m_dataCatalogWidget`, `QTabWidget *m_workspaceSideTabs`, `QgsBrowserGuiModel *browserModel() const`, and `void loadCatalogDataSource(const QgsMimeDataUtils::Uri &uri)`.

- [ ] **Step 2: Replace the single titled layer panel**

Create an untitled dashboard frame containing a `QTabWidget` with `setTabPosition(QTabWidget::South)`. Add the catalog first and the existing `m_layerTreePanelWidget` second; set the current tab to “图层管理”. Preserve the existing splitter sizing and collapse behavior.

- [ ] **Step 3: Implement one layer loader**

Use `QgsMimeDataUtils::Uri::vectorLayer` or `rasterLayer`, reject invalid layers with the existing message bar/status bar, compare provider and source against current project layers to prevent duplicates, then add the new layer to `QgsProject` and select it in the existing layer tree.

- [ ] **Step 4: Refresh database metadata after login**

Call `m_dataCatalogWidget->refreshDatabaseData()` immediately after `gCurrentUserSession = session`. Do not attempt database catalog loading before authentication succeeds.

- [ ] **Step 5: Expose the browser model to plugins**

Return `mApp ? mApp->browserModel() : nullptr` from `LTZKQgisInterface::browserModel()`.

- [ ] **Step 6: Run the focused regression check**

Expected: PASS with `Workspace data catalog integration verified.`

### Task 4: Add scoped theme styling

**Files:**
- Modify: `Resources/styles/ltzk_sci_fi.qss`
- Modify: `Resources/styles/ltzk_black.qss`
- Modify: `Resources/styles/ltzk_white.qss`
- Modify: `Resources/styles/ltzk_field_notes.qss`
- Modify: `Resources/styles/ltzk_steel_warm_white.qss`
- Modify: `tests/verify_workspace_data_catalog.ps1`

- [ ] **Step 1: Scope styles to the new object names**

Style `#workspaceSideTabs`, `#databaseCatalogTree`, `#localCatalogTree`, and catalog toolbar buttons without changing global `QTabWidget` or `QTreeView` behavior.

- [ ] **Step 2: Extend the check across every platform theme**

Verify every shipped theme contains the catalog selectors and a selected-tab state.

- [ ] **Step 3: Run the focused check**

Expected: PASS.

### Task 5: Verify regressions and build

**Files:**
- Verify: `tests/*.ps1`
- Build: `LTZK.sln`

- [ ] **Step 1: Run focused workspace, layer-tree, login and attribute-table checks**

Run the new catalog check together with `verify_workspace_layer_tree_reparent_safe.ps1`, `verify_workspace_resizable_panels.ps1`, `verify_login_permissions_postgis.ps1`, `verify_layer_tree_bridge_no_invalid_slots.ps1`, and `verify_attribute_table_loads_layer.ps1`.

Expected: every script exits with code 0.

- [ ] **Step 2: Build Release**

Run: `msbuild LTZK.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m`

Expected: exit code 0 with no compile or link errors.

- [ ] **Step 3: Inspect the final changes**

Confirm the existing layer tree is still created once, the catalog does not expose `user_permissions`, local folders are persisted by QGIS favorites, and no unrelated source areas changed. This workspace has no Git metadata, so report changed files directly instead of committing.

