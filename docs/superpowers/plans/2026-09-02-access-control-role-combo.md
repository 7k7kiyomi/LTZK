# 权限管理角色下拉框首次展开修复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ensure the permission-management role combo shows all three roles and a visible themed arrow on its first opening.

**Architecture:** Keep role data and `LTZKAccessControlDialog` unchanged. Exclude transient Qt popup windows from the global plugin-window polish lifecycle, and declare the existing theme-aware SVG as the combo down-arrow in each platform QSS.

**Tech Stack:** C++/Qt 5.15.3, Qt Style Sheets, PowerShell static regression checks, MSBuild/Visual Studio.

---

### Task 1: Add the failing regression contract

**Files:**
- Create: `E:\AAALTZK\tests\verify_access_control_role_combo_initialization.ps1`

- [ ] **Step 1: Assert the three roles remain present and the editor does not clear them**
- [ ] **Step 2: Assert transient popup windows are excluded from deferred plugin polishing**
- [ ] **Step 3: Assert every platform QSS declares its matching SVG down-arrow**
- [ ] **Step 4: Run the script and confirm it fails on the current implementation**

### Task 2: Implement the minimal lifecycle and arrow fixes

**Files:**
- Modify: `E:\AAALTZK\LTZK.cpp` in `LTZK::eventFilter`
- Modify: `E:\AAALTZK\Resources\styles\ltzk_sci_fi.qss`
- Modify: `E:\AAALTZK\Resources\styles\ltzk_black.qss`
- Modify: `E:\AAALTZK\Resources\styles\ltzk_white.qss`
- Modify: `E:\AAALTZK\Resources\styles\ltzk_field_notes.qss`
- Modify: `E:\AAALTZK\Resources\styles\ltzk_steel_warm_white.qss`

- [ ] **Step 1: Skip `Qt::Popup` and `Qt::ToolTip` windows in the generic plugin polish branch**
- [ ] **Step 2: Add dark SVG arrows to the dark themes and light SVG arrows to the light themes**

### Task 3: Verify behavior and build

**Files:**
- Verify: `E:\AAALTZK\tests\verify_access_control_role_combo_initialization.ps1`
- Verify: related combo and theme regression scripts
- Build: `E:\AAALTZK\LTZK.sln`

- [ ] **Step 1: Run the new regression script and related existing scripts**
- [ ] **Step 2: Build Release x64 with MSBuild if available**
- [ ] **Step 3: Review changed-file scope and report the absence of Git metadata if applicable**

