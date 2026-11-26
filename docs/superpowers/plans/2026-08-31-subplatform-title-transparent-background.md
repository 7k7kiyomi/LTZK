# 子平台标题背景透明化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the subplatform titles for the four business modules and permission management transparent across the sci-fi, black, and white themes while preserving existing title typography and accent lines.

**Architecture:** Keep the existing shared `dashboardTitle` object name and title update flow. Give the workspace title wrapper an explicit `workspaceTitleContainer` object name, then use scoped QSS rules to make both the label and wrapper transparent; the permission-management title continues to reuse `dashboardTitle` directly.

**Tech Stack:** Qt/C++, Qt Style Sheets (QSS), PowerShell static regression checks, Visual Studio/MSBuild when available.

---

### Task 1: Add the failing transparency contract test

**Files:**
- Create: `tests/verify_subplatform_title_transparent_background.ps1`
- Test: `LTZK.cpp`, `Resources/styles/ltzk_sci_fi.qss`, `Resources/styles/ltzk_black.qss`, `Resources/styles/ltzk_white.qss`

- [x] **Step 1: Write the failing static test**

The test must fail against the current source because the workspace title wrapper has no object name and the theme-specific `#dashboardTitle` rules do not explicitly declare a transparent background. It must verify:

```powershell
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content (Join-Path $root 'LTZK.cpp') -Raw -Encoding UTF8
$themeFiles = @(
    'Resources\styles\ltzk_sci_fi.qss',
    'Resources\styles\ltzk_black.qss',
    'Resources\styles\ltzk_white.qss'
)

if ($source -notmatch 'titleContainer->setObjectName\(QStringLiteral\("workspaceTitleContainer"\)\)') {
    throw 'Workspace subplatform title container must have an explicit transparent-style object name.'
}

$accessBlock = [regex]::Match(
    $source,
    'QWidget\* LTZK::buildAccessControlPage\(\)[\s\S]*?QWidget\* LTZK::buildSciFiWorkspacePage\(\)'
).Value
if ([string]::IsNullOrWhiteSpace($accessBlock) -or
    $accessBlock -notmatch 'titleLabel->setObjectName\(QStringLiteral\("dashboardTitle"\)\)') {
    throw 'Permission-management subplatform title must continue using dashboardTitle.'
}

foreach ($themeFile in $themeFiles) {
    $qss = Get-Content (Join-Path $root $themeFile) -Raw -Encoding UTF8

    $titleRule = [regex]::Match($qss, '#dashboardTitle\s*\{(?<body>[^}]*)\}')
    if (-not $titleRule.Success -or $titleRule.Groups['body'].Value -notmatch 'background:\s*transparent') {
        throw "$themeFile must make dashboardTitle background transparent."
    }

    $containerRule = [regex]::Match($qss, '#workspaceTitleContainer\s*\{(?<body>[^}]*)\}')
    if (-not $containerRule.Success -or
        $containerRule.Groups['body'].Value -notmatch 'background:\s*transparent' -or
        $containerRule.Groups['body'].Value -notmatch 'border:\s*0') {
        throw "$themeFile must make workspaceTitleContainer transparent and borderless."
    }
}

Write-Host 'Subplatform title transparency contract verified.'
```

- [x] **Step 2: Run the new test and confirm the expected failure**

Run from `E:\AAALTZK`:

```powershell
.\tests\verify_subplatform_title_transparent_background.ps1
```

Expected result before implementation: a terminating error stating that the workspace title container is missing its explicit transparent-style object name.

### Task 2: Implement the shared transparent title styling

**Files:**
- Modify: `LTZK.cpp:3640-3652` (workspace title container creation)
- Modify: `Resources/styles/ltzk_sci_fi.qss:64-65`
- Modify: `Resources/styles/ltzk_black.qss:58-59`
- Modify: `Resources/styles/ltzk_white.qss:58-59`

- [x] **Step 1: Name the workspace title container**

Immediately after constructing the existing title container, add:

```cpp
QWidget* titleContainer = new QWidget(topBar);
titleContainer->setObjectName(QStringLiteral("workspaceTitleContainer"));
```

Keep the existing layout, title label, glow, alignment, and module-title update logic unchanged. The permission-management page already creates its title label with `dashboardTitle`; do not add a separate access-control title style.

- [x] **Step 2: Add explicit transparent rules to all themes**

Keep every existing color, font, left border, padding, and alignment declaration. Extend each theme with the following object-level rule near `#dashboardTitle`:

```qss
#workspaceTitleContainer { background: transparent; border: 0; }
```

Add `background: transparent;` inside each existing `#dashboardTitle` rule. The resulting title rules must retain the current theme accent declarations. The exact rule bodies should be:

```qss
/* sci-fi theme */
#dashboardTitle { font-family: "Microsoft YaHei UI", "Microsoft YaHei", sans-serif; font-size: 36px; font-weight: 700; letter-spacing: 0; color: #D9F7FF; background: transparent; border-left: 4px solid #55D9FF; padding-left: 14px; qproperty-alignment: AlignLeft; }

/* black theme */
#dashboardTitle { font-family: "Microsoft YaHei UI", "Microsoft YaHei", sans-serif; font-size: 36px; font-weight: 700; letter-spacing: 0; color: #F3F4F6; background: transparent; border-left: 4px solid #D97706; padding-left: 14px; qproperty-alignment: AlignLeft; }

/* white theme */
#dashboardTitle { font-family: "Microsoft YaHei UI", "Microsoft YaHei", sans-serif; font-size: 36px; font-weight: 700; letter-spacing: 0; color: #1E3A5F; background: transparent; border-left: 4px solid #2563EB; padding-left: 14px; qproperty-alignment: AlignLeft; }
```

The sci-fi theme keeps `#55D9FF`, the black theme keeps `#D97706`, and the white theme keeps `#2563EB` as the left border color.

### Task 3: Run focused verification and inspect the diff

**Files:**
- Test: `tests/verify_subplatform_title_transparent_background.ps1`
- Update: `tests/verify_workspace_module_title_style.ps1`, `tests/verify_access_control_title_style.ps1`
- Inspect: `LTZK.cpp`, `Resources/styles/ltzk_sci_fi.qss`, `Resources/styles/ltzk_black.qss`, `Resources/styles/ltzk_white.qss`

- [x] **Step 1: Run the transparency contract test**

Run:

```powershell
.\tests\verify_subplatform_title_transparent_background.ps1
```

Expected result: `Subplatform title transparency contract verified.` and exit code 0.

- [x] **Step 2: Run related existing title checks**

Run:

```powershell
.\tests\verify_all_subplatform_titles.ps1
.\tests\verify_workspace_module_title_style.ps1
.\tests\verify_access_control_title_style.ps1
```

Expected result: all three scripts complete successfully, confirming that the title text, shared object name, glow, and permission-management style contract remain intact. The two legacy scripts were synchronized with the current QSS-based title implementation and current permission-management title text before this verification.

- [x] **Step 3: Check the final diff for whitespace and scope**

If Git metadata is available, run:

```powershell
git diff --check
git diff -- LTZK.cpp Resources/styles/ltzk_sci_fi.qss Resources/styles/ltzk_black.qss Resources/styles/ltzk_white.qss tests/verify_subplatform_title_transparent_background.ps1
```

In the current workspace `git status` reports that `E:\AAALTZK` is not a Git repository, so the diff command cannot run; use the changed-file list and file contents to confirm that no unrelated files were modified.

### Task 4: Build validation

**Files:**
- Build: `LTZK.sln`

- [x] **Step 1: Locate the configured MSBuild tool**

Run:

```powershell
Get-ChildItem -Path 'C:\Program Files\Microsoft Visual Studio','C:\Program Files (x86)\Microsoft Visual Studio' -Filter 'MSBuild.exe' -Recurse -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
```

- [x] **Step 2: Build the existing Release configuration when available**

Run from `E:\AAALTZK`:

```powershell
msbuild .\LTZK.sln /m /p:Configuration=Release /p:Platform=x64
```

Expected result: build succeeds. The initial sandboxed attempt could not write the existing build log; the elevated retry succeeded with 0 warnings and 0 errors. If the configured compiler, Qt, or QGIS dependencies are unavailable in a future run, preserve the source/test results and report the exact build error instead of changing project configuration.

- [x] **Step 3: Record completion status**

Report the changed files, focused test result, related test result, and whether Release build validation was available. Do not claim a Git commit because the current workspace has no `.git` directory.
