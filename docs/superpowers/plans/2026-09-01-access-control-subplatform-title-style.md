# 权限管理子平台标题样式统一实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans (inline execution with checkpoints). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the permission-management subplatform title use the same current-theme title and top-bar styling as the four business subplatforms across all five themes.

**Architecture:** Keep the shared `dashboardTitle` selector and current theme loader. Make the access-control page start from `platformStyleSheet()`, remove the fixed dark page/top-bar overrides that conflict with theme QSS, align the access title glow with the workspace title glow, and ensure every theme maps `#accessControlTopBar` alongside `#dashboardTopBar`.

**Tech Stack:** C++/Qt, Qt Style Sheets, PowerShell static regression checks, MSBuild/Visual Studio.

---

### Task 1: Add the failing regression contract

**Files:**
- Modify: `tests/verify_access_control_title_style.ps1`

- [ ] **Step 1: Assert the access page uses the current platform style**
  Require the access-page block to contain `page->setStyleSheet(platformStyleSheet() + QStringLiteral(`.

- [ ] **Step 2: Assert title glow parity**
  Require the access title glow to use blur radius 14 and color `QColor(86, 185, 220, 105)`, matching the workspace title.

- [ ] **Step 3: Assert all five themes map both top bars**
  Read every `Resources/styles/*.qss` file and require a `#dashboardTopBar, #accessControlTopBar` selector.

- [ ] **Step 4: Assert fixed dark top-bar overrides are absent**
  Fail if the access-page block still contains the old `background: rgba(7, 28, 49, 245);` rule.

- [ ] **Step 5: Run the test and confirm the expected failure**
  Run `.\\tests\\verify_access_control_title_style.ps1`; it must fail because the current source still calls `workspaceDarkStyleSheet()`, retains the orange/22px glow, and the Sci-Fi QSS lacks the paired access top-bar selector.

### Task 2: Implement the unified title styling

**Files:**
- Modify: `LTZK.cpp:2227-2230`
- Modify: `LTZK.cpp:3650-3718`
- Modify: `LTZK.cpp:3731-3735`
- Modify: `Resources/styles/ltzk_sci_fi.qss:63`

- [ ] **Step 1: Remove the obsolete fixed Sci-Fi workspace helper**
  Remove `workspaceDarkStyleSheet()`, which is only used by the permission-management page and always returns the Sci-Fi QSS.

- [ ] **Step 2: Use the current theme stylesheet**
  Change the access page stylesheet source to `platformStyleSheet()`.

- [ ] **Step 3: Remove only conflicting fixed page/top-bar rules**
  Remove the access-page `#accessControlPage` and `#accessControlTopBar` hardcoded dark blocks. Preserve the editor, table, header, button, and hover rules.

- [ ] **Step 4: Match the workspace title glow**
  Set `accessTitleGlow->setBlurRadius(14);` and `accessTitleGlow->setColor(QColor(86, 185, 220, 105));`.

- [ ] **Step 5: Pair the Sci-Fi top-bar selectors**
  Change the Sci-Fi selector to `#dashboardTopBar, #accessControlTopBar` while retaining its existing gradient and border.

### Task 3: Verify behavior and build

**Files:**
- Verify: `tests/verify_access_control_title_style.ps1`
- Verify: `tests/verify_subplatform_title_transparent_background.ps1`
- Verify: `tests/verify_all_subplatform_titles.ps1`
- Verify: `tests/verify_platform_theme_expansion.ps1`
- Build: `LTZK.sln`

- [ ] **Step 1: Run the new regression contract and related static checks**
  All scripts must exit 0 with their success messages.

- [ ] **Step 2: Inspect changed-file scope**
  Confirm only the access-page source, five theme QSS files if needed, the regression script, and this plan/spec are in scope. The current workspace has no Git metadata, so use direct content checks instead of Git diff.

- [ ] **Step 3: Build Release x64**
  Run `msbuild .\\LTZK.sln /m /p:Configuration=Release /p:Platform=x64` when the configured MSBuild tool is available. Record the exact result.

- [ ] **Step 4: Report verification evidence**
  Report test outputs, build result, and any environment limitation without claiming a commit.

