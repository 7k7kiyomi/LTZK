# QGIS Editing Toolbar Implementation Plan

> **For agentic workers:** Execute tasks in order. Steps use checkbox syntax for tracking.

**Goal:** Add a QGIS-style editing workflow to the map workspace, including edit-session controls and geometry editing tools.

**Architecture:** Add a fifth movable toolbar to the existing map-canvas toolbar host. Keep edit-session decisions in a small tested policy helper; use QGIS 3.28 map tools and vector-layer edit buffers for drawing, vertex movement, deletion, undo, and redo. Route layer-tree context actions and existing attribute-table state through the same `QgsVectorLayer` session.

**Tech Stack:** C++17, Qt 5, QGIS 3.28.15, MSBuild, existing standalone C++ policy-test convention.

---

### Task 1: Define and test editability policy

**Files:**
- Create: `WorkspaceEditingPolicy.h`
- Create: `tests/workspace_editing_policy_test.cpp`

- [ ] Write tests for read-only, invalid, non-editable-provider, editable, and modified-layer save/rollback action states.
- [ ] Compile the standalone test before adding the policy header and confirm compilation fails because the helper is missing.
- [ ] Add pure policy functions for edit eligibility and save/rollback availability; use those functions in the toolbar state update.
- [ ] Rebuild and run the policy test; expect exit code 0.

### Task 2: Add current-layer edit-session controls

**Files:**
- Modify: `LTZK.h`
- Modify: `WorkspaceLayerTools.cpp`
- Modify: `LTZK.cpp`

- [ ] Add the “要素编辑” toolbar to `initWorkspaceToolbars()` so it inherits persisted visibility, floating, and authenticated-workspace behavior.
- [ ] Add edit toggle, save, rollback, end-edit, and selected/all-layer save/rollback/cancel actions with existing QGIS SVG assets.
- [ ] Synchronize enablement and checked state from current-layer changes, `editingStarted`, `editingStopped`, modification, and undo-stack signals.
- [ ] Prompt Save / Discard / Cancel before stopping an edited layer; keep editing active after a failed commit.
- [ ] Add the same current-layer edit toggle/save/rollback actions to vector-layer context menus; preserve the attribute table’s existing layer-bound edit controls.

### Task 3: Add canvas digitizing and vertex editing

**Files:**
- Modify: `WorkspaceLayerTools.cpp`
- Modify: `LTZK.h`
- Modify: `LTZK.qrc`

- [ ] Create point, line, and polygon capture tools using `QgsMapToolDigitizeFeature`; insert accepted features into the current layer edit buffer through the QGIS attribute form.
- [ ] Create a vertex-drag map tool that locates the nearest vertex in screen space, previews the moved geometry, and records one QGIS edit command on release.
- [ ] Add selected-feature deletion and layer undo/redo actions backed by the vector layer edit buffer and undo stack.
- [ ] Disable tools when the current layer/provider/geometry type cannot support the operation; cancel an active geometry tool when the current layer changes.
- [ ] Register existing edit SVG assets in the application resource collection and use Chinese labels and tooltips.

### Task 4: Build and validate

**Files:**
- Build: `LTZK.vcxproj`

- [ ] Build Release x64 to an alternate target name so a running `LTZK.exe` does not block output replacement.
- [ ] Run the edit-policy test and inspect compiler output for errors.
- [ ] Inspect the final source diff for state synchronization, data-loss prompts, geometry-type gating, resource registration, and Chinese UI text.
