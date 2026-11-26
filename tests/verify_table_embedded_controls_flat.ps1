$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$qss = Join-Path $root 'Resources\styles\ltzk_sci_fi.qss'

if (-not (Test-Path $qss)) {
    throw "Missing required file: $qss"
}

$text = Get-Content -Raw -Encoding UTF8 $qss

foreach ($required in @(
    'QTableView QLineEdit, QTableWidget QLineEdit, QTreeView QLineEdit, QTreeWidget QLineEdit',
    'QTableView QLineEdit:focus, QTableWidget QLineEdit:focus, QTreeView QLineEdit:focus, QTreeWidget QLineEdit:focus',
    'QTableView QPushButton, QTableWidget QPushButton, QTreeView QPushButton, QTreeWidget QPushButton',
    'border: 0',
    'border-left: 1px solid rgba(91, 206, 255, 95)',
    'min-height: 22px',
    'max-height: 28px'
)) {
    if (-not $text.Contains($required)) {
        throw "Table embedded control flat style missing: $required"
    }
}

if ($text -notmatch 'QTableView QPushButton,\s*QTableWidget QPushButton,\s*QTreeView QPushButton,\s*QTreeWidget QPushButton\s*\{[^}]*border:\s*0;[^}]*border-left:\s*1px solid rgba\(91,\s*206,\s*255,\s*95\)') {
    throw 'Table embedded buttons must avoid full frames and only use a left separator.'
}

if ($text -notmatch 'QTableView QLineEdit:focus,\s*QTableWidget QLineEdit:focus,\s*QTreeView QLineEdit:focus,\s*QTreeWidget QLineEdit:focus\s*\{[^}]*border:\s*0;[^}]*border-bottom:\s*1px solid rgba\(91,\s*206,\s*255,\s*110\)') {
    throw 'Table embedded line edits must use only an underline on focus so borders do not cover neighboring text.'
}


if ($text -match 'QTableView QLineEdit[^{}]*\{[^}]*background:\s*transparent') {
    throw 'Table embedded line edits must not use transparent backgrounds because backing map-name text can show through.'
}

if ($text -notmatch 'QTableView QLineEdit,\s*QTableWidget QLineEdit,\s*QTreeView QLineEdit,\s*QTreeWidget QLineEdit\s*\{[^}]*background:\s*rgba\(4,\s*18,\s*34,\s*255\)') {
    throw 'Table embedded line edits must use an opaque background so backing item text cannot show through.'
}

if ($text -notmatch 'QTableView QLineEdit:focus,\s*QTableWidget QLineEdit:focus,\s*QTreeView QLineEdit:focus,\s*QTreeWidget QLineEdit:focus\s*\{[^}]*background:\s*rgba\(8,\s*38,\s*68,\s*255\)') {
    throw 'Table embedded focused line edits must use an opaque background so backing item text cannot show through.'
}

Write-Host 'Table and tree embedded controls use readable in-cell styling.'


