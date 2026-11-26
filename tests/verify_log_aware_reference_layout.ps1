$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$source = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8

foreach ($required in @(
    'constexpr int kHomeUtilityButtonFontSize = 16',
    'const qreal viewportHeightScale = viewportRect.height() > 0',
    'targetScale = qMin(screenScale, viewportHeightScale)',
    'const int topActionButtonHeight = topBarScaledInt(topActionStrip, 84)',
    'pluginScrollArea->setFixedHeight(topActionButtonHeight)',
    'm_homePage->contentsRect()',
    'canvas->setFixedSize(targetCanvasSize)'
)) {
    if (-not $source.Contains($required)) {
        throw "Log-aware reference layout requirement missing: $required"
    }
}

foreach ($forbidden in @(
    'constexpr int kHomeUtilityButtonFontSize = 24',
    'topActionButtonHeight = topBarScaledInt(topActionStrip, 64)',
    'topActionContentHeight = topActionButtonHeight +',
    'contentHeight + topActionScrollBarReserveHeight'
)) {
    if ($source.Contains($forbidden)) {
        throw "Stale fixed visual metric remains: $forbidden"
    }
}

$pluginRoots = @(
    (Join-Path $root 'map_product_manager'),
    (Join-Path $root 'map_product_manager_tools')
)
foreach ($pluginRoot in $pluginRoots) {
    if (Test-Path -LiteralPath $pluginRoot) {
        $leaks = Get-ChildItem -LiteralPath $pluginRoot -Recurse -File -Include '*.cpp','*.h' |
            Where-Object { (Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8).Contains('viewportHeightScale') }
        if ($leaks) {
            throw "Platform home layout code leaked into business plugin files: $($leaks.FullName -join ', ')"
        }
    }
}

foreach ($qssPath in Get-ChildItem -LiteralPath (Join-Path $root 'Resources/styles') -Filter '*.qss' -File) {
    $qss = Get-Content -LiteralPath $qssPath.FullName -Raw -Encoding UTF8
    if ($qss.Contains('QToolButton[topBarTool="true"]') -and
        $qss -notmatch 'QToolButton\[topBarTool="true"\][^\{]*\{[^}]*min-height:\s*84px') {
        throw "Top-bar QSS height is still too small in $($qssPath.Name)."
    }
    if ($qss.Contains('QToolButton[homeUtilityButton="true"]') -and
        $qss -match 'QToolButton\[homeUtilityButton="true"\][^\{]*\{[^}]*font-weight:\s*750') {
        throw "Home utility button weight is still oversized in $($qssPath.Name)."
    }
}

Write-Host 'Log-aware home layout, utility typography, and top-bar height verified.'
