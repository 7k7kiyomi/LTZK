$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path $sourcePath)) { throw "Missing required file: $sourcePath" }

$source = Get-Content -Raw -Encoding UTF8 $sourcePath

$polishStart = $source.IndexOf('void polishDrawingLayoutTreeEmbeddedCombos(')
if ($polishStart -lt 0) { throw 'Could not locate drawing-layout combo polish helper.' }

$helperBody = $source.Substring(0, $polishStart)
$polishEnd = $source.IndexOf('void suppressPluginTableEmbeddedWidgetTexts(', $polishStart)
if ($polishEnd -lt 0) { throw 'Could not locate end of drawing-layout combo polish helper.' }
$polishBody = $source.Substring($polishStart, $polishEnd - $polishStart)

foreach ($required in @(
    'void ensureDrawingLayoutTreeColumnWidths(QWidget* widget)',
    'tree->objectName() != QStringLiteral("treeWidget_csv")',
    'tree->columnCount() != 6',
    'header->setStretchLastSection(false)',
    'header->setSectionResizeMode(column, QHeaderView::Interactive)',
    'tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded)',
    'const int minimumColumnWidths[] = { 105, 150, 180, 245, 120, 160 }',
    'qMax(tree->columnWidth(column), minimumColumnWidths[column])',
    'ensureDrawingLayoutTreeColumnWidths(widget)'
)) {
    if (-not ($helperBody.Contains($required) -or $polishBody.Contains($required))) {
        throw "Drawing-layout table column-width behavior missing: $required"
    }
}

Write-Host 'Drawing-layout table column widths and horizontal scrolling verified.'
