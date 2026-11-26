$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root "LTZK.cpp"

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$sourceText = Get-Content $source -Raw -Encoding Default

if ($sourceText -match "model\(\)->removeRow") {
    throw "Layer removal still deletes rows directly from QgsLayerTreeModel"
}

if ($sourceText -notmatch "actionRemoveGroupOrLayer") {
    throw "Layer removal does not use QgsLayerTreeViewDefaultActions::actionRemoveGroupOrLayer"
}

if ($sourceText -notmatch "QAction\*\s+actionRemoveLayer\s*=\s*m_layerTreeView->defaultActions\(\)->actionRemoveGroupOrLayer") {
    throw "Layer tree toolbar remove action is not the QGIS default remove action"
}

if ($sourceText -notmatch "QAction\*\s+actionRemoveContextLayer\s*=\s*m_layerTreeView->defaultActions\(\)->actionRemoveGroupOrLayer") {
    throw "Layer tree context menu does not offer the QGIS default remove action"
}

Write-Host "Layer removal uses QGIS default actions."
