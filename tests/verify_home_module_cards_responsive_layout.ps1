$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding UTF8 $source

foreach ($required in @(
    'class ResponsiveHomeModuleButton final : public QToolButton',
    'void applyScaledCardSize(const QSize& size)',
    'm_scaledCardSize',
    'void resizeEvent(QResizeEvent* event) override',
    'void updateScaledLayout()',
    'constexpr int kHomeModuleCardBaseWidth = 462',
    'constexpr int kHomeModuleCardBaseHeight = 234',
    'return scaledSize(widget, kHomeModuleCardBaseWidth, kHomeModuleCardBaseHeight)',
    'class HomeModulesPanel final : public QWidget',
    'void updateModuleCardSizes()',
    'const QSize cardSize = homeModuleCardSize(this)',
    'dynamic_cast<ResponsiveHomeModuleButton*>(button)',
    'moduleButton->applyScaledCardSize(cardSize)',
    'setFixedSize(m_scaledCardSize)',
    'setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed)',
    'scaledSize(this, 286, 103)',
    'font.setPixelSize(scaledInt(this, 18))',
    'm_codeLabel->setMinimumHeight(scaledInt(this, 20))',
    'font.setPixelSize(scaledInt(this, 22))',
    'font.setPixelSize(scaledInt(this, 12))',
    'm_subtitleLabel->setMinimumHeight(scaledInt(this, 20))',
    'const QPixmap originalPixmap = normalizedIconPixmap(m_icon.pixmap(iconSize))',
    'm_iconLabel->setPixmap(themedPixmap)',
    'panel = new HomeModulesPanel(parent)',
    'stageLayout->addStretch(1)',
    'stageLayout->addLayout(grid, 0)',
    'm_homeContentStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding)'
)) {
    if (-not $text.Contains($required)) {
        throw "Uniformly scaled home module card requirement missing: $required"
    }
}

foreach ($forbidden in @(
    'adaptiveHomeModuleCardSize',
    'responsiveScaleForSize',
    'cardScaledInt',
    'qBound<qreal>(0.65, scale, 1.3)',
    'setMaximumWidth(m_responsiveCardSize.width())',
    'const qreal widthScale = size.width() / 710.0',
    'const qreal heightScale = size.height() / 320.0'
)) {
    if ($text.Contains($forbidden)) {
        throw "Independent home-card scaling remains: $forbidden"
    }
}

Write-Host 'Home module cards use one global 1920x1080 scale.'

















