# Result Output Event Scan Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Load newly produced SHP files from each feature's user-selected output directory while eliminating idle recursive scans.

**Architecture:** Arm monitoring for configured result-capable plugins, bind it to the invoked plugin window's semantic output-directory control, and use `QFileSystemWatcher` plus a single-shot debounce timer. Preserve the existing bounded SHP loader, but invoke it only after a directory-change event.

**Tech Stack:** C++/Qt 5 (`QFileSystemWatcher`, `QTimer`, `QPointer`), QGIS vector-layer API, PowerShell static regression tests, MSBuild Release build.

---

### Task 1: Add failing event-driven monitoring regression

**Files:**
- Create: `E:\AAALTZK\tests\verify_result_output_scan_event_driven.ps1`
- Test: `E:\AAALTZK\tests\verify_result_output_scan_event_driven.ps1`

- [x] Write assertions that require a `QFileSystemWatcher`, a single-shot debounce timer, semantic output-control binding, and a directory-change handler.
- [x] Assert that `executePluginFeatureAction` does not invoke recursive SHP synchronization before or immediately after `pluginAction->trigger()`.
- [x] Assert that the timer is not started by `beginQualityResultMonitoring` and is not configured as a repeating two-second poller.
- [x] Run `powershell -ExecutionPolicy Bypass -File .\tests\verify_result_output_scan_event_driven.ps1` and confirm it fails because event-driven monitoring is absent.

### Task 2: Implement selected-directory event monitoring

**Files:**
- Modify: `E:\AAALTZK\LTZK.h`
- Modify: `E:\AAALTZK\LTZK.cpp`
- Test: `E:\AAALTZK\tests\verify_result_output_scan_event_driven.ps1`

- [x] Add watcher/window/control state and small methods for arming, associating a plugin window, registering its actual selected output directory, clearing stale paths, and handling a debounced directory change.
- [x] Reconfigure the existing scan timer as single-shot and invoke bounded recursive scanning only from its timeout after a filesystem notification.
- [x] Extend the existing application event filter to associate only windows containing `isQualityResultOutputPathControl` line edits while a configured plugin action is pending.
- [x] Connect the selected output line edit's text changes so the watcher always follows the exact user-selected directory.
- [x] Remove immediate and repeating scan calls from the plugin-action path.
- [x] Run the new test and confirm it passes.

### Task 3: Update existing monitoring contracts and package copy

**Files:**
- Modify: `E:\AAALTZK\tests\verify_quality_result_scan_after_action_only.ps1`
- Modify: `E:\AAALTZK\tests\verify_quality_result_panel_autoload.ps1`
- Modify: `E:\AAALTZK\LTZK_Developer_Package\01_Source\LTZK\LTZK.h`
- Modify: `E:\AAALTZK\LTZK_Developer_Package\01_Source\LTZK\LTZK.cpp`
- Test: focused `verify_*result*` scripts

- [x] Change stale test expectations from periodic/immediate scanning to event-driven scanning.
- [x] Synchronize source/header changes into the developer package source tree.
- [x] Run focused result-monitor and developer-package synchronization tests.

### Task 4: Build and deploy the verified runtime

**Files:**
- Build: `E:\AAALTZK\LTZK.vcxproj`
- Deploy: `E:\LTZK\bin\LTZK.exe`

- [x] Build Release with the configured Visual Studio MSBuild executable and confirm zero compile/link errors.
- [x] Copy `E:\AAALTZK\x64\Release\LTZK.exe` to `E:\LTZK\bin\LTZK.exe` without changing the runtime plugin binding configuration.
- [x] Compare SHA-256 hashes of source and deployed executables.
- [x] Re-run focused tests against the deployed configuration and report any unrelated pre-existing failures separately.
