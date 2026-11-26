$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding UTF8 $source

foreach ($required in @(
    '#include "LTZKUiScale.h"',
    'qreal visualScaleForWidget(const QWidget* widget)',
    'return LTZKUiScale::factorForWidget(widget)',
    'int scaledInt(const QWidget* widget, int value)',
    'LTZKUiScale::scaledInt(value, visualScaleForWidget(widget))',
    'QSize scaledSize(const QWidget* widget, int width, int height)',
    'QMargins scaledMargins(const QWidget* widget, int left, int top, int right, int bottom)',
    'stageLayout->setContentsMargins(scaledMargins(stage, 34, 4, 34, 0))',
    'resultHint->setMinimumHeight(qMax(34, scaledInt(stage, 38)))',
    'panel->setFixedSize(scaledSize(page, kHomeLoginPanelBaseWidth, kHomeLoginPanelBaseHeight))',
    'panelLayout->setContentsMargins(scaledMargins(panel, 40, 24, 40, 22))',
    'homeLoginTitleFont.setPixelSize(scaledInt(panel, 54))',
    'm_scaledCardSize = homeModuleCardSize(this)',
    'setFixedSize(m_scaledCardSize)',
    'const QPixmap originalPixmap = normalizedIconPixmap(m_icon.pixmap(iconSize))',
    'm_iconLabel->setPixmap(themedPixmap)',
    'void updateScaledLayout()',
    'titleFont.setPixelSize(scaledInt(this, 66))',
    'const qreal bandHeight = qMin<qreal>(height, scaledReal(52.0))',
    'const qreal topWidth = qMin<qreal>(width, scaledReal(1220.0))',
    'pluginScrollArea->setMinimumWidth(scaledInt(topActionStrip, 720))',
    'const int topActionButtonHeight = topBarScaledInt(topActionStrip, 84)',
    'const int topActionContentHeight = topActionButtonHeight',
    'pluginScrollArea->setFixedHeight(topActionButtonHeight)',
    'setTopBarToolButtonResponsiveMetrics(button, parent ? parent : this)',
    'layerManagerPanel->setMinimumWidth(scaledInt(page, 260))'
)) {
    if (-not $text.Contains($required)) {
        throw "Visual scaling requirement missing: $required"
    }
}

foreach ($forbidden in @(
    'topBarScaleForWidget',
    'height * 0.34',
    'designWidth * 0.62'
)) {
    if ($text.Contains($forbidden)) {
        throw "Independent or container-derived visual scale remains: $forbidden"
    }
}

$visualStart = $text.IndexOf('qreal visualScaleForWidget(const QWidget* widget)')
$visualEnd = $text.IndexOf('int scaledInt(const QWidget* widget, int value)', $visualStart)
if ($visualStart -lt 0 -or $visualEnd -lt 0) {
    throw 'Could not isolate visualScaleForWidget function body.'
}
$visualScaleBody = $text.Substring($visualStart, $visualEnd - $visualStart)
foreach ($forbidden in @(
    'constexpr qreal kDesignBaseWidth = 2048.0',
    'constexpr qreal kDesignBaseHeight = 1152.0',
    'const QSize contentSize = window->contentsRect().size()',
    'return qBound<qreal>(0.56, qMin(widthScale, heightScale), 1.0)',
    'button->setFixedSize(QSize(1000, 480))',
    'panel->setFixedWidth(960)',
    'pluginScrollArea->setFixedWidth(620)',
    'button->setMinimumSize(`r`n        QSize(104, 68))',
    'const qreal widthScale = availableGeometry.width() / kDesignBaseWidth',
    'const qreal heightScale = availableGeometry.height() / kDesignBaseHeight',
    'return qBound<qreal>(0.64, qMin(widthScale, heightScale), 1.35)'
)) {
    if ($visualScaleBody.Contains($forbidden)) {
        throw "Unscaled fixed or reverted window-scale change remains: $forbidden"
    }
}

Write-Host 'Baseline visual scaling helpers and key layout dimensions verified.'











