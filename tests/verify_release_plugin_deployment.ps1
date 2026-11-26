$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'LTZK.vcxproj'
$pluginName = 'plugin_map_quality_check_tools.dll'
$rootPlugin = Join-Path $root "plugins\$pluginName"
$debugPlugin = Join-Path $root "x64\Debug\plugins\$pluginName"
$releasePlugin = Join-Path $root "x64\Release\plugins\$pluginName"

if (-not (Test-Path $project)) {
    throw "Missing required file: $project"
}

$projectText = Get-Content -Raw -Encoding UTF8 $project

foreach ($required in @(
    'CopyLtzKPlugins',
    '$(MSBuildProjectDirectory)\plugins\*.dll',
    '$(OutDir)plugins',
    'CopyLtzKPluginConfig',
    '$(MSBuildProjectDirectory)\config\plugin_bindings.json',
    '$(OutDir)config'
)) {
    if (-not $projectText.Contains($required)) {
        throw "Project does not copy plugin DLLs and config into the active output folder: $required"
    }
}

foreach ($path in @($rootPlugin, $debugPlugin, $releasePlugin)) {
    if (-not (Test-Path $path)) {
        throw "Missing deployed quality plugin: $path"
    }
}

$rootHash = (Get-FileHash $rootPlugin -Algorithm SHA256).Hash
foreach ($path in @($debugPlugin, $releasePlugin)) {
    $hash = (Get-FileHash $path -Algorithm SHA256).Hash
    if ($hash -ne $rootHash) {
        throw "Deployed plugin differs from root plugins copy: $path"
    }
}

foreach ($path in @(
    (Join-Path $root 'config\plugin_bindings.json'),
    (Join-Path $root 'x64\Debug\config\plugin_bindings.json'),
    (Join-Path $root 'x64\Release\config\plugin_bindings.json')
)) {
    if (-not (Test-Path $path)) {
        throw "Missing deployed plugin binding config: $path"
    }
}

Write-Host 'Debug and Release plugin deployment verified.'
