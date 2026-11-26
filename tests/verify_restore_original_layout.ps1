$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding UTF8 $source

foreach ($required in @(
    'stageLayout->addWidget(m_homeContentStack, 1)',
    'm_homeContentStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding)',
    'm_scaledCardSize = homeModuleCardSize(this)',
    'setFixedSize(m_scaledCardSize)',
    'm_cardLayout->setContentsMargins(horizontalMargin, topMargin, horizontalMargin, bottomMargin)',
    'm_iconLabel->setPixmap(themedPixmap)',
    'void updateScaledLayout()',
    'topActionStrip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)',
    'topActionLayout->addWidget(pluginScrollArea, 1, Qt::AlignVCenter)',
    'topLayout->addWidget(topActionStrip, 1)',
    'setTopBarToolButtonResponsiveMetrics(backHomeButton, topActionStrip)'
)) {
    if (-not $text.Contains($required)) {
        throw "Original layout requirement missing: $required"
    }
}

foreach ($forbidden in @(
    'homeContentScrollArea',
    'workspaceTopActionScrollArea',
    'button->setMinimumSize(QSize(320, 180))',
    'cardLayout->setContentsMargins(scaledMargins(button, 28, 20, 28, 24))',
    'iconLabel->setScaledContents(true)',
    'iconLabel->setMaximumHeight(180)',
    'QSizePolicy::Preferred, QSizePolicy::Fixed'
)) {
    if ($text.Contains($forbidden)) {
        throw "Unexpected responsive rewrite remains: $forbidden"
    }
}

Write-Host 'Original homepage and workspace layout restored.'
