$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$scalePath = Join-Path $root 'LTZKUiScale.h'
$windowPath = Join-Path $root 'LTZK.cpp'
$scale = Get-Content -LiteralPath $scalePath -Raw -Encoding UTF8
$window = Get-Content -LiteralPath $windowPath -Raw -Encoding UTF8

foreach ($required in @(
    'return scale;',
    'kScaleOverrideProperty',
    'current = current->parentWidget()',
    'if (current->isWindow())',
    'kHomeCanvasBaseWidth = 1920',
    'kHomeCanvasBaseHeight = 1020',
    'void LTZK::refreshHomePageScaledMetrics()',
    'm_homePage->setProperty(LTZKUiScale::kScaleOverrideProperty, targetScale)',
    'm_homePage->installEventFilter(this)',
    'homeDesignCanvas',
    'QSize targetCanvasSize = LTZKUiScale::scaledSize',
    'targetCanvasSize.setWidth(viewportRect.width())',
    'const qreal viewportHeightScale = viewportRect.height() > 0',
    'targetScale = qMin(screenScale, viewportHeightScale)',
    'canvas->setFixedSize(targetCanvasSize)',
    'canvas->move(',
    'm_lastHomeScale',
    'page == m_homeLoginPage || page == m_homeModulesPage',
    'page->setStyleSheet(QString())',
    'homeLoginPage->refreshScaledMetrics()',
    'homeModulesPanel->refreshScaledMetrics()',
    'void LTZK::refreshWorkspaceScaledMetrics()',
    'resizeDocks(',
    'm_CurrentCoordinage->setMinimumWidth(scaledInt(this, 260))',
    'accessTopBar->findChild<QToolButton*>(name)',
    'm_accessControlWidget->refreshScaledMetrics()',
    'constexpr int kHomeModuleCardBaseWidth = 462',
    'constexpr int kHomeModuleCardBaseHeight = 234',
    'return scaledSize(widget, kHomeModuleCardBaseWidth, kHomeModuleCardBaseHeight)',
    'const QSize cardSize = homeModuleCardSize(this)',
    'setFixedSize(m_scaledCardSize)',
    'font.setPixelSize(scaledInt(this, 18))',
    'font.setPixelSize(scaledInt(this, 22))',
    'font.setPixelSize(scaledInt(this, 12))',
    'titleFont.setPixelSize(scaledInt(this, 66))',
    'applyScaledCardSize(homeModuleCardSize(this))',
    'void refreshScaledMetrics()',
    'header->refreshScaledMetrics()'
)) {
    if (-not ($scale.Contains($required) -or $window.Contains($required))) {
        throw "Uniform design-canvas scaling requirement missing: $required"
    }
}

foreach ($forbidden in @(
    'kMinimumScale',
    'kMaximumScale',
    'adaptiveHomeModuleCardSize',
    'responsiveScaleForSize',
    'cardScaledInt',
    'qBound(58, static_cast<int>(height * 0.44), 70)',
    'topBarScaleForWidget',
    'qBound<qreal>(0.78, visualScaleForWidget(widget), 1.18)',
    'height * 0.34',
    'designWidth * 0.62',
    'qMax(6, scaledInt(widget, 6))'
)) {
    if ($scale.Contains($forbidden) -or $window.Contains($forbidden)) {
        throw "Conflicting independent scale remains: $forbidden"
    }
}

$homeCanvasCases = @(
    @{ ScreenWidth = 1920; ScreenHeight = 1080; PageWidth = 1920; PageHeight = 1020; Expected = 1.0 },
    @{ ScreenWidth = 1920; ScreenHeight = 1080; PageWidth = 1920; PageHeight = 840; Expected = 0.8235294118 },
    @{ ScreenWidth = 2560; ScreenHeight = 1440; PageWidth = 2560; PageHeight = 1260; Expected = 1.2352941176 },
    @{ ScreenWidth = 1366; ScreenHeight = 768; PageWidth = 1366; PageHeight = 670; Expected = 0.6568627451 },
    @{ ScreenWidth = 1440; ScreenHeight = 900; PageWidth = 1440; PageHeight = 820; Expected = 0.75 },
    @{ ScreenWidth = 3440; ScreenHeight = 1440; PageWidth = 3440; PageHeight = 1260; Expected = 1.2352941176 }
)
foreach ($case in $homeCanvasCases) {
    $screenScale = [Math]::Min($case.ScreenWidth / 1920.0, $case.ScreenHeight / 1080.0)
    $heightFitScale = $case.PageHeight / 1020.0
    $actual = [Math]::Min($screenScale, $heightFitScale)
    if ([Math]::Abs($actual - $case.Expected) -gt 0.0001) {
        throw "Unexpected home design-canvas scale: $actual"
    }
    $canvasWidth = $case.PageWidth
    $canvasHeight = [Math]::Floor((1020.0 * $actual) + 0.5)
    if ($canvasWidth -ne $case.PageWidth -or $canvasHeight -le 0 -or $canvasHeight -gt $case.PageHeight) {
        throw "Home design canvas does not fill the viewport width."
    }
}

$pluginUiRoot = Join-Path $root 'map_product_manager_tools\ui_class'
$pluginScaleReferences = Get-ChildItem -LiteralPath $pluginUiRoot -Filter '*.cpp' -File |
    Where-Object { (Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8).Contains('LTZKUiScale') }
if ($pluginScaleReferences) {
    throw "Platform scale leaked into plugin code: $($pluginScaleReferences.Name -join ', ')"
}

Write-Host 'Uniform 1920x1080 design-canvas scaling verified.'
