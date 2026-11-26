$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding UTF8 $source

foreach ($required in @(
    'QSize homeModuleCardSize(const QWidget* widget)',
    'constexpr int kHomeModuleCardBaseWidth = 462',
    'constexpr int kHomeModuleCardBaseHeight = 234',
    'return scaledSize(widget, kHomeModuleCardBaseWidth, kHomeModuleCardBaseHeight)',
    'class HomeModulesPanel final : public QWidget',
    'void applyScaledCardSize(const QSize& size)',
    'm_scaledCardSize = homeModuleCardSize(this)',
    'setFixedSize(m_scaledCardSize)',
    'setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed)',
    'constexpr int kHomeGridHorizontalSpacing = 80',
    'constexpr int kHomeGridVerticalSpacing = 74',
    'grid->setHorizontalSpacing(scaledInt(panel, kHomeGridHorizontalSpacing))',
    'grid->setVerticalSpacing(scaledInt(panel, kHomeGridVerticalSpacing))',
    'stageLayout->addStretch(1)',
    'QSpacerItem* introGap = new QSpacerItem(',
    'm_introGap->changeSize(0, scaledInt(this, 10)',
    'stageLayout->addLayout(grid, 0)',
    'grid->addWidget(achievementButton, 0, 0, Qt::AlignCenter)',
    'grid->addWidget(generalizationButton, 0, 1, Qt::AlignCenter)',
    'grid->addWidget(qualityButton, 1, 0, Qt::AlignCenter)',
    'grid->addWidget(serviceButton, 1, 1, Qt::AlignCenter)'
)) {
    if (-not $text.Contains($required)) {
        throw "Home module card global visual-size requirement missing: $required"
    }
}

foreach ($forbidden in @(
    'adaptiveHomeModuleCardSize',
    'responsiveScaleForSize',
    'cardScaledInt',
    'setMaximumWidth(m_responsiveCardSize.width())'
)) {
    if ($text.Contains($forbidden)) {
        throw "Home module card still uses an independent scale: $forbidden"
    }
}

Write-Host 'Home module cards keep the 1920x1080 visual proportion.'





