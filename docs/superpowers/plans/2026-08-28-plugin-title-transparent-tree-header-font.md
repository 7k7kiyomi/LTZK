# Plugin Title Transparency and Tree Header Font Implementation Plan

**Goal:** Remove the visual box behind plugin section titles, align those titles with their corresponding content area, and explicitly control quality-task tree header font size.

**Architecture:** Keep the existing runtime title-overlay mechanism. Make both the theme-level `QGroupBox::title` and runtime overlay transparent, then derive the overlay x-coordinate from the first visible functional child rather than a fixed value. Add a scoped `QTreeWidget QHeaderView::section` rule so the tree header size is independent from the table body.

**Tech Stack:** Qt/C++, Qt Style Sheets (QSS), PowerShell source verification scripts.

---

### Task 1: Add the requested style contracts

**Files:**
- Modify: `tests/verify_plugin_title_transparency_and_tree_header_font.ps1`

- [x] Use the existing regression script as the red test. It already checks transparent title rules, dynamic title alignment, and a scoped tree-header font size.

### Task 2: Implement theme and runtime title styling

**Files:**
- Modify: `Resources/styles/ltzk_sci_fi.qss`
- Modify: `Resources/styles/ltzk_white.qss`
- Modify: `Resources/styles/ltzk_black.qss`
- Modify: `LTZK.cpp` in `polishPluginGroupBoxTitles()`

- [ ] Set `QGroupBox::title` backgrounds transparent and borders off in all themes.
- [ ] Set the runtime title overlay background transparent, border off, and remove its visual-box padding.
- [ ] Compute the overlay left edge from the first visible functional child geometry in the group box coordinate system, with a contents-rect fallback.

### Task 3: Implement scoped tree-header font sizing

**Files:**
- Modify: `Resources/styles/ltzk_sci_fi.qss`
- Modify: `Resources/styles/ltzk_white.qss`
- Modify: `Resources/styles/ltzk_black.qss`

- [ ] Add `QTreeWidget QHeaderView::section { font-size: 14px; }` after the generic header rules so only tree headers receive the explicit size.

### Task 4: Verify

**Files:**
- Test: `tests/verify_plugin_title_transparency_and_tree_header_font.ps1`

- [ ] Run the focused regression script and confirm exit code 0.
- [ ] Inspect `git diff --check` and the final diff to ensure no unrelated changes were made.
