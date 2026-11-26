$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path -LiteralPath $sourcePath)) { throw "Missing required file: $sourcePath" }

$source = Get-Content -Raw -Encoding UTF8 $sourcePath
$polishStart = $source.IndexOf('void LTZK::polishSeriesContentTableWindow(QWidget* widget)')
$polishEnd = $source.IndexOf('void LTZK::applyPlatformStyleToTopLevelWidgets()', $polishStart)
if ($polishStart -lt 0 -or $polishEnd -lt 0) {
    throw 'Could not locate series content table polishing implementation.'
}
$polishBody = $source.Substring($polishStart, $polishEnd - $polishStart)

$helperStart = $polishBody.IndexOf('auto polishSeriesItemWidgetPaintState')
$helperEnd = $polishBody.IndexOf('std::function<void(QTreeWidget*, QTreeWidgetItem*)>', $helperStart)
if ($helperStart -lt 0 -or $helperEnd -lt 0) {
    throw 'Could not isolate the series item-widget polishing helper.'
}
$helperBody = $polishBody.Substring($helperStart, $helperEnd - $helperStart)

foreach ($required in @(
    'if (QLayout* itemWidgetLayout = itemWidget->layout())',
    'itemWidgetLayout->setContentsMargins(0, 0, 0, 0);',
    'itemWidgetLayout->setSpacing(0);'
)) {
    if (-not $helperBody.Contains($required)) {
        throw "Series table compound cell layouts must remove the line-edit/button gap: $required"
    }
}

Write-Host 'Series table compound cell layouts remove the line-edit/button gap.'
