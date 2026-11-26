# Catalog Tree Icons Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace generic catalog tree icons with semantic icons for both database and local nodes.

**Architecture:** Store original SVGs in `Resources/catalog_icons/` and register them in `LTZK.qrc`. A helper in `DataCatalogWidget.cpp` maps directory names, file suffixes, and layer geometry to resource icons for both tree models.

**Tech Stack:** Qt 5, QGIS browser model, SVG, MSBuild.

---

### Task 1: Create vector assets

**Files:** Create `Resources/catalog_icons/*.svg`; modify `LTZK.qrc`.

- [x] Draw compact, distinct 24 × 24 icons for database, folder, three top level categories, their named children, file formats, and geometry types.
- [x] Register every new SVG in `LTZK.qrc` under `/LTZK`.
- [x] Parse every SVG as XML and assert every resource path exists.

### Task 2: Connect database tree

**Files:** Modify `DataCatalogWidget.cpp`.

- [x] Add one resource icon resolver and one semantic directory resolver for the known directory names.
- [x] Use the new icons for database root, fixed directory nodes, products, and legacy schema nodes. Preserve layer types and node type behavior.
- [x] Verify current product catalog static checks and build.

### Task 3: Connect local tree

**Files:** Modify `DataCatalogWidget.cpp`.

- [x] In `CatalogBrowserProxyModel::data(Qt::DecorationRole)`, map known directory names to the same semantic icons and unknown directories to the new folder icon.
- [x] Resolve local file suffix before QGIS layer geometry; retain the cached geometry result for other layers.
- [x] Verify local tree static checks, resource paths, and final build; visually inspect the rendered icon sheet.

No Git commit step is possible because `E:\AAALTZK` is not a Git repository.
