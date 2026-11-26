$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$cppPath = Join-Path $repoRoot "LTZK.cpp"
$headerPath = Join-Path $repoRoot "LTZK.h"

$cpp = Get-Content -LiteralPath $cppPath -Raw -Encoding UTF8
$header = Get-Content -LiteralPath $headerPath -Raw -Encoding UTF8

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

function Assert-NotContains {
    param(
        [string]$Content,
        [string]$Pattern,
        [string]$Message
    )

    if ($Content -match $Pattern) {
        throw $Message
    }
}

Assert-Contains $cpp 'contextMenuAboutToShow' "QgsMapCanvas should use its native context menu extension signal."
Assert-Contains $cpp 'QMenu\* menu, QgsMapMouseEvent\* event' "QGIS native context menu callback should receive the menu and mouse event."
Assert-Contains $cpp 'visibleLayersContainShp\(canvasLayers, true\)' "Canvas context menu should include the system basemap when checking for visible SHP layers."
Assert-Contains $cpp '选择画布颜色' "Map canvas context menu should label the canvas color action in Chinese."
Assert-Contains $cpp 'connect\(actionChooseCanvasColor, &QAction::triggered, this, &LTZK::chooseMapCanvasColor\)' "Canvas context menu should reuse chooseMapCanvasColor."
Assert-NotContains $cpp 'workspaceMapCanvasColorButton' "The old floating canvas color button should be removed."
Assert-NotContains $header 'm_mapCanvasColorButton' "The old floating canvas color member should be removed."

Write-Host "SHP canvas context menu verified."
