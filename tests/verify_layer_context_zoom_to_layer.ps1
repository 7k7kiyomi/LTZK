$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$sourceText = Get-Content $source -Raw -Encoding Default
$zoomLabel = -join ([char[]](0x7F29, 0x653E, 0x5230, 0x56FE, 0x5C42))

$start = $sourceText.IndexOf('void LTZK::initLayerTreeView()')
$end = $sourceText.IndexOf('void LTZK::on_actionOpenAttrTable_triggered()', $start + 1)

if ($start -lt 0 -or $end -lt 0 -or $end -le $start) {
    throw 'Cannot find initLayerTreeView method.'
}

$body = $sourceText.Substring($start, $end - $start)

if (-not $body.Contains($zoomLabel)) {
    throw 'Layer context menu is missing Zoom to Layer action text.'
}

if ($body -notmatch 'QAction\*\s+actionZoomToLayer\s*=\s*menu\.addAction') {
    throw 'Layer context menu does not create a Zoom to Layer action.'
}

if ($body -notmatch 'connect\s*\(\s*actionZoomToLayer\s*,\s*&QAction::triggered') {
    throw 'Zoom to Layer action is not connected.'
}

if ($body -notmatch 'layerExtentToOutputExtent\s*\(\s*layer\s*,\s*layer->extent\s*\(\s*\)\s*\)') {
    throw 'Zoom to Layer should transform the layer extent into map canvas output coordinates.'
}

if ($body -notmatch 'm_mapCanvas->zoomToFeatureExtent\s*\(') {
    throw 'Zoom to Layer should use QgsMapCanvas::zoomToFeatureExtent so point layers also zoom correctly.'
}

if ($body -notmatch 'm_mapCanvas->refresh\s*\(\s*\)') {
    throw 'Zoom to Layer does not refresh the map canvas.'
}

Write-Host 'Layer context menu zoom-to-layer action verified.'
