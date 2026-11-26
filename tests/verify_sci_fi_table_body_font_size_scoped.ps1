$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$qssPath = Join-Path $root 'Resources\styles\ltzk_sci_fi.qss'
$sourcePath = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $qssPath)) { throw "Missing required file: $qssPath" }
if (-not (Test-Path $sourcePath)) { throw "Missing required file: $sourcePath" }

$qss = Get-Content -Raw -Encoding UTF8 $qssPath
$source = Get-Content -Raw -Encoding UTF8 $sourcePath

$wrongGlobalRule = 'QTableView, QTableWidget, QTableView::item, QTableWidget::item, QTableView QLineEdit, QTableWidget QLineEdit, QTableView QLineEdit:focus, QTableWidget QLineEdit:focus, QTableView QPushButton, QTableWidget QPushButton, QTableView QToolButton, QTableWidget QToolButton { font-size: 16px; }'
if ($qss.Contains($wrongGlobalRule)) {
    throw 'Do not use a global sci-fi QTableView/QTableWidget QSS rule for this plugin table font fix.'
}

foreach ($required in @(
    'constexpr qreal kSeriesCrossScaleRelationTablePointSize = 16.0;',
    'auto isSeriesCrossScaleRelationView = [](QAbstractItemView* view) -> bool',
    'text.contains(QStringLiteral("跨比例尺"))',
    'text.contains(QStringLiteral("已完成"))',
    'applySeriesCrossScaleRelationTableFont(view)',
    'ensureSeriesCrossScaleRelationTableBodyRowHeights(view, tableFont)',
    'QFontMetrics tableFontMetrics(tableFont)',
    'tableView->setRowHeight(row, qMax(tableView->rowHeight(row), targetRowHeight));',
    'model->setData(index, tableFont, Qt::FontRole)',
    'const bool normalizeSeriesRelationControlFont = isSeriesCrossScaleRelationView(view);',
    'fontStyleSheetFragment(seriesCrossScaleRelationTableFont(view->font()))',
    'const QList<QLabel*> labels = widget->findChildren<QLabel*>();'
)) {
    if (-not $source.Contains($required)) {
        throw "Series cross-scale relation table font override missing: $required"
    }
}

foreach ($forbidden in @(
    'enlargedPluginTableBodyFont',
    'verticalHeader->setDefaultSectionSize'
)) {
    if ($source.Contains($forbidden)) {
        throw "Do not change global table layout/font behavior for this narrow font fix: $forbidden"
    }
}

Write-Host 'Series cross-scale relation table font override is scoped to the target plugin table.'
