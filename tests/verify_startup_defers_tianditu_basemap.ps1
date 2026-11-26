$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$sourceText = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8

$ctorStart = $sourceText.IndexOf('LTZK::LTZK(QWidget* parent)')
$dtorStart = $sourceText.IndexOf('LTZK::~LTZK()', $ctorStart)
if ($ctorStart -lt 0 -or $dtorStart -lt 0) {
    throw 'Could not locate LTZK constructor block.'
}

$ctorText = $sourceText.Substring($ctorStart, $dtorStart - $ctorStart)
$deferredCall = 'QTimer::singleShot(0, this, &LTZK::loadLocalGansuShpBasemap);'
if (-not $ctorText.Contains($deferredCall)) {
    throw "Startup basemap load must be deferred until after LTZK construction: $deferredCall"
}

$directCallIndex = $ctorText.IndexOf('loadLocalGansuShpBasemap();')
$deferredCallIndex = $ctorText.IndexOf($deferredCall)
if ($directCallIndex -ge 0 -and $directCallIndex -ne $deferredCallIndex + 'QTimer::singleShot(0, this, &LTZK::'.Length) {
    throw 'LTZK constructor must not synchronously load local Gansu SHP basemap during widget construction.'
}

Write-Host 'Startup queues local Gansu SHP basemap preload without a result panel refresh.'

