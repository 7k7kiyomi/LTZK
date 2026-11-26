$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding UTF8 $source

foreach ($required in @(
    'WorkspaceTopBarGradientScrollArea* pluginScrollArea =',
    'new WorkspaceTopBarGradientScrollArea(topActionStrip)',
    'QWidget* topActionScrollContent = new QWidget(pluginScrollArea)',
    'topActionScrollContent->setObjectName(QStringLiteral("workspaceTopActionScrollContent"))',
    'QWidget* pluginStrip = new QWidget(topActionScrollContent)',
    'pluginStrip->setObjectName(QStringLiteral("workspacePluginActionStrip"))',
    'topActionStrip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)',
    'pluginScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)',
    'pluginScrollArea->setMinimumWidth(scaledInt(topActionStrip, 720))',
    'topActionScrollContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)',
    'topActionScrollContentLayout->addWidget(pluginStrip, 0, Qt::AlignVCenter)',
    'pluginScrollArea->setWidget(topActionScrollContent)',
    'topActionLayout->addWidget(pluginScrollArea, 1, Qt::AlignVCenter)',
    'topLayout->addWidget(topActionStrip, 1)',
    'void setTopBarToolButtonResponsiveMetrics(QToolButton* button, const QWidget* scaleWidget)',
    'setTopBarToolButtonResponsiveMetrics(button, parent ? parent : this)',
    'setTopBarToolButtonResponsiveMetrics(m_workspaceLogoutButton, topActionStrip)',
    'setTopBarToolButtonResponsiveMetrics(backHomeButton, topActionStrip)',
    'setWorkspacePluginToolButtonCompactMetrics(featureButton, pluginStrip)',
    'createTopBarToolButton(',
    'topActionScrollContent);',
    'topActionScrollContentLayout->addWidget(',
    'topActionScrollContentLayout->addWidget(m_workspaceLogoutButton, 0, Qt::AlignVCenter)',
    'topActionScrollContentLayout->addWidget(backHomeButton, 0, Qt::AlignVCenter)',
    'topActionScrollContentLayout->addStretch(1)'
)) {
    if (-not $text.Contains($required)) {
        throw "Shared top action scroll requirement missing: $required"
    }
}

foreach ($forbidden in @(
    'createTopBarToolButton(
                entry.action,
                topActionStrip)',
    'createTopBarToolButton(
                entry.action,
                pluginStrip)',
    'topActionLayout->addWidget(
            button,
            0,
            Qt::AlignVCenter)',
    'pluginStripLayout->addWidget(
            button,
            0,
            Qt::AlignVCenter)',
    'topActionLayout->addWidget(m_workspaceLogoutButton, 0, Qt::AlignVCenter)',
    'pluginStripLayout->addWidget(m_workspaceLogoutButton, 0, Qt::AlignVCenter)',
    'topActionLayout->addWidget(backHomeButton, 0, Qt::AlignVCenter)',
    'pluginStripLayout->addWidget(backHomeButton, 0, Qt::AlignVCenter)',
    'pluginScrollArea->setFixedWidth(scaledInt(topActionStrip, 620))',
    'pluginScrollArea->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed)',
    'topLayout->addStretch(1)',
    'topLayout->addWidget(topActionStrip, 0, Qt::AlignRight | Qt::AlignVCenter)',
    'topLayout->addWidget(topActionStrip, 1, Qt::AlignRight | Qt::AlignVCenter)'

)) {
    if ($text.Contains($forbidden)) {
        throw "Top action remains outside shared scroll content or inside dynamic plugin strip: $forbidden"
    }
}

Write-Host 'All workspace top action buttons share one horizontal scroll area without being cleared by plugin reloads.'




