$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -Raw (Join-Path $root 'LTZK.cpp')

function Assert-Contains([string]$text, [string]$needle, [string]$message) {
    if (-not $text.Contains($needle)) { throw $message }
}

function Assert-DoesNotContain([string]$text, [string]$needle, [string]$message) {
    if ($text.Contains($needle)) { throw $message }
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
    Assert-Contains $qss 'QToolButton[topBarTool="true"]:hover' "$theme must style the visible workspace action buttons on hover."
    Assert-Contains $qss 'QToolButton[topBarTool="true"][topBarRaised="true"]' "$theme must keep the raised state explicitly styled."
    Assert-Contains $qss 'border-bottom: 4px solid' "$theme must draw a visible lower support edge on raised actions."
}

$sciFiQss = Get-Content -Raw (Join-Path $root 'Resources/styles/ltzk_sci_fi.qss')
Assert-Contains $sciFiQss 'border: 2px solid transparent' 'Sci-Fi base menu item must reserve the enhanced hover border.'
Assert-Contains $sciFiQss 'border: 2px solid #9af1ff' 'Sci-Fi hover must use an enhanced bright cyan border.'
Assert-Contains $sciFiQss 'background: rgba(42, 197, 255, 155)' 'Sci-Fi hover must use a high-contrast cyan fill.'

Assert-Contains $source 'class TopBarFloatingHoverEffect' 'Visible top bar buttons must own the floating hover effect.'
Assert-Contains $source 'QEvent::Enter' 'The top bar floating effect must respond to pointer entry.'
Assert-Contains $source 'QEvent::Leave' 'The top bar floating effect must reset after pointer exit.'
Assert-Contains $source 'QPropertyAnimation' 'The raised shadow must animate instead of changing abruptly.'
Assert-Contains $source 'installTopBarFloatingHoverEffect(m_workspaceLogoutButton)' 'Logout button must get the visible top bar hover effect.'
Assert-Contains $source 'installTopBarFloatingHoverEffect(backHomeButton)' 'Return-home button must get the visible top bar hover effect.'
Assert-Contains $source 'installTopBarFloatingHoverEffect(button)' 'Dynamic plugin buttons must get the visible top bar hover effect.'
Assert-Contains $source 'topActionContentHeight = topActionButtonHeight' 'The top action strip must align flush with the workspace bar.'
Assert-DoesNotContain $source 'topActionContentHeight = topActionButtonHeight + (topActionHoverOverflow * 2)' 'The top action strip must not reserve a floating gap.'
Assert-Contains $source 'refreshTopBarFloatingHoverStyles(this, themeKey)' 'Theme changes must refresh visible top bar hover colors.'
Assert-DoesNotContain $source 'FloatingMenuBarOverlay' 'The effect must not target the hidden QMenuBar.'
Assert-DoesNotContain $source 'installFloatingMenuBarOverlay(menuBar())' 'The effect must not be installed on the hidden QMenuBar.'

Write-Output 'Top bar hover lift static checks passed.'
