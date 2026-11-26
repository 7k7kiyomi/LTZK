$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourceFiles = @(
    'LTZK.cpp',
    'LTZK.h',
    'PluginBindingConfig.cpp',
    'PluginBindingConfig.h'
)

foreach ($relativePath in $sourceFiles) {
    $rootPath = Join-Path $root $relativePath
    $packagePath = Join-Path $root (Join-Path 'LTZK_Developer_Package\01_Source' $relativePath)
    if (-not (Test-Path -LiteralPath $packagePath)) {
        throw "Missing developer-package source file: $packagePath"
    }

    $rootText = Get-Content -Raw -Encoding UTF8 $rootPath
    $packageText = Get-Content -Raw -Encoding UTF8 $packagePath
    foreach ($required in @('watchResultOutputs', 'm_resultLayerModules', 'businessModuleMenuTitle')) {
        if ($rootText.Contains($required) -and -not $packageText.Contains($required)) {
            throw "$relativePath is not synchronized into the developer package: $required"
        }
    }
}

$rootConfigPaths = @(
    'config\plugin_bindings.json',
    'dist\LTZK\config\plugin_bindings.json',
    'x64\Debug\config\plugin_bindings.json',
    'x64\Release\config\plugin_bindings.json',
    'x64\Release\verify-build\config\plugin_bindings.json',
    'LTZK_Developer_Package\01_Source\config\plugin_bindings.json',
    'LTZK_Developer_Package\02_Runtime\LTZK\config\plugin_bindings.json'
)
foreach ($relativePath in $rootConfigPaths) {
    $path = Join-Path $root $relativePath
    $json = Get-Content -Raw -Encoding UTF8 $path | ConvertFrom-Json
    foreach ($dll in @('plugin_map_quality_check_tools.dll', 'plugin_map_generalization.dll')) {
        $plugin = @($json.plugins | Where-Object { $_.dll -eq $dll }) | Select-Object -First 1
        if (-not $plugin -or $plugin.watchResultOutputs -ne $true) {
            throw "$relativePath must enable result monitoring for $dll."
        }
    }
}

Write-Host 'Developer-package result panel sources and configs are synchronized.'
