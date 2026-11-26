# Workspace table, status, and icon implementation plan

**Goal:** Implement the three approved desktop GIS improvements.

**Architecture:** Track attribute dialogs by layer ID using QPointer. Add a map-local status frame using QgsScaleWidget and canvas CRS coordinates. Resolve leaf icons independently from GDB container identity.

**Tech stack:** C++17, Qt 5, QGIS 3.28, MSBuild, offscreen integration tests.

## Task 1: Runtime regression coverage

- [x] Add tests/workspace_table_status_icons_test.cpp and tests/run_workspace_table_status_icons_test.cmd using the established QGIS host-linking pattern.
- [x] Run the tests against current objects; require failures for duplicate same-layer windows, absent status widgets, and incorrectly classified GDB leaves.

## Task 2: Attribute dialog reuse

Files: LTZK.h, WorkspaceLayerTools.cpp.

- [x] Add `QMap<QString, QPointer<QDialog>> m_attributeTableDialogs`.
- [x] In openAttributeTable, validate the layer, reuse the dialog found by layer ID, restore/raise/activate, and create only if absent.
- [x] Expose an internal filter setter on WorkspaceAttributeTableDialog. Apply a supplied nonempty expression to an existing dialog; empty reopen preserves the current filter.
- [x] Remove registry entries on dialog destruction without retaining a raw layer pointer. Preserve existing close/edit prompts and layer-destruction handling.

## Task 3: Canvas status row

Files: LTZK.h, LTZK.cpp, WorkspaceLayerTools.cpp.

- [x] Declare initWorkspaceMapStatusBar and coordinate/CRS update helpers. Add parent-owned coordinate/CRS widgets and QgsScaleWidget member.
- [x] Call initialization after mapLayout adds m_mapCanvasToolBarHost. Append the status frame to that same layout.
- [x] Connect canvas mouse, CRS, and scale signals. Initialize scale from canvas; connect the widget's scaleChanged to zoomScale.
- [x] Format X/Y using the canvas CRS units. Reset stale coordinates on CRS change. Transform the existing geographic readout from canvas CRS to EPSG:4326 using project transform context with exception handling.

## Task 4: GDB leaf icons

File: DataCatalogWidget.cpp.

- [x] Resolve raster/geometry/table leaf types before source-container format. Fall back to real file format icons for ordinary document/image products.
- [x] Reuse the same leaf resolver for node_type=1 directory records. Exclude these from later collection-icon overwrite.
- [x] Retain collection icons for actual GDB containers and source-directory names; do not propagate source identity to unrelated ancestor directories.

## Task 5: Build and verification

- [x] Build Release to workspace_table_status_icons_build with isolated output/intermediate directories and existing QtMsBuild configuration.
- [x] Run tests/run_workspace_table_status_icons_test.cmd with the new host object path; require all test groups to pass.
- [x] Run existing attribute-action and coordinate-label checks plus related catalog checks. Inspect any failures for obsolete assertions before changing them.
- [x] Review modified source and capture an offscreen status-bar preview; report exact verification and output path.

## Verification results (2026-10-26)

- Baseline: 11 reproduced runtime failures before implementation.
- Final: 41 runtime checks passed, 0 failures; includes unsaved-edit close cancellation, immediate reopen before deferred deletion, typed GDB leaves, ordinary folders, and category-like layer names.
- Release build succeeded; final incremental build reported 0 warnings and 0 errors.
- Five existing checks passed: attribute actions, coordinate labels, product directory tree, local layer leaves, database product resolution. Run PowerShell scripts in PowerShell 7 to parse their existing UTF-8 correctly.
- Review confirmed both discovered icon edge cases fixed.
- Preview: workspace_table_status_icons_build/map_status.png.
- Launcher: workspace_table_status_icons_build/bin/start_LTZK.bat.
