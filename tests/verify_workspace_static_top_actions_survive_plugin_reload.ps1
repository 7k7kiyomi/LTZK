$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding UTF8 $source

foreach ($required in @(
    'workspaceTopActionScrollContent',
    'QWidget* pluginStrip = new QWidget(topActionScrollContent)',
    'pluginStrip->setObjectName(QStringLiteral("workspacePluginActionStrip"))',
    'QHBoxLayout* topActionScrollContentLayout = new QHBoxLayout(topActionScrollContent)',
    'pluginScrollArea->setWidget(topActionScrollContent)',
    'topActionScrollContentLayout->addWidget(pluginStrip, 0, Qt::AlignVCenter)',
    'createTopBarToolButton(
                entry.action,
                topActionScrollContent)',
    'topActionScrollContentLayout->addWidget(
            button,
            0,
            Qt::AlignVCenter)',
    'm_workspaceLogoutButton = new QToolButton(topActionScrollContent)',
    'topActionScrollContentLayout->addWidget(m_workspaceLogoutButton, 0, Qt::AlignVCenter)',
    'QToolButton* backHomeButton = new QToolButton(topActionScrollContent)',
    'topActionScrollContentLayout->addWidget(backHomeButton, 0, Qt::AlignVCenter)'
)) {
    if (-not $text.Contains($required)) {
        throw "Static top action preservation requirement missing: $required"
    }
}

$setterStart = $text.IndexOf('void LTZK::setWorkspacePluginActions')
$setterEnd = $text.IndexOf('void LTZK::activateBusinessModule', $setterStart)
if ($setterStart -lt 0 -or $setterEnd -lt 0) {
    throw 'Could not locate setWorkspacePluginActions body.'
}
$setterBody = $text.Substring($setterStart, $setterEnd - $setterStart)

if (-not $setterBody.Contains('findChild<QWidget*>(QStringLiteral("workspacePluginActionStrip"))')) {
    throw 'setWorkspacePluginActions must clear only the dynamic plugin strip.'
}

foreach ($forbidden in @(
    'createTopBarToolButton(
                entry.action,
                pluginStrip)',
    'pluginStripLayout->addWidget(
            button,
            0,
            Qt::AlignVCenter)',
    'm_workspaceLogoutButton = new QToolButton(pluginStrip)',
    'pluginStripLayout->addWidget(m_workspaceLogoutButton, 0, Qt::AlignVCenter)',
    'QToolButton* backHomeButton = new QToolButton(pluginStrip)',
    'pluginStripLayout->addWidget(backHomeButton, 0, Qt::AlignVCenter)'
)) {
    if ($text.Contains($forbidden)) {
        throw "Static top action would be deleted by plugin reload: $forbidden"
    }
}

Write-Host 'Static workspace top actions remain in the shared scroll area and survive plugin reloads.'
