# 检查结果 SHP 双通道显示 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (\`- [ ]\`) syntax for tracking.

**Goal:** 将检查结果 SHP 统一登记，使其同时出现在左侧图层管理和结果面板，同时保持结果面板对地图显隐的唯一控制权。

**Architecture:** 增加统一结果层登记方法。目录扫描和插件主动加层都调用该方法；扫描结果使用默认 `addMapLayer(layer)` 加入图层树。新增图层只在当前结果监控输出目录范围内登记，避免误收录输入数据。

**Tech Stack:** C++17、Qt 5、QGIS 3.28、PowerShell 静态回归脚本、MSBuild x64。

---

### Task 1: Add the failing regression

**Files:**
- Create: `tests/verify_quality_result_dual_display.ps1`

- [x] **Step 1: Write the failing test**

Require the source to contain a shared registration method, a `layersAdded` handler receiving added layers, a default `addMapLayer(layer)` call inside the result scan, output-directory scoping for externally added SHP layers, and the existing panel-only canvas visibility path.

- [x] **Step 2: Run the test and verify it fails**

Run:

```powershell
powershell.exe -NoProfile -NonInteractive -File .\tests\verify_quality_result_dual_display.ps1
```

Expected: FAIL because the scanner currently uses `addMapLayer(layer, false)` and there is no shared external-layer registration path.

### Task 2: Implement unified result-layer registration

**Files:**
- Modify: `LTZK.h`
- Modify: `LTZK.cpp`

- [x] **Step 1: Add declarations**

Add private methods for checking whether a layer source is inside the selected result roots and registering a `QgsVectorLayer` as a result layer.

- [x] **Step 2: Register scanned layers and add them to the layer tree**

Make the scan call the registration method and use `QgsProject::instance()->addMapLayer(layer)` so the same layer is visible in the left tree.

- [x] **Step 3: Register plugin-added result layers**

Change the `layersAdded` connection to inspect newly added layers. Register only valid SHP layers from the active monitoring module and selected output roots, then refresh the panel.

### Task 3: Verify behavior

- [x] **Step 1: Run the new regression and existing result checks**

```powershell
powershell.exe -NoProfile -NonInteractive -File .\tests\verify_quality_result_dual_display.ps1
powershell.exe -NoProfile -NonInteractive -File .\tests\verify_quality_result_panel_autoload.ps1
powershell.exe -NoProfile -NonInteractive -File .\tests\verify_quality_result_board_registry_layers.ps1
powershell.exe -NoProfile -NonInteractive -File .\tests\verify_result_output_scan_event_driven.ps1
powershell.exe -NoProfile -NonInteractive -File .\tests\verify_quality_result_scan_after_action_completion.ps1
powershell.exe -NoProfile -NonInteractive -File .\tests\verify_quality_result_scan_waits_for_long_running_action.ps1
```

- [ ] **Step 2: Build the Release target**

```powershell
msbuild .\LTZK.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m
```

- [x] **Step 3: Inspect the final diff and report any runtime limitation**

Confirm only the result registration path, regression test, and design/plan documents changed. If the external plugin DLL still emits the MULTIPOINT-to-point SHP warning, report that as an upstream output-generation issue separate from host layer registration.
