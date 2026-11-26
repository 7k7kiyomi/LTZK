$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing required source file: $sourcePath"
}

$source = [System.IO.File]::ReadAllText($sourcePath, [System.Text.Encoding]::UTF8)
$normalizeStart = $source.IndexOf('void LTZK::normalizeQualityWindowTableColumnWidths(QWidget* widget)')
$normalizeEnd = $source.IndexOf('void LTZK::polishPluginGroupBoxTitles', $normalizeStart)
$applyStart = $source.IndexOf('void LTZK::applyPlatformStyleToPluginWindow(QWidget* widget)')
$applyEnd = $source.IndexOf('void LTZK::normalizeQualityWindowTableColumnWidths', $applyStart)
if ($normalizeStart -lt 0 -or $normalizeEnd -lt 0 -or $applyStart -lt 0 -or $applyEnd -lt 0) {
    throw 'Could not locate quality-window column-width functions.'
}

$normalizeBody = $source.Substring($normalizeStart, $normalizeEnd - $normalizeStart)
$applyBody = $source.Substring($applyStart, $applyEnd - $applyStart)
$eventFilterStart = $source.IndexOf('bool LTZK::eventFilter(QObject* watched, QEvent* event)')
$eventFilterEnd = $source.IndexOf('void LTZK::schedulePluginWindowPolish', $eventFilterStart)
if ($eventFilterStart -lt 0 -or $eventFilterEnd -lt 0) {
    throw 'Could not locate the plugin-window event filter.'
}
$eventFilterBody = $source.Substring($eventFilterStart, $eventFilterEnd - $eventFilterStart)

foreach ($required in @(
    'const bool isQualitySchemeWindowTable = windowTitle.contains(QStringLiteral(',
    'model->columnCount() >= 6',
    'Qt::ScrollBarAsNeeded',
    'QAbstractItemModel::modelReset',
    'QAbstractItemModel::columnsInserted',
    'const int qualitySchemeColumns[] = { nameColumn, descriptionColumn, valueColumn }',
    'const int qualitySchemeColumnWidths[] = { 250, 250, 250 }',
    'header->resizeSection(column, qualitySchemeColumnWidths[index])'
)) {
    if (-not $normalizeBody.Contains($required)) {
        throw "Quality scheme header lifecycle handling is missing: $required"
    }
}

foreach ($required in @(
    'QEvent::LayoutRequest',
    'QEvent::ChildAdded',
    'scheduleQualityWindowTableColumnWidthNormalization(window)'
)) {
    if (-not $eventFilterBody.Contains($required)) {
        throw "Quality scheme table lifecycle reapplication is missing: $required"
    }
}

if ($normalizeBody -match 'isQualitySchemeWindowTable[\s\S]{0,350}view->isVisible\(\)') {
    throw 'Quality scheme table fallback must not require the table to be visible during the first pass.'
}

foreach ($required in @(
    'normalizeQualityWindowTableColumnWidths(widget);',
    'const int retryDelays[] = { 0, 120, 300, 600 }',
    'QTimer::singleShot(delay, this'
)) {
    if (-not $applyBody.Contains($required)) {
        throw "Quality scheme delayed normalization is missing: $required"
    }
}

Write-Host 'Quality scheme table header-width lifecycle handling is present.'
