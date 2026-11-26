$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'

foreach ($path in @($source, $header)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$sourceText = Get-Content -Raw -Encoding UTF8 $source
$headerText = Get-Content -Raw -Encoding UTF8 $header
foreach ($required in @(
    'syncQualityResultShpLayers',
    'collectQualityResultSearchRoots',
    'isQualityResultShpPath',
    'findChildren<QLineEdit*>',
    'QDirIterator::Subdirectories',
    'new QgsVectorLayer(shpPath',
    'QgsProject::instance()->addMapLayer(layer)',
    'registerQualityResultLayer',
    'ltzk/qualityResultShp',
    'm_qualityResultScanTimer'
)) {
    if (-not $sourceText.Contains($required) -and -not $headerText.Contains($required)) {
        throw "Quality result panel auto-load wiring missing: $required"
    }
}

if ($sourceText -notmatch 'connect\(m_qualityResultScanTimer,\s*&QTimer::timeout') {
    throw 'Quality result result scan timer is not connected.'
}

if ($sourceText -notmatch 'QFileInfo\(sourcePath\)\.canonicalFilePath\(\)') {
    throw 'Quality result shp de-duplication should use canonical file paths.'
}

if ($sourceText -match '<<\s*\(looksLikeError\s*\?\s*QStringLiteral\("错误结果 shp"\)\s*:\s*QStringLiteral\("shp 图层"\)\)') {
    throw 'Quality result panel should list error result shp only, not every generic shp layer.'
}

Write-Host 'Quality result SHP auto-load and selected-directory scanning remain wired without the removed result panel.'


