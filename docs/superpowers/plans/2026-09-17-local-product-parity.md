# Local Product Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make local spatial files match database product nodes in icon, context-menu layout, and platform-side range/main-area/theme dispatch without changing database behavior or plugin source.

**Architecture:** Add a local-only callback beside the existing database callback and serialize local QGIS URIs through the existing hidden plugin action. Keep the database callback and C ABI path unchanged; do not edit `map_product_manager_tools`.

**Tech Stack:** C++17, Qt 5, QGIS 3 APIs, PowerShell regression checks, MSBuild.

---

### Task 1: Add a failing parity regression

**Files:**
- Create: `tests/verify_local_product_full_parity.ps1`

- [ ] Assert the local proxy overrides `Qt::DecorationRole` with database product/folder icons.
- [ ] Assert local files use the directory-style menu and selected URI fallback.
- [ ] Assert a local operation callback carries `range`, `main-area`, and `theme` requests.
- [ ] Assert the host serializes `localSources` and bypasses the database-only C ABI for those requests.
- [ ] Run the test and verify it fails because these contracts do not yet exist.

### Task 2: Align local icons and menu behavior

**Files:**
- Modify: `DataCatalogWidget.h`
- Modify: `DataCatalogWidget.cpp`

- [ ] Add `LocalProductOperationCallback` and setter.
- [ ] Cache and return database-style icons from `CatalogBrowserProxyModel::data()`.
- [ ] Treat local layer nodes as directory-layout product nodes while preserving direct URI fallback for an unchecked file.
- [ ] Enable and dispatch range/main-area/theme actions through the local callback.
- [ ] Run the parity regression.

### Task 3: Bridge local requests from the platform

**Files:**
- Modify: `LTZK.cpp`

- [ ] Serialize local URI fields into `localSources`.
- [ ] Keep database requests on the existing C ABI and route local requests through the QVariant action bridge.
- [ ] Run the parity regression.

### Task 4: Build, verify, and deploy

**Files:**
- Verify: `LTZK.sln`

- [ ] Build the platform Release target in a clean task-specific output directory.
- [ ] Run all relevant verification scripts and confirm zero failures.
- [ ] Back up the installed EXE/plugin DLL, deploy the verified binaries, and compare hashes.
- [ ] Report that a full application restart is required for the replaced platform executable.
