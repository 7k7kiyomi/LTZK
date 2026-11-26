# Local Product Platform Adapter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make local-data “按范围导出” and “按制图导出” execute in the platform without sending a local URI to the database-only product-manager plugin, while keeping database product exports unchanged.

**Architecture:** Keep `DataCatalogWidget`’s existing local URI callback as the boundary between the catalog UI and the host. Replace the callback’s QVariant/plugin dispatch with a platform-owned `LocalProductOperation` adapter that opens a small Qt dialog, clips local vector/raster sources with QGIS APIs, writes Shapefile/GeoPackage/FileGDB-compatible outputs, and optionally loads successful outputs into the current project. The existing database callback, `ltzkProductCatalogRequest` fields, C ABI dispatch, and `map_product_manager_tools` source remain untouched.

**Tech Stack:** C++17, Qt 5 Widgets, QGIS 3.28 core/gui APIs, PowerShell source-contract regression, MSBuild Release x64.

---

### Task 1: Add the failing platform-boundary regression

**Files:**
- Create: `tests/verify_local_product_platform_adapter.ps1`

- [x] **Step 1: Write the failing test**

The test must require a platform adapter call from the local callback, reject the old plugin-slot log, require vector/raster clipping APIs in the new adapter, require the new files in `LTZK.vcxproj`, and reject local-source bridge tokens inside `map_product_manager_tools`.

- [x] **Step 2: Run the test and verify it fails**

Run:

```powershell
.\tests\verify_local_product_platform_adapter.ps1
```

Expected: FAIL because `LocalProductOperation.h/.cpp` and the platform adapter call do not exist yet.

### Task 2: Implement the platform local adapter

**Files:**
- Create: `LocalProductOperation.h`
- Create: `LocalProductOperation.cpp`
- Modify: `LTZK.vcxproj`
- Modify: `LTZK.vcxproj.filters`

- [x] **Step 1: Add a no-moc public boundary**

Declare:

```cpp
class LocalProductOperation final
{
public:
    static bool dispatch(const QString& operation,
                         const QgsMimeDataUtils::UriList& uris,
                         QgsMapCanvas* canvas,
                         QWidget* parent,
                         QString* errorMessage = nullptr);
};
```

- [x] **Step 2: Add the range dialog and local clipping implementation**

The adapter must:

1. Validate non-empty local vector/raster URIs.
2. Show a platform-owned “按范围导出” dialog with numeric extent fields, current-map-extent fill, output format/path, and auto-load checkbox.
3. Treat dialog coordinates as the map canvas CRS, or the first source CRS when no canvas CRS is available.
4. Transform the requested rectangle into each source CRS with `QgsCoordinateTransform::transformBoundingBox`.
5. Clip vector features with rectangle intersection, preserve fields/attributes/CRS, and write them through `QgsVectorFileWriter::writeAsVectorFormatV3`.
6. Crop raster data with `QgsRasterFileWriter::writeRaster` using the intersected source extent and source pixel size.
7. Use unique output names and never overwrite the source file.
8. Report per-source success/failure in a summary dialog and load successful outputs when requested.

- [x] **Step 3: Add the local main-area path**

For `main-area`, show a platform-owned dialog that chooses a local vector file, lists an attribute field and selectable features, unions the selected feature bounding boxes, and feeds that rectangle into the same local clipping writer. This keeps the operation local and does not require database directory/product IDs.

- [x] **Step 4: Make theme behavior explicit**

For `theme`, do not call the database plugin. Show a clear platform message that a local file cannot be written into a database theme without importing it first; return a handled result so the UI does not silently dispatch a meaningless request.

- [x] **Step 5: Run the regression**

Run:

```powershell
.\tests\verify_local_product_platform_adapter.ps1
```

Expected: PASS.

### Task 3: Route local catalog operations to the adapter

**Files:**
- Modify: `LTZK.cpp`

- [x] **Step 1: Include and wire the adapter**

Include `LocalProductOperation.h` and change the existing `setLocalProductOperationCallback` lambda to call `LocalProductOperation::dispatch(operation, localUris, m_mapCanvas, this, &errorMessage)`.

- [x] **Step 2: Remove the obsolete local plugin-slot route**

Delete the `localSources` branch in `executePluginFeatureAction` that calls `invokeProductCatalogAction` and logs “Local product catalog action dispatched through plugin slot.” Keep the database-only `buildProductCatalogApiRequest` branch and its `localSources.isEmpty()` guard unchanged.

- [x] **Step 3: Verify database isolation**

Confirm the database callback still creates `directoryId`, `productId`, `productIds`, `connection`, and dispatches `成果目录操作` exactly as before; only the local callback bypasses that path.

### Task 4: Build and verify

**Files:**
- Verify: `LTZK.sln`

- [x] **Step 1: Build Release x64**

Run:

```powershell
msbuild .\LTZK.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m:1
```

Expected: Build succeeds and the generated `LTZK.exe` contains the new platform adapter.

- [x] **Step 2: Run relevant regressions**

Run:

```powershell
.\tests\verify_local_product_platform_adapter.ps1
.\tests\verify_local_product_full_parity.ps1
.\tests\verify_local_product_dispatch_fallback.ps1
.\tests\verify_product_catalog_api.ps1
```

Expected: all scripts pass, and no file under `map_product_manager_tools` is modified.

- [x] **Step 3: Deploy only the platform executable**

Back up the installed `E:\LTZK\bin\LTZK.exe`, copy the verified platform executable, compare SHA-256 hashes, and leave `E:\LTZK\plugins\map_product_manager_tools.dll` unchanged. A full application restart is required before the new executable can be exercised.
