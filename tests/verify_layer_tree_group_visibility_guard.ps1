$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -LiteralPath (Join-Path $root 'LTZK.cpp') -Raw -Encoding UTF8

if ($source -notmatch 'class\s+LTZKLayerTreeModel\b') {
    throw 'The application layer-tree model guard is missing.'
}

$setDataStart = $source.IndexOf('bool setData(')
$setDataEnd = $source.IndexOf("};", $setDataStart)
if ($setDataStart -lt 0 -or $setDataEnd -le $setDataStart) {
    throw 'Could not locate the layer-tree model setData guard.'
}

$setDataBody = $source.Substring($setDataStart, $setDataEnd - $setDataStart)
foreach ($required in @(
        'Qt::CheckStateRole',
        'QgsLayerTreeModel::setData(index, value, role)',
        'QgsLayerTree::isGroup',
        'setItemVisibilityCheckedRecursive')) {
    if (-not $setDataBody.Contains($required)) {
        throw "Layer-tree group visibility guard is missing: $required"
    }
}

$modelConstruction = $source.IndexOf('new LTZKLayerTreeModel(')
if ($modelConstruction -lt 0) {
    throw 'initLayerTreeView must construct LTZKLayerTreeModel.'
}

Write-Host 'Layer-tree group visibility guard verified.'
