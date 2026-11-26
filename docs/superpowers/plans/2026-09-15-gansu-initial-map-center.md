# 默认甘肃地图初始居中 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让默认甘肃底图按实际显示范围居中进入页面，同时保持现有缩放与地图交互。

**Architecture:** `setGansuInitialMapView()` 继续使用现有固定初始范围控制缩放，只把中心点改为系统底图图层范围转换到 EPSG:3857 后的中心；没有有效系统底图时回退到现有固定中心。新增静态测试锁定范围转换、中心计算和调用顺序。

**Tech Stack:** Qt/QGIS C++、PowerShell 静态验证脚本、Visual Studio 工程。

---

### Task 1: 建立初始地图居中失败检查

**Files:**
- Create: `tests/verify_gansu_initial_map_center.ps1`

- [ ] **Step 1: 写入静态回归检查**

脚本读取 `LTZK.cpp`，检查以下契约：

```powershell
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path -LiteralPath $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding Default $source
$start = $text.IndexOf('void LTZK::setGansuInitialMapView()')
$end = $text.IndexOf('void LTZK::setDefaultPanMapTool()', $start)
if ($start -lt 0 -or $end -lt 0) {
    throw 'Could not locate the Gansu initial map view function.'
}
$body = $text.Substring($start, $end - $start)

foreach ($required in @(
    'QgsPointXY gansuCenter = gansuWebMercatorCenter();',
    'const QgsRectangle basemapExtent =',
    'm_mapCanvas->mapSettings().layerExtentToOutputExtent(layer, layer->extent())',
    'gansuCenter = basemapExtent.center();',
    'm_mapCanvas->setExtent(gansuExtent);',
    'm_mapCanvas->setCenter(gansuCenter);'
)) {
    if (-not $body.Contains($required)) {
        throw "Gansu initial map center contract is missing: $required"
    }
}

$extentIndex = $body.IndexOf('m_mapCanvas->setExtent(gansuExtent);')
$centerIndex = $body.IndexOf('m_mapCanvas->setCenter(gansuCenter);')
if ($extentIndex -lt 0 -or $centerIndex -lt 0 -or $extentIndex -gt $centerIndex) {
    throw 'Gansu initial view must set the existing extent before applying its center.'
}

Write-Host 'Gansu initial map center verification passed.'
```

- [ ] **Step 2: 运行检查确认旧代码失败**

Run:

```powershell
.\tests\verify_gansu_initial_map_center.ps1
```

Expected: FAIL because the current function has no `basemapExtent`-based center calculation.

### Task 2: 按实际甘肃底图范围设置初始中心

**Files:**
- Modify: `LTZK.cpp:8381-8401`

- [ ] **Step 1: 添加系统底图范围中心计算**

在 `setGansuInitialMapView()` 中保留固定中心初始化，并在设置画布范围前加入：

```cpp
    QgsPointXY gansuCenter = gansuWebMercatorCenter();
    for (QgsMapLayer* layer : QgsProject::instance()->mapLayers())
    {
        if (!layer || !isSystemBasemapLayer(layer) || !layer->isValid())
        {
            continue;
        }

        const QgsRectangle basemapExtent =
            m_mapCanvas->mapSettings().layerExtentToOutputExtent(layer, layer->extent());
        if (basemapExtent.isEmpty() || !basemapExtent.isFinite())
        {
            continue;
        }

        gansuCenter = basemapExtent.center();
        break;
    }

    m_constrainingMapView = true;
    m_mapCanvas->setExtent(gansuExtent);
    m_mapCanvas->setCenter(gansuCenter);
```

The existing `m_mapCanvas->setCenter(gansuCenter);` is replaced by `initialCenter`; all remaining refresh and initial-view persistence code stays unchanged.

- [ ] **Step 2: 运行新回归检查确认通过**

Run:

```powershell
.\tests\verify_gansu_initial_map_center.ps1
```

Expected: `Gansu initial map center verification passed.` and exit code 0.

- [ ] **Step 3: 运行相关既有静态验证**

Run each command separately:

```powershell
.\tests\verify_local_gansu_json_basemap.ps1
```

```powershell
.\tests\verify_local_gansu_render_stability.ps1
```

```powershell
.\tests\verify_theme_aware_startup_workspace.ps1
```

Expected: each script exits 0. If an existing script fails on a pre-existing unrelated assertion, record the exact failure without changing unrelated behavior.

- [ ] **Step 4: 运行构建能力检查**

Run:

```powershell
msbuild .\LTZK.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m /nologo
```

Expected: build exit code 0; if `msbuild` is unavailable, report that the environment lacks the build command and rely on the static checks above.
