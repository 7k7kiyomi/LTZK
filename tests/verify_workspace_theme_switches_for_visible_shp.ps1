$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'

foreach ($path in @($source, $header)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$sourceText = Get-Content -Raw -Encoding Default $source
$headerText = Get-Content -Raw -Encoding Default $header

foreach ($required in @(
    'workspaceDarkStyleSheet()',
    'visibleLayersContainShp',
    'applyWorkspaceShpCanvasBackground',
    'm_workspaceUsesLightShpCanvas',
    'QColor(Qt::white)',
    'QColor(3, 16, 29)',
    'ltzk/qualityResultShp',
    'isSystemBasemapLayer(layer)',
    'suffix().compare(QStringLiteral("shp"), Qt::CaseInsensitive) == 0'
)) {
    if (-not $sourceText.Contains($required) -and -not $headerText.Contains($required)) {
        throw "Workspace shp theme switching is missing: $required"
    }
}

$refreshStart = $sourceText.IndexOf('void LTZK::refreshMapCanvasLayerStack()')
$refreshEnd = $sourceText.IndexOf('void LTZK::setQualityResultLayerVisible', $refreshStart)
if ($refreshStart -lt 0 -or $refreshEnd -lt 0) {
    throw 'Could not locate refreshMapCanvasLayerStack.'
}

$refreshBody = $sourceText.Substring($refreshStart, $refreshEnd - $refreshStart)
foreach ($required in @(
    'const bool hasVisibleShp = visibleLayersContainShp(canvasLayers);',
    'applyWorkspaceShpCanvasBackground(hasVisibleShp);',
    'm_mapCanvas->setLayers(canvasLayers);'
)) {
    if (-not $refreshBody.Contains($required)) {
        throw "Map canvas refresh must drive theme from visible layers: $required"
    }
}


$visibleStart = $sourceText.IndexOf('bool LTZK::visibleLayersContainShp')
$visibleEnd = $sourceText.IndexOf('void LTZK::setQualityResultLayerVisible', $visibleStart)
if ($visibleStart -lt 0 -or $visibleEnd -lt 0) {
    throw 'Could not locate visibleLayersContainShp.'
}
$visibleBody = $sourceText.Substring($visibleStart, $visibleEnd - $visibleStart)
if (-not $visibleBody.Contains('isSystemBasemapLayer(layer)')) {
    throw 'System basemap shp must not switch the map canvas to white.'
}
if ($sourceText.Contains('workspaceLightShpStyleSheet()')) {
    throw 'Only the map canvas background may switch to white; the workspace stylesheet must stay dark.'
}

if ($sourceText.Contains('m_mapCanvasPage->setStyleSheet(useLightTheme')) {
    throw 'Visible shp must not switch the whole workspace theme.'
}

Write-Host 'Workspace keeps dark theme and switches only map canvas background for visible shp layers.'


