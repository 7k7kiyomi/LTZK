# 数据库与本地数据勾选批量操作 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让数据库数据和本地可加载数据可以通过复选框组成右键操作集合，并让方案 A 的批量/单项操作语义在五个主题下稳定可用。

**Architecture:** 保留现有 `QStandardItemModel` 数据库树和 QGIS 本地浏览树，在数据库项上直接启用 `Qt::CheckStateRole`，在本地树外包一层轻量 `QgsBrowserProxyModel` 扩展并以数据路径保存勾选状态。右键菜单根据当前项是否勾选生成操作目标；批量操作使用目标集合，单项操作继续使用当前上下文。

**Tech Stack:** C++17、Qt 5 item model/view、QGIS 3.28 `QgsBrowserProxyModel`/`QgsDataItem`、Qt Style Sheets、PowerShell 静态验证脚本、MSBuild Release x64。

---

### Task 1: Add red tests for checkable catalog and operation-target semantics

**Files:**
- Create: `E:\AAALTZK\tests\verify_data_catalog_checked_operations.ps1`
- Test: `E:\AAALTZK\DataCatalogWidget.h`, `E:\AAALTZK\DataCatalogWidget.cpp`, `E:\AAALTZK\Resources\styles\ltzk_sci_fi.qss`, `ltzk_black.qss`, `ltzk_white.qss`, `ltzk_field_notes.qss`, `ltzk_steel_warm_white.qss`

- [ ] **Step 1: Write the failing static test**

  The script must read the catalog header, implementation, and all five theme files. It must fail until the following contracts exist:

  ```powershell
  $requiredSourceTokens = @(
      'm_checkedDatabaseKeys',
      'm_checkedLocalPaths',
      'Qt::ItemIsUserCheckable',
      'Qt::CheckStateRole',
      'checkedContextsFor',
      'addContextItemsToMap',
      'refreshContextItems',
      'batchExportContextItems'
  )
  foreach ($token in $requiredSourceTokens) {
      if (-not ($headerText.Contains($token) -or $sourceText.Contains($token))) {
          throw "Missing catalog checked-operation contract: $token"
      }
  }

  foreach ($theme in $themeFiles) {
      if ($themeText -notmatch 'QTreeView::indicator:checked') {
          throw "Theme is missing checked catalog indicator styling: $theme"
      }
      if ($themeText -notmatch 'QTreeView::indicator:unchecked') {
          throw "Theme is missing unchecked catalog indicator styling: $theme"
      }
  }
  ```

- [ ] **Step 2: Run the test and verify the failure is expected**

  Run:

  ```powershell
  .\tests\verify_data_catalog_checked_operations.ps1
  ```

  Expected: `FAIL`/non-zero because the new checked-state members and target helpers do not exist yet. Fix the test itself if it errors due to a path or parsing typo; do not add production code before observing this expected failure.

### Task 2: Add model-level checked state for database and local catalog rows

**Files:**
- Modify: `E:\AAALTZK\DataCatalogWidget.h`
- Modify: `E:\AAALTZK\DataCatalogWidget.cpp`

- [ ] **Step 1: Extend the widget state and helper declarations**

  Add `QSet<QString>` members for database identity keys and local `QgsDataItem::path()` keys. Add helpers with these exact responsibilities:

  ```cpp
  QString databaseCheckKey(const QStandardItem* item) const;
  bool isCheckableContext(const CatalogContext& context) const;
  bool isCheckedContext(const CatalogContext& context) const;
  QVector<CatalogContext> checkedContextsFor(const CatalogContext& context) const;
  QVector<CatalogContext> checkedDatabaseContexts() const;
  QVector<CatalogContext> checkedLocalContexts() const;
  void collectCheckedDatabaseContexts(
      const QStandardItem* item,
      QVector<CatalogContext>* contexts) const;
  ```

  Add batch wrappers without changing the existing single-context methods:

  ```cpp
  void addContextItemsToMap(const QVector<CatalogContext>& contexts);
  void refreshContextItems(const QVector<CatalogContext>& contexts);
  void batchExportContextItems(const QVector<CatalogContext>& contexts);
  QgsMimeDataUtils::UriList exportUrisForContexts(
      const QVector<CatalogContext>& contexts,
      int* unloadedCount) const;
  ```

- [ ] **Step 2: Make spatial database items checkable and restore their state**

  In `refreshDatabaseData()`, after assigning the database roles to a spatial table item, add `Qt::ItemIsUserCheckable` to its flags and set its state from `m_checkedDatabaseKeys`. Keep ordinary database tables disabled and non-checkable. Connect `m_databaseModel->itemChanged` once in `buildUi()` so checked and unchecked spatial items update `m_checkedDatabaseKeys` using `databaseCheckKey()`.

  The key must include database name, schema, table, layer type, and geometry column, matching the existing display-alias identity dimensions so a refresh does not confuse duplicate geometry entries.

- [ ] **Step 3: Add a local browser proxy that exposes check states**

  Define a non-`Q_OBJECT` subclass in the anonymous namespace so no additional Qt moc entry is needed:

  ```cpp
  class CatalogBrowserProxyModel final : public QgsBrowserProxyModel
  {
  public:
      explicit CatalogBrowserProxyModel(QObject* parent = nullptr);
      void setCheckedPaths(QSet<QString>* paths);
      Qt::ItemFlags flags(const QModelIndex& index) const override;
      QVariant data(const QModelIndex& index, int role) const override;
      bool setData(const QModelIndex& index,
                   const QVariant& value,
                   int role = Qt::EditRole) override;
  };
  ```

  Only local items with at least one supported vector/raster URI receive `Qt::ItemIsUserCheckable`. `data()` reads `m_checkedLocalPaths` by `QgsDataItem::path()`, and `setData()` inserts/removes that path and emits `dataChanged()` for `Qt::CheckStateRole`. Instantiate this subclass in `initializeLocalBrowser()` while retaining the `QgsBrowserProxyModel*` member type and all existing source/proxy mapping.

- [ ] **Step 4: Run the static test to verify the model contracts pass**

  Run:

  ```powershell
  .\tests\verify_data_catalog_checked_operations.ps1
  ```

  Expected: the model-related assertions pass; assertions for right-click target helpers and theme indicators remain failing until Tasks 3 and 4 are complete.

### Task 3: Route right-click operations according to方案 A

**Files:**
- Modify: `E:\AAALTZK\DataCatalogWidget.h`
- Modify: `E:\AAALTZK\DataCatalogWidget.cpp`

- [ ] **Step 1: Implement checked-context collection**

  `checkedContextsFor(context)` must return one current context when the current row is not checkable or not checked. When it is checked, it must return all checked contexts from the same tree only. Database contexts are collected by recursively walking `m_databaseModel` and reading each item's `checkState()`. Local contexts are collected by recursively walking the current proxy-tree root and reading each visible item's `Qt::CheckStateRole`; this avoids losing checked rows when `findPath()` cannot resolve a lazily loaded path.

- [ ] **Step 2: Add deduplicating batch map loading and refresh**

  Implement `addContextItemsToMap()` by collecting supported database/local URIs from all contexts, calling `appendUniqueUri()`, and invoking `m_loadCallback` once per unique URI. Implement `refreshContextItems()` so multiple database contexts trigger one `refreshDatabaseData()` call; local contexts refresh each valid source index and schedule the existing `showFavoritesInTree()` update.

- [ ] **Step 3: Add multi-context batch export**

  Implement `exportUrisForContexts()` to collect leaf URIs from checked contexts and recursively collect URIs for a collection context, deduplicating them and preserving the existing unloaded-count reporting. `batchExportContextItems()` must open one directory dialog, export all collected URIs with the existing unique output-name logic, and show one success/failure summary. Preserve `batchExportContextItem()` for the existing single collection path by delegating to the new collector where practical.

- [ ] **Step 4: Wire the menu actions with correct scope**

  In `populateContextMenu()` capture `const QVector<CatalogContext> operationContexts = checkedContextsFor(context);` and use it only for:

  ```cpp
  connect(addToMap, &QAction::triggered, this,
      [this, operationContexts]() { addContextItemsToMap(operationContexts); });
  connect(refresh, &QAction::triggered, this,
      [this, operationContexts]() { refreshContextItems(operationContexts); });
  connect(batchExport, &QAction::triggered, this,
      [this, context, operationContexts]() {
          if (isCatalogCollection(context)) {
              batchExportContextItem(context);
          } else {
              batchExportContextItems(operationContexts);
          }
      });
  ```

  Keep `importData`, `dataClip`, `info`, `exportData`, and `rename` connected to the current `context`. Enable `batchExport` for a collection or when the checked target list contains at least one exportable layer. An unchecked right-click must therefore keep every action single-item scoped.

- [ ] **Step 5: Run the static test and inspect the diff**

  Run:

  ```powershell
  .\tests\verify_data_catalog_checked_operations.ps1
  git diff --check
  ```

  Expected: the checked-operation assertions pass and `git diff --check` reports no whitespace errors. The repository is currently not a Git worktree, so if `git diff --check` reports “not a repository”, record that limitation and continue with direct file inspection.

### Task 4: Match catalog checkbox rendering across all five themes

**Files:**
- Modify: `E:\AAALTZK\Resources\styles\ltzk_sci_fi.qss`
- Modify: `E:\AAALTZK\Resources\styles\ltzk_black.qss`
- Modify: `E:\AAALTZK\Resources\styles\ltzk_white.qss`
- Modify: `E:\AAALTZK\Resources\styles\ltzk_field_notes.qss`
- Modify: `E:\AAALTZK\Resources\styles\ltzk_steel_warm_white.qss`

- [ ] **Step 1: Add theme-specific tree indicator rules**

  Immediately after each theme’s existing `QCheckBox::indicator` rules, add the corresponding `QTreeView` rules. For example, the sci-fi theme must use the same visible colors as its checkbox controls:

  ```css
  QTreeView::indicator { width: 16px; height: 16px; border: 1px solid #1bc9ff; background: rgba(5, 24, 43, 220); }
  QTreeView::indicator:checked { background: #ffb238; border: 1px solid #ffcf72; }
  QTreeView::indicator:unchecked { background: rgba(5, 24, 43, 220); border: 1px solid #1bc9ff; }
  ```

  Use each of the five existing theme color pairs rather than a single global color. Do not alter the previously added branch-arrow rules.

- [ ] **Step 2: Run the theme assertions**

  Run:

  ```powershell
  .\tests\verify_data_catalog_checked_operations.ps1
  .\tests\verify_layer_tree_branch_arrows_all_themes.ps1
  .\tests\verify_workspace_visual_polish.ps1
  ```

  Expected: all three scripts pass.

### Task 5: Compile and perform integration-level verification

**Files:**
- Modify: `E:\AAALTZK\LTZK.qrc` only if new checkbox assets are needed after visual inspection; otherwise leave the resource file unchanged.
- Modify: `E:\AAALTZK\DataCatalogWidget.cpp`, `DataCatalogWidget.h`, and five QSS files from previous tasks.

- [ ] **Step 1: Build Release x64 with an isolated output directory**

  Run the build with output redirected to a writable verification directory, preserving the project’s existing configuration and Qt/QGIS environment:

  ```powershell
  $verificationRoot = 'C:\Users\孟子腾\.codex\visualizations\2026\09\09\01a0849a-4faf-7532-b4b7-9bde400db65a\catalog-checks'
  New-Item -ItemType Directory -Force "$verificationRoot\build\release" | Out-Null
  New-Item -ItemType Directory -Force "$verificationRoot\build\intermediate" | Out-Null
  msbuild .\LTZK.vcxproj /m /t:Build /p:Configuration=Release /p:Platform=x64 /p:OutDir="$verificationRoot\build\release\" /p:IntDir="$verificationRoot\build\intermediate\"
  ```

  Expected: `LTZK.vcxproj` builds with zero errors. Existing warnings may remain, but no new error may be introduced.

- [ ] **Step 2: Run all targeted validation scripts again**

  Run:

  ```powershell
  .\tests\verify_data_catalog_checked_operations.ps1
  .\tests\verify_quality_result_panel_autoload.ps1
  .\tests\verify_layer_tree_branch_arrows_all_themes.ps1
  .\tests\verify_workspace_visual_polish.ps1
  ```

  Expected: all scripts pass.

- [ ] **Step 3: Perform a focused manual smoke check when the executable is available**

  Open the built program and verify:

  1. A database spatial row and several local rows show visible checkboxes.
  2. Checking multiple rows does not load or hide map layers.
  3. Right-clicking a checked row and choosing “添加到地图” adds every checked row in that tree once.
  4. Right-clicking an unchecked row affects only that row.
  5. “基本信息”“重命名”“数据裁剪”“导出数据” remain current-row operations.
  6. All five themes show checked and unchecked catalog indicators with sufficient contrast.

- [ ] **Step 4: Update the plan checkboxes and report verification evidence**

  Record the exact build command/result, test-script outputs, and any limitation preventing manual UI verification. Do not claim the feature is complete without the build and static tests passing.

## Execution record

- Static checked-operation verification: passed.
- Regression fix: batch target collection now reads the current database/local model check state directly; no longer depends on local `findPath()` reverse lookup or only the auxiliary checked-path/key sets.
- Existing quality-result-panel auto-load verification: passed.
- Five-theme branch-arrow verification: passed.
- Workspace visual-polish verification: passed.
- Release x64 build: passed with 0 errors; existing unrelated `FeatureAttribute.cpp/.h` encoding and unused-variable warnings remain.
- Build artifact: `C:\Users\孟子腾\.codex\visualizations\2026\09\09\01a0849a-4faf-7532-b4b7-9bde400db65a\catalog-checks\build\checked-ops\bin\LTZK.exe`.
- Default output build was blocked by a running `LTZK` process holding `x64\\Release\\LTZK.tlog\\LTZK.lastbuildstate`; the isolated-output build completed successfully.
- Integration limitation: no Git repository was detected at `E:\AAALTZK`, so no commit or branch integration was performed.
