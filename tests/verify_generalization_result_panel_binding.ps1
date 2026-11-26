$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -Raw -Encoding Default (Join-Path $root 'LTZK.cpp')
$header = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'PluginBindingConfig.h')
$parser = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'PluginBindingConfig.cpp')

if (-not $header.Contains('watchResultOutputs')) {
    throw 'PluginDefinition must expose watchResultOutputs.'
}
if (-not $parser.Contains('watchResultOutputs')) {
    throw 'PluginBindingConfig parser must read watchResultOutputs.'
}

$generalizationTitle = -join ([char[]](0x5730, 0x56FE, 0x667A, 0x80FD, 0x7EFC, 0x5408))
foreach ($required in @(
    'const bool isResultModule =',
    $generalizationTitle,
    'plugin.watchResultOutputs',
    'm_resultLayerModules',
    'm_currentBusinessModuleTitle'
)) {
    if (-not $source.Contains($required)) {
        throw "Generalization result panel wiring is missing: $required"
    }
}

$configPaths = @(
    'config\plugin_bindings.json',
    'dist\LTZK\config\plugin_bindings.json',
    'x64\Debug\config\plugin_bindings.json',
    'x64\Release\config\plugin_bindings.json',
    'x64\Release\verify-build\config\plugin_bindings.json',
    'LTZK_Developer_Package\01_Source\config\plugin_bindings.json',
    'LTZK_Developer_Package\02_Runtime\LTZK\config\plugin_bindings.json'
)
$runtimeConfigPath = 'E:\LTZK\config\plugin_bindings.json'
if (Test-Path $runtimeConfigPath) {
    $configPaths += $runtimeConfigPath
}
foreach ($relativePath in $configPaths) {
    $path = if ([IO.Path]::IsPathRooted($relativePath)) {
        $relativePath
    }
    else {
        Join-Path $root $relativePath
    }
    if (-not (Test-Path $path)) {
        throw "Missing deployed plugin config: $path"
    }
    $json = Get-Content -Raw -Encoding UTF8 $path | ConvertFrom-Json
    foreach ($dll in @('plugin_map_quality_check_tools.dll', 'plugin_map_generalization.dll')) {
        $plugin = @($json.plugins | Where-Object { $_.dll -eq $dll }) | Select-Object -First 1
        if (-not $plugin -or $plugin.watchResultOutputs -ne $true) {
            throw "$relativePath must enable watchResultOutputs for $dll."
        }
    }
}

Write-Host 'Generalization result panel binding verified.'
