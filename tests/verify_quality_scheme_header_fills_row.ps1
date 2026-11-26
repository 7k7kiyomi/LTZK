$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing required source file: $sourcePath"
}

$source = [System.IO.File]::ReadAllText($sourcePath, [System.Text.Encoding]::UTF8)
$normalizeStart = $source.IndexOf('void LTZK::normalizeQualityWindowTableColumnWidths(QWidget* widget)')
$normalizeEnd = $source.IndexOf('void LTZK::polishPluginGroupBoxTitles', $normalizeStart)
if ($normalizeStart -lt 0 -or $normalizeEnd -lt 0) {
    throw 'Could not locate quality-window column-width normalization.'
}

$normalizeBody = $source.Substring($normalizeStart, $normalizeEnd - $normalizeStart)
foreach ($required in @(
    'const int unitColumn = qualityUnitColumn >= 0 ? qualityUnitColumn : 5;',
    'header->setSectionResizeMode(unitColumn, QHeaderView::Stretch);'
)) {
    if (-not $normalizeBody.Contains($required)) {
        throw "Quality scheme header row fill handling is missing: $required"
    }
}

Write-Host 'Quality scheme header fills the remaining first-row width.'
