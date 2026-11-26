$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path $sourcePath)) { throw "Missing required file: $sourcePath" }

$source = Get-Content -Raw -Encoding UTF8 $sourcePath
$start = $source.IndexOf('void polishDrawingLayoutTreeEmbeddedCombos(')
$end = $source.IndexOf('void suppressPluginTableEmbeddedWidgetTexts(', $start)
if ($start -lt 0 -or $end -lt 0) { throw 'Could not locate drawing-layout combo polish helper.' }

$body = $source.Substring($start, $end - $start)

if ($body -notmatch 'const QString preservedComboDisplayText\s*=\s*combo->currentText\(\)') {
    throw 'Combo polish must capture the existing currentText before reconfiguring the combo.'
}

if ($body -notmatch 'combo->lineEdit\(\)->setText\(preservedComboDisplayText\)') {
    throw 'Combo polish must restore the captured text to the internal line edit.'
}

$captureIndex = $body.IndexOf('preservedComboDisplayText')
$editableIndex = $body.IndexOf('combo->setEditable(true)')
$restoreIndex = $body.IndexOf('combo->lineEdit()->setText(preservedComboDisplayText)')
$paletteIndex = $body.IndexOf('combo->lineEdit()->setPalette(lineEditPalette)')
if ($captureIndex -lt 0 -or $editableIndex -lt 0 -or $restoreIndex -lt 0 -or $paletteIndex -lt 0) {
    throw 'Could not verify combo display-text ordering.'
}
if ($captureIndex -gt $editableIndex) {
    throw 'Combo display text must be captured before setEditable(true).'
}
if ($restoreIndex -lt $paletteIndex) {
    throw 'Combo display text must be restored after the internal line edit styling.'
}

Write-Host 'Drawing-layout combo display-text preservation verified.'
