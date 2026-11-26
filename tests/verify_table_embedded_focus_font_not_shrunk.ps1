$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$themeFiles = @(
    'Resources\styles\ltzk_sci_fi.qss',
    'Resources\styles\ltzk_black.qss',
    'Resources\styles\ltzk_white.qss'
)

foreach ($relativePath in $themeFiles) {
    $path = Join-Path $root $relativePath
    if (-not (Test-Path $path)) { throw "Missing required file: $path" }
    $text = Get-Content -Raw -Encoding UTF8 $path

    $focusRules = [regex]::Matches($text, 'QTableView QLineEdit:focus[^{}]*\{[^}]*\}')
    if ($focusRules.Count -lt 1) {
        throw "Missing table embedded QLineEdit focus style in ${relativePath}."
    }
    foreach ($rule in $focusRules) {
        if ($rule.Value -match 'font-size:\s*14px') {
            throw "Focused table embedded QLineEdit must not shrink to 14px in ${relativePath}."
        }
    }
}

$sourcePath = Join-Path $root 'LTZK.cpp'
$source = Get-Content -Raw -Encoding UTF8 $sourcePath
$seriesPolishStart = $source.IndexOf('void LTZK::polishSeriesContentTableWindow(QWidget* widget)')
$seriesPolishEnd = $source.IndexOf('void LTZK::applyPlatformStyleToTopLevelWidgets', $seriesPolishStart)
if ($seriesPolishStart -lt 0 -or $seriesPolishEnd -lt 0) { throw 'Could not locate series table polish body.' }
$seriesPolishBody = $source.Substring($seriesPolishStart, $seriesPolishEnd - $seriesPolishStart)
foreach ($forbidden in @('font-size: 14px;', 'font-size: 14px')) {
    if ($seriesPolishBody.Contains($forbidden)) {
        throw "Series table polish must not override readableTableFont() with a smaller hard-coded font: $forbidden"
    }
}

Write-Host 'Focused table embedded controls keep their configured table font size.'