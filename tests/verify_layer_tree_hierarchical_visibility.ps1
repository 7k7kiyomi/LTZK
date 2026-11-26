$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing source file: $sourcePath"
}

$source = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8
$initStart = $source.IndexOf('void LTZK::initLayerTreeView()')
if ($initStart -lt 0) {
    throw 'Could not locate LTZK::initLayerTreeView().'
}

$flagStart = $source.IndexOf('model->setFlag(', $initStart)
$flagEnd = $source.IndexOf('model->setAutoCollapseLegendNodes', $flagStart)
if ($flagStart -lt 0 -or $flagEnd -le $flagStart) {
    throw 'Could not locate the QgsLayerTreeModel flag configuration.'
}

$flagText = $source.Substring($flagStart, $flagEnd - $flagStart)
$flagTextNormalized = $flagText -replace '\s+', ''
foreach ($requiredFlag in @(
        'QgsLayerTreeModel::AllowNodeChangeVisibility',
        'QgsLayerTreeModel::ActionHierarchical')) {
    if (-not $flagTextNormalized.Contains($requiredFlag)) {
        throw "Layer-tree model must enable $requiredFlag."
    }
}

Write-Host 'Hierarchical layer-tree visibility configuration verified.'
