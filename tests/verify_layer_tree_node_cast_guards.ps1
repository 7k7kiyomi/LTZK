$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing source: $sourcePath"
}

$source = Get-Content -Raw -Encoding UTF8 $sourcePath

$groupHelperPattern = '(?s)QgsLayerTreeGroup\*\s+asLayerTreeGroupNode\s*\(\s*QgsLayerTreeNode\*\s+node\s*\).*?QgsLayerTree::isGroup\s*\(\s*node\s*\).*?QgsLayerTree::toGroup\s*\(\s*node\s*\).*?nullptr'
$layerHelperPattern = '(?s)QgsLayerTreeLayer\*\s+asLayerTreeLayerNode\s*\(\s*QgsLayerTreeNode\*\s+node\s*\).*?QgsLayerTree::isLayer\s*\(\s*node\s*\).*?QgsLayerTree::toLayer\s*\(\s*node\s*\).*?nullptr'
if ($source -notmatch $groupHelperPattern) {
    throw 'Missing checked group-node conversion helper'
}
if ($source -notmatch $layerHelperPattern) {
    throw 'Missing checked layer-node conversion helper'
}

$directCasts = [regex]::Matches(
    $source,
    'QgsLayerTree::to(?:Layer|Group)\s*\(')
if ($directCasts.Count -ne 2) {
    throw "Direct QGIS layer-tree casts are only allowed inside checked helpers; found $($directCasts.Count)"
}

$rememberStart = $source.IndexOf('void LTZK::rememberQualityResultGroupPaths')
$rememberEnd = $source.IndexOf('bool LTZK::isQualityResultGroupPathSuppressed', $rememberStart)
if ($rememberStart -lt 0 -or $rememberEnd -le $rememberStart) {
    throw 'Missing quality-result group path traversal'
}
$rememberBody = $source.Substring($rememberStart, $rememberEnd - $rememberStart)
if ($rememberBody -notmatch 'asLayerTreeGroupNode\s*\(\s*child\s*\)') {
    throw 'Quality-result group recursion must use checked group conversion'
}

$cleanupStart = $source.IndexOf('int LTZK::removeMapQualitySeriesIntermediateResultNodes')
$cleanupEnd = $source.IndexOf('void LTZK::pruneQualityResultLayerRegistry', $cleanupStart)
if ($cleanupStart -lt 0 -or $cleanupEnd -le $cleanupStart) {
    throw 'Missing quality-result cleanup traversal'
}
$cleanupBody = $source.Substring($cleanupStart, $cleanupEnd - $cleanupStart)
$layerConversions = [regex]::Matches(
    $cleanupBody,
    'asLayerTreeLayerNode\s*\(\s*child\s*\)')
$groupConversions = [regex]::Matches(
    $cleanupBody,
    'asLayerTreeGroupNode\s*\(\s*child\s*\)')
if ($layerConversions.Count -lt 2 -or $groupConversions.Count -lt 2) {
    throw 'Quality-result cleanup must use checked conversions in both recursive traversals'
}

Write-Output 'Layer-tree node cast guards verified.'
