$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$cpp = Get-Content -LiteralPath (Join-Path $repoRoot "LTZK.cpp") -Raw -Encoding UTF8
$header = Get-Content -LiteralPath (Join-Path $repoRoot "LTZK.h") -Raw -Encoding UTF8

function Assert-Contains {
    param(
        [string]$Content,
        [string]$Pattern,
        [string]$Message
    )

    if ($Content -notmatch $Pattern) {
        throw $Message
    }
}

Assert-Contains $header 'visibleLayersContainShp\([^)]*includeSystemBasemap' "SHP visibility helper should expose an explicit system-basemap inclusion option."
Assert-Contains $cpp '\(\!includeSystemBasemap && isSystemBasemapLayer\(layer\)\)' "The generic visible SHP check should keep excluding system basemaps unless explicitly requested."
Assert-Contains $cpp '(?s)menu->clear\(\).*visibleLayersContainShp\(canvasLayers, true\)' "Canvas context menu should clear native QGIS items and include the system basemap when checking for SHP."

Write-Host "SHP canvas context menu behavior verified."
