# Data Catalog Icon Buttons Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the four data-catalog header text actions with 28×28 icon-only Qt tool buttons while preserving tooltips, accessibility names, existing signal bindings, and all other text actions.

**Architecture:** Reuse three existing QGIS-style SVG assets through the application resource file. Centralize icon-only button sizing and accessibility setup in the existing `createCatalogButton` helper, then pass explicit icons at the four `buildUi()` call sites. Keep all action slots and data flow unchanged; update only the scoped toolbar QSS and static verification contract.

**Tech Stack:** C++/Qt `QToolButton`, Qt Resource Collection (`LTZK.qrc`), Qt style sheets, PowerShell static verification, Visual Studio/MSBuild.

---

## File map

- Modify: `tests/verify_workspace_data_catalog.ps1` — define the failing and passing source contract for icon-only catalog buttons and load `LTZK.qrc` for resource assertions.
- Modify: `LTZK.qrc` — register `mActionRefresh.svg`, `mIconFolderLink.svg`, and `mActionUnlink.svg` under the existing `/LTZK` resource prefix.
- Modify: `DataCatalogWidget.cpp:361-385` — replace text-width sizing with icon, tooltip, accessibility, and fixed-size setup; update four `buildUi()` button constructions around lines 1024 and 1126.
- Modify: `Resources/styles/ltzk_sci_fi.qss`, `ltzk_black.qss`, `ltzk_white.qss`, `ltzk_field_notes.qss`, `ltzk_steel_warm_white.qss` — reduce `catalogToolbarButton` padding for the fixed icon control without changing colors or hover states.
- Create: `docs/superpowers/specs/2026-09-22-data-catalog-icon-buttons-design.md` — approved design record already written before implementation.

### Task 1: Add the failing static contract

**Files:**
- Modify: `tests/verify_workspace_data_catalog.ps1`

- [x] **Step 1: Add assertions that describe the icon-only helper and four resource mappings.**

Before the existing `$headerText`/`$catalogText` reads, add the QRC path and text load next to the other source paths:

```powershell
$qrcFile = Join-Path $root 'LTZK.qrc'
```

Add the QRC file to the existing source-file existence check and load it with the other source text:

```powershell
if (-not (Test-Path -LiteralPath $qrcFile)) {
    throw "Missing Qt resource collection: $qrcFile"
}
$qrcText = Get-Content -Raw -Encoding UTF8 $qrcFile
```

Insert the following block after the existing `$buildUiText` header checks and before the old `compactCatalogButton` assertion block:

```powershell
foreach ($required in @(
    'button->setIcon(QIcon(iconPath));',
    'button->setIconSize(QSize(18, 18));',
    'button->setToolButtonStyle(Qt::ToolButtonIconOnly);',
    'button->setFixedSize(28, 28);',
    'button->setAccessibleName(accessibleName);',
    'button->setText(QString());',
    ':/LTZK/Resources/mActionRefresh.svg',
    ':/LTZK/Resources/mIconFolderLink.svg',
    ':/LTZK/Resources/mActionUnlink.svg'
)) {
    if (-not $catalogText.Contains($required)) {
        throw "Data catalog icon button contract is missing: $required"
    }
}

if ($catalogText.Contains('compactCatalogButton(') -or
    $catalogText.Contains('fontMetrics().horizontalAdvance(button->text())')) {
    throw 'Data catalog icon buttons must not use text-width sizing.'
}

foreach ($resource in @(
    'Resources/mActionRefresh.svg',
    'Resources/mIconFolderLink.svg',
    'Resources/mActionUnlink.svg'
)) {
    if (-not $qrcText.Contains($resource)) {
        throw "Catalog icon resource is missing from LTZK.qrc: $resource"
    }
}
```

The existing block that requires `compactCatalogButton` must be replaced by the new negative assertion; otherwise the old behavior would remain required.

- [x] **Step 2: Run the focused verification and confirm it fails for the missing icon contract.**

Run:

```powershell
.\tests\verify_workspace_data_catalog.ps1
```

Expected result: `FAIL`/terminating error naming a missing icon-button contract or missing QRC resource. The failure must come from the new assertions, not from a syntax error.

### Task 2: Register existing icon assets

**Files:**
- Modify: `LTZK.qrc`

- [x] **Step 1: Add the three existing SVG files to the `/LTZK` resource collection.**

Add these entries near the other action icons:

```xml
<file>Resources/mActionRefresh.svg</file>
<file>Resources/mIconFolderLink.svg</file>
<file>Resources/mActionUnlink.svg</file>
```

- [x] **Step 2: Confirm all three source files exist and the QRC paths match their case.**

Run:

```powershell
Test-Path -LiteralPath 'Resources\mActionRefresh.svg'
Test-Path -LiteralPath 'Resources\mIconFolderLink.svg'
Test-Path -LiteralPath 'Resources\mActionUnlink.svg'
```

Expected result: three `True` values.

### Task 3: Convert the catalog buttons to icon-only controls

**Files:**
- Modify: `DataCatalogWidget.cpp:361-385`
- Modify: `DataCatalogWidget.cpp:1024-1033`
- Modify: `DataCatalogWidget.cpp:1126-1146`

- [x] **Step 1: Replace the helper signature and implementation with the minimal fixed icon contract.**

Replace the current helper with:

```cpp
QToolButton* createCatalogButton(
    const QString& iconPath,
    const QString& accessibleName,
    const QString& tooltip,
    QWidget* parent)
{
    QToolButton* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("catalogToolbarButton"));
    button->setProperty("catalogToolbarButton", true);
    button->setIcon(QIcon(iconPath));
    button->setIconSize(QSize(18, 18));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setFixedSize(28, 28);
    button->setAccessibleName(accessibleName);
    button->setText(QString());
    button->setToolTip(tooltip);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoRaise(true);
    return button;
}
```

Delete `compactCatalogButton(QToolButton*)`; icon controls no longer calculate widths from text.

- [x] **Step 2: Pass explicit icons and accessible names at each build site.**

Use these four calls, preserving the existing local variables and parent widgets:

```cpp
QToolButton* refreshDatabaseButton = createCatalogButton(
    QStringLiteral(":/LTZK/Resources/mActionRefresh.svg"),
    QStringLiteral("刷新"),
    QStringLiteral("刷新数据库数据"),
    databaseSection);
```

```cpp
QToolButton* connectFolderButton = createCatalogButton(
    QStringLiteral(":/LTZK/Resources/mIconFolderLink.svg"),
    QStringLiteral("连接文件夹"),
    QStringLiteral("连接一个本地数据文件夹"),
    localSection);
m_disconnectFolderButton = createCatalogButton(
    QStringLiteral(":/LTZK/Resources/mActionUnlink.svg"),
    QStringLiteral("断开连接"),
    QStringLiteral("断开当前选中的文件夹连接"),
    localSection);
QToolButton* refreshLocalButton = createCatalogButton(
    QStringLiteral(":/LTZK/Resources/mActionRefresh.svg"),
    QStringLiteral("刷新"),
    QStringLiteral("刷新本地数据"),
    localSection);
```

Remove the three `compactCatalogButton(...)` calls. Leave the `createSectionHeader(...)` calls and all four `connect(...)` calls unchanged.

- [x] **Step 3: Run the focused verification and confirm it is green.**

Run:

```powershell
.\tests\verify_workspace_data_catalog.ps1
```

Expected result: the script completes without throwing an error from the icon-button contract. If it reports a different pre-existing assertion, fix only the mismatch caused by the helper/call-site change and rerun.

### Task 4: Tune all catalog toolbar themes

**Files:**
- Modify: `Resources/styles/ltzk_sci_fi.qss`
- Modify: `Resources/styles/ltzk_black.qss`
- Modify: `Resources/styles/ltzk_white.qss`
- Modify: `Resources/styles/ltzk_field_notes.qss`
- Modify: `Resources/styles/ltzk_steel_warm_white.qss`

- [x] **Step 1: Change only the catalog toolbar padding in each theme.**

In every `QToolButton#catalogToolbarButton` rule, change the existing `padding: 3px 6px;` declaration to:

```css
padding: 2px;
```

Keep each theme's existing color, background, border, and hover declarations unchanged.

- [x] **Step 2: Run the static verification again.**

Run:

```powershell
.\tests\verify_workspace_data_catalog.ps1
```

Expected result: `PASS`/no terminating error, including the five-theme checks.

### Task 5: Verify build and inspect the final diff

**Files:**
- Inspect: `LTZK.qrc`, `DataCatalogWidget.cpp`, `tests/verify_workspace_data_catalog.ps1`, and the five theme files.

- [x] **Step 1: Check for unintended old text-width behavior or out-of-scope changes.**

Run:

```powershell
rg -n "compactCatalogButton|horizontalAdvance\(button->text\(\)\)|mActionRefresh.svg|mIconFolderLink.svg|mActionUnlink.svg|ToolButtonIconOnly|setFixedSize\(28, 28\)" DataCatalogWidget.cpp LTZK.qrc tests\verify_workspace_data_catalog.ps1
```

Expected result: only the new resource mappings, icon-only helper, fixed-size setup, and negative test contract remain; no `compactCatalogButton` implementation or call remains.

- [x] **Step 2: Run the project build if the configured Visual Studio toolchain is available.**

Run:

```powershell
msbuild .\LTZK.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m
```

Expected result: build succeeds. If the environment lacks the configured Qt/QGIS or MSBuild dependency, record the exact toolchain error and retain the passing static verification as the available evidence.

Observed: `msbuild` and `devenv` are not available in the current environment, so compilation could not be run.

- [x] **Step 3: Review the final source diff.**

Run:

```powershell
git -C E:\AAALTZK diff -- LTZK.qrc DataCatalogWidget.cpp tests/verify_workspace_data_catalog.ps1 Resources\styles
```

Expected result: this reports that the workspace is not a Git repository; use direct file inspection and the focused verification output instead. No commit is attempted because Git metadata is absent.
