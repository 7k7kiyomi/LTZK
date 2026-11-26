$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -LiteralPath (Join-Path $root 'LTZK.cpp') -Raw -Encoding UTF8
$header = Get-Content -LiteralPath (Join-Path $root 'LTZK.h') -Raw -Encoding UTF8
$config = Get-Content -LiteralPath (Join-Path $root 'config\plugin_bindings.json') -Raw -Encoding UTF8 | ConvertFrom-Json

$qualityPlugin = @($config.plugins | Where-Object { $_.id -eq 'map_quality_check_tools' })[0]
if ($null -eq $qualityPlugin) {
    throw 'The map quality-check plugin binding is missing.'
}

$edgeName = -join ([char[]](0x56FE, 0x5E45, 0x63A5, 0x8FB9, 0x68C0, 0x67E5))
$crossName = -join ([char[]](0x8DE8, 0x6BD4, 0x4F8B, 0x5C3A, 0x68C0, 0x67E5))
$taskName = -join ([char[]](0x8D28, 0x68C0, 0x4EFB, 0x52A1, 0x7BA1, 0x7406))

$edgeBinding = @($qualityPlugin.bindings | Where-Object { $_.featureName -eq $edgeName })[0]
$crossBinding = @($qualityPlugin.bindings | Where-Object { $_.featureName -eq $crossName })[0]
$taskBinding = @($qualityPlugin.bindings | Where-Object { $_.featureName -eq $taskName })[0]
if ($null -eq $edgeBinding -or $null -eq $crossBinding -or $null -eq $taskBinding) {
    throw 'One of the quality-check feature bindings is missing.'
}

if ($edgeBinding.resultOrganization -ne 'mapQualityError' -or
    $crossBinding.resultOrganization -ne 'mapQualityError') {
    throw 'Edge-match and cross-scale features must use the special result organization.'
}
if ($null -ne $taskBinding.resultOrganization -and
    -not [string]::IsNullOrWhiteSpace([string]$taskBinding.resultOrganization)) {
    throw 'The task-manager feature must keep the generic result organization.'
}

$helperDeclaration = 'int removeMapQualitySeriesIntermediateResultNodes();'
$helperDefinition = 'int LTZK::removeMapQualitySeriesIntermediateResultNodes()'
$seriesGroupLiteral = 'QStringLiteral("\u7CFB\u5217\u56FE\u68C0\u67E5")'
foreach ($required in @($helperDeclaration, $helperDefinition, $seriesGroupLiteral)) {
    if (-not ($header + $source).Contains($required)) {
        throw "Special series-result cleanup contract is missing: $required"
    }
}

$helperStart = $source.IndexOf($helperDefinition)
$helperEnd = $source.IndexOf('void LTZK::pruneQualityResultLayerRegistry()', $helperStart)
if ($helperStart -lt 0 -or $helperEnd -le $helperStart) {
    throw 'Could not locate the special series-result cleanup implementation.'
}
$helperBody = $source.Substring($helperStart, $helperEnd - $helperStart)
foreach ($required in @(
    'if (!usesMapQualityErrorResultOrganization())',
    'QgsProject::instance()',
    'root->children()',
    'QgsLayerTree::toGroup',
    'collectMapQualityErrorSearchRoots()',
    'isQualityResultLayerInSearchRoots',
    'merged_index',
    'completeBaseName()',
    'containsCanonicalResult',
    'containsIntermediateResult',
    'removeChildNode',
    'layerTreeRegistryBridge',
    'bridge->setEnabled(false)'
)) {
    if (-not $helperBody.Contains($required)) {
        throw "Special cleanup must contain the guarded tree-removal operation: $required"
    }
}

$timerStart = $source.IndexOf('connect(m_qualityResultScanTimer, &QTimer::timeout')
$timerEnd = $source.IndexOf('m_qualityResultOutputWatcher = new QFileSystemWatcher', $timerStart)
if ($timerStart -lt 0 -or $timerEnd -le $timerStart) {
    throw 'Could not locate the quality-result scan timer callback.'
}
$timerBody = $source.Substring($timerStart, $timerEnd - $timerStart)
if (-not $timerBody.Contains('removeMapQualitySeriesIntermediateResultNodes()')) {
    throw 'The post-action result synchronization does not remove the plugin intermediate group.'
}

Write-Host 'Edge/cross-scale series-result visibility regression checks passed.'
