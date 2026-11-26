# Map Service Topbar Contour Watermark Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a subtle theme-aware contour watermark to the unused right side of the map-service workspace top bar without moving its existing buttons.

**Architecture:** Replace the scroll area's default viewport with a focused transparent widget that paints decorative contour paths only when an activation property is true. Centralize module-specific visibility in an `LTZK` helper invoked during business-module activation so all other modules retain their current appearance.

**Tech Stack:** C++17, Qt Widgets/QPainter, PowerShell source-verification tests, MSBuild.

---

### Task 1: Add the failing source contract test

**Files:**
- Create: `tests/verify_map_service_topbar_contour_watermark.ps1`
- Test: `tests/verify_map_service_topbar_contour_watermark.ps1`

- [ ] **Step 1: Write the failing test**

Require a dedicated `WorkspaceTopBarContourViewport`, a named viewport object, low-alpha contour painting, a module-scoped update helper, and the exact map-service module condition. Reject stylesheet background images and changes that center the plugin layout.

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File tests/verify_map_service_topbar_contour_watermark.ps1`

Expected: FAIL because `WorkspaceTopBarContourViewport` is not defined yet.

### Task 2: Implement theme-aware contour painting

**Files:**
- Modify: `LTZK.cpp`
- Modify: `LTZK.h`
- Test: `tests/verify_map_service_topbar_contour_watermark.ps1`

- [ ] **Step 1: Add the transparent viewport class**

Add a private translation-unit class derived from `QWidget`. In `paintEvent`, read the `contourWatermarkActive` property; when true, draw antialiased offset ellipses and short guide lines using palette-derived colors with alpha values below 50.

- [ ] **Step 2: Install the viewport**

Create `workspaceTopBarContourViewport`, disable automatic background fill, install it with `pluginScrollArea->setViewport(...)`, and retain the existing transparent scroll-area behavior.

- [ ] **Step 3: Add module-scoped visibility**

Declare and define `updateWorkspaceTopBarContourWatermark(const QString&)`. Set the viewport property true only for `QStringLiteral("地图服务管理")`, call `update()`, and invoke the helper after assigning `m_currentBusinessModuleTitle`.

- [ ] **Step 4: Run the focused test**

Run: `powershell -ExecutionPolicy Bypass -File tests/verify_map_service_topbar_contour_watermark.ps1`

Expected: PASS.

### Task 3: Verify regressions and build

**Files:**
- Verify: `LTZK.cpp`
- Verify: `LTZK.h`
- Verify: `tests/*.ps1`

- [ ] **Step 1: Run related workspace tests**

Run the plugin overflow, shared-scroll, static-action, theme-switcher, and new contour-watermark PowerShell tests.

Expected: all scripts exit with code 0.

- [ ] **Step 2: Build the solution**

Run the repository's established MSBuild command for the active configuration.

Expected: build exits with code 0 and reports no compile errors.

