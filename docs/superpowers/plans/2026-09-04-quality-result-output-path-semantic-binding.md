# 质检结果输出路径语义识别 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让结果面板读取插件界面中用户选择的结果输出目录，并递归加载该目录中的 SHP 结果文件，即使路径控件使用通用对象名 `lineEdit`。

**Architecture:** 质检结果扫描仍由主程序生命周期控制，但输出目录识别改为“控件对象名或相邻语义标签”双重识别。主程序只保存可见质检插件窗口中明确标注为结果输出路径的目录，不依赖目录名或 SHP 文件名；扫描器继续使用当前检查动作时间、递归目录和 registry-only 图层机制。

**Tech Stack:** C++17、Qt Widgets、QGIS 3.28、PowerShell 静态回归检查、MSBuild Release x64。

---

### Task 1: Add the failing regression check for semantic output-path binding

**Files:**
- Create: `tests/verify_quality_result_output_path_semantic_binding.ps1`

- [x] **Step 1: Write the failing test**

The test must require a semantic output-path helper, the screenshot label variants, generic `lineEdit` support through label association, and use of that helper inside the live refresh loop. It must fail against the current implementation, which only accepts object names containing `output`.

- [x] **Step 2: Run the test to verify it fails**

Run: `& '.\tests\verify_quality_result_output_path_semantic_binding.ps1'`

Expected: FAIL because the source has no semantic label helper and the refresh loop rejects the generic `lineEdit` control.

### Task 2: Implement semantic output-directory discovery

**Files:**
- Modify: `LTZK.cpp:2080-2107,4554-4590`
- Test: `tests/verify_quality_result_output_path_semantic_binding.ps1`

- [x] **Step 1: Add output-label matching helpers**

Add helpers in the existing anonymous namespace to normalize label text and recognize `结果输出目录`, `输出结果路径`, `结果输出路径`, `输出路径`, `输出目录`, and their English equivalents. Associate a `QLineEdit` with an output label through `QLabel::buddy()` first, then through same-row global geometry so Qt Designer forms without buddy metadata are supported.

- [x] **Step 2: Add the minimal output-control predicate**

Accept a line edit when its object name contains `output` or its associated label is an output-path label. This preserves existing named controls while admitting the plugin's generic `lineEdit` only when its UI meaning is output-related.

- [x] **Step 3: Replace the name-only filter in the refresh loop**

Keep scanning only visible top-level plugin windows, require an existing directory value, normalize it, and append it through `appendUniqueDir()`. Retain the last valid directory when a plugin window is temporarily hidden or its field is blank, so an output path captured before the check remains available while SHP files are being generated.

- [x] **Step 4: Run the regression test to verify it passes**

Run: `& '.\tests\verify_quality_result_output_path_semantic_binding.ps1'`

Expected: PASS and confirmation that a generic `lineEdit` can be used when its adjacent label identifies the result output path.

### Task 3: Verify the complete result-panel behavior and build surface

**Files:**
- Test: `tests/verify_quality_result_output_path_semantic_binding.ps1`
- Existing checks: `tests/verify_quality_result_scan_uses_selected_output_dirs.ps1`, `tests/verify_quality_result_scan_ignores_input_layer_dirs.ps1`, `tests/verify_quality_result_scan_after_action_only.ps1`, `tests/verify_quality_result_panel_autoload.ps1`, `tests/verify_quality_result_panel_grouped_message.ps1`, `tests/verify_quality_result_board_registry_layers.ps1`, `tests/verify_quality_result_panel_feature_cap.ps1`

- [x] **Step 1: Run all targeted static checks**

Run each listed PowerShell check and require exit code 0 with its success message.

- [x] **Step 2: Build the Release x64 application**

Run: `msbuild LTZK.sln /m /p:Configuration=Release /p:Platform=x64`

Expected: successful build and refreshed `x64\Release\LTZK.exe`/plugin deployment.

- [x] **Step 3: Inspect changed files and whitespace**

Run: `git diff --check`

Expected: if Git metadata is absent, record that limitation; otherwise require no whitespace errors and inspect only the intended source/test/plan files.

---

## Plan self-review

- Coverage: semantic labels, generic `lineEdit`, exact selected-directory scan boundary, existing scan lifecycle, regression tests, and Release build are all covered by Tasks 1–3.
- Placeholder scan: no TODO/TBD or unspecified implementation step is required.
- Type consistency: the helper names and `QLineEdit*`/`QWidget*` arguments are used consistently between Tasks 1–2.
- Scope: the fix stays within output-path discovery and does not refactor the result registry or panel UI.
