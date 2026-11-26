# 质检结果全量扫描与路径归组 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让全量质检完成后加载并按真实源路径归组所有结果 SHP，不再因单次 50 层限制而遗漏严重程度目录。

**Architecture:** 在现有 `syncQualityResultShpLayers()` 的递归发现逻辑中移除图层数量上限；新增一个轻量的项目图层归组 helper，在完成后补扫定时器中先由原扫描逻辑加载尚未进入项目的 SHP，再遍历项目内的结果图层统一按真实路径归组。路径解析和图层树移动复用现有 helper，避免改变输出目录选择和稳定性判断。

**Tech Stack:** C++/Qt/QGIS API；PowerShell 静态回归检查；现有 MSBuild 工程。

---

### Task 1: Add the failing regression check

**Files:**
- Create: `tests/verify_quality_result_full_regrouping.ps1`

- [ ] **Step 1: Assert the 50-layer cap is absent and the completion regroup path exists**

The test reads `LTZK.cpp` and `LTZK.h` and fails until the implementation has removed `kMaxQualityResultLayersPerScan`, removed its scan-loop condition, declared and defined `regroupQualityResultLayersBySourcePath()`, and invoked it from the completion scan callback. It also checks that the helper iterates the project map via `project->mapLayers()`, filters selected output paths, and calls the existing safe move helper.

- [ ] **Step 2: Run the new check and confirm the expected failure**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_quality_result_full_regrouping.ps1
```

Expected: FAIL because the current source still contains `kMaxQualityResultLayersPerScan` and has no completion regroup helper.

### Task 2: Remove the single-scan layer limit

**Files:**
- Modify: `LTZK.cpp:2468-2469` and the `syncQualityResultShpLayers()` scan loop

- [ ] **Step 1: Delete the 50-layer constant**

Keep the 2000-file-per-root traversal guard. Remove only the layer-count constant.

- [ ] **Step 2: Remove the `addedCount >= kMaxQualityResultLayersPerScan` short-circuit**

The loop may still stop when the explicit per-root file safety guard is reached, but it must not stop because `addedCount` reaches 50.

### Task 3: Regroup all loaded result SHP layers after completion

**Files:**
- Modify: `LTZK.h` near the quality-result tree helpers
- Modify: `LTZK.cpp` near `syncQualityResultShpLayers()` and the post-action scan timer callback

- [ ] **Step 1: Declare `regroupQualityResultLayersBySourcePath()`**

Add a private helper returning the number of newly registered or moved layers.

- [ ] **Step 2: Implement the helper**

Iterate `project->mapLayers()` from the current `QgsProject`, cast vector layers, obtain each source with `localPathFromLayerSource(layer->source())`, require a valid `.shp` inside the selected output directories, register it as a quality result layer, and call `moveQualityResultLayerToGroupSafely(layer)`.

- [ ] **Step 3: Invoke the helper in the completion-gated timer**

Run `syncQualityResultShpLayers()` and then the regroup helper in the completion-gated timer, so both newly discovered and already loaded layers are normalized. Refresh the panel when either regrouping or new-layer loading changes the tree.

### Task 4: Verify the complete change

**Files:**
- Test: `tests/verify_quality_result_full_regrouping.ps1`
- Test: existing quality-result PowerShell checks under `tests/`

- [ ] **Step 1: Run the new regression check and all related checks**

Run the new check plus the existing completion-gated scan, dual-display, selected-output, post-action, worker-completion, panel-autoload, and board-registry checks. Expected: all pass.

- [ ] **Step 2: Build the affected project configuration**

Run the repository's MSBuild target for the affected configuration and confirm it completes without compiler errors.
