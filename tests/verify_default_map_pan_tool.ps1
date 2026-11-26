$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding Default $source

foreach ($required in @(
    'm_panTool',
    'new QgsMapToolPan',
    'm_mapCanvas',
    'm_mapCanvas->setMapTool(',
    'QTimer::singleShot(0, this, &LTZK::setDefaultPanMapTool)',
    'void LTZK::setDefaultPanMapTool()',
    'm_zoomInTool',
    'new QgsMapToolZoom',
    'm_zoomOutTool',
    'on_actionZoomIn_triggered',
    'on_actionZoomOut_triggered',
    'on_actionPan_triggered'
)) {
    if (-not $text.Contains($required)) {
        throw "Map pan/zoom wiring missing: $required"
    }
}

if ($text.Contains('m_mapCanvas->setWheelFactor(0)') -or $text.Contains('NoWheel')) {
    throw 'Map canvas wheel zoom appears to be disabled.'
}

Write-Host 'Default map drag pan and zoom tool wiring verified.'