$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$headerPath = Join-Path $root 'LTZK.h'

foreach ($path in @($sourcePath, $headerPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing source: $path"
    }
}

$source = Get-Content -Raw -Encoding UTF8 $sourcePath
$header = Get-Content -Raw -Encoding UTF8 $headerPath
$all = "$header`n$source"

foreach ($required in @(
    'selectedNodes(true)',
    'QgsLayerTree::toGroup',
    '删除组',
    'renameLayerTreeGroup',
    'QInputDialog::getText',
    'groupNode->setName',
    'removeChildNode',
    'm_removedQualityResultGroupPaths',
    'isQualityResultGroupPathSuppressed',
    'rememberQualityResultGroupPaths'
)) {
    if (-not $all.Contains($required)) {
        throw "Missing layer-tree group contract: $required"
    }
}

if ($source -notmatch 'setSelectionMode\s*\(\s*QAbstractItemView::ExtendedSelection\s*\)') {
    throw 'Missing layer-tree group contract: ExtendedSelection'
}

$removeLayerStart = $source.IndexOf('void LTZK::removeLayer()')
$removeLayerEnd = $source.IndexOf('void LTZK::initLayerTreeView()', $removeLayerStart)
if ($removeLayerStart -lt 0 -or $removeLayerEnd -le $removeLayerStart) {
    throw 'Missing layer-tree removal function body'
}
$removeLayerBody = $source.Substring(
    $removeLayerStart,
    $removeLayerEnd - $removeLayerStart)

if ($source -notmatch 'm_layerTreeView\s*->\s*selectedNodes\s*\(\s*true\s*\)') {
    throw 'Batch removal must request top-level selected nodes from QGIS'
}
if ($source -match 'm_layerTreeView\s*->\s*selectedNodes\s*\(\s*false\s*\)') {
    throw 'Batch removal must not include descendants of selected groups'
}
$executeBody = $source.Substring(
    $source.IndexOf('void LTZK::removeLayerTreeNodes'),
    $source.IndexOf('void LTZK::onCurrentLayerChanged', $source.IndexOf('void LTZK::removeLayerTreeNodes')) -
        $source.IndexOf('void LTZK::removeLayerTreeNodes'))
if ($executeBody -notmatch 'QgsLayerTree::isLayer\s*\(\s*node\s*\)') {
    throw 'Layer-tree removal must verify a node is a layer before toLayer()'
}
if ($executeBody -notmatch 'QgsLayerTree::isGroup\s*\(\s*node\s*\)') {
    throw 'Layer-tree removal must verify a node is a group before toGroup()'
}
if ($source -match 'selectionModel\s*->\s*clear\s*\(') {
    throw 'Layer-tree removal must let the QGIS model update selection during node removal'
}
if ($executeBody -match 'm_curMapLayer\s*=\s*m_layerTreeView\s*->\s*currentLayer\s*\(\s*\)') {
    throw 'Batch layer-tree removal must not read currentLayer after deleting nodes'
}

if ($source -notmatch 'contextGroup\s*=\s*asLayerTreeGroupNode\s*\(\s*contextNode\s*\)') {
    throw 'Group context-menu dispatch must use the checked group conversion helper'
}
if ($source -notmatch 'contextLayerNode\s*=\s*asLayerTreeLayerNode\s*\(\s*contextNode\s*\)') {
    throw 'Layer context-menu dispatch must use the checked layer conversion helper'
}

$executeStart = $source.IndexOf('void LTZK::removeLayerTreeNodes')
$executeEnd = $source.IndexOf('void LTZK::onCurrentLayerChanged', $executeStart)
if ($executeStart -lt 0 -or $executeEnd -le $executeStart) {
    throw 'Unable to isolate the complete group removal helper'
}
$executeBody = $source.Substring(
    $executeStart,
    $executeEnd - $executeStart)
if ($executeBody -notmatch 'parentGroup\s*->\s*removeChildNode\s*\(\s*groupNode\s*\)') {
    throw 'Group removal helper must remove the selected group node from its parent'
}
if ($executeBody -match 'QFile::remove|removeRecursively|DROP\s+TABLE') {
    throw 'Group removal branch must not delete project layers or source data'
}

foreach ($forbidden in @(
    'QFile::remove',
    'QDir::removeRecursively',
    'DROP TABLE'
)) {
    if ($source.Contains($forbidden)) {
        throw "Group removal must not delete source data: $forbidden"
    }
}

Write-Output 'Layer tree group context-menu contract passed.'
