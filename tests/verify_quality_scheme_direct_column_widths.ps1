$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing required source file: $sourcePath"
}

$source = [System.IO.File]::ReadAllText($sourcePath, [System.Text.Encoding]::UTF8)
$eventFilterStart = $source.IndexOf('bool LTZK::eventFilter(QObject* watched, QEvent* event)')
$eventFilterEnd = $source.IndexOf('void LTZK::schedulePluginWindowPolish', $eventFilterStart)
$normalizeStart = $source.IndexOf('void LTZK::normalizeQualityWindowTableColumnWidths(QWidget* widget)')
$normalizeEnd = $source.IndexOf('void LTZK::polishPluginGroupBoxTitles', $normalizeStart)
if ($eventFilterStart -lt 0 -or $eventFilterEnd -lt 0 -or $normalizeStart -lt 0 -or $normalizeEnd -lt 0) {
    throw 'Could not locate quality-window event and column-width functions.'
}

$eventFilterBody = $source.Substring($eventFilterStart, $eventFilterEnd - $eventFilterStart)
$normalizeBody = $source.Substring($normalizeStart, $normalizeEnd - $normalizeStart)

foreach ($required in @(
    'QEvent::LayoutRequest',
    'QEvent::ChildAdded',
    'scheduleQualityWindowTableColumnWidthNormalization(window)',
    'const int qualitySchemeColumns[] = { nameColumn, descriptionColumn, valueColumn }',
    'const int qualitySchemeColumnWidths[] = { 250, 250, 250 }',
    'header->setSectionResizeMode(column, QHeaderView::Interactive)',
    'header->resizeSection(column, qualitySchemeColumnWidths[index])'
)) {
    if (-not ($eventFilterBody.Contains($required) -or $normalizeBody.Contains($required))) {
        throw "Direct quality-scheme column-width handling is missing: $required"
    }
}

if ($normalizeBody.Contains('const int qualitySchemeColumnWidths[] = { 80, 160, 250, 250, 250, 170 }')) {
    throw 'Quality-scheme handling should not use the broad six-column width array.'
}

Write-Host 'Direct quality-scheme name, description, and value column widths are verified.'
