$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$themeFiles = @(
    'Resources\styles\ltzk_sci_fi.qss',
    'Resources\styles\ltzk_black.qss',
    'Resources\styles\ltzk_white.qss'
)

foreach ($relativePath in $themeFiles) {
    $path = Join-Path $root $relativePath
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }

    $text = Get-Content -Raw -Encoding UTF8 $path
    foreach ($required in @(
        'QTableView QLineEdit, QTableWidget QLineEdit',
        'QTableView QLineEdit:focus, QTableWidget QLineEdit:focus',
        'QTableView QPushButton, QTableWidget QPushButton',
        'QTableView QPushButton:hover, QTableWidget QPushButton:hover',
        'QTableView QToolButton, QTableWidget QToolButton',
        'QTreeView QLineEdit, QTreeWidget QLineEdit',
        'QTreeView QPushButton, QTreeWidget QPushButton',
        'QTreeView QToolButton, QTreeWidget QToolButton',
        'min-height: 22px',
        'max-height: 28px',
        'min-width: 40px',
        'border: 0'
    )) {
        if (-not $text.Contains($required)) {
            throw "Table embedded control theme style missing from ${relativePath}: $required"
        }
    }

    $lineEditRules = [regex]::Matches($text, 'QTableView QLineEdit[^{}]*\{[^}]*\}|QTableView QLineEdit:focus[^{}]*\{[^}]*\}')
    if ($lineEditRules.Count -lt 2) {
        throw "Expected embedded table/tree line-edit rules in ${relativePath}."
    }
    foreach ($rule in $lineEditRules) {
        if ($rule.Value -match 'background:\s*transparent') {
            throw "Embedded table/tree line edits must use opaque backgrounds so hidden map-name text cannot bleed through: ${relativePath}"
        }
    }
}

Write-Host 'All platform themes keep compact table embedded control styling.'





