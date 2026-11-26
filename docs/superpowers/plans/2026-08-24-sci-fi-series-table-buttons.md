# Sci-Fi Series Table Buttons Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ensure embedded “选择/配置” buttons use an opaque sci-fi background without changing white or black themes.

**Architecture:** Keep both stylesheet layers aligned. The resource QSS defines the normal sci-fi table appearance, while `polishSeriesContentTableWindow()` applies an equivalent theme-aware local rule so runtime polishing cannot restore transparency.

**Tech Stack:** Qt 5 C++, QSS, PowerShell static regression tests

---

### Task 1: Add the failing regression

**Files:**
- Create: `tests/verify_sci_fi_series_table_button_background.ps1`
- Test: `tests/verify_sci_fi_series_table_button_background.ps1`

- [ ] **Step 1: Write a test requiring opaque sci-fi button backgrounds in both QSS and runtime styling.**
- [ ] **Step 2: Run `.\tests\verify_sci_fi_series_table_button_background.ps1` and confirm it fails on `background: transparent`.**

### Task 2: Apply the minimal theme-scoped fix

**Files:**
- Modify: `Resources/styles/ltzk_sci_fi.qss:34-36`
- Modify: `LTZK.cpp:6267-6268`

- [ ] **Step 1: Give sci-fi table buttons an opaque `#06111d` background, `#f8feff` text, and existing cyan separator.**
- [ ] **Step 2: Make the runtime button style select the same opaque rule only for `PlatformTheme::SciFi`; preserve the existing rule for white and black.**
- [ ] **Step 3: Run the new regression and confirm it passes.**

### Task 3: Run related regressions

**Files:**
- Test: `tests/verify_table_embedded_controls_flat.ps1`
- Test: `tests/verify_table_embedded_controls_all_themes.ps1`
- Test: `tests/verify_plugin_table_embedded_control_layout.ps1`
- Test: `tests/verify_plugin_table_text_suppression_lifecycle.ps1`

- [ ] **Step 1: Run all four existing scripts plus the new regression.**
- [ ] **Step 2: Confirm all scripts finish without exceptions.**

This workspace is not a Git repository, so commit steps are not available.
