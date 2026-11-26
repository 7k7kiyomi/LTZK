$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$headerPath = Join-Path $root 'LTZK.h'

foreach ($path in @($sourcePath, $headerPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing required source file: $path"
    }
}

$source = [System.IO.File]::ReadAllText($sourcePath, [System.Text.Encoding]::UTF8)
$header = [System.IO.File]::ReadAllText($headerPath, [System.Text.Encoding]::UTF8)

foreach ($required in @(
    'void normalizeQualityWindowTableColumnWidths(QWidget* widget);',
    'void LTZK::normalizeQualityWindowTableColumnWidths(QWidget* widget)',
    'normalizeQualityWindowTableColumnWidths(widget);',
    'const int mapPathColumnWidths[] = { 180, 250, 280 }',
    'const int qualitySchemeColumns[] = { nameColumn, descriptionColumn, valueColumn }',
    'const int qualitySchemeColumnWidths[] = { 250, 250, 250 }',
    'for (int index = 0; index < 3; ++index)',
    'header->resizeSection(column, qualitySchemeColumnWidths[index])',
    'header->setSectionResizeMode(lastColumn, QHeaderView::Stretch)',
    'scheduleQualityWindowTableColumnWidthNormalization(QWidget* widget)'
)) {
    if (-not ($source.Contains($required) -or $header.Contains($required))) {
        throw "Quality window table column-width handling is missing: $required"
    }
}

if ($source -notmatch 'compactQualityWindow\)\s*\{[^}]*normalizeQualityWindowTableColumnWidths\(widget\);') {
    throw 'Quality window column-width normalization must be restricted to compact quality windows.'
}

Write-Host 'Quality window table column-width handling is present and scoped.'
