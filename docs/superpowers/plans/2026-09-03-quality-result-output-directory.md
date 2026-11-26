# 质检结果输出目录识别 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让结果面板按照质检插件当前选择的输出目录扫描 SHP，不再依赖“输出结果”等固定目录名。

**Architecture:** 质检动作开始后，主程序从可见质检插件窗口的目录型路径控件收集并保存用户选择的目录；后续扫描只遍历这些目录及子目录。扫描仍使用质检动作开始时间过滤旧文件，并把新生成的有效 SHP 作为 registry-only 图层交给现有结果面板。

**Tech Stack:** C++17、Qt Widgets、QGIS 3.28、PowerShell 静态回归脚本。

---

### Task 1: Add the failing regression check

**Files:**
- Create: `tests/verify_quality_result_scan_uses_selected_output_dirs.ps1`

- [x] **Step 1: Write the failing test**

Create a PowerShell check that extracts the result-directory collection block and requires the selected-directory state, live directory collection, and output-only scan boundary. It must reject directory-name heuristics, project/input-layer fallback roots, and filename-name heuristics:

```powershell
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$headerPath = Join-Path $root 'LTZK.h'
$source = Get-Content -LiteralPath $sourcePath -Raw
$header = Get-Content -LiteralPath $headerPath -Raw

foreach ($required in @(
    'm_qualityResultOutputDirs',
    'refreshQualityResultOutputDirectories',
    'QFileInfo(text).isDir()',
    'return m_qualityResultOutputDirs;'
)) {
    if (-not ($source.Contains($required) -or $header.Contains($required))) {
        throw "Selected quality output directory handling is missing: $required"
    }
}

$collectStart = $source.IndexOf('QStringList LTZK::collectQualityResultSearchRoots() const')
$collectEnd = $source.IndexOf('int LTZK::syncQualityResultShpLayers()', $collectStart)
if ($collectStart -lt 0 -or $collectEnd -lt 0) {
    throw 'Could not locate result search-root collection.'
}
$collectBody = $source.Substring($collectStart, $collectEnd - $collectStart)

foreach ($forbidden in @(
    'directoryNameLooksLikeResultRoot',
    'QgsProject::instance()->mapLayers()',
    'localPathFromLayerSource(',
    'sourceInfo.absolutePath()',
    'absoluteFilePath(QStringLiteral(".."))'
)) {
    if ($collectBody.Contains($forbidden)) {
        throw "Result scanning must not infer roots from fixed names or input layers: $forbidden"
    }
}

$qualityPathStart = $source.IndexOf('bool isQualityResultShpPath(const QString& shpPath)')
$qualityPathEnd = $source.IndexOf('constexpr int kMaxQualityResultFeatureRowsPerLayer', $qualityPathStart)
if ($qualityPathStart -lt 0 -or $qualityPathEnd -lt 0) {
    throw 'Could not locate result SHP path validation.'
}
$qualityPathBody = $source.Substring($qualityPathStart, $qualityPathEnd - $qualityPathStart)
foreach ($forbidden in @(
    'contains(QStringLiteral("error"))',
    'contains(QStringLiteral("err"))',
    'contains(QStringLiteral("错误"))',
    'contains(QStringLiteral("异常"))',
    'contains(QStringLiteral("质量结果"))',
    'contains(QStringLiteral("质检结果"))'
)) {
    if ($qualityPathBody.Contains($forbidden)) {
        throw "Result SHP validation must not depend on filename/path keywords: $forbidden"
    }
}

if ($source -notmatch 'refreshQualityResultOutputDirectories\(\);\s*\r?\n\s*const int loadedCount = syncQualityResultShpLayers') {
    throw 'The scan timer must refresh the user-selected output directories before scanning.'
}

Write-Host 'Quality result scan uses selected output directories without fixed-name heuristics.'
```

- [x] **Step 2: Run the test to verify it fails**

Run: `.	estserify_quality_result_scan_uses_selected_output_dirs.ps1`

Expected: FAIL because the current source has no `m_qualityResultOutputDirs` or `refreshQualityResultOutputDirectories`, uses `directoryNameLooksLikeResultRoot`, and falls back to input map-layer directories.

### Task 2: Store and refresh the selected output directories

**Files:**
- Modify: `LTZK.h:218-281`
- Modify: `LTZK.cpp:2080-2107,4564-4650`

- [x] **Step 1: Add the output-directory state and helper declaration**

Add `void refreshQualityResultOutputDirectories();` beside the existing result-monitoring helpers and add `QStringList m_qualityResultOutputDirs;` beside `m_qualityResultLayers`.

- [x] **Step 2: Remove fixed directory-name filtering**

Delete `directoryNameLooksLikeResultRoot()` and make `isQualityResultShpPath()` accept any existing `.shp` file. The output directory chosen by the user, rather than a path/name keyword, defines whether a file is a quality result.

- [x] **Step 3: Implement live selected-directory collection**

Implement `refreshQualityResultOutputDirectories()` by iterating visible top-level plugin windows, accepting only `QLineEdit` controls whose lower-cased object name contains `output`, reading their directory values, normalizing them with `appendUniqueDir()`, and updating `m_qualityResultOutputDirs`. Do not inspect `QgsProject::instance()->mapLayers()` or derive parent/child roots from input layers.

- [x] **Step 4: Make search roots return only recorded directories**

Change `collectQualityResultSearchRoots()` to return `m_qualityResultOutputDirs` and remove the project-file and input-layer fallback branch.

- [x] **Step 5: Run the failing test again**

Run: `.	estserify_quality_result_scan_uses_selected_output_dirs.ps1`

Expected: PASS, proving that arbitrary output directory names and output SHP filenames are accepted while input-layer fallback and fixed-name heuristics are gone.

### Task 3: Wire directory refresh into the quality scan lifecycle

**Files:**
- Modify: `LTZK.cpp:2522-2531,4564-4571,6908-6920`

- [x] **Step 1: Reset selected directories at the start of a quality action**

In `beginQualityResultMonitoring()`, clear `m_qualityResultOutputDirs`, collect currently visible directory controls once, reset `m_qualityResultScanStartedAt`, and start the timer as before.

- [x] **Step 2: Refresh before every timer scan**

In the timer callback, call `refreshQualityResultOutputDirectories();` immediately before `syncQualityResultShpLayers();`, so directories selected after the plugin window opens are captured before generated SHP files are scanned.

- [x] **Step 3: Schedule a bounded post-action scan**

In `executePluginFeatureAction()`, keep the existing monitoring start before `pluginAction->trigger()`, then schedule a bounded asynchronous scan after the action. The retry window must cover multi-minute checks, so plugins that overwrite files in an already-watched directory without emitting a reliable directory-change signal are still captured; the timer callback refreshes the selected directories before each scan.

### Task 4: Verify the result-panel regression and build surface

**Files:**
- Run: `tests/verify_quality_result_scan_uses_selected_output_dirs.ps1`
- Run: `tests/verify_quality_result_scan_ignores_input_layer_dirs.ps1`
- Run: `tests/verify_quality_result_scan_after_action_only.ps1`
- Run: `tests/verify_quality_result_panel_autoload.ps1`
- Run: `tests/verify_quality_result_panel_grouped_message.ps1`
- Run: `tests/verify_quality_result_board_registry_layers.ps1`
- Run: `tests/verify_quality_result_panel_feature_cap.ps1`

- [x] **Step 1: Run all targeted static checks**

Run:

```powershell
.	estserify_quality_result_scan_uses_selected_output_dirs.ps1
.	estserify_quality_result_scan_ignores_input_layer_dirs.ps1
.	estserify_quality_result_scan_after_action_only.ps1
.	estserify_quality_result_panel_autoload.ps1
.	estserify_quality_result_panel_grouped_message.ps1
.	estserify_quality_result_board_registry_layers.ps1
.	estserify_quality_result_panel_feature_cap.ps1
```

Expected: each script prints its success message and exits with code 0.

- [x] **Step 2: Compile the application if the configured Qt/MSBuild toolchain is available**

Run: `msbuild LTZK.sln /m /p:Configuration=Release /p:Platform=x64`

Expected: a successful Release x64 build. If `msbuild` is unavailable, report that limitation and retain the static-check evidence.

- [x] **Step 3: Inspect the final diff**

Run: `git diff --check`

Expected: no whitespace errors. This workspace currently has no Git metadata, so if the command cannot run, report that limitation rather than altering unrelated files.
