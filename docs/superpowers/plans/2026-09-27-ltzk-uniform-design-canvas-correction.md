# LTZK Uniform 1920×1080 Design Canvas Correction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Correct LTZK platform scaling so the confirmed 1920×1080 appearance is uniformly scaled once on other Windows resolutions and non-16:9 screens retain aspect ratio with centered spare space.

**Architecture:** Keep Qt high-DPI mapping enabled, but make `LTZKUiScale` the only resolution-scale source. Remove lower-bound clipping and the home page's container/card-local scale formulas, express the confirmed home appearance as reference metrics, and derive every platform-owned size from the shared scale. Existing expanding map/table regions may consume spare space, while platform chrome and home content stay uniformly scaled.

**Tech Stack:** C++17, Qt 5 Widgets/Gui, QGIS 3.28, PowerShell contract tests, MSBuild Release x64.

---

## File structure

- Modify `LTZKUiScale.h`: remove scale clamps that prevent strict proportional shrinkage.
- Modify `LTZK.cpp`: replace adaptive home-card scaling with fixed reference metrics multiplied once by the shared scale; scale the custom-painted home header.
- Modify `tests/verify_global_resolution_scaling.ps1`: verify unclamped continuous scaling.
- Replace expectations in `tests/verify_home_module_cards_responsive_layout.ps1`: require one-scale home-card behavior and forbid the old adaptive functions.
- Modify `tests/verify_visual_scale_baseline_layout.ps1`: require globally scaled card/header metrics.
- Add `tests/verify_uniform_design_canvas_scaling.ps1`: focused regression contract for the reported bug and plugin-scope boundary.

The workspace is not a Git repository, so commit steps are replaced by explicit RED/GREEN checkpoints and file-scope reviews.

### Task 1: Add failing regression contracts

**Files:**
- Create: `tests/verify_uniform_design_canvas_scaling.ps1`
- Modify: `tests/verify_global_resolution_scaling.ps1`
- Modify: `tests/verify_home_module_cards_responsive_layout.ps1`
- Modify: `tests/verify_visual_scale_baseline_layout.ps1`

- [x] **Step 1: Write the uniform-scale regression test**

Require `factorForLogicalSize()` to return the raw positive `qMin(widthScale, heightScale)` without `kMinimumScale`, require home cards to use `homeModuleCardSize(this)`, require fixed width and height, require header font/geometry to call shared scaling helpers, and forbid `adaptiveHomeModuleCardSize`, `responsiveScaleForSize`, `cardScaledInt`, `0.65` card clamps, and platform-scale references under `map_product_manager_tools/ui_class`.

- [x] **Step 2: Update existing contracts to the corrected behavior**

Change the expected formula cases to include `1280×720 -> 0.6666667` and `1024×768 -> 0.5333333`. Replace the previous tests that required adaptive available-area cards with requirements for one global card size and globally scaled internal metrics.

- [x] **Step 3: Verify RED**

Run:

```powershell
.\tests\verify_uniform_design_canvas_scaling.ps1
.\tests\verify_global_resolution_scaling.ps1
.\tests\verify_home_module_cards_responsive_layout.ps1
.\tests\verify_visual_scale_baseline_layout.ps1
```

Expected: at least the new contract and the updated card/global contracts fail because production still contains the clamps and independent card scales.

### Task 2: Correct the global scale formula

**Files:**
- Modify: `LTZKUiScale.h:14-35`
- Test: `tests/verify_global_resolution_scaling.ps1`
- Test: `tests/verify_uniform_design_canvas_scaling.ps1`

- [x] **Step 1: Remove arbitrary scale bounds**

Keep invalid/empty-size fallback and finite-positive validation, then return `qMin(widthScale, heightScale)` directly. Do not multiply by `devicePixelRatio`; Qt remains responsible for logical-to-physical mapping.

- [x] **Step 2: Verify the formula tests become GREEN or advance to the next missing home requirement**

Run the two focused tests. Expected formula values:

```text
1024×768  -> 0.5333333
1280×720  -> 0.6666667
1366×768  -> 0.7111111
1920×1080 -> 1.0
2560×1440 -> 1.3333333
3840×2160 -> 2.0
```

### Task 3: Replace home-card adaptive scaling with reference metrics

**Files:**
- Modify: `LTZK.cpp:1270-1330`
- Modify: `LTZK.cpp:1490-1742`
- Test: `tests/verify_home_module_cards_responsive_layout.ps1`
- Test: `tests/verify_uniform_design_canvas_scaling.ps1`

- [x] **Step 1: Define the confirmed reference card metrics**

Use the effective dimensions produced by the confirmed 1920×1080 reference appearance: card `462×234`, content margins `18/13/18/16`, spacing `3`, code/title/subtitle fonts `18/22/12`, label minimum heights `20/36/20`, themed icon pixmap `286×103`, and icon-label minimum/maximum heights `70/122`. `homeModuleCardSize(widget)` returns `scaledSize(widget, 462, 234)`.

- [x] **Step 2: Remove independent card formulas**

Delete `adaptiveHomeModuleCardSize`, `responsiveScaleForSize`, and `cardScaledInt`. Change `HomeModulesPanel::updateModuleCardSizes()` to obtain `homeModuleCardSize(this)` only.

- [x] **Step 3: Apply both card dimensions atomically**

Set the scaled card size as both minimum and maximum size (or `setFixedSize`) and use `QSizePolicy::Fixed`. This prevents the layout from retaining a wide card while compressing its height.

- [x] **Step 4: Scale card contents from reference values**

Use `scaledInt(this, baseValue)` and `scaledSize(this, baseWidth, baseHeight)` for margins, spacing, icon pixmap, label heights, and the `01`/title/subtitle fonts. A resize event may repaint, but it must not derive a new scale from the current card size.

- [x] **Step 5: Verify GREEN**

Run the two card-focused tests. Expected: no old adaptive identifiers or clamps remain and every card metric uses the shared screen scale.

### Task 4: Scale the custom home header from the same source

**Files:**
- Modify: `LTZK.cpp:1871-2080`
- Test: `tests/verify_uniform_design_canvas_scaling.ps1`
- Test: `tests/verify_sci_fi_module_homepage.ps1`

- [x] **Step 1: Replace raw painter constants**

Use the 1920×1080 reference values `150..162` header height, `66` title pixels, title rectangle `y=18, height=96`, `52` band height, `1220` top-center width, `28` corner radius, and the existing reference border/offset values. Derive each value from `visualScaleForWidget(this)` through `scaledInt` or a local floating-point equivalent. Width-dependent centering may remain, but must be capped by a scaled 1920 reference width so wider aspect ratios produce centered spare space instead of stretched artwork.

- [x] **Step 2: Update the homepage source contract**

Replace its required raw `qBound(58, height * 0.44, 70)` expression with the globally scaled title-font expression.

- [x] **Step 3: Verify GREEN**

Run both header/homepage tests. Expected: the header no longer contains a raw 58–70 logical-pixel font clamp.

### Task 5: Regression verification and build

**Files:**
- Verify all platform files above.
- Verify no files below `map_product_manager` or `map_product_manager_tools` changed.

- [x] **Step 1: Run focused scaling tests**

```powershell
.\tests\verify_uniform_design_canvas_scaling.ps1
.\tests\verify_global_resolution_scaling.ps1
.\tests\verify_home_module_cards_responsive_layout.ps1
.\tests\verify_visual_scale_baseline_layout.ps1
.\tests\verify_sci_fi_module_homepage.ps1
.\tests\verify_message_log_dock_and_splash_delay.ps1
```

Expected: all commands exit `0` and print their success messages.

- [x] **Step 2: Run adjacent platform UI tests**

```powershell
.\tests\verify_platform_qss_extraction.ps1
.\tests\verify_platform_theme_switcher.ps1
.\tests\verify_login_dialog_responsive_layout.ps1
.\tests\verify_embedded_login_access_pages.ps1
.\tests\verify_workspace_resizable_panels.ps1
.\tests\verify_qgis_runtime_config_before_application.ps1
```

Expected: all commands exit `0`.

- [x] **Step 3: Build Release x64**

```powershell
msbuild LTZK.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m:1
```

Expected: zero compiler and linker errors.

- [x] **Step 4: Review scope and runtime handoff**

Confirm only platform source/tests/docs changed. Record that final visual acceptance still requires launching `x64\Release\LTZK.exe` at the user's target Windows resolution and comparing it with the 1920×1080 reference using the same log-Dock visibility state.

### Task 6: Close independent-scale and runtime-refresh gaps found in review

**Files:**
- Modify: `LTZKUiScale.h`
- Modify: `LTZK.cpp`
- Modify: `LTZK.h`
- Modify: focused scaling tests above

- [x] **Step 1: Add an inherited page-scale override**

Allow the homepage to publish one positive `ltzkUiScaleOverride` value that every descendant reads before falling back to the current screen scale.

- [x] **Step 2: Fit the complete homepage design canvas**

Compute the homepage scale from the smaller of the current-screen scale and the available homepage viewport against the `1920×1020` client-area reference. This keeps the full composition visible when the message-log dock reduces available height and preserves centered spare space on other aspect ratios.

- [x] **Step 3: Make homepage refresh idempotent**

Reapply header, cards, fonts, login panel, margins, grid gaps, utility buttons, footer, shadow, and page-local QSS from reference values after a page resize, show event, resolution change, or monitor change.

- [x] **Step 4: Remove the workspace top-bar clamp**

Delete the independent `0.78..1.18` scale and use the shared screen scale for top-bar buttons, icon sizes, action strip height, scroll area, splitter controls, map toolbox, access-control chrome, status coordinate label, and log-dock height.

- [x] **Step 5: Correct custom header geometry**

Use the exact scaled `52` band height and `1220` panel width, with widget dimensions used only as a clipping bound. Remove width/height-derived local geometry ratios.

- [x] **Step 6: Rebuild, run the complete focused regression suite, and obtain final review**
