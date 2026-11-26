$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding Default $source

foreach ($required in @(
    'm_mapCanvas->setCachingEnabled(true);',
    'm_mapCanvas->setParallelRenderingEnabled(true);',
    'm_mapCanvas->setPreviewJobsEnabled(false);',
    'm_mapCanvas->setMapUpdateInterval(250);',
    'QTimer::singleShot(0, this, &LTZK::loadLocalGansuShpBasemap);',
    'setGansuInitialMapView();'
)) {
    if (-not $text.Contains($required)) {
        throw "Local Gansu render stability setting missing: $required"
    }
}

foreach ($forbidden in @(
    'QTimer::singleShot(0, this, &LTZK::setGansuInitialMapView)',
    'QTimer::singleShot(300, this, &LTZK::setGansuInitialMapView)',
    'QTimer::singleShot(1200, this, &LTZK::setGansuInitialMapView)'
)) {
    if ($text.Contains($forbidden)) {
        throw "Local Gansu initial view should not be repeatedly reset after load: $forbidden"
    }
}

Write-Host 'Local Gansu render stability settings verified.'
