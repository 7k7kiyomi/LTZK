$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'
$config = Join-Path $root 'config\plugin_bindings.json'

foreach ($path in @($source, $header, $config)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$sourceText = Get-Content -Raw -Encoding Default $source
$headerText = Get-Content -Raw -Encoding Default $header
$json = Get-Content -Raw -Encoding UTF8 $config | ConvertFrom-Json

$qualityModuleTitle = -join ([char[]](0x5730, 0x56FE, 0x6210, 0x679C, 0x8D28, 0x68C0))
$serviceModuleTitle = -join ([char[]](0x5730, 0x56FE, 0x670D, 0x52A1))

$qualityPlugin = $json.plugins | Where-Object { $_.dll -eq 'plugin_map_quality_check_tools.dll' } | Select-Object -First 1
if (-not $qualityPlugin) {
    throw 'Quality plugin dll is missing from plugin_bindings.json.'
}

$qualityBinding = $qualityPlugin.bindings | Where-Object {
    $_.menuPath.Count -gt 0 -and $_.menuPath[0] -eq $qualityModuleTitle
} | Select-Object -First 1
if (-not $qualityBinding) {
    throw 'Quality plugin must be scoped to the quality module through menuPath.'
}

$wrongBinding = $qualityPlugin.bindings | Where-Object {
    $_.menuPath.Count -gt 0 -and $_.menuPath[0] -eq $serviceModuleTitle
} | Select-Object -First 1
if ($wrongBinding) {
    throw 'Quality plugin must not be scoped to the map service module.'
}

foreach ($required in @(
    'PluginBindingConfig m_pluginBindingConfig;',
    'QList<QAction*> configuredFeatureActionsForModule(const QString& moduleTitle) const;',
    'void LTZK::setWorkspacePluginActions(const QList<QAction*>& actions)',
    'QList<QAction*> LTZK::configuredFeatureActionsForModule(const QString& moduleTitle) const',
    'businessModuleMenuTitle(binding.menuPath.first()) == businessModuleMenuTitle(moduleTitle)',
    'action->objectName().startsWith(QStringLiteral("pluginFeature_"))',
    'configuredFeatureActionsForModule(moduleTitle)',
    'setWorkspacePluginActions(workspacePluginActions)',
    'configuredFeatureActionsForModule(moduleTitle)',
    'setWorkspacePluginActions(workspacePluginActions)'
)) {
    if (-not $sourceText.Contains($required) -and -not $headerText.Contains($required)) {
        throw "Quality module plugin toolbar loading is missing: $required"
    }
}

$activateStart = $sourceText.IndexOf('void LTZK::activateBusinessModule')
$activateEnd = $sourceText.IndexOf('void LTZK::beginQualityResultMonitoring', $activateStart)
if ($activateStart -lt 0 -or $activateEnd -lt 0) {
    throw 'Could not locate activateBusinessModule body.'
}

$activateBody = $sourceText.Substring($activateStart, $activateEnd - $activateStart)
if ($activateBody -notmatch 'configuredFeatureActionsForModule\s*\(\s*moduleTitle\s*\)') {
    throw 'The workspace must show configured feature actions when a module is activated.'
}

if ($activateBody -match 'loadConfiguredPluginActionsForModule\s*\(\s*moduleTitle\s*\)') {
    throw 'The workspace top strip must not depend on hidden dll toolbar actions being registered during activation.'
}

Write-Host 'Quality module loads configured dll toolbar icons only for its workspace.'
