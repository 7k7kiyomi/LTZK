$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding Default $source

foreach ($required in @(
    '#include <QSplitter>',
    'QSplitter* workspaceSplitter',
    'workspaceSplitter->setChildrenCollapsible(false)',
    'workspaceSplitter->addWidget(layerManagerPanel)',
    'workspaceSplitter->addWidget(mapPanel)',
    'workspaceSplitter->setStretchFactor(0, 0)',
    'workspaceSplitter->setStretchFactor(1, 1)',
    'workspaceSplitter->setCollapsible(0, false)',
    'workspaceSplitter->setCollapsible(1, false)',
    'workspaceSplitter->setSizes(QList<int>() << 320 << 1600)',
    'QWidget* layerManagerHandle = workspaceSplitter->handle(1)',
    'workspaceLayerManagerCollapseButton',
    'layerManagerPanel->setMaximumWidth(collapsedLayerWidth)',
    'layerManagerPanel->setMaximumWidth(QWIDGETSIZE_MAX)',
    'new MapServiceWorkspace(workspaceSplitter, page)'
    'rootLayout->addWidget(m_workspaceContentStack, 1)'
)) {
    if (-not $text.Contains($required)) {
        throw "Workspace resizable panel implementation is missing: $required"
    }
}

foreach ($forbidden in @(
    'layerManagerPanel->setMaximumWidth(340)',
    'm_qualityResultPanel->setMaximumWidth(420)',
    'bodyLayout->addWidget(layerManagerPanel)',
    'bodyLayout->addWidget(m_qualityResultPanel)',
    'rightSplitter->addWidget(mapPanel)',
    'rightSplitter->addWidget(m_qualityResultPanel)'
)) {
    if ($text.Contains($forbidden)) {
        throw "Workspace side panel should no longer be fixed by old layout detail: $forbidden"
    }
}

Write-Host 'Workspace resizable side panels verified.'


