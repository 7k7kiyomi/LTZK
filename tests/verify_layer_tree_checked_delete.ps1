$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing source: $sourcePath"
}

$source = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8

$modelStart = $source.IndexOf('class LTZKLayerTreeModel')
$modelEnd = $source.IndexOf('};', $modelStart)
if ($modelStart -lt 0 -or $modelEnd -le $modelStart) {
    throw 'Could not locate LTZKLayerTreeModel.'
}
$modelBody = $source.Substring($modelStart, $modelEnd - $modelStart)

foreach ($required in @(
        'QVariant data(',
        'Qt::CheckStateRole',
        'Qt::Checked',
        'Qt::PartiallyChecked',
        'Qt::Unchecked',
        'isItemVisibilityCheckedRecursive',
        'isItemVisibilityUncheckedRecursive',
        'QgsLayerTreeModel::data(index, role)')) {
    if (-not $modelBody.Contains($required)) {
        throw "Layer-tree model is missing three-state checkbox behavior: $required"
    }
}

foreach ($required in @(
        'collectCheckedLayerTreeNodes',
        'mergeLayerTreeRemovalTargets',
        'confirmLayerTreeRemoval',
        'removeLayerTreeNodes',
        'selectedNodes(true)',
        'Qt::CheckStateRole',
        'QMessageBox::question')) {
    if (-not $source.Contains($required)) {
        throw "Checked-delete implementation is missing: $required"
    }
}
if (-not $source.Contains('if (!state.isValid())')) {
    throw 'Nodes without a visible CheckStateRole must not be treated as checked.'
}
if ($source -notmatch '(QgsProject::instance\(\)|project)\s*->\s*removeMapLayers') {
    throw 'Checked-delete implementation must batch remove map layers.'
}

$collectStart = $source.IndexOf('collectCheckedLayerTreeNodes')
$collectEnd = $source.IndexOf('QList<QgsLayerTreeNode*> mergeLayerTreeRemovalTargets', $collectStart)
if ($collectStart -lt 0 -or $collectEnd -le $collectStart) {
    throw 'Unable to isolate checked-node collection.'
}
$collectBody = $source.Substring($collectStart, $collectEnd - $collectStart)
foreach ($required in @(
        'root->children()',
        'Qt::Checked',
        'Qt::PartiallyChecked',
        'collectCheckedLayerTreeNodes')) {
    if (-not $collectBody.Contains($required)) {
        throw "Checked-node collection is missing: $required"
    }
}

$mergeStart = $source.IndexOf('QList<QgsLayerTreeNode*> mergeLayerTreeRemovalTargets')
$mergeEnd = $source.IndexOf('bool confirmLayerTreeRemoval', $mergeStart)
if ($mergeStart -lt 0 -or $mergeEnd -le $mergeStart) {
    throw 'Unable to isolate target merge.'
}
$mergeBody = $source.Substring($mergeStart, $mergeEnd - $mergeStart)
foreach ($required in @(
        'selectedNodes(true)',
        'contextNode',
        'QSet<QgsLayerTreeNode*>',
        'parent()')) {
    if (-not $mergeBody.Contains($required)) {
        throw "Target merge is missing: $required"
    }
}

$confirmStart = $source.IndexOf('bool confirmLayerTreeRemoval')
$confirmEnd = $source.IndexOf('void LTZK::removeLayer()', $confirmStart)
if ($confirmStart -lt 0 -or $confirmEnd -le $confirmStart) {
    throw 'Unable to isolate confirmation helper.'
}
$confirmBody = $source.Substring($confirmStart, $confirmEnd - $confirmStart)
foreach ($required in @(
        'QMessageBox::question',
        'QMessageBox::No',
        'QgsLayerTreeNode')) {
    if (-not $confirmBody.Contains($required)) {
        throw "Deletion confirmation is missing: $required"
    }
}

$removeStart = $source.IndexOf('void LTZK::removeLayer()')
$removeEnd = $source.IndexOf('void LTZK::onCurrentLayerChanged', $removeStart)
if ($removeStart -lt 0 -or $removeEnd -le $removeStart) {
    throw 'Unable to isolate removeLayer.'
}
$removeBody = $source.Substring($removeStart, $removeEnd - $removeStart)
if ($removeBody -notmatch 'mergeLayerTreeRemovalTargets') {
    throw 'removeLayer must collect checked and selected targets.'
}

$executeStart = $source.IndexOf('removeLayerTreeNodes')
$executeStart = $source.LastIndexOf('void LTZK::', $executeStart)
$executeEnd = $source.IndexOf('void LTZK::onCurrentLayerChanged', $executeStart)
if ($executeStart -lt 0 -or $executeEnd -le $executeStart) {
    throw 'Unable to isolate node-removal execution.'
}
$executeBody = $source.Substring($executeStart, $executeEnd - $executeStart)
foreach ($required in @(
        'rememberQualityResultGroupPaths',
        'removeChildNode',
        'removeMapLayers')) {
    if (-not $executeBody.Contains($required)) {
        throw "Safe node-removal execution is missing: $required"
    }
}
foreach ($forbidden in @(
        'QFile::remove',
        'QDir::removeRecursively',
        'DROP TABLE')) {
    if ($executeBody.Contains($forbidden)) {
        throw "Layer-tree deletion must not delete source data: $forbidden"
    }
}

Write-Output 'Layer tree checked-delete contract passed.'
