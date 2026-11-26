$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$layoutTest = Join-Path $root 'tests\verify_plugin_table_embedded_control_layout.ps1'

if (-not (Test-Path $source)) { throw "Missing required file: $source" }
if (-not (Test-Path $layoutTest)) { throw "Missing required file: $layoutTest" }

$text = Get-Content -Raw -Encoding UTF8 $source

foreach ($required in @(
    'QComboBox { color: %4;',
    'palette.setColor(QPalette::ButtonText, comboTextPaletteColor);',
    'palette.setColor(QPalette::WindowText, comboTextPaletteColor);',
    'palette.setColor(QPalette::Text, comboTextPaletteColor);',
    'lineEditPalette.setColor(QPalette::Text, comboTextPaletteColor);',
    'lineEditPalette.setColor(QPalette::WindowText, comboTextPaletteColor);',
    'for (QWidget* parent = lineEdit; parent; parent = parent->parentWidget())',
    'qobject_cast<QComboBox*>(parent)'
)) {
    if (-not $text.Contains($required)) {
        throw "Drawing layout combo native text visibility missing: $required"
    }
}

foreach ($forbidden in @(
    'class DrawingLayoutComboLineEditOnlyStyle final : public QProxyStyle',
    'labelOption.currentText.clear();',
    'ltzkDrawingLayoutComboLineEditOnlyText',
    'ltzkDrawingLayoutComboLineEditOnlyStyleInstalled',
    'QComboBox { color: transparent;',
    'palette.setColor(QPalette::WindowText, QColor(255, 255, 255, 0));',
    'palette.setColor(QPalette::Text, QColor(255, 255, 255, 0));',
    'qobject_cast<QComboBox*>(lineEdit->parentWidget())->setGeometry(editRect)',
    'combo->lineEdit()->setGeometry(editRect)'
)) {
    if ($text.Contains($forbidden)) {
        throw "QComboBox current text must not be hidden or repositioned: $forbidden"
    }
}

Write-Host 'Drawing layout combo native text visibility verified.'