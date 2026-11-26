$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZKUiScale.h'

foreach ($path in @($source, $header)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing required file: $path"
    }
}

$text = Get-Content -LiteralPath $source -Raw -Encoding UTF8
$scale = Get-Content -LiteralPath $header -Raw -Encoding UTF8

foreach ($required in @(
    'constexpr int kHomeLoginPanelBaseWidth = 540',
    'constexpr int kHomeLoginPanelBaseHeight = 390',
    'constexpr int kHomeUtilityButtonWidth = 172',
    'constexpr int kHomeUtilityButtonHeight = 54',
    'constexpr int kHomeUtilityButtonFontSize = 16',
    'constexpr int kHomeGridHorizontalSpacing = 80',
    'constexpr int kHomeGridVerticalSpacing = 74',
    'constexpr int kHomeGridToUtilityGap = 45',
    'QSize targetCanvasSize = LTZKUiScale::scaledSize',
    'const qreal screenScale = LTZKUiScale::factorForScreen',
    'devicePixelRatio()',
    'normalizedIconPixmap',
    'm_iconLabel->setFixedHeight',
    'topActionButtonHeight = topBarScaledInt(topActionStrip, 84)',
    'const qreal viewportHeightScale = viewportRect.height() > 0',
    'targetScale = qMin(screenScale, viewportHeightScale)',
    'pluginScrollArea->setFixedHeight(topActionButtonHeight)'
)) {
    if (-not $text.Contains($required)) {
        throw "Reference visual metric contract missing: $required"
    }
}

foreach ($forbidden in @(
    'constexpr int kHomeLoginPanelBaseWidth = 960',
    'constexpr int kHomeLoginPanelBaseHeight = 580',
    'constexpr int kHomeUtilityButtonWidth = 196',
    'constexpr int kHomeUtilityButtonHeight = 60',
    'homeUtilityButtonFont.setPixelSize(scaledInt(panel, 32))',
    'panel->setFixedSize(scaledSize(this, 960, 580))',
    'panel->setFixedSize(scaledSize(page, 960, 580))',
    'grid->setHorizontalSpacing(scaledInt(panel, 146))',
    'grid->setVerticalSpacing(scaledInt(panel, 142))',
    'topActionHoverOverflow',
    'topActionScrollBarReserveHeight',
    'contentHeight + topActionScrollBarReserveHeight',
    'targetScale = qMin(screenScale, availableScale)'
)) {
    if ($text.Contains($forbidden)) {
        throw "Stale visual metric or secondary scale remains: $forbidden"
    }
}

if ($scale.Contains('devicePixelRatio') -and $scale.Contains('factorForScreen')) {
    throw 'The shared layout scale must not multiply devicePixelRatio.'
}

$pluginRoots = @(
    (Join-Path $root 'map_product_manager'),
    (Join-Path $root 'map_product_manager_tools')
)
foreach ($pluginRoot in $pluginRoots) {
    if (Test-Path -LiteralPath $pluginRoot) {
        $leaks = Get-ChildItem -LiteralPath $pluginRoot -Recurse -File -Include '*.cpp','*.h' |
            Where-Object { (Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8).Contains('LTZKUiScale') }
        if ($leaks) {
            throw "Platform scaling leaked into business plugin files: $($leaks.FullName -join ', ')"
        }
    }
}

Write-Host 'Reference visual metrics and platform-only scaling contract verified.'
