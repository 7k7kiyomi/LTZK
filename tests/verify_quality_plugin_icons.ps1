$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$qrc = Join-Path $root 'LTZK.qrc'
$source = Join-Path $root 'LTZK.cpp'
$config = Join-Path $root 'config\plugin_bindings.json'

if (-not (Test-Path $qrc)) {
    throw "Missing required file: $qrc"
}

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}
if (-not (Test-Path $config)) {
    throw "Missing required file: $config"
}

$icons = @(
    'mActionScaleAccuracy.svg',
    'mActionMapLayoutConfig.svg',
    'mActionMapDataPreprocess.svg',
    'mActionQualitySchemeConfig.svg',
    'mActionQualityTaskManager.svg',
    'mActionSheetEdgeSeries.svg',
    'mActionQualityReportExport.svg'
)

$qrcText = Get-Content -Raw -Encoding UTF8 $qrc
$sourceText = Get-Content -Raw -Encoding Default $source
$configText = Get-Content -Raw -Encoding UTF8 $config

foreach ($icon in $icons) {
    $iconPath = Join-Path $root "Resources\$icon"
    if (-not (Test-Path $iconPath)) {
        throw "Missing icon file: Resources\$icon"
    }

    if (-not $qrcText.Contains("<file>Resources/$icon</file>")) {
        throw "Icon is not registered in LTZK.qrc: $icon"
    }

    if (-not $configText.Contains("Resources/$icon")) {
        throw "Icon resource is not configured for plugin action icon replacement: $icon"
    }

    [xml]$svg = Get-Content -Raw -Encoding UTF8 $iconPath
    if ($svg.svg.width -ne '24' -or $svg.svg.height -ne '24' -or $svg.svg.viewBox -ne '0 0 24 24') {
        throw "Icon must match the 24x24 SVG toolbar format: Resources\$icon"
    }
}

foreach ($legacyPngIcon in @(
    'mActionScaleAccuracy.png',
    'mActionMapLayoutConfig.png',
    'mActionMapDataPreprocess.png',
    'mActionQualitySchemeConfig.png',
    'mActionQualityTaskManager.png',
    'mActionSheetEdgeSeries.png',
    'mActionQualityReportExport.png'
)) {
    if ($qrcText.Contains("<file>Resources/$legacyPngIcon</file>")) {
        throw "Do not register legacy PNG toolbar icons in LTZK.qrc: $legacyPngIcon"
    }

    if ($configText.Contains("Resources/$legacyPngIcon")) {
        throw "Do not configure legacy PNG toolbar icons for plugin actions: $legacyPngIcon"
    }
}

foreach ($required in @(
    'configuredIcon',
    'applyConfiguredPluginActionIcons',
    'action->setIcon(icon)',
    'featureAction->setIcon(icon)'
)) {
    if (-not $sourceText.Contains($required)) {
        throw "Missing quality plugin icon implementation detail: $required"
    }
}

Write-Host 'Quality plugin icons are registered and applied.'
