$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding UTF8 $source

foreach ($required in @(
    'topActionStrip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)',
    'pluginScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)',
    'pluginScrollArea->setMinimumWidth(scaledInt(topActionStrip, 720))',
    'pluginScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded)',
    'topActionLayout->addWidget(pluginScrollArea, 1, Qt::AlignVCenter)',
    'topLayout->addWidget(topActionStrip, 1)'
)) {
    if (-not $text.Contains($required)) {
        throw "Shrinkable top toolbar requirement missing: $required"
    }
}

foreach ($forbidden in @(
    'setMinimumWidth(scaledInt(topActionStrip, 1820))',
    'setMinimumWidth(1820)',
    'setFixedWidth(scaledInt(topActionStrip, 1820))',
    'setFixedWidth(1820)',
    'pluginScrollArea->setFixedWidth',
    'workspaceTopActionScrollArea',
    'topLayout->addWidget(topActionStrip, 1, Qt::AlignRight | Qt::AlignVCenter)'
)) {
    if ($text.Contains($forbidden)) {
        throw "Top toolbar still has non-shrinkable width constraint: $forbidden"
    }
}

Write-Host 'Workspace top toolbar fills remaining title-bar width and keeps horizontal overflow.'


