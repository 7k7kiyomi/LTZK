$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$cpp = Get-Content -LiteralPath (Join-Path $repoRoot "LTZK.cpp") -Raw -Encoding UTF8

function Assert-Contains {
    param([string]$Content, [string]$Pattern, [string]$Message)
    if ($Content -notmatch $Pattern) { throw $Message }
}

function Assert-NotContains {
    param([string]$Content, [string]$Pattern, [string]$Message)
    if ($Content -match $Pattern) { throw $Message }
}

Assert-Contains $cpp 'contextMenuAboutToShow' "QgsMapCanvas should use its native context menu extension signal."
Assert-Contains $cpp 'QMenu\* menu, QgsMapMouseEvent\* event' "Canvas context menu callback should receive QGIS menu and mouse event."
Assert-Contains $cpp 'menu->addAction\(QStringLiteral\("选择画布颜色"\)\)' "Canvas color action should be appended to the native QGIS menu."
Assert-Contains $cpp 'connect\(actionChooseCanvasColor, &QAction::triggered, this, &LTZK::chooseMapCanvasColor\)' "Native canvas menu action should reuse chooseMapCanvasColor."
Assert-NotContains $cpp 'm_mapCanvas->setContextMenuPolicy\(Qt::CustomContextMenu\)' "Canvas should not replace QGIS native context menu policy."
Assert-NotContains $cpp 'm_mapCanvas, &QWidget::customContextMenuRequested' "Canvas color action should not depend on QWidget customContextMenuRequested."

Write-Host "QGIS native SHP canvas context menu integration verified."
