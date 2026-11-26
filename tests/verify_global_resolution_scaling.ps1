$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$scaleHeaderPath = Join-Path $root 'LTZKUiScale.h'
$mainPath = Join-Path $root 'main.cpp'
$windowPath = Join-Path $root 'LTZK.cpp'
$loginPath = Join-Path $root 'auth\LTZKLoginDialog.cpp'
foreach ($path in @($scaleHeaderPath, $mainPath, $windowPath, $loginPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing required file: $path"
    }
}

$scaleHeader = Get-Content -LiteralPath $scaleHeaderPath -Raw -Encoding UTF8
$main = Get-Content -LiteralPath $mainPath -Raw -Encoding UTF8
$window = Get-Content -LiteralPath $windowPath -Raw -Encoding UTF8
$login = Get-Content -LiteralPath $loginPath -Raw -Encoding UTF8
$accessControl = Get-Content -LiteralPath (Join-Path $root 'auth\LTZKAccessControlDialog.cpp') -Raw -Encoding UTF8

foreach ($required in @(
    'kBaseLogicalWidth = 1920.0',
    'kBaseLogicalHeight = 1080.0',
    'qMin(widthScale, heightScale)',
    'return scale;',
    'screen->geometry().size()',
    'scaleStyleSheetPixels',
    'qRound(basePixels * scale)'
)) {
    if (-not $scaleHeader.Contains($required)) {
        throw "Global scale contract missing: $required"
    }
}

foreach ($forbidden in @(
    'kMinimumScale',
    'kMaximumScale',
    'qBound(kMinimumScale, scale, kMaximumScale)'
)) {
    if ($scaleHeader.Contains($forbidden)) {
        throw "Global scale must remain strictly proportional: $forbidden"
    }
}

$appIndex = $main.IndexOf('QgsApplication a(argc, argv, true);')
if ($appIndex -lt 0) {
    throw 'QgsApplication construction marker is missing.'
}
foreach ($attribute in @('AA_EnableHighDpiScaling', 'AA_UseHighDpiPixmaps')) {
    $index = $main.IndexOf($attribute)
    if ($index -lt 0 -or $index -gt $appIndex) {
        throw "Qt DPI attribute must be set before QgsApplication: $attribute"
    }
}

foreach ($required in @(
    'qApp->setProperty("ltzkBaseApplicationFont"',
    'LTZKUiScale::scaledFont',
    'LTZKUiScale::factorForScreen'
)) {
    if (-not $main.Contains($required)) {
        throw "Application font scaling marker missing: $required"
    }
}

foreach ($required in @(
    '#include "LTZKUiScale.h"',
    'LTZKUiScale::factorForWidget(widget)',
    'LTZKUiScale::scaleStyleSheetPixels',
    'loadPlatformThemeQss(platformThemeKey(m_currentTheme)),',
    'titleFont.setPixelSize(scaledInt(target, 16))',
    'target->setMinimumHeight(qMax(target->minimumHeight(), scaledInt(target, 40)))',
    'bindUiScaleToCurrentScreen()',
    'refreshUiScale()'
)) {
    if (-not $window.Contains($required)) {
        throw "Main window global scaling marker missing: $required"
    }
}

foreach ($required in @(
    'combo->setStyleSheet(LTZKUiScale::scaleStyleSheetPixels(',
    'lineEdit->setStyleSheet(LTZKUiScale::scaleStyleSheetPixels(lineEditStyle',
    'button->setStyleSheet(LTZKUiScale::scaleStyleSheetPixels(buttonStyle'
)) {
    if (-not $window.Contains($required)) {
        throw "Plugin embedded-control stylesheet scaling marker missing: $required"
    }
}

foreach ($required in @(
    '#include "../LTZKUiScale.h"',
    'LTZKUiScale::factorForWidget(this)',
    'LTZKUiScale::scaledInt'
)) {
    if (-not $login.Contains($required)) {
        throw "Login dialog global scaling marker missing: $required"
    }
}

foreach ($forbidden in @(
    'kDesignBaseWidth = 2048.0',
    'kDesignBaseHeight = 1152.0',
    'width() / kBaseDialogWidth',
    'height() / kBaseDialogHeight'
)) {
    if ($window.Contains($forbidden) -or $login.Contains($forbidden)) {
        throw "Legacy scaling marker remains: $forbidden"
    }
}

foreach ($required in @(
    '#include "../LTZKUiScale.h"',
    'mPermissionsEdit->setMaximumHeight(LTZKUiScale::scaledInt(90, uiScale))'
)) {
    if (-not $accessControl.Contains($required)) {
        throw "Access-control scaling marker missing: $required"
    }
}

$pluginUiRoot = Join-Path $root 'map_product_manager_tools\ui_class'
$pluginScaleReferences = Get-ChildItem -LiteralPath $pluginUiRoot -Filter '*.cpp' -File |
    Where-Object { (Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8).Contains('LTZKUiScale') }
if ($pluginScaleReferences) {
    throw "Platform scaling leaked into map_product_manager_tools: $($pluginScaleReferences.Name -join ', ')"
}

$scaleCases = @(
    @{ Width = 1024; Height = 768; Expected = 0.5333333333 },
    @{ Width = 1280; Height = 720; Expected = 0.6666666667 },
    @{ Width = 1366; Height = 768; Expected = 0.7111111111 },
    @{ Width = 1600; Height = 900; Expected = 0.8333333333 },
    @{ Width = 1920; Height = 1080; Expected = 1.0 },
    @{ Width = 2560; Height = 1440; Expected = 1.3333333333 },
    @{ Width = 3840; Height = 2160; Expected = 2.0 },
    @{ Width = 2560; Height = 1080; Expected = 1.0 }
)
foreach ($case in $scaleCases) {
    $actual = [Math]::Min($case.Width / 1920.0, $case.Height / 1080.0)
    if ([Math]::Abs($actual - $case.Expected) -gt 0.0001) {
        throw "Unexpected scale for $($case.Width)x$($case.Height): $actual"
    }
}

Write-Host 'Global 1920x1080 logical resolution scaling contract verified.'
