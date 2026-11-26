$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$qss = Join-Path $root 'Resources\styles\ltzk_sci_fi.qss'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}
if (-not (Test-Path $qss)) {
    throw "Missing required file: $qss"
}

$text = Get-Content -Raw -Encoding Default $source
$qssText = Get-Content -Raw -Encoding UTF8 $qss

foreach ($required in @(
    'sciFiApplicationStyleSheet',
    'qApp->setStyleSheet(platformStyleSheet())',
    'QDialog',
    'QGroupBox',
    'QLineEdit',
    'QPushButton',
    'QTableView',
    'QHeaderView::section',
    'QTableCornerButton::section',
    'QAbstractScrollArea::corner',
    'QTableView QPushButton',
    'QTableView QSpinBox',
    'padding: 4px 8px',
    'padding: 4px 12px',
    'min-height: 24px',
    'QDateEdit::drop-down',
    'QComboBox::drop-down',
    'QProgressBar',
    'QComboBox',
    'QCheckBox',
    'QRadioButton',
    'QScrollBar',
    'QWidget:disabled',
    '#1bc9ff',
    '#ffb238'
)) {
    if (-not ($text.Contains($required) -or $qssText.Contains($required))) {
        throw "Global sci-fi plugin dialog stylesheet missing: $required"
    }
}

if ($text -notmatch 'QDialog\s*,\s*QMessageBox') {
    throw 'Global stylesheet must target plugin dialogs and message boxes together.'
}

if ($text -notmatch 'QPushButton:hover') {
    throw 'Global sci-fi style must include hover feedback for plugin dialog buttons.'
}

if ($text -notmatch 'QLineEdit:focus') {
    throw 'Global sci-fi style must include focused input state.'
}

Write-Host 'Global sci-fi plugin dialog stylesheet verified.'
