# Data Catalog Batch Add To Map Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make “添加到地图” honor all checked local/database catalog items instead of only the item under the mouse.

**Architecture:** Keep the existing callbacks and dispatch method unchanged. In `populateProductContextMenu`, derive one map-operation context list with `checkedContextsFor(context)`, aggregate database product IDs or local URIs, then pass those aggregates through the existing `dispatchProductOperation` call.

**Tech Stack:** C++/Qt, `QList<int>`, `QgsMimeDataUtils::UriList`, PowerShell static verification.

---

## Task 1: Test the batch map contract first

**Files:**
- Modify: `tests/verify_data_catalog_checked_operations.ps1`

- [x] **Step 1: Add assertions for global map context aggregation, database ID aggregation, local URI aggregation, deduplication, and dispatch.**

- [x] **Step 2: Run `.\tests\verify_data_catalog_checked_operations.ps1` and observe the expected failure on the missing aggregation contract.**

## Task 2: Aggregate the selected map inputs

**Files:**
- Modify: `DataCatalogWidget.cpp` in `populateProductContextMenu()`.

- [x] **Step 1: Build the map operation inputs after the existing current-context data values.**

Add `mapContexts`, `mapProductIds`, and `mapLocalUris`. When `context` is checked, iterate all checked contexts; use `productIdsForContext(mapContext)` for database contexts and `localProductUrisForContext(mapContext, true)` plus `appendUniqueUri()` for local contexts. When `context` is not checked, preserve current directory/all-URI behavior.

- [x] **Step 2: Use the aggregated inputs for both directory and non-directory “添加到地图” actions.**

Enable the action from the aggregated list and call:

```cpp
dispatchProductOperation(
    QStringLiteral("map"),
    context,
    mapProductIds,
    mapLocalUris);
```

Do not change the existing product-operation callbacks or local loader.

## Task 3: Verify the fix and regressions

**Files:**
- Inspect: `DataCatalogWidget.cpp`, `tests/verify_data_catalog_checked_operations.ps1`.

- [x] **Step 1: Run the focused checked-operations verification and confirm it passes.**

- [x] **Step 2: Run the related data-catalog static checks:**

```powershell
.\tests\verify_workspace_data_catalog.ps1
.\tests\verify_data_catalog_product_tree.ps1
.\tests\verify_data_catalog_manual_connection.ps1
.\tests\verify_data_catalog_range_export_auth.ps1
.\tests\verify_local_catalog_layer_leaf.ps1
```

- [x] **Step 3: Scan the final source to confirm there is one global checked-context path and no current-item-only map dispatch remains in `populateProductContextMenu()`.**
