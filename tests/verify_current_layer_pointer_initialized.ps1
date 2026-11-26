$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$header = Join-Path $root 'LTZK.h'
$source = Join-Path $root 'LTZK.cpp'
$iface = Join-Path $root 'LTZKQgisInterface.cpp'

foreach ($file in @($header, $source, $iface)) {
    if (-not (Test-Path $file)) {
        throw "Missing required file: $file"
    }
}

$headerText = Get-Content -Raw -Encoding Default $header
$sourceText = Get-Content -Raw -Encoding Default $source
$ifaceText = Get-Content -Raw -Encoding Default $iface

if (-not $ifaceText.Contains('return mApp ? mApp->currentMapLayer() : nullptr;')) {
    throw 'QGIS interface activeLayer no longer exposes LTZK currentMapLayer; update this test if the interface contract changed.'
}

if (-not ($headerText.Contains('QgsMapLayer* m_curMapLayer = nullptr;') -or $sourceText.Contains('m_curMapLayer = nullptr;'))) {
    throw 'm_curMapLayer must be initialized to nullptr before plugins can query activeLayer during startup.'
}

Write-Host 'Current layer pointer is initialized before plugin activeLayer access.'
