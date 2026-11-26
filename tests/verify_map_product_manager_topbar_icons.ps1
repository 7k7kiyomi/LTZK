$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$configPath = Join-Path $root 'config\plugin_bindings.json'
$qrcPath = Join-Path $root 'LTZK.qrc'

foreach ($path in @($configPath, $qrcPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing required file: $path"
    }
}

$config = Get-Content -Raw -Encoding UTF8 $configPath | ConvertFrom-Json
$plugin = $config.plugins | Where-Object { $_.id -eq 'map_product_manager_tools' } | Select-Object -First 1
if (-not $plugin) {
    throw 'plugin_bindings.json must define map_product_manager_tools.'
}

$qrcText = Get-Content -Raw -Encoding UTF8 $qrcPath
if (-not $qrcText.Contains('<qresource prefix="/LTZK">')) {
    throw 'Main application resources must keep the /LTZK prefix used by configuredIcon().'
}

$iconPaths = @($plugin.actionIcons | ForEach-Object { $_.icon })
$iconPaths += @($plugin.bindings | ForEach-Object { $_.icon })
$iconPaths = $iconPaths |
    Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and $_.StartsWith('Resources/') } |
    Sort-Object -Unique

if ($iconPaths.Count -eq 0) {
    throw 'map_product_manager_tools must configure at least one main-resource icon.'
}

foreach ($iconPath in $iconPaths) {
    $sourcePath = Join-Path $root ($iconPath.Replace('/', '\'))
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        throw "Configured map-product icon file is missing: $iconPath"
    }
    if (-not $qrcText.Contains("<file>$iconPath</file>")) {
        throw "Configured map-product icon is not embedded in LTZK.qrc: $iconPath"
    }
}

Write-Host 'Map product manager top-bar icons are embedded in the main application resource bundle.'
