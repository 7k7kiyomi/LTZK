$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$sourceText = Get-Content -Raw -Encoding UTF8 $source
$headerText = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'LTZK.h')

foreach ($forbidden in @(
    'workspaceBottomResultPanel',
    'qualityResultHoverHint',
    'm_qualityResultPanel',
    'm_qualityResultSummary',
    'm_qualityResultList'
)) {
    if ($sourceText.Contains($forbidden) -or $headerText.Contains($forbidden)) {
        throw "Removed result panel UI still has display wiring: $forbidden"
    }
}

Write-Host 'Quality result panel display and hover tooltip are removed.'
