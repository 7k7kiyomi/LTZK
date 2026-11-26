$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$mainPath = Join-Path $root 'main.cpp'
$mainText = Get-Content -LiteralPath $mainPath -Raw

foreach ($forbidden in @(
    '#include <QEventLoop>',
    'QEventLoop splashDelayLoop;',
    'splashDelayLoop.exec();'
)) {
    if ($mainText.Contains($forbidden)) {
        throw "main.cpp must not run a nested event loop before LTZK is constructed: $forbidden"
    }
}

if ($mainText.Contains('QApplication a(argc, argv);')) {
    throw 'QGIS apps should construct QgsApplication, not plain QApplication.'
}
if (-not $mainText.Contains('QgsApplication a(argc, argv, true);')) {
    throw 'main.cpp must construct QgsApplication a(argc, argv, true).'
}

$initIndex = $mainText.IndexOf('QgsApplication::initQgis();')
$windowIndex = $mainText.IndexOf('LTZK w;')
$execIndex = $mainText.IndexOf('return a.exec();')
if ($initIndex -lt 0 -or $windowIndex -lt 0 -or $execIndex -lt 0) {
    throw 'Could not locate QGIS init, LTZK construction, and application exec calls.'
}
if ($windowIndex -lt $initIndex) {
    throw 'LTZK must be constructed after QGIS initialization.'
}
if ($execIndex -lt $windowIndex) {
    throw 'Application event loop must start after LTZK is constructed.'
}

Write-Host 'QGIS startup avoids pre-window nested event loops.'