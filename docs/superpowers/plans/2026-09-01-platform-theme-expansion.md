# LTZK Platform Theme Expansion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the user-approved Field Notes and Steel & Warm White themes while preserving the existing three themes, page content, layout, and business behavior.

**Architecture:** Extend the existing `PlatformTheme` enum and key-based QSS loader, add one complete QSS file per new theme, register both files in the Qt resource collection, and update the existing custom-paint/dynamic-style branches so new light themes do not fall into the Sci-Fi default path. Keep the current application and resource-loading boundaries; do not refactor page construction or plugin logic.

**Tech Stack:** C++/Qt, Qt Style Sheets (QSS), Qt Resource System (`.qrc`), MSBuild/Visual Studio solution, PowerShell static verification scripts.

---

### Task 1: Add a failing static acceptance test

**Files:**
- Create: `tests/verify_platform_theme_expansion.ps1`
- Read: `LTZK.h`, `LTZK.cpp`, `LTZK.qrc`, `Resources/styles/ltzk_field_notes.qss`, `Resources/styles/ltzk_steel_warm_white.qss`

- [ ] **Step 1: Write the test script before implementation**

Create a PowerShell script that fails with named diagnostics until the enum, resources, mappings, five-item theme list, and explicit new-theme branches exist:

```powershell
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$header = Get-Content -Raw (Join-Path $root 'LTZK.h')
$source = Get-Content -Raw (Join-Path $root 'LTZK.cpp')
$qrc = Get-Content -Raw (Join-Path $root 'LTZK.qrc')

function Assert-Contains([string]$text, [string]$needle, [string]$message) {
    if (-not $text.Contains($needle)) { throw $message }
}

function Assert-File([string]$relativePath) {
    $path = Join-Path $root $relativePath
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing file: $relativePath" }
}

Assert-Contains $header 'FieldNotes' 'PlatformTheme must contain FieldNotes.'
Assert-Contains $header 'SteelWarmWhite' 'PlatformTheme must contain SteelWarmWhite.'
Assert-File 'Resources/styles/ltzk_field_notes.qss'
Assert-File 'Resources/styles/ltzk_steel_warm_white.qss'
Assert-Contains $qrc 'Resources/styles/ltzk_field_notes.qss' 'Field Notes QSS must be registered in LTZK.qrc.'
Assert-Contains $qrc 'Resources/styles/ltzk_steel_warm_white.qss' 'Steel & Warm White QSS must be registered in LTZK.qrc.'
Assert-Contains $source 'field-notes' 'Field Notes key mapping is missing.'
Assert-Contains $source 'steel-warm-white' 'Steel & Warm White key mapping is missing.'
Assert-Contains $source '绿色系' 'Green theme label is missing.'
Assert-Contains $source '灰色系' 'Gray theme label is missing.'
Assert-Contains $source 'PlatformTheme::FieldNotes' 'Field Notes selection entry or branch is missing.'
Assert-Contains $source 'PlatformTheme::SteelWarmWhite' 'Steel & Warm White selection entry or branch is missing.'
Assert-Contains $source '#C6D8D2' 'Steel & Warm White title color is missing.'

$fieldQss = Get-Content -Raw (Join-Path $root 'Resources/styles/ltzk_field_notes.qss')
$steelQss = Get-Content -Raw (Join-Path $root 'Resources/styles/ltzk_steel_warm_white.qss')
Assert-Contains $fieldQss '#3D5540' 'Field Notes forest green palette is missing.'
Assert-Contains $fieldQss '#F4EBD7' 'Field Notes paper palette is missing.'
Assert-Contains $steelQss '#34464B' 'Steel & Warm White steel palette is missing.'
Assert-Contains $steelQss '#F4F2EB' 'Steel & Warm White warm-white palette is missing.'

Write-Output 'Platform theme expansion static checks passed.'
```

- [ ] **Step 2: Run the test and verify that it fails for the expected missing theme work**

Run:

```powershell
pwsh -NoProfile -File .\tests\verify_platform_theme_expansion.ps1
```

Expected: FAIL at the first missing enum member, before any source implementation is changed.

### Task 2: Add complete QSS themes

**Files:**
- Create: `Resources/styles/ltzk_field_notes.qss`
- Create: `Resources/styles/ltzk_steel_warm_white.qss`

- [ ] **Step 1: Create both files from the existing complete selector surface**

Use `Resources/styles/ltzk_white.qss` as the selector/spacing baseline so both new files cover the same controls as the existing light theme. Keep selectors and behavior rules equivalent; change only palette, borders, backgrounds, selection colors, title colors, and non-functional visual details.

- [ ] **Step 2: Apply the Field Notes palette**

Use these values consistently: paper `#F4EBD7`, forest `#3D5540`, sage `#6C7956`, map `#E6DFC5`, clay `#B86F4F`, text `#3E4D3D`. Use restrained rounded corners and no Sci-Fi glow/gradient treatment.

- [ ] **Step 3: Apply the Steel & Warm White palette**

Use steel `#34464B`, side steel `#5B6C70`, warm white `#F4F2EB`, work area `#E4E7E3`, title/brand `#C6D8D2`, helper teal `#668B8C`. Keep orange out of title/brand styling; reserve a warm warning color only for semantic warning or key feedback states.

### Task 3: Register and expose the two themes

**Files:**
- Modify: `LTZK.h:130-136`
- Modify: `LTZK.cpp:2020-2032`
- Modify: `LTZK.cpp:2586-2631`
- Modify: `LTZK.cpp:2678-2684`
- Modify: `LTZK.qrc:54-56`

- [ ] **Step 1: Extend the enum without changing existing order or default**

Add `FieldNotes` and `SteelWarmWhite` after `SciFi`, leaving `m_currentTheme = PlatformTheme::SciFi` unchanged.

- [ ] **Step 2: Map keys to QSS filenames**

Extend `platformThemeQssFileName()` with exact mappings:

```cpp
if (themeKey == QStringLiteral("field-notes"))
{
    return QStringLiteral("ltzk_field_notes.qss");
}
if (themeKey == QStringLiteral("steel-warm-white"))
{
    return QStringLiteral("ltzk_steel_warm_white.qss");
}
```

Keep `white`, `black`, and the Sci-Fi fallback unchanged.

- [ ] **Step 3: Add enum-to-key, enum-to-label, and key-to-enum cases**

Use exact pairs:

```cpp
case PlatformTheme::FieldNotes:
    return QStringLiteral("field-notes");
case PlatformTheme::SteelWarmWhite:
    return QStringLiteral("steel-warm-white");
```

and labels:

```cpp
case PlatformTheme::FieldNotes:
    return QStringLiteral("绿色系");
case PlatformTheme::SteelWarmWhite:
    return QStringLiteral("灰色系");
```

`platformThemeFromKey()` must recognize both exact keys and retain Sci-Fi fallback for unknown values.

- [ ] **Step 4: Add both enum values to the theme dialog list**

Append both values to the existing `themes` initializer. Do not remove or reorder the existing three options.

- [ ] **Step 5: Register both QSS resources**

Add both `<file>` entries under the existing `Resources/styles` resource prefix in `LTZK.qrc`.

### Task 4: Make custom-painted and dynamic style paths theme-aware

**Files:**
- Modify: `LTZK.h` near the existing theme helper declarations
- Modify: `LTZK.cpp` in `SciFiHomePage::paintEvent()` and `SciFiHomeHeader::paintEvent()`
- Modify: `LTZK.cpp` in `titleLabelStyle()`, `pluginWindowGlowColor()`, map canvas color branches, and embedded-control styling branches

- [ ] **Step 1: Add explicit light-theme classification helpers**

Add a helper whose result is true for `White`, `FieldNotes`, and `SteelWarmWhite`, and use it only where the current code means “light canvas/light text treatment.” Keep `Black` and `SciFi` explicit where their behavior differs.

- [ ] **Step 2: Add Field Notes and Steel & Warm White custom-paint palettes**

In the homepage/background and header painters, branch on `field-notes` and `steel-warm-white` before the existing Sci-Fi fallback. Use the approved paper/forest palette for Field Notes and steel/warm-white/teal for Steel & Warm White. Keep the existing White, Black, and Sci-Fi branches byte-for-byte behaviorally equivalent.

- [ ] **Step 3: Update dynamic plugin titles and visual effects**

Ensure dynamically overlaid plugin section titles, group-box titles, plugin panel borders, and glow effects use the new palette. New light themes must not inherit Sci-Fi cyan glow or dark title text. Steel & Warm White title/brand text must use `#C6D8D2` where it is rendered on the steel header.

- [ ] **Step 4: Update map canvas and embedded widget style decisions**

Use the light-theme classification when `m_workspaceUsesLightShpCanvas` is false: use `#E6DFC5` for `FieldNotes`, `#E4E7E3` for `SteelWarmWhite`, and keep the existing White/Black/Sci-Fi colors otherwise. Keep user-selected custom canvas colors unchanged.

### Task 5: Run static tests and compile

**Files:**
- Verify: `tests/verify_platform_theme_expansion.ps1`
- Build: `LTZK.sln`

- [ ] **Step 1: Run the static test**

Run:

```powershell
pwsh -NoProfile -File .\tests\verify_platform_theme_expansion.ps1
```

Expected: `Platform theme expansion static checks passed.`

- [ ] **Step 2: Build the solution**

Run the existing approved build command for the solution, or use:

```powershell
msbuild .\LTZK.sln /m /p:Configuration=Release /p:Platform=x64
```

Expected: exit code 0 with no resource compilation errors.

- [ ] **Step 3: Inspect the diff scope**

Run:

```powershell
git diff -- Resources/styles/ltzk_field_notes.qss Resources/styles/ltzk_steel_warm_white.qss LTZK.qrc LTZK.h LTZK.cpp tests/verify_platform_theme_expansion.ps1
```

If Git metadata is unavailable, inspect the listed files directly and confirm no page/business files changed.

### Task 6: Runtime acceptance checks

- [ ] **Step 1: Launch the built application and open “风格选择”**

Confirm five options are shown: 科幻系、黑色系、白色系、绿色系、灰色系。

- [ ] **Step 2: Switch to 绿色系**

Confirm the homepage, workspace, map canvas, tables, trees, inputs, dialogs, plugin windows, and dynamically generated titles use the paper/forest/sage palette without Sci-Fi cyan glow.

- [ ] **Step 3: Switch to 灰色系**

Confirm the steel/warm-white palette is applied and the title/brand color is `#C6D8D2`; confirm orange is not used as the title/brand color.

- [ ] **Step 4: Verify persistence and regression**

Restart after selecting each new theme and confirm it restores correctly. Switch back through 科幻系、黑色系、白色系 and confirm each original theme remains available and visually unchanged.
