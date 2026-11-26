$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$sourceText = Get-Content -LiteralPath $sourcePath -Raw

$collectStart = $sourceText.IndexOf('QStringList LTZK::collectQualityResultSearchRoots() const')
$collectEnd = $sourceText.IndexOf('int LTZK::syncQualityResultShpLayers()', $collectStart)
if ($collectStart -lt 0 -or $collectEnd -lt 0) {
    throw 'Could not locate collectQualityResultSearchRoots.'
}

$collectBody = $sourceText.Substring($collectStart, $collectEnd - $collectStart)
foreach ($forbidden in @(
    'QgsProject::instance()->mapLayers()',
    'localPathFromLayerSource(',
    'sourceInfo.absolutePath()',
    'absoluteFilePath(QStringLiteral(".."))'
)) {
    if ($collectBody.Contains($forbidden)) {
        throw "Quality result scanning must not use input map layer directories as search roots: $forbidden"
    }
}

if (-not $collectBody.Contains('return m_qualityResultOutputDirs;')) {
    throw 'Quality result scanning should use only recorded user-selected output directories.'
}

Write-Host 'Quality result scan ignores input map layer directories.'
