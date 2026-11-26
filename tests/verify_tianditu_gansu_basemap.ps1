$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'

foreach ($path in @($source, $header)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$sourceText = Get-Content -Raw -Encoding UTF8 $source
$headerText = Get-Content -Raw -Encoding UTF8 $header

foreach ($required in @(
    'loadLocalGansuShpBasemap',
    'styleLocalGansuBasemapLayer',
    'localGansuShpBasemapPath',
    'localShpBasemapDirectories',
    'local_shp',
    'QDirIterator it(root, QStringList() << QStringLiteral("*.shp")',
    'QCoreApplication::applicationDirPath()',

    'gansuLayer->setSubsetString',
    '\u7518\u8083\u7701',
    'QgsVectorLayer* gansuLayer',
    'applyWorkspaceShpCanvasBackground(m_workspaceUsesLightShpCanvas)',
    'm_mapCanvas->refresh()',
    'new QgsVectorLayer(gansuPath, layerName, QStringLiteral("ogr"))',
    'setCustomProperty(QStringLiteral("ltzk/systemBasemap"), true)',
    'QgsProject::instance()->addMapLayer(gansuLayer, false)',
    'QgsProject::instance()->layerTreeRoot()->insertLayer(0, gansuLayer)',
    'QgsFillSymbol::createSimple',
    'QgsSingleSymbolRenderer',
    'setOpacity(isWhiteTheme ? 1.0 : 0.82)',
    'setGansuInitialMapView',
    'EPSG:3857',
    'm_mapCanvas->setExtent(gansuExtent)',
    'QgsPointXY gansuWebMercatorCenter()',
    '103.8263',
    '36.0594',
    'm_mapCanvas->setCenter(gansuCenter)',
    'm_initialMapExtent = m_mapCanvas->extent()',
    'QTimer::singleShot(0, this, &LTZK::loadLocalGansuShpBasemap)'
)) {
    if (-not $sourceText.Contains($required) -and -not $headerText.Contains($required)) {
        throw "Local Gansu SHP basemap wiring missing: $required"
    }
}


$loadStart = $sourceText.IndexOf('void LTZK::loadLocalGansuShpBasemap()')
$loadEnd = $sourceText.IndexOf('void LTZK::styleLocalGansuBasemapLayer', $loadStart)
if ($loadStart -lt 0 -or $loadEnd -lt 0) {
    throw 'Could not locate loadLocalGansuShpBasemap.'
}
$loadBody = $sourceText.Substring($loadStart, $loadEnd - $loadStart)
$themeCanvasIndex = $loadBody.IndexOf('applyWorkspaceShpCanvasBackground(m_workspaceUsesLightShpCanvas);')
$initialRefreshIndex = $loadBody.IndexOf('m_mapCanvas->refresh();')
$layerCreateIndex = $loadBody.IndexOf('new QgsVectorLayer(gansuPath, layerName, QStringLiteral("ogr"))')
if ($themeCanvasIndex -lt 0 -or $initialRefreshIndex -lt 0 -or $layerCreateIndex -lt 0 -or $themeCanvasIndex -gt $layerCreateIndex -or $initialRefreshIndex -gt $layerCreateIndex) {
    throw 'Local Gansu SHP basemap must reapply the current theme canvas before loading the shp layer.'
}
if ($loadBody.Contains('m_mapCanvas->setCanvasColor(QColor(3, 16, 29));')) {
    throw 'Local Gansu SHP basemap must not force the map canvas to sci-fi dark.'
}
foreach ($forbidden in @(
    '\u0032\u0030\u0032\u0034\u5e74\u521d\u5730\u7ea7.shp',
    '\u0032\u0030\u0032\u0034\u5e74\u5730\u5e02\u7ea7\u8fb9\u754c',
    'QTimer::singleShot(0, this, &LTZK::loadTiandituGansuBasemap)',
    'tiandituXyzUri(QStringLiteral("ibo_w"))',
    'tiandituXyzUri(QStringLiteral("cia_w"))',
    'QgsRasterLayer* roadLayer',
    'QgsRasterLayer* labelLayer',
    'styleTiandituOverlayLayer'
)) {
    if ($sourceText.Contains($forbidden) -or $headerText.Contains($forbidden)) {
        throw "Local Gansu SHP basemap should replace Tianditu raster wiring: $forbidden"
    }
}

foreach ($coord in @('103.25', '32.1', '109.7', '42.95')) {
    if (-not $sourceText.Contains($coord)) {
        throw "Gansu initial view should include province bounds coordinate: $coord"
    }
}

Write-Host 'Local Gansu SHP theme-aware basemap wiring verified.'









