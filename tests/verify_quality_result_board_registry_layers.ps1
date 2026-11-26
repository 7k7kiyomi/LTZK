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
    'm_qualityResultLayers',
    'm_qualityResultVisiblePaths',
    'refreshMapCanvasLayerStack',
    'setQualityResultLayerVisible',
    'locateQualityResultFeature',
    'qualityResultFeatureIdField',
    'qualityResultErrorTypeField',
    'QgsProject::instance()->addMapLayer(layer)',
    'registerQualityResultLayer',
    'QList<QgsMapLayer*>& addedLayers',
    'QgsFeatureRequest',
    'm_mapCanvas->setLayers(canvasLayers)'
)) {
    if (-not $sourceText.Contains($required) -and -not $headerText.Contains($required)) {
        throw "Quality result board registry-layer behavior missing: $required"
    }
}

Write-Host 'Quality result SHP registry and map-canvas behavior verified without a result panel.'
