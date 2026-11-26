$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$mainPath = Join-Path $root 'main.cpp'
$mainText = Get-Content -LiteralPath $mainPath -Raw

foreach ($required in @(
    'void configureQgisRuntimeEnvironment(const QString& qgisPrefixPath, const QString& appDir)',
    'setRuntimePath("QGIS_PREFIX_PATH", qgisPrefixPath);',
    'setRuntimePath("PROJ_LIB", projDataPath);',
    'setRuntimePath("PROJ_DATA", projDataPath);',
    'setRuntimePath("GDAL_DATA", gdalDataPath);',
    'setRuntimePath("QT_PLUGIN_PATH", qtPluginPath);',
    'setRuntimePath("QT_QPA_PLATFORM_PLUGIN_PATH", qtPlatformPluginPath);'
)) {
    if (-not $mainText.Contains($required)) {
        throw "main.cpp must configure QGIS runtime environment before initQgis: $required"
    }
}

$configureIndex = $mainText.IndexOf('configureQgisRuntimeEnvironment(qgisPrefixPath, appDir);')
$setPrefixIndex = $mainText.IndexOf('QgsApplication::setPrefixPath(qgisPrefixPath, true);')
$initIndex = $mainText.IndexOf('QgsApplication::initQgis();')
if ($configureIndex -lt 0 -or $setPrefixIndex -lt 0 -or $initIndex -lt 0) {
    throw 'main.cpp is missing QGIS startup sequence calls.'
}
if ($configureIndex -gt $setPrefixIndex -or $configureIndex -gt $initIndex) {
    throw 'QGIS runtime environment must be configured before setPrefixPath/initQgis.'
}

$projDb = 'E:\extern\OSGeo4W_32815\share\proj\proj.db'
if (-not (Test-Path -LiteralPath $projDb)) {
    throw "Expected PROJ database was not found: $projDb"
}

Write-Host 'QGIS startup runtime environment guard passed.'