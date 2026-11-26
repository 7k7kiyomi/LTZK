$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path -LiteralPath $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding Default $source
$start = $text.IndexOf('void LTZK::setGansuInitialMapView()')
$end = $text.IndexOf('void LTZK::setDefaultPanMapTool()', $start)
if ($start -lt 0 -or $end -lt 0) {
    throw 'Could not locate the Gansu initial map view function.'
}
$body = $text.Substring($start, $end - $start)

foreach ($required in @(
    'QgsPointXY gansuCenter = gansuWebMercatorCenter();',
    'const QgsRectangle basemapExtent =',
    'isSystemBasemapLayer(layer)',
    'm_mapCanvas->mapSettings().layerExtentToOutputExtent(layer, layer->extent())',
    'gansuCenter = basemapExtent.center();',
    'm_mapCanvas->setExtent(gansuExtent);',
    'm_mapCanvas->setCenter(gansuCenter);'
)) {
    if (-not $body.Contains($required)) {
        throw "Gansu initial map center contract is missing: $required"
    }
}

$extentIndex = $body.IndexOf('m_mapCanvas->setExtent(gansuExtent);')
$centerIndex = $body.IndexOf('m_mapCanvas->setCenter(gansuCenter);')
if ($extentIndex -lt 0 -or $centerIndex -lt 0 -or $extentIndex -gt $centerIndex) {
    throw 'Gansu initial view must set the existing extent before applying its center.'
}

Write-Host 'Gansu initial map center verification passed.'
