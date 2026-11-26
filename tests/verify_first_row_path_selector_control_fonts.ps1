$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding Default $source

foreach ($required in @(
    'isFirstRowPathSelectorControl',
    'index.row() == 0',
    'isPathLineEdit',
    'button->text().contains(QStringLiteral("配置")',
    'button->text().contains(QStringLiteral("选择")',
    'firstRowPathSelectorControlFont(view->font())',
    'embeddedLargeControlMetricsFragment'
)) {
    if (-not $text.Contains($required)) {
        throw "First-row path selector font normalization missing: $required"
    }
}

if ($text.Contains('button->setFont(embeddedButtonFont);')) {
    throw 'Do not set every embedded table button font; only first-row path/config selector controls should be enlarged.'
}

if ($text -match 'QTableView QLineEdit[^{}]*\{[^}]*min-height:\s*24px') {
    throw 'Global table embedded QLineEdit height must not be enlarged for this narrow first-row fix.'
}

if ($text -match 'QTableView QPushButton[^{}]*\{[^}]*max-height:\s*32px') {
    throw 'Global table embedded button height must not be enlarged for this narrow first-row fix.'
}
Write-Host 'First-row path selector controls use scoped font normalization.'


