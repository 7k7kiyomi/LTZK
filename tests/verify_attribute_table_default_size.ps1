$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing required file: $sourcePath"
}

$source = Get-Content -LiteralPath $sourcePath -Raw -Encoding Default
$start = $source.IndexOf('void LTZK::on_actionOpenAttrTable_triggered()')
$end = $source.IndexOf('void LTZK::on_actionOpenFields_triggered()', $start + 1)

if ($start -lt 0 -or $end -lt 0 -or $end -le $start) {
    throw 'Cannot find on_actionOpenAttrTable_triggered method.'
}

$body = $source.Substring($start, $end - $start)

if ($body -notmatch 'availableGeometry') {
    throw 'Attribute table default size must use the current screen available geometry.'
}

if ($body -notmatch 'setMinimumSize\s*\(\s*attributeTableMinimumSize\s*\)') {
    throw 'Attribute table must set a usable minimum window size.'
}

if ($body -notmatch 'resize\s*\(\s*attributeTableDefaultSize\s*\)') {
    throw 'Attribute table must apply a larger default window size before showing.'
}

if ($body -match 'showMaximized\s*\(\s*\)') {
    throw 'Attribute table should not be forced to maximized mode on every open.'
}

Write-Host 'Attribute table default and minimum window sizes are configured.'
