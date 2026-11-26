$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding UTF8 $source

foreach ($required in @(
    '#include <QScrollArea>',
    '#include <QScrollBar>',
    'workspacePluginActionScrollArea',
    'workspaceTopActionScrollContent',
    'pluginScrollArea->setWidgetResizable(false)',
    'pluginScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded)',
    'pluginScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff)',
    'const int topActionButtonHeight = topBarScaledInt(topActionStrip, 84)',
    'const int topActionContentHeight = topActionButtonHeight',
    'pluginScrollArea->setFixedHeight(topActionButtonHeight)',
    'pluginScrollArea->setMinimumWidth(scaledInt(topActionStrip, 720))',
    'topActionScrollContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)',
    'scrollContent->setFixedHeight(contentHeight)',
    'pluginStrip->setFixedHeight(contentHeight)',
    'pluginStrip->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed)',
    'topActionScrollContentLayout->addWidget(pluginStrip, 0, Qt::AlignVCenter)',
    'pluginScrollArea->setWidget(topActionScrollContent)',
    'topActionLayout->addWidget(pluginScrollArea, 1, Qt::AlignVCenter)',
    'pluginStrip->adjustSize()',
    'pluginStrip->updateGeometry()',
    'pluginStrip->show()',
    'topActionScrollContent->adjustSize()',
    'topActionScrollContent->updateGeometry()',
    'topActionStrip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)',
    'topLayout->addWidget(topActionStrip, 1)',
    'm_scaledCardSize = homeModuleCardSize(this)',
    'setFixedSize(m_scaledCardSize)',
    'stageLayout->addWidget(m_homeContentStack, 1)'
)) {
    if (-not $text.Contains($required)) {
        throw "Workspace plugin overflow scroll requirement missing: $required"
    }
}

foreach ($forbidden in @(
    'homeContentScrollArea',
    'workspaceTopActionScrollArea',
    'pluginScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn)',
    'pluginScrollArea->setWidgetResizable(true)',
    'pluginScrollArea->setMinimumWidth(0)',
    'pluginScrollArea->setFixedWidth(scaledInt(topActionStrip, 620))',
    'pluginScrollArea->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed)',
    'pluginScrollArea->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed)',
    'topActionHoverOverflow',
    'topActionScrollBarReserveHeight',
    'contentHeight + topActionScrollBarReserveHeight',
    'button->setMinimumSize(QSize(320, 180))',
    'iconLabel->setScaledContents(true)'
)) {
    if ($text.Contains($forbidden)) {
        throw "Unwanted broader responsive layout change found: $forbidden"
    }
}

Write-Host 'Workspace plugin overflow horizontal scroll verified without deleting static top actions.'









