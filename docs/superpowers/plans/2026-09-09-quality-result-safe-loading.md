# Quality Result Safe Loading Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every quality-check result SHP appear automatically in both the left layer manager and the result panel without reparenting the QGIS layer tree during `layersAdded` or reading files while a worker is still writing them.

**Architecture:** The host owns one safe result-layer insertion path. It registers result layers with `QgsProject::addMapLayer(layer, false)` and then appends the layer directly to a folder group derived from the selected output directory. The project `layersAdded` signal only observes additions; completion-driven scanning is armed after all monitored worker threads finish and each SHP sidecar set is stable. The result registry uses guarded layer pointers and prunes removed layers before the panel or canvas reads them.

**Tech Stack:** C++17, Qt 5, QGIS 3.28 layer registry/tree APIs, PowerShell source-level regression checks.

---

### Task 1: Add regression coverage for safe loading

**Files:**
- Create: `tests/verify_quality_result_safe_tree_loading.ps1`
- Modify: `tests/verify_quality_result_scan_after_action_completion.ps1`

- [ ] **Step 1: Assert the safe host insertion and completion-gated scan contracts.**

  The new script checks for the host insertion API, folder metadata, `addMapLayer(layer, false)`, sidecar stability checks, guarded layer references, and completion state. It also extracts the `layersAdded` callback and rejects `takeChild`, `addChildNode`, synchronous panel refresh, and synchronous registration from that callback.

- [ ] **Step 2: Update the existing scheduler expectation.**

  The post-action scheduler must set a pending flag and wait for worker completion; it must not start the scan timer directly from `scheduleQualityResultPostActionScan`.

- [ ] **Step 3: Run the new test before production changes.**

  Run `powershell -ExecutionPolicy Bypass -File tests/verify_quality_result_safe_tree_loading.ps1`.

  Expected result before implementation: failure because the current code still adds result layers to the root, starts scanning immediately, and refreshes the panel from `layersAdded`.

### Task 2: Route plugin-created vector layers through the host

**Files:**
- Modify: `LTZK.h`
- Modify: `LTZK.cpp`
- Modify: `LTZKQgisInterface.cpp`

- [ ] **Step 1: Expose `LTZK::addPluginVectorLayer`.**

  Construct and validate the layer in `LTZKQgisInterface::addVectorLayer`, then delegate to `LTZK::addPluginVectorLayer`. For a result SHP, call `QgsProject::addMapLayer(layer, false)` and `addQualityResultLayerToTree(layer)`; for an ordinary layer preserve the existing default legend insertion.

- [ ] **Step 2: Implement folder creation without tree-node reparenting.**

  Resolve the longest selected-output-directory prefix, create or reuse groups marked with `ltzk/qualityResultFolder` and `ltzk/qualityResultFolderPath`, and append a newly registered layer with `targetGroup->addLayer(layer)`. Existing layers are only moved by a bridge-disabled compatibility helper during a post-completion sync, never from `layersAdded`.

- [ ] **Step 3: Use the same safe insertion path for scanner-created layers.**

  The scanner loads a valid SHP into the registry with `addMapLayer(layer, false)` and appends it to the calculated group. This preserves the normal QGIS registry ownership while keeping the result tree organized.

### Task 3: Make scan timing and file reads completion-safe

**Files:**
- Modify: `LTZK.h`
- Modify: `LTZK.cpp`

- [ ] **Step 1: Add explicit pending/ready state.**

  `scheduleQualityResultPostActionScan` marks a scan pending and stops any active timer. `handleQualityResultWorkerFinished` waits until no monitored worker thread is running, then arms the bounded retry timer. Directory-change notifications only trigger a scan after the ready state is armed.

- [ ] **Step 2: Require stable SHP sidecars.**

  Track signatures for `.shp`, `.shx`, `.dbf`, and optional `.prj`/`.cpg` files. A candidate is loaded only after required sidecars exist and the signature is unchanged across scans.

- [ ] **Step 3: Keep retries bounded.**

  Reuse the existing retry limit and interval, but only while monitoring is active and the scan is ready. Reset pending, ready, attempts, and signatures on module/window changes.

### Task 4: Harden result registry and panel/canvas reads

**Files:**
- Modify: `LTZK.h`
- Modify: `LTZK.cpp`

- [ ] **Step 1: Store `QPointer<QgsVectorLayer>`.**

  Before rendering the panel or canvas, remove null pointers and layers no longer present in `QgsProject::mapLayer(layer->id())`.

- [ ] **Step 2: Preserve dual display behavior.**

  The layer manager receives the organized tree node, while the result panel continues to list feature details and controls `m_qualityResultVisiblePaths` for canvas visibility.

- [ ] **Step 3: Run all quality-result regression scripts.**

  Run the new safe-loading test plus the existing result-panel, dual-display, output-directory, worker-completion, and module-switch scripts. Then attempt the Release build without terminating a running LTZK process; report any file-lock limitation separately.

### Task 5: Final review and verification

**Files:**
- Review: `LTZK.h`, `LTZK.cpp`, `LTZKQgisInterface.cpp`, `tests/verify_quality_result_safe_tree_loading.ps1`

- [ ] **Step 1: Inspect the final diff and search for forbidden signal-time reparenting.**

  Confirm no `takeChild` or `addChildNode` appears inside the `layersAdded` callback and that all result panel reads use guarded pointers.

- [ ] **Step 2: Run verification commands again from a clean PowerShell invocation.**

  Only report completion after the regression scripts produce PASS output and the build result is known.
