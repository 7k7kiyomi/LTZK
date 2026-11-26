$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$mainPath = Join-Path $root 'main.cpp'
$mainText = Get-Content -LiteralPath $mainPath -Raw

$appCtorIndex = $mainText.IndexOf('QgsApplication a(argc, argv, true);')
$configureIndex = $mainText.IndexOf('configureQgisRuntimeEnvironment(qgisPrefixPath, appDir);')

if ($appCtorIndex -lt 0) {
    throw 'main.cpp must create the QgsApplication instance explicitly.'
}
if ($configureIndex -lt 0) {
    throw 'main.cpp must configure the QGIS runtime environment.'
}
if ($configureIndex -gt $appCtorIndex) {
    throw 'PROJ/GDAL/QGIS paths must be configured before QgsApplication is constructed.'
}

foreach ($required in @(
    'QString appDirFromArgv0',
    'const QString appDir = appDirFromArgv0(argc, argv);',
    'findQgisPrefixPath(appDir)',
    'configureQgisRuntimeEnvironment(qgisPrefixPath, appDir)'
)) {
    if (-not $mainText.Contains($required)) {
        throw "Startup must resolve runtime paths without depending on an already-created QCoreApplication: $required"
    }
}

Write-Host 'QGIS runtime is configured before QgsApplication construction.'
