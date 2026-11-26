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
    'new QgsMapToolPan',
    'new QgsMapToolZoom',
    'm_mapCanvas->setMapTool(',
    'QTimer::singleShot(0, this, &LTZK::setDefaultPanMapTool)',
    'void LTZK::setDefaultPanMapTool()'
)) {
    if (-not $sourceText.Contains($required) -and -not $headerText.Contains($required)) {
        throw "Required map interaction wiring missing: $required"
    }
}

foreach ($forbidden in @(
    '&QgsMapCanvas::extentsChanged, this, [this]()',
    '&LTZK::constrainMapViewToGansu',
    'void LTZK::constrainMapViewToGansu()',
    'm_mapCanvas->setWheelFactor(0)',
    'NoWheel'
)) {
    if ($sourceText.Contains($forbidden) -or $headerText.Contains($forbidden)) {
        throw "Local Gansu map interaction should not be constrained or wheel-disabled: $forbidden"
    }
}

Write-Host 'Local Gansu map interaction is stable and unconstrained.'