$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -LiteralPath (Join-Path $root 'LTZK.cpp') -Raw -Encoding UTF8
$header = Get-Content -LiteralPath (Join-Path $root 'LTZK.h') -Raw -Encoding UTF8

foreach ($required in @(
    'registerQualityResultLayer',
    'isQualityResultLayerInSelectedOutputDirs',
    'QList<QgsMapLayer*>& addedLayers',
    'ltzk/qualityResultShp',
    'm_qualityResultLayers.insert',
    'm_resultLayerModules.insert'
)) {
    if (-not ($source.Contains($required) -or $header.Contains($required))) {
        throw "Dual-display result registration is missing: $required"
    }
}

$syncStart = $source.IndexOf('int LTZK::syncQualityResultShpLayers()')
$syncEnd = $source.IndexOf('int LTZK::regroupQualityResultLayersBySourcePath()', $syncStart)
if ($syncStart -lt 0 -or $syncEnd -lt 0) {
    throw 'Could not locate the result SHP scan block.'
}
$syncBody = $source.Substring($syncStart, $syncEnd - $syncStart)

if ($syncBody -notmatch 'QgsProject::instance\(\)->addMapLayer\(layer, false\)') {
    throw 'The result scan must register the result layer without implicit root insertion.'
}
if ($syncBody -notmatch 'addQualityResultLayerToTree\(layer\)') {
    throw 'The result scan must add the result layer to the visible layer tree through the host path.'
}

if ($source -notmatch 'layersAdded,\s*this,\s*\[this\]\(const QList<QgsMapLayer\*>& addedLayers\)') {
    throw 'New project layers must be inspected for plugin-added result SHP layers.'
}
$hasExistingLayerRegistration = $source.Contains('QgsVectorLayer* existingVectorLayer') -and $source.Contains('registerQualityResultLayer(') -and $source.Contains('existingVectorLayer, m_currentBusinessModuleTitle')
if (-not $hasExistingLayerRegistration) {
    throw 'Already-added SHP layers in selected output directories must be registered in the result registry.'
}

$canvasStart = $source.IndexOf('void LTZK::refreshMapCanvasLayerStack()')
$canvasEnd = $source.IndexOf('void LTZK::hideQualityResultCanvasLayers()', $canvasStart)
if ($canvasStart -lt 0 -or $canvasEnd -lt 0) {
    throw 'Could not locate the map canvas layer-stack logic.'
}
$canvasBody = $source.Substring($canvasStart, $canvasEnd - $canvasStart)
foreach ($required in @(
    'ltzk/qualityResultShp',
    'm_qualityResultVisiblePaths',
    'm_resultLayerModules.value(resultPath)'
)) {
    if (-not $canvasBody.Contains($required)) {
        throw "Result layer map visibility state is missing: $required"
    }
}

Write-Host 'Quality result SHP loading remains wired for the layer tree and map canvas state.'
