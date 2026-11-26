$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

if (-not (Test-Path $header)) {
    throw "Missing required file: $header"
}

$text = Get-Content -Raw -Encoding Default $source
$headerText = Get-Content -Raw -Encoding Default $header

foreach ($required in @(
    'm_layerTreePanelWidget = new QWidget(this)',
    'm_layerTreeView = new QgsLayerTreeView(m_layerTreePanelWidget)',
    'QToolBar* toolbar = new QToolBar(m_layerTreePanelWidget)',
    'QVBoxLayout* vBoxLayout = new QVBoxLayout(m_layerTreePanelWidget)',
    'layerManagerLayout->addWidget(m_layerTreePanelWidget, 1)'
)) {
    if (-not $text.Contains($required)) {
        throw "Layer tree single-owner workspace wiring missing: $required"
    }
}

if (-not $headerText.Contains('QWidget* m_layerTreePanelWidget = nullptr;')) {
    throw 'LTZK.h must keep an explicit workspace-owned layer tree panel widget pointer.'
}

foreach ($forbidden in @(
    'ui.LayerTreeControl->setWidget(nullptr)',
    'this->ui.LayerTreeControl->setWidget(widget)',
    'm_layerTreeView->setParent(layerManagerPanel)',
    'delete m_layerTreeView',
    'QWidget* layerTreeControlWidget = ui.LayerTreeControl->widget()',
    'm_layerTreeView = new QgsLayerTreeView();'
)) {
    if ($text.Contains($forbidden)) {
        throw "Unsafe layer tree ownership pattern remains: $forbidden"
    }
}

$viewCreationMatches = [regex]::Matches($text, 'm_layerTreeView\s*=\s*new\s+QgsLayerTreeView')
if ($viewCreationMatches.Count -ne 1) {
    throw "Layer tree view must be created exactly once, found $($viewCreationMatches.Count)."
}

Write-Host 'Workspace layer tree uses a single Qt owner and avoids dock-widget transfer.'
