$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$headerPath = Join-Path $root 'LTZK.h'
$sourceText = Get-Content -LiteralPath $sourcePath -Raw
$headerText = Get-Content -LiteralPath $headerPath -Raw

$bindStart = $sourceText.IndexOf('void LTZK::bindPluginFeatureActions')
$bindEnd = $sourceText.IndexOf('void LTZK::addProviderMapLayer', $bindStart)
if ($bindStart -lt 0 -or $bindEnd -lt 0) {
    throw 'Could not locate bindPluginFeatureActions body.'
}
$bindBody = $sourceText.Substring($bindStart, $bindEnd - $bindStart)
if ($bindBody.Contains('m_pluginManager->loadPlugin(')) {
    throw 'Plugins must not be loaded while binding startup menus; load them only when a feature action is triggered.'
}

foreach ($required in @(
    'void executePluginFeatureAction(const PluginDefinition& plugin, const FeatureBinding& binding);',
    'void LTZK::executePluginFeatureAction(const PluginDefinition& plugin, const FeatureBinding& binding)',
    'm_pluginManager->loadPlugin(pluginPath, &pluginError)',
    'pluginAction->trigger();'
)) {
    if (-not ($headerText.Contains($required) -or $sourceText.Contains($required))) {
        throw "Lazy plugin execution is missing: $required"
    }
}

Write-Host 'Plugins are lazy-loaded when feature actions run.'