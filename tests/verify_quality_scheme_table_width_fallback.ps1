$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing required source file: $sourcePath"
}

$source = [System.IO.File]::ReadAllText($sourcePath, [System.Text.Encoding]::UTF8)

foreach ($required in @(
    'const bool isQualitySchemeWindowTable = windowTitle.contains(QStringLiteral(',
    'qobject_cast<QTableView*>(view) != nullptr',
    'model->columnCount() >= 6',
    'const int qualitySchemeColumns[] = { nameColumn, descriptionColumn, valueColumn }',
    'const int qualitySchemeColumnWidths[] = { 250, 250, 250 }',
    'header->resizeSection(column, qualitySchemeColumnWidths[index])'
)) {
    if (-not $source.Contains($required)) {
        throw "Quality scheme table fallback handling is missing: $required"
    }
}

if ($source -match 'isQualitySchemeWindowTable[\s\S]{0,350}view->isVisible\(\)') {
    throw 'Quality scheme table fallback must not require the table to be visible during the first pass.'
}

Write-Host 'Quality scheme table fallback column widths are present.'
