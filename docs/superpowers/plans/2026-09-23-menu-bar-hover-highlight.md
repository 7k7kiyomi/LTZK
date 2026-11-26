# 菜单栏悬停高亮增强实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为五套平台主题中实际显示的顶部功能按钮增加方案 B 的明显悬停浮起效果。

**Architecture:** 实际可见功能栏由 `QToolButton[topBarTool="true"]` 组成。五套主题为这些按钮提供强化边框、顶部亮边和底部支撑线；`LTZK.cpp` 的 `TopBarFloatingHoverEffect` 在指针进入/离开时动画调整阴影。滚动内容上下预留空间，主题切换时同步刷新效果颜色。主 `QMenuBar` 初始化时隐藏，因此不作为此交互的目标。

**Tech Stack:** Qt 5 C++、Qt Style Sheets、PowerShell 静态验证、MSBuild/Visual Studio。

---

### Task 1: 添加失败的菜单栏高亮静态测试

**Files:**
- Create: `tests/verify_menu_bar_hover_highlight.ps1`
- Read: `Resources/styles/ltzk_sci_fi.qss`
- Read: `Resources/styles/ltzk_white.qss`
- Read: `Resources/styles/ltzk_black.qss`
- Read: `Resources/styles/ltzk_field_notes.qss`
- Read: `Resources/styles/ltzk_steel_warm_white.qss`

- [x] **Step 1: Write the failing test**

Create the following script. It checks each QSS file for the base item rule and the three interaction states, and verifies that the hover/selected rules contain both a border and a background declaration.

```powershell
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Assert-Contains([string]$text, [string]$needle, [string]$message) {
    if (-not $text.Contains($needle)) { throw $message }
}

$themes = @(
    'ltzk_sci_fi.qss',
    'ltzk_white.qss',
    'ltzk_black.qss',
    'ltzk_field_notes.qss',
    'ltzk_steel_warm_white.qss'
)

foreach ($theme in $themes) {
    $relativePath = Join-Path 'Resources/styles' $theme
    $path = Join-Path $root $relativePath
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing theme QSS: $relativePath" }

    $qss = Get-Content -Raw $path
    Assert-Contains $qss 'QMenuBar::item {' "$theme must define a stable base menu item rule."
    Assert-Contains $qss 'QMenuBar::item:hover' "$theme must define a menu item hover rule."
    Assert-Contains $qss 'QMenuBar::item:selected' "$theme must define a menu item selected rule."
    Assert-Contains $qss 'QMenuBar::item:pressed' "$theme must define a menu item pressed rule."
}

$sciFiQss = Get-Content -Raw (Join-Path $root 'Resources/styles/ltzk_sci_fi.qss')
Assert-Contains $sciFiQss 'border: 2px solid transparent' 'Sci-Fi base menu item must reserve the enhanced hover border. '
Assert-Contains $sciFiQss 'border: 2px solid #9af1ff' 'Sci-Fi hover must use an enhanced bright cyan border. '
Assert-Contains $sciFiQss 'background: rgba(42, 197, 255, 155)' 'Sci-Fi hover must use a high-contrast cyan fill. '

Write-Output 'Menu bar hover highlight static checks passed.'
```

- [x] **Step 2: Run the test and verify the expected failure**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_menu_bar_hover_highlight.ps1
```

Expected: FAIL because the current QSS files do not contain `QMenuBar::item:hover` or the other new top-level menu item rules.

### Task 2: Implement the approved scheme B in all themes

**Files:**
- Modify: `Resources/styles/ltzk_sci_fi.qss:95`
- Modify: `Resources/styles/ltzk_white.qss:78`
- Modify: `Resources/styles/ltzk_black.qss:78`
- Modify: `Resources/styles/ltzk_field_notes.qss:151`
- Modify: `Resources/styles/ltzk_steel_warm_white.qss:151`
- Modify: `LTZK.cpp` (install animated hover shadows on visible top bar buttons and reserve shadow space)

- [x] **Step 1: Add the Sci-Fi menu item states**

Immediately after the Sci-Fi `QMenuBar` rule, add:

```css
QMenuBar::item {
    padding: 4px 12px;
    border: 2px solid transparent;
    border-radius: 3px;
}
QMenuBar::item:hover, QMenuBar::item:selected {
    background: rgba(42, 197, 255, 155);
    border: 2px solid #9af1ff;
    color: #ffffff;
}
QMenuBar::item:pressed {
    background: rgba(42, 197, 255, 215);
    border: 2px solid #e4fbff;
    color: #ffffff;
}
```

- [x] **Step 2: Add theme-matched states to White and Black**

Use the same selectors and geometry. White uses `#2563eb`/`#dbeafe`/`#1e3a8a`; Black uses the existing warm accent `#f59e0b`/`#78350f`/`#fef3c7`. Keep the bright border and filled hover treatment structurally identical to Sci-Fi.

- [x] **Step 3: Add theme-matched states to Field Notes and Steel & Warm White**

Add the rules after each theme’s final grouped `QMenuBar, QToolBar` rule so they override the base light-theme declaration. Use Field Notes `#B86F4F`/`#F4EBD7`/`#3D5540` and Steel & Warm White `#668B8C`/`#B9CFCB`/`#213437` for border, fill, and text respectively.

- [x] **Step 4: Preserve dropdown menu behavior**

Do not alter existing `QMenu::item` or `QMenu::item:selected` declarations. The new rules must target only `QMenuBar::item`.

- [x] **Step 5: Apply the floating effect to the visible top bar buttons**

Remove the overlay from the hidden `QMenuBar`. Install `TopBarFloatingHoverEffect` on dynamic plugin buttons, Return Home, and Logout. Animate the shadow blur and offset and use the theme QSS raised state for the bright rim and lower rail. Add vertical room to the scroll content so the shadow is not clipped.

### Task 3: Run the red-green verification and compile resources

**Files:**
- Verify: `tests/verify_menu_bar_hover_highlight.ps1`
- Build: `LTZK.sln`

- [ ] **Step 1: Run the static test after the visible-button correction**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_menu_bar_hover_highlight.ps1
```

Expected: `Menu bar hover highlight static checks passed.` with exit code 0.

- [x] **Step 2: Build the Release x64 solution after the correction**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Msbuild\Current\Bin\amd64\MSBuild.exe' .\LTZK.sln /m /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143
```

Expected: exit code 0, including successful Qt resource compilation of the five QSS files. Confirmed with the current source: 0 warnings, 0 errors.

- [x] **Step 3: Inspect the changed-file scope**

Run:

```powershell
rg -n "QMenuBar::item|QMenu::item" Resources/styles tests/verify_menu_bar_hover_highlight.ps1
```

Confirm that the change is limited to the five theme QSS files and the new verification script, and that no `QMenu::item` rule was removed.

### Task 4: Runtime acceptance

- [ ] **Step 1: Open the rebuilt application and hover visible workspace actions**

Move the pointer across plugin actions, Return Home, and Logout. Confirm each button's border, top highlight, lower rail, and animated shadow make it visibly rise without moving neighboring buttons.

- [ ] **Step 2: Verify all theme variants**

Switch to White, Black, Field Notes, and Steel & Warm White. Confirm the same raised treatment and readable text use each theme's accent color.

- [ ] **Step 3: Verify regression boundaries**

Open plugin actions and exercise Return Home and Logout. Confirm the existing actions still trigger normally.
