$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root "LTZK.vcxproj"
$filters = Join-Path $root "LTZK.vcxproj.filters"
$source = Join-Path $root "LTZK.cpp"
$header = Join-Path $root "LTZK.h"
$configHeader = Join-Path $root "PluginBindingConfig.h"

foreach ($path in @($project, $filters, $source, $header, $configHeader)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$projectText = Get-Content $project -Raw -Encoding UTF8
$filterText = Get-Content $filters -Raw -Encoding UTF8
$sourceText = Get-Content $source -Raw -Encoding Default
$headerText = Get-Content $header -Raw -Encoding Default

$requiredProjectItems = @(
    "LTZKPluginManager.cpp",
    "LTZKPluginManager.h",
    "LTZKQgisInterface.cpp",
    "LTZKQgisInterface.h",
    "PluginBindingConfig.cpp",
    "PluginBindingConfig.h"
)

foreach ($item in $requiredProjectItems) {
    if ($projectText -notmatch [regex]::Escape($item)) {
        throw "Project does not include $item"
    }
    if ($filterText -notmatch [regex]::Escape($item)) {
        throw "Filters file does not include $item"
    }
}

foreach ($required in @(
    "on_actionLoadQgisPlugin_triggered",
    "loadPluginBindings",
    "bindPluginFeatureActions",
    "m_pluginManager->loadPlugin"
)) {
    if (-not $sourceText.Contains($required)) {
        throw "LTZK.cpp does not expose plugin host wiring: $required"
    }
}

if ($headerText -notmatch "on_actionLoadQgisPlugin_triggered") {
    throw "LTZK.h does not declare the load plugin slot"
}

if ($headerText -notmatch "bindPluginFeatureActions") {
    throw "LTZK.h does not declare config-driven plugin feature binding"
}

Write-Host "QGIS plugin host wiring verified."
