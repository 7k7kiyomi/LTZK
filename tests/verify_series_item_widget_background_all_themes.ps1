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

foreach ($required in @(
    'const QColor seriesItemWidgetBackgroundColor = m_currentTheme == PlatformTheme::White',
    'QColor(QStringLiteral("#ffffff"))',
    'QColor(QStringLiteral("#0f0f0f"))',
    'QColor(QStringLiteral("#06111d"))',
    'auto polishSeriesItemWidgetPaintState = [seriesItemWidgetBackgroundColor]',
    'QPalette itemWidgetPalette = itemWidget->palette();',
    'itemWidgetPalette.setColor(QPalette::Window, seriesItemWidgetBackgroundColor);',
    'itemWidget->setPalette(itemWidgetPalette);',
    'itemWidget->setAutoFillBackground(true);'
)) {
    if (-not $polishBody.Contains($required)) {
        throw "Series table item-widget gaps must use the active theme background: $required"
    }
}

Write-Host 'Series table item-widget containers use explicit backgrounds in all themes.'
