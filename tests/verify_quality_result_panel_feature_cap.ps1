$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$sourceText = Get-Content -LiteralPath $sourcePath -Raw

foreach ($required in @(
    'constexpr int kMaxQualityResultFeatureRowsPerLayer',
    'displayedFeatureRows',
    'displayedFeatureRows >= kMaxQualityResultFeatureRowsPerLayer',
    'QStringLiteral("\u4EC5\u663E\u793A\u524D%1\u6761")'
)) {
    if (-not $sourceText.Contains($required)) {
        throw "Result panel must cap per-layer feature rows to avoid bad_alloc: $required"
    }
}

Write-Host 'Quality result panel feature row cap guard passed.'
