$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'WorkspaceLayerTools.cpp'
if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding UTF8 $source

foreach ($required in @(
    'constexpr int kWorkspaceToolbarStateVersion = 4',
    'editingToolbar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed)',
    'toolbarMenuBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)',
    'toolbarMenuSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred)',
    'toolbarMenuBar->addWidget(toolbarMenuSpacer)',
    'void LTZK::applyDefaultWorkspaceToolbarLayout()',
    'm_mapCanvasToolBarHost->removeToolBarBreak(editingToolbar)',
    'm_mapCanvasToolBarHost->addToolBarBreak(Qt::TopToolBarArea)',
    'm_mapCanvasToolBarHost->addToolBar(Qt::TopToolBarArea, toolbarMenuBar)',
    'applyDefaultWorkspaceToolbarLayout();',
    'mapToolbarState, kWorkspaceToolbarStateVersion)',
    'shouldPersistDefaultToolbarLayout = !m_mapCanvasToolBarHost->restoreState(',
    'saveState(kWorkspaceToolbarStateVersion)'
)) {
    if (-not $text.Contains($required)) {
        throw "First-row toolbar menu and left-aligned edit row requirement missing: $required"
    }
}

$resetStart = $text.IndexOf('void LTZK::resetWorkspaceToolbarLayout()')
$resetEnd = $text.IndexOf('void LTZK::saveMapWorkspaceToolbarState()', $resetStart)
if ($resetStart -lt 0 -or $resetEnd -lt 0) {
    throw 'Could not locate the toolbar reset implementation.'
}
$resetBody = $text.Substring($resetStart, $resetEnd - $resetStart)
if (-not $resetBody.Contains('applyDefaultWorkspaceToolbarLayout();')) {
    throw 'Reset to default must restore the first-row toolbar menu and left-aligned edit row.'
}

if (-not $text.Contains('if (shouldPersistDefaultToolbarLayout)')) {
    throw 'An incompatible saved toolbar layout must be migrated and persisted after defaults are applied.'
}

if ($text.Contains('editingToolbarSpacer')) {
    throw 'The editing toolbar must not contain a leading expanding spacer.'
}

Write-Host 'The toolbar menu is right-aligned on row one, and the editing tools start at the left of row two.'
