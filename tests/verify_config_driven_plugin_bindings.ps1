$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$configPath = Join-Path $root 'config\plugin_bindings.json'
$configHeader = Join-Path $root 'PluginBindingConfig.h'
$configSource = Join-Path $root 'PluginBindingConfig.cpp'
$mainSource = Join-Path $root 'LTZK.cpp'
$project = Join-Path $root 'LTZK.vcxproj'
$filters = Join-Path $root 'LTZK.vcxproj.filters'

foreach ($path in @($configPath, $configHeader, $configSource, $mainSource, $project, $filters)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file for config-driven plugin bindings: $path"
    }
}

$json = Get-Content -Raw -Encoding UTF8 $configPath | ConvertFrom-Json
if (-not $json.plugins -or $json.plugins.Count -lt 1) {
    throw 'plugin_bindings.json must define at least one plugin.'
}

$firstPlugin = $json.plugins[0]
foreach ($required in @('id', 'dll', 'enabled', 'bindings')) {
    if (-not ($firstPlugin.PSObject.Properties.Name -contains $required)) {
        throw "First plugin config is missing required property: $required"
    }
}

$firstBinding = $firstPlugin.bindings[0]
foreach ($required in @('menuPath', 'featureName', 'actionKeywords', 'fallbackActionIndex', 'icon')) {
    if (-not ($firstBinding.PSObject.Properties.Name -contains $required)) {
        throw "First plugin binding is missing required property: $required"
    }
}

if ($firstPlugin.dll -ne 'plugin_map_quality_check_tools.dll') {
    throw 'Default config should map the current quality-check plugin DLL.'
}

$sheetEdgeFeatureName = -join ([char[]](0x56FE, 0x5E45, 0x63A5, 0x8FB9, 0x4E0E, 0x7CFB, 0x5217, 0x5316, 0x8868, 0x8FBE))
$sheetEdgeBinding = $firstPlugin.bindings | Where-Object { $_.featureName -eq $sheetEdgeFeatureName } | Select-Object -First 1
if (-not $sheetEdgeBinding) {
    throw 'Default config should map the sheet-edge and series expression feature.'
}

$mainText = Get-Content -Raw -Encoding Default $mainSource
foreach ($forbidden in @(
    'mapQualityCheckToolsPluginPathCandidates',
    'mapQualityCheckToolsPluginPath()',
    'mapQualityCheckToolsIconPaths'
)) {
    if ($mainText.Contains($forbidden)) {
        throw "LTZK.cpp should not keep hard-coded quality plugin helper: $forbidden"
    }
}

foreach ($required in @(
    '#include "PluginBindingConfig.h"',
    'loadPluginBindings',
    'resolvePluginDllPath',
    'bindPluginFeatureActions',
    'FeatureBinding',
    'appDir + QStringLiteral("/../../config/plugin_bindings.json")',
    'appDir + QStringLiteral("/plugins/") + dllName',
    'appDir + QStringLiteral("/../plugins/") + dllName'
)) {
    if (-not $mainText.Contains($required)) {
        throw "LTZK.cpp is missing config-driven binding detail: $required"
    }
}

$headerText = Get-Content -Raw -Encoding UTF8 $configHeader
$sourceText = Get-Content -Raw -Encoding UTF8 $configSource
foreach ($required in @('PluginBindingConfig', 'PluginDefinition', 'FeatureBinding')) {
    if (-not $headerText.Contains($required)) {
        throw "PluginBindingConfig.h is missing type: $required"
    }
}
foreach ($required in @('QJsonDocument', 'QJsonObject', 'QJsonArray', 'loadPluginBindingConfig')) {
    if (-not $sourceText.Contains($required)) {
        throw "PluginBindingConfig.cpp is missing parser detail: $required"
    }
}

$projectText = Get-Content -Raw -Encoding UTF8 $project
foreach ($required in @(
    '<ClCompile Include="PluginBindingConfig.cpp"',
    '<ClInclude Include="PluginBindingConfig.h"',
    'config\plugin_bindings.json',
    'CopyLtzKPluginConfig'
)) {
    if (-not $projectText.Contains($required)) {
        throw "LTZK.vcxproj is missing config-driven binding item: $required"
    }
}

$filtersText = Get-Content -Raw -Encoding UTF8 $filters
foreach ($required in @('PluginBindingConfig.cpp', 'PluginBindingConfig.h', 'config\plugin_bindings.json')) {
    if (-not $filtersText.Contains($required)) {
        throw "LTZK.vcxproj.filters is missing config-driven binding item: $required"
    }
}

Write-Host 'Config-driven plugin bindings verified.'

