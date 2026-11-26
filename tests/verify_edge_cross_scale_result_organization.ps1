$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$configPath = Join-Path $root 'config\plugin_bindings.json'
$config = Get-Content -LiteralPath $configPath -Raw -Encoding UTF8 | ConvertFrom-Json
$header = Get-Content -LiteralPath (Join-Path $root 'PluginBindingConfig.h') -Raw -Encoding UTF8
$parser = Get-Content -LiteralPath (Join-Path $root 'PluginBindingConfig.cpp') -Raw -Encoding UTF8
$source = Get-Content -LiteralPath (Join-Path $root 'LTZK.cpp') -Raw -Encoding UTF8
$ltzkHeader = Get-Content -LiteralPath (Join-Path $root 'LTZK.h') -Raw -Encoding UTF8

$qualityPlugin = @($config.plugins | Where-Object { $_.id -eq 'map_quality_check_tools' })[0]
if ($null -eq $qualityPlugin) {
    throw 'The map quality-check plugin binding is missing.'
}

$edgeName = -join ([char[]](0x56FE, 0x5E45, 0x63A5, 0x8FB9, 0x68C0, 0x67E5))
$crossName = -join ([char[]](0x8DE8, 0x6BD4, 0x4F8B, 0x5C3A, 0x68C0, 0x67E5))
$taskName = -join ([char[]](0x8D28, 0x68C0, 0x4EFB, 0x52A1, 0x7BA1, 0x7406))
$qualityErrorFolderLiteral = 'QStringLiteral("\u8D28\u68C0\u9519\u8BEF\u7ED3\u679C")'

$edgeBinding = @($qualityPlugin.bindings | Where-Object { $_.featureName -eq $edgeName })[0]
$crossBinding = @($qualityPlugin.bindings | Where-Object { $_.featureName -eq $crossName })[0]
$taskBinding = @($qualityPlugin.bindings | Where-Object { $_.featureName -eq $taskName })[0]

if ($null -eq $edgeBinding -or $null -eq $crossBinding -or $null -eq $taskBinding) {
    throw 'One of the quality-check feature bindings is missing.'
}

if ($edgeBinding.resultOrganization -ne 'mapQualityError') {
    throw 'The edge-match feature must opt into mapQualityError result organization.'
}
if ($crossBinding.resultOrganization -ne 'mapQualityError') {
    throw 'The cross-scale feature must opt into mapQualityError result organization.'
}
if ($null -ne $taskBinding.resultOrganization -and
    -not [string]::IsNullOrWhiteSpace([string]$taskBinding.resultOrganization)) {
    throw 'The task-manager feature must keep the generic result organization.'
}

foreach ($required in @(
    'QString resultOrganization;',
    'resultOrganization = object.value(QStringLiteral("resultOrganization"))'
)) {
    if (-not ($header.Contains($required) -or $parser.Contains($required))) {
        throw "Binding result organization metadata is missing: $required"
    }
}

foreach ($required in @(
    'usesMapQualityErrorResultOrganization',
    'collectMapQualityErrorSearchRoots',
    'collectQualityErrorDirectories',
    'isQualityResultLayerInSearchRoots'
)) {
    if (-not ($ltzkHeader.Contains($required) -or $source.Contains($required))) {
        throw "Special result organization contract is missing: $required"
    }
}
if (-not $source.Contains($qualityErrorFolderLiteral)) {
    throw 'The canonical quality-error folder name is missing from the host implementation.'
}

$collectStart = $source.IndexOf('QStringList LTZK::collectQualityResultSearchRoots() const')
$collectEnd = $source.IndexOf('int LTZK::syncQualityResultShpLayers()', $collectStart)
if ($collectStart -lt 0 -or $collectEnd -le $collectStart) {
    throw 'Could not locate quality-result search-root collection.'
}
$collectBody = $source.Substring($collectStart, $collectEnd - $collectStart)
if (-not $collectBody.Contains('usesMapQualityErrorResultOrganization()')) {
    throw 'Search-root collection does not branch for the two special features.'
}
if (-not $collectBody.Contains('collectMapQualityErrorSearchRoots()')) {
    throw 'Search-root collection does not use canonical quality-error roots.'
}
if (-not $collectBody.Contains('return m_qualityResultOutputDirs;')) {
    throw 'Generic result search roots no longer return the user-selected directories.'
}

if ($source -notmatch 'beginQualityResultMonitoring\([^;]*binding\.resultOrganization') {
    throw 'The active binding result organization is not passed into result monitoring.'
}

Write-Host 'Edge/cross-scale result organization regression checks passed.'
