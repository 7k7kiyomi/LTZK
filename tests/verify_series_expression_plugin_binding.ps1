$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$config = Join-Path $root 'config\plugin_bindings.json'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}
if (-not (Test-Path $config)) {
    throw "Missing required file: $config"
}

$text = Get-Content -Raw -Encoding Default $source
$json = Get-Content -Raw -Encoding UTF8 $config | ConvertFrom-Json

$loadFailedTitle = -join ([char[]](0x63D2, 0x4EF6, 0x52A0, 0x8F7D, 0x5931, 0x8D25))
$unavailableTitle = -join ([char[]](0x63D2, 0x4EF6, 0x4E0D, 0x53EF, 0x7528))

if ($text.Contains('E:/GSZJPlugins/MPQIS/plugins/plugin_map_quality_check_tools.dll')) {
    throw 'The map quality check plugin still uses the developer machine absolute path.'
}

if ($text -match 'const\s+QString\s+kMapQualityCheckToolsPluginPath\s*=\s*QCoreApplication::applicationDirPath') {
    throw 'The plugin path is initialized too early as a global QString.'
}

foreach ($required in @(
    'loadPluginBindings',
    'resolvePluginDllPath',
    'bindPluginFeatureActions',
    'findConfiguredPluginAction',
    'ensureConfiguredMenuPath'
)) {
    if (-not $text.Contains($required)) {
        throw "The map quality check plugin should be bound by config-driven helpers: $required"
    }
}

if ($text -notmatch 'collectPluginExecutableActions\s*\(') {
    throw 'Plugin actions are not collected from the hidden plugin menu/toolbar.'
}

if ($text -notmatch 'm_pluginManager->loadPlugin\s*\(\s*pluginPath') {
    throw 'Configured plugins are not loaded through LTZKPluginManager.'
}

if ($text -notmatch 'connect\s*\(\s*featureAction\s*,\s*&QAction::triggered\s*,\s*pluginAction\s*,\s*&QAction::trigger\s*\)') {
    throw 'Configured business menu actions are not bound to the loaded plugin action.'
}

if (-not $text.Contains("QMessageBox::critical(this, QStringLiteral(`"$loadFailedTitle`"), pluginError);")) {
    throw 'Plugin load failures are not reported when the business menu is clicked.'
}

if (-not $text.Contains("QMessageBox::warning(this, QStringLiteral(`"$unavailableTitle`"),")) {
    throw 'Missing warning for plugins that load but register no executable action.'
}

$plugin = $json.plugins | Where-Object { $_.dll -eq 'plugin_map_quality_check_tools.dll' } | Select-Object -First 1
if (-not $plugin) {
    throw 'plugin_bindings.json does not map plugin_map_quality_check_tools.dll.'
}

$sheetEdgeFeatureName = -join ([char[]](0x56FE, 0x5E45, 0x63A5, 0x8FB9, 0x4E0E, 0x7CFB, 0x5217, 0x5316, 0x8868, 0x8FBE))
$binding = $plugin.bindings | Where-Object { $_.featureName -eq $sheetEdgeFeatureName } | Select-Object -First 1
if (-not $binding) {
    throw 'plugin_bindings.json does not map the sheet-edge and series expression feature.'
}

Write-Host 'Series expression menu is bound through plugin_bindings.json.'
