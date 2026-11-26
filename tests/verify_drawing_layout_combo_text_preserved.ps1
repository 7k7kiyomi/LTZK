$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $sourcePath)) {
    throw "Missing required file: $sourcePath"
}

$source = Get-Content -Raw -Encoding UTF8 $sourcePath
$start = $source.IndexOf('auto polishCombo =')
$end = $source.IndexOf('const QList<QComboBox*> combos =', $start)
if ($start -lt 0 -or $end -lt 0) {
    throw 'Could not locate the Drawing_Layout combo polishing body.'
}

$body = $source.Substring($start, $end - $start)
$capture = 'const QString preservedComboDisplayText = combo->currentText();'
$editable = 'combo->setEditable(true);'
$restore = 'combo->lineEdit()->setText(preservedComboDisplayText);'

if ($body.IndexOf($capture) -lt 0) {
    throw "Missing current combo display-text capture: $capture"
}
if ($body.IndexOf($restore) -lt 0) {
    throw "Missing internal line-edit text restoration: $restore"
}
if ($body.IndexOf($capture) -gt $body.IndexOf($editable)) {
    throw 'The combo display text must be captured before setEditable(true).'
}
if ($body.IndexOf($restore) -lt $body.IndexOf($editable)) {
    throw 'The combo display text must be restored after setEditable(true).'
}

Write-Host 'Drawing_Layout combo display-text preservation verified.'
