$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$pluginSourcePath = Join-Path $root 'map_product_manager_tools\map_product_manager_tools.cpp'
$configPaths = @(
    (Join-Path $root 'config\plugin_bindings.json'),
    (Join-Path $root 'x64\Release\config\plugin_bindings.json')
)

foreach ($path in @($pluginSourcePath) + $configPaths) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing plugin action index input: $path"
    }
}

$pluginSource = Get-Content -Raw -Encoding Default -LiteralPath $pluginSourcePath
$actionMatches = [regex]::Matches(
    $pluginSource,
    '(?m)^\s*(mAction[A-Za-z0-9_]+)\s*=\s*new QAction\('
)
$actionNames = @($actionMatches | ForEach-Object { $_.Groups[1].Value })
if ($actionNames.Count -eq 0) {
    throw 'Could not derive plugin QAction order from map_product_manager_tools.cpp.'
}

$expectedActionByIcon = @{
    'Resources/icon_db_config.svg' = 'mActionDbConfig'
    'Resources/icon_data_import.svg' = 'mActionDataImport'
    'Resources/icon_product_storage.svg' = 'mActionProductStorage'
    'Resources/icon_metadata.svg' = 'mActionMetadataManager'
    'Resources/icon_product_search.svg' = 'mActionProductSearch'
    'Resources/icon_version_diff.svg' = 'mActionVersionDiff'
    'Resources/icon_theme_manager.svg' = 'mActionThemeManager'
    'Resources/icon_map_service.svg' = 'mActionMapService'
}
$expectedIconPaths = @(
    'Resources/icon_db_config.svg',
    'Resources/icon_data_import.svg',
    'Resources/icon_product_storage.svg',
    'Resources/icon_metadata.svg',
    'Resources/icon_product_search.svg',
    'Resources/icon_version_diff.svg',
    'Resources/icon_theme_manager.svg',
    'Resources/icon_map_service.svg'
)

$configHashes = @()
foreach ($configPath in $configPaths) {
    $configHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $configPath).Hash
    $configHashes += $configHash

    try {
        $config = Get-Content -Raw -Encoding UTF8 -LiteralPath $configPath | ConvertFrom-Json -ErrorAction Stop
    }
    catch {
        throw "Plugin binding JSON is invalid at $configPath`: $($_.Exception.Message)"
    }

    $plugin = $config.plugins | Where-Object { $_.id -eq 'map_product_manager_tools' } | Select-Object -First 1
    if (-not $plugin) {
        throw "map_product_manager_tools is missing from $configPath"
    }

    if ($plugin.actionIcons.Count -ne $expectedIconPaths.Count) {
        throw "Expected $($expectedIconPaths.Count) product-manager action icon bindings in $configPath"
    }
    for ($index = 0; $index -lt $expectedIconPaths.Count; $index++) {
        $expectedIcon = $expectedIconPaths[$index]
        $configuredIcon = [string]$plugin.actionIcons[$index].icon
        if ($configuredIcon -ne $expectedIcon) {
            throw "Unexpected action icon order in $configPath at position $index`: expected $expectedIcon, got $configuredIcon"
        }
        $actionName = $expectedActionByIcon[$expectedIcon]
        $expectedIndex = [Array]::IndexOf($actionNames, $actionName)
        $configuredIndex = [int]$plugin.actionIcons[$index].fallbackActionIndex
        if ($expectedIndex -lt 0 -or $configuredIndex -ne $expectedIndex) {
            throw "Action icon index mismatch in $configPath at position $index`: expected $expectedIndex for $actionName, got $configuredIndex"
        }
    }

    foreach ($iconPath in $expectedIconPaths) {
        $binding = @($plugin.bindings | Where-Object { $_.icon -eq $iconPath })
        if ($binding.Count -ne 1) {
            throw "Expected exactly one binding for '$iconPath' in $configPath, got $($binding.Count)"
        }

        $actionName = $expectedActionByIcon[$iconPath]
        $expectedIndex = [Array]::IndexOf($actionNames, $actionName)
        $configuredIndex = [int]$binding[0].fallbackActionIndex
        if ($expectedIndex -lt 0 -or $configuredIndex -ne $expectedIndex) {
            throw "Feature fallback index mismatch in $configPath for '$iconPath': expected $expectedIndex, got $configuredIndex"
        }
    }
}

if ($configHashes[0] -ne $configHashes[1]) {
    throw 'Root and x64 Release plugin binding JSON files must remain identical.'
}

Write-Host 'Map product manager action icon and feature fallback indices match plugin QAction order in both configs.'
