$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path $sourcePath)) {
    throw "Missing required file: $sourcePath"
}

$source = Get-Content -Raw -Encoding UTF8 $sourcePath

foreach ($required in @(
    'auto isFirstRowPathSelectorControl = [](const QModelIndex& index) -> bool',
    'const bool normalizePathLineEditFont = isFirstRowPathSelectorControl(index) && isPathLineEdit;',
    'const bool normalizeSeriesRelationControlFont = isSeriesCrossScaleRelationView(view);',
    'normalizeSeriesRelationControlFont ? seriesRelationControlFontFragment : fontStyleSheetFragment(firstRowPathSelectorControlFont(view->font()))',
    'const bool normalizeSelectorButtonFont = isFirstRowPathSelectorControl(index)',
    'normalizeSeriesRelationControlFont',
    '? fontStyleSheetFragment(seriesCrossScaleRelationTableFont(view->font()))',
    ': fontStyleSheetFragment(firstRowPathSelectorControlFont(view->font())),',
    'const QFont buttonMetricFont = normalizeSeriesRelationControlFont',
    'const QFontMetrics buttonFontMetrics(buttonMetricFont);',
    'buttonFontMetrics.horizontalAdvance(button->text())'
)) {
    if (-not $source.Contains($required)) {
        throw "Plugin table embedded control font sync missing: $required"
    }
}

if ($source.Contains('button->fontMetrics().horizontalAdvance(button->text())')) {
    throw 'Embedded table buttons must calculate width from the synchronized view font.'
}

foreach ($forbidden in @(
    'QFont embeddedControlFont = view->font();',
    'lineEdit->setFont(embeddedControlFont);',
    'QFont embeddedButtonFont = view->font();',
    'button->setFont(embeddedButtonFont);'
)) {
    if ($source.Contains($forbidden)) {
        throw "Plugin table embedded control font sync must stay scoped to first-row path/config selector controls: $forbidden"
    }
}

Write-Host 'Plugin table embedded controls scope font sync to first-row path/config selectors.'
