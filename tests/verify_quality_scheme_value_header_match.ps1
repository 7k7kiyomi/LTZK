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
$qualityValueHeader = [string]::Concat(
    [char]0x8D28, [char]0x68C0, [char]0x89C4, [char]0x5219, [char]0x6570, [char]0x503C)
$qualityQuantityHeader = [string]::Concat(
    [char]0x8D28, [char]0x68C0, [char]0x89C4, [char]0x5219, [char]0x6570, [char]0x91CF)
foreach ($required in @(
    "const int qualityValueColumn = findHeaderColumn(model, QStringLiteral(`"$qualityValueHeader`"));",
    '&& qualityValueColumn >= 0',
    'const int valueColumn = qualityValueColumn >= 0 ? qualityValueColumn : 4;',
    'const int qualitySchemeColumns[] = { nameColumn, descriptionColumn, valueColumn };',
    'const int qualitySchemeColumnWidths[] = { 250, 250, 250 }'
)) {
    if (-not $normalizeBody.Contains($required)) {
        throw "Quality scheme real value-header handling is missing: $required"
    }
}

if ($normalizeBody.Contains("QStringLiteral(`"$qualityQuantityHeader`")")) {
    throw 'Quality scheme must match the plugin value header, not the old quantity header.'
}

Write-Host 'Quality scheme real value-header matching is verified.'
