$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$headerPath = Join-Path $root 'LTZK.h'
$source = Get-Content -LiteralPath $sourcePath -Raw
$header = Get-Content -LiteralPath $headerPath -Raw

foreach ($required in @(
    'm_qualityResultOutputDirs',
    'refreshQualityResultOutputDirectories',
    'QFileInfo(text).isDir()',
    'const QString controlName = lineEdit->objectName().toLower()',
    'controlName.contains(QStringLiteral("output"))',
    'return m_qualityResultOutputDirs;'
)) {
    if (-not ($source.Contains($required) -or $header.Contains($required))) {
        throw "Selected quality output directory handling is missing: $required"
    }
}

$collectStart = $source.IndexOf('QStringList LTZK::collectQualityResultSearchRoots() const')
$collectEnd = $source.IndexOf('int LTZK::syncQualityResultShpLayers()', $collectStart)
if ($collectStart -lt 0 -or $collectEnd -lt 0) {
    throw 'Could not locate result search-root collection.'
}
$collectBody = $source.Substring($collectStart, $collectEnd - $collectStart)

foreach ($forbidden in @(
    'directoryNameLooksLikeResultRoot',
    'QgsProject::instance()->mapLayers()',
    'localPathFromLayerSource(',
    'sourceInfo.absolutePath()',
    'absoluteFilePath(QStringLiteral(".."))'
)) {
    if ($collectBody.Contains($forbidden)) {
        throw "Result scanning must not infer roots from fixed names or input layers: $forbidden"
    }
}

$qualityPathStart = $source.IndexOf('bool isQualityResultShpPath(const QString& shpPath)')
$qualityPathEnd = $source.IndexOf('constexpr int kMaxQualityResultFeatureRowsPerLayer', $qualityPathStart)
if ($qualityPathStart -lt 0 -or $qualityPathEnd -lt 0) {
    throw 'Could not locate result SHP path validation.'
}
$qualityPathBody = $source.Substring($qualityPathStart, $qualityPathEnd - $qualityPathStart)
foreach ($forbidden in @(
    'contains(QStringLiteral("error"))',
    'contains(QStringLiteral("err"))',
    'contains(QStringLiteral("\u9519\u8BEF"))',
    'contains(QStringLiteral("\u5F02\u5E38"))',
    'contains(QStringLiteral("\u8D28\u91CF\u7ED3\u679C"))',
    'contains(QStringLiteral("\u8D28\u68C0\u7ED3\u679C"))'
)) {
    if ($qualityPathBody.Contains($forbidden)) {
        throw "Result SHP validation must not depend on filename/path keywords: $forbidden"
    }
}

if ($source -notmatch 'refreshQualityResultOutputDirectories\(\);\s*\r?\n\s*const int loadedCount = syncQualityResultShpLayers') {
    throw 'The scan timer must refresh the user-selected output directories before scanning.'
}

Write-Host 'Quality result scan uses selected output directories without fixed-name heuristics.'
