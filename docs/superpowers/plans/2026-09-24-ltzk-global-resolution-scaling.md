# LTZK Global Resolution Scaling Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make LTZK use one 1920×1080-based application scale across Windows logical resolutions while preserving responsive map/table workspaces and avoiding DPI double scaling.

**Architecture:** Add a small header-only `LTZKUiScale` utility as the single source for scale calculation and metric conversion. Enable Qt high-DPI handling before `QgsApplication`, route the existing main-window and login-dialog scaling through the utility, scale theme QSS pixel metrics once, and refresh the scale when the main window changes screens. Existing responsive layouts continue to distribute surplus space; only visual metrics use the global factor.

**Tech Stack:** C++17, Qt 5 Widgets/Gui, QGIS 3.28, MSBuild, PowerShell verification scripts.

---

## File structure

- Create `LTZKUiScale.h`: pure scale calculation, widget/screen lookup, metric conversion, font conversion, and QSS pixel conversion.
- Modify `main.cpp`: enable Qt high-DPI attributes before application construction and initialize the application font from the primary logical screen.
- Modify `LTZK.cpp`: replace the 2048×1152 window-content formula, scale platform QSS, and refresh the main shell after a screen change.
- Modify `LTZK.h`: declare screen-change hooks and store the active screen connection.
- Modify `auth/LTZKLoginDialog.cpp`: remove dialog-size-derived scaling and use the same global screen scale.
- Modify `auth/LTZKLoginDialog.h`: remove obsolete resize-derived scale declarations.
- Modify `LTZK.vcxproj`: include the shared header in the Visual Studio project.
- Create `tests/verify_global_resolution_scaling.ps1`: source-level contract for baseline, DPI bootstrap, one-scale usage, and no legacy baseline.
- Modify `tests/verify_visual_scale_baseline_layout.ps1`: update existing expectations from the legacy 2048×1152 formula to the shared 1920×1080 helper.
- Modify `tests/verify_login_dialog_responsive_layout.ps1`: verify the login dialog consumes the shared screen scale instead of deriving scale from its own size.

The workspace is not a Git repository, so commit steps are replaced by explicit verification checkpoints and source-diff reviews.

### Task 1: Lock the global scaling contract with failing tests

**Files:**
- Create: `tests/verify_global_resolution_scaling.ps1`
- Modify: `tests/verify_visual_scale_baseline_layout.ps1`
- Modify: `tests/verify_login_dialog_responsive_layout.ps1`

- [ ] **Step 1: Add the new failing contract test**

Create `tests/verify_global_resolution_scaling.ps1` with the following checks:

```powershell
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$scaleHeader = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'LTZKUiScale.h')
$main = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'main.cpp')
$window = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'LTZK.cpp')
$login = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'auth\LTZKLoginDialog.cpp')

foreach ($required in @(
    'kBaseLogicalWidth = 1920.0',
    'kBaseLogicalHeight = 1080.0',
    'kMinimumScale = 0.70',
    'kMaximumScale = 2.00',
    'qMin(widthScale, heightScale)',
    'QScreen::geometry',
    'scaleStyleSheetPixels'
)) {
    if (-not $scaleHeader.Contains($required)) { throw "Global scale contract missing: $required" }
}

$appIndex = $main.IndexOf('QgsApplication a(argc, argv, true);')
foreach ($attribute in @('AA_EnableHighDpiScaling', 'AA_UseHighDpiPixmaps')) {
    $index = $main.IndexOf($attribute)
    if ($index -lt 0 -or $index -gt $appIndex) { throw "Qt DPI attribute must be set before QgsApplication: $attribute" }
}

if (-not $window.Contains('LTZKUiScale::factorForWidget')) { throw 'Main window is not using the shared scale.' }
if (-not $login.Contains('LTZKUiScale::factorForWidget')) { throw 'Login dialog is not using the shared scale.' }
if ($window.Contains('kDesignBaseWidth = 2048.0') -or $window.Contains('kDesignBaseHeight = 1152.0')) {
    throw 'Legacy 2048x1152 baseline remains.'
}

Write-Host 'Global 1920x1080 logical resolution scaling contract verified.'
```

- [ ] **Step 2: Update legacy verification expectations**

Change `tests/verify_visual_scale_baseline_layout.ps1` to require `#include "LTZKUiScale.h"`, `LTZKUiScale::factorForWidget(widget)`, and `LTZKUiScale::scaledInt(...)`; remove expectations for `kDesignBaseWidth`, `kDesignBaseHeight`, `contentsRect()` scaling, and the legacy `0.56..1.0` clamp.

Change `tests/verify_login_dialog_responsive_layout.ps1` to require `#include "../LTZKUiScale.h"` and `LTZKUiScale::factorForWidget(this)`; forbid the old `width() / kBaseDialogWidth`, `height() / kBaseDialogHeight`, and `qBound(0.85, ...)` formula.

- [ ] **Step 3: Run the tests and verify RED**

Run:

```powershell
.\tests\verify_global_resolution_scaling.ps1
.\tests\verify_visual_scale_baseline_layout.ps1
.\tests\verify_login_dialog_responsive_layout.ps1
```

Expected: the new test fails because `LTZKUiScale.h` does not exist, and the updated legacy tests fail because production code still uses the old formulas.

### Task 2: Implement the shared scale utility

**Files:**
- Create: `LTZKUiScale.h`
- Modify: `LTZK.vcxproj`

- [ ] **Step 1: Add the scale utility**

Implement a header-only namespace with these public operations:

```cpp
namespace LTZKUiScale
{
    constexpr qreal kBaseLogicalWidth = 1920.0;
    constexpr qreal kBaseLogicalHeight = 1080.0;
    constexpr qreal kMinimumScale = 0.70;
    constexpr qreal kMaximumScale = 2.00;

    inline qreal factorForLogicalSize(const QSize& logicalSize)
    {
        if (!logicalSize.isValid() || logicalSize.isEmpty())
            return 1.0;
        const qreal widthScale = logicalSize.width() / kBaseLogicalWidth;
        const qreal heightScale = logicalSize.height() / kBaseLogicalHeight;
        const qreal scale = qMin(widthScale, heightScale);
        if (!qIsFinite(scale) || scale <= 0.0)
            return 1.0;
        return qBound(kMinimumScale, scale, kMaximumScale);
    }

    inline qreal factorForScreen(const QScreen* screen)
    {
        return screen ? factorForLogicalSize(screen->geometry().size()) : 1.0;
    }

    inline qreal factorForWidget(const QWidget* widget)
    {
        const QWidget* window = widget ? widget->window() : nullptr;
        const QScreen* screen = window ? window->screen() : QGuiApplication::primaryScreen();
        return factorForScreen(screen);
    }

    inline int scaledInt(int baseValue, qreal scale)
    {
        return qMax(1, qRound(baseValue * scale));
    }
}
```

Also implement `scaledSize`, `scaledMargins`, `scaledFont`, and `scaleStyleSheetPixels`. The QSS converter must use a `QRegularExpression` that replaces numeric `px` values only, preserves colors/percentages, and rounds each converted value to at least one pixel.

- [ ] **Step 2: Add the header to the Visual Studio project**

Add:

```xml
<ClInclude Include="LTZKUiScale.h" />
```

to the header item group in `LTZK.vcxproj`.

- [ ] **Step 3: Run the global contract test**

Run:

```powershell
.\tests\verify_global_resolution_scaling.ps1
```

Expected: it progresses past header checks and fails at the first production integration check.

### Task 3: Bootstrap Qt high-DPI handling and the application font

**Files:**
- Modify: `main.cpp`
- Test: `tests/verify_global_resolution_scaling.ps1`
- Test: `tests/verify_qgis_runtime_config_before_application.ps1`

- [ ] **Step 1: Set Qt attributes before constructing `QgsApplication`**

Add `#include "LTZKUiScale.h"` and set:

```cpp
QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
```

after QGIS runtime environment configuration and before:

```cpp
QgsApplication a(argc, argv, true);
```

Do not set `QT_SCALE_FACTOR`.

- [ ] **Step 2: Scale the application font once from the primary logical screen**

After constructing `QgsApplication`, keep a copy of its default font, apply `LTZKUiScale::scaledFont(defaultFont, LTZKUiScale::factorForScreen(a.primaryScreen()))`, and call `a.setFont(...)`. This provides a common baseline for standard Qt and QGIS widgets; page-specific pixel fonts remain handled by their metric functions.

- [ ] **Step 3: Run startup-order tests**

Run:

```powershell
.\tests\verify_global_resolution_scaling.ps1
.\tests\verify_qgis_runtime_config_before_application.ps1
.\tests\verify_qgis_startup_runtime_environment.ps1
```

Expected: the two startup-order tests pass; the global contract test fails with `Main window is not using the shared scale.` until Task 4 is complete.

### Task 4: Route the main window through the shared scale and refresh on screen changes

**Files:**
- Modify: `LTZK.cpp`
- Modify: `LTZK.h`
- Test: `tests/verify_global_resolution_scaling.ps1`
- Test: `tests/verify_visual_scale_baseline_layout.ps1`

- [ ] **Step 1: Replace the legacy main-window formula**

Include `LTZKUiScale.h`, delete `kDesignBaseWidth` and `kDesignBaseHeight`, and reduce the local wrappers to:

```cpp
qreal visualScaleForWidget(const QWidget* widget)
{
    return LTZKUiScale::factorForWidget(widget);
}

int scaledInt(const QWidget* widget, int value)
{
    return LTZKUiScale::scaledInt(value, visualScaleForWidget(widget));
}

QSize scaledSize(const QWidget* widget, int width, int height)
{
    return LTZKUiScale::scaledSize(QSize(width, height), visualScaleForWidget(widget));
}
```

Route `scaledMargins` through the shared helper. Keep card-local responsive sizing, because it handles component layout rather than screen resolution.

- [ ] **Step 2: Scale theme QSS once**

Change `platformStyleSheet()` so the loaded baseline stylesheet passes through:

```cpp
return LTZKUiScale::scaleStyleSheetPixels(
    loadPlatformThemeQss(platformThemeKey(m_currentTheme)),
    LTZKUiScale::factorForWidget(this));
```

This scales fixed pixel fonts, padding, borders, and minimum sizes in the shared theme without manually editing every QSS rule.

- [ ] **Step 3: Add screen-change binding**

Declare these private methods in `LTZK.h`:

```cpp
void bindUiScaleToCurrentScreen();
void refreshUiScale();
```

Store the last applied scale and `QMetaObject::Connection` values for screen geometry/DPI signals. After the main window has a window handle, connect `QWindow::screenChanged`; reconnect `QScreen::geometryChanged` and `QScreen::logicalDotsPerInchChanged` whenever the screen changes.

`refreshUiScale()` must return early when the scale delta is below `0.001`; otherwise it must refresh the application font from an unscaled baseline, reapply the platform theme, update home utility metrics, update responsive home cards, and polish visible top-level windows. Do not resize a maximized main window manually.

- [ ] **Step 4: Run focused main-window tests**

Run:

```powershell
.\tests\verify_global_resolution_scaling.ps1
.\tests\verify_visual_scale_baseline_layout.ps1
.\tests\verify_platform_qss_extraction.ps1
.\tests\verify_platform_theme_switcher.ps1
.\tests\verify_home_module_cards_responsive_layout.ps1
.\tests\verify_workspace_resizable_panels.ps1
```

Expected: all listed tests pass.

### Task 5: Move the login dialog to the global screen scale

**Files:**
- Modify: `auth/LTZKLoginDialog.cpp`
- Modify: `auth/LTZKLoginDialog.h`
- Test: `tests/verify_login_dialog_responsive_layout.ps1`

- [ ] **Step 1: Replace self-size-derived scaling**

Include `../LTZKUiScale.h`. Replace:

```cpp
double LTZKLoginDialog::adaptiveScale() const
```

with a screen-derived method:

```cpp
double LTZKLoginDialog::uiScale() const
{
    return LTZKUiScale::factorForWidget(this);
}

int LTZKLoginDialog::scaled(int value) const
{
    return LTZKUiScale::scaledInt(value, uiScale());
}
```

Rename the header declaration accordingly. The resize event can retain layout refresh behavior, but resizing the dialog must no longer change its font or control scale.

- [ ] **Step 2: Scale the initial and minimum dialog sizes**

Compute the constructor scale after the dialog has a parent/screen fallback, then apply scaled versions of the 500×300 initial size and 440×260 minimum size. Existing layout metrics continue to use `scaled(...)`.

- [ ] **Step 3: Run login and global tests**

Run:

```powershell
.\tests\verify_login_dialog_responsive_layout.ps1
.\tests\verify_global_resolution_scaling.ps1
.\tests\verify_embedded_login_access_pages.ps1
```

Expected: all listed tests pass.

### Task 6: Audit platform-owned fixed UI metrics

**Files:**
- Modify: `LTZK.cpp`
- Test: `tests/verify_global_resolution_scaling.ps1`

- [ ] **Step 1: Inventory remaining screen UI fixed metrics**

Run:

```powershell
rg -n --glob '!x64/**' "resize\(|setFixed(Size|Width|Height)\(|setMinimum(Size|Width|Height)\(|setMaximum(Size|Width|Height)\(|font-size:\s*[0-9]+px" LTZK.cpp auth
```

Classify matches as platform screen UI or output/rendering code. Do not apply UI scaling to map labels or export geometry. Do not modify `map_product_manager_tools` or any other business-plugin source.

- [ ] **Step 2: Convert platform-owned visible metrics**

Convert platform-owned local stylesheets and fixed visual metrics through the shared helper. Preserve responsive workspace sizing and do not scale geometry data, map feature coordinates, or plugin-owned controls.

- [ ] **Step 3: Convert visible fixed pixel QSS owned by LTZK**

Where a platform widget applies a local stylesheet containing fixed `px` metrics, pass the baseline stylesheet through `LTZKUiScale::scaleStyleSheetPixels(style, LTZKUiScale::factorForWidget(widget))`. Do not repeatedly transform an already scaled string.

- [ ] **Step 4: Re-run fixed-metric inventory**

Run the same `rg` command. Also verify that no `.cpp` file under `map_product_manager_tools/ui_class` contains `LTZKUiScale`; any such reference is a scope-boundary test failure.

### Task 7: Build and regression verification

**Files:**
- Verify all files modified above.

- [ ] **Step 1: Run scaling and UI source tests**

Run:

```powershell
.\tests\verify_global_resolution_scaling.ps1
.\tests\verify_visual_scale_baseline_layout.ps1
.\tests\verify_login_dialog_responsive_layout.ps1
.\tests\verify_home_module_cards_responsive_layout.ps1
.\tests\verify_topbar_button_text_not_elided.ps1
.\tests\verify_workspace_topbar_scroll_shrinks.ps1
.\tests\verify_attribute_table_default_size.ps1
.\tests\verify_platform_qss_extraction.ps1
.\tests\verify_qgis_runtime_config_before_application.ps1
```

Expected: every script prints its success message and exits with code 0.

- [ ] **Step 2: Build Release x64**

Run:

```powershell
msbuild LTZK.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m:1
```

Expected: build succeeds with zero compiler/linker errors.

- [ ] **Step 3: Verify representative scaling calculations**

Confirm the utility produces these clamped logical scales:

```text
1366×768  -> approximately 0.711
1600×900  -> approximately 0.833
1920×1080 -> 1.000
2560×1440 -> approximately 1.333
3840×2160 -> 2.000
2560×1080 -> 1.000
```

- [ ] **Step 4: Review the final source diff**

Because no Git metadata is available, use file-by-file inspection and timestamp/output review. Confirm no output DPI, map-coordinate, printing, or export geometry code was accidentally routed through `LTZKUiScale`.

- [ ] **Step 5: Manual Windows smoke-test handoff**

Launch the built application normally and check 1920×1080 first. Then use Windows display settings or test machines to inspect 1366×768, 2560×1440, 4K at 100%, 4K at 200%, and an ultrawide resolution. Confirm there is no double scaling, clipping, unreadable text, or distorted map canvas.
