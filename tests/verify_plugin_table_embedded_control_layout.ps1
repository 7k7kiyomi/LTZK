$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'

if (-not (Test-Path $source)) { throw "Missing required file: $source" }
if (-not (Test-Path $header)) { throw "Missing required file: $header" }

$text = Get-Content -Raw -Encoding UTF8 $source
$headerText = Get-Content -Raw -Encoding UTF8 $header

foreach ($required in @(
    '#include <QAbstractButton>',
    '#include <QAbstractItemView>',
    '#include <QRegularExpression>',
    '#include <QTableView>',
    '#include <QTreeView>',
    '#include <QLabel>',
    'void normalizePluginTableEmbeddedControls(QWidget* widget);',
    'void LTZK::normalizePluginTableEmbeddedControls(QWidget* widget)',
    'widget->findChildren<QAbstractItemView*>',
    'auto viewForChild = [](QWidget* child) -> QAbstractItemView*',
    'QAbstractItemView* view = viewForChild(lineEdit)',
    'bool isComboBoxInternalLineEdit(QLineEdit* lineEdit)',
    'qobject_cast<QComboBox*>(parent)',
    'if (isComboBoxInternalLineEdit(lineEdit))',
    'view->indexWidget(index)',
    'QRect cellRect = view->visualRect(index)',
    'void hidePluginCellLabels(QWidget* widget, QAbstractItemView* view, const QModelIndex& index)',
    'const QList<QLabel*> cellLabels = widget->findChildren<QLabel*>();',
    'label->mapTo(view->viewport(), QPoint(0, 0))',
    'label->setVisible(false)',
    'lineEdit->setAttribute(Qt::WA_StyledBackground, true)',
    'lineEdit->setStyleSheet(QStringLiteral("QLineEdit { background: palette(base); }"))',
    'const QRegularExpression drivePathPattern',
    'drivePathMatch.hasMatch()',
    'lineEdit->setText(pathText)',
    'lineEdit->setCursorPosition(0)',
    'const bool normalizePathLineEditFont = isFirstRowPathSelectorControl(index) && isPathLineEdit;',
    'const bool normalizeSeriesRelationControlFont = isSeriesCrossScaleRelationView(view);',
    'normalizeSeriesRelationControlFont ? seriesRelationControlFontFragment : fontStyleSheetFragment(firstRowPathSelectorControlFont(view->font()))',
    'const int labelReserve = index.column() == 0 ? treeIndicatorReserve : 0',
    'const int editWidth = qMax(80, editRight - editLeft + 1)',
    'lineEdit->setGeometry(editRect)',
    'const int editHeight = (normalizeSeriesRelationControlFont || normalizePathLineEditFont)',
    'const QList<QAbstractButton*> buttons = widget->findChildren<QAbstractButton*>();',
    'const bool normalizeSelectorButtonFont = isFirstRowPathSelectorControl(index)',
    'normalizeSeriesRelationControlFont',
    '? fontStyleSheetFragment(seriesCrossScaleRelationTableFont(view->font()))',
    ': fontStyleSheetFragment(firstRowPathSelectorControlFont(view->font())),',
    'const QFont buttonMetricFont = normalizeSeriesRelationControlFont',
    'buttonFontMetrics.horizontalAdvance(button->text())',
    'button->setGeometry(buttonRect)',
    'const int buttonHeight = (normalizeSeriesRelationControlFont || normalizeSelectorButtonFont)'
)) {
    if (-not ($text.Contains($required) -or $headerText.Contains($required))) {
        throw "Plugin tree/table embedded control layout normalization missing: $required"
    }
}

$styleStart = $text.IndexOf('void LTZK::applyPlatformStyleToPluginWindow(QWidget* widget)')
if ($styleStart -lt 0) {
    throw 'Could not locate applyPlatformStyleToPluginWindow.'
}
$styleEnd = $text.IndexOf('void LTZK::normalizePluginTableEmbeddedControls', $styleStart)
if ($styleEnd -lt 0) {
    throw 'Could not locate end of applyPlatformStyleToPluginWindow.'
}
$styleBody = $text.Substring($styleStart, $styleEnd - $styleStart)
foreach ($forbidden in @(
    'QTimer::singleShot(0, widget',
    'QTimer::singleShot(80, widget',
    'QTimer::singleShot(250, widget',
    'normalizePluginTableEmbeddedControls'
)) {
    if ($styleBody.Contains($forbidden)) {
        throw "Plugin table embedded control normalization must not be scheduled for external plugin windows: $forbidden"
    }
}
foreach ($forbidden in @(
    'const int labelReserve = index.column() == 0 ? 96 : 0',
    '? editRight - editWidth + 1',
    'siblingLabelReserve',
    'modelLabelReserve',
    'measuredLabelReserve',
    'originalLabelReserve',
    'view->model()->setData(index, QString(), Qt::DisplayRole)',
    'button->setFont(embeddedButtonFont);',
    'lineEdit->setFont(embeddedControlFont);',
    'qobject_cast<QComboBox*>(parent)->setGeometry(editRect)',
    'combo->lineEdit()->setGeometry(editRect)'
)) {
    if ($text.Contains($forbidden)) {
        throw "Map-name reserve/model mutation logic should be removed: $forbidden"
    }
}

foreach ($required in @(
    '#include <QStyledItemDelegate>',
    'class PluginEmbeddedControlDelegate final : public QStyledItemDelegate',
    'view->indexWidget(index)',
    'option.text.clear()',
    'viewStyle->drawControl(QStyle::CE_ItemViewItem, &option, painter, option.widget)',
    'void ensurePluginEmbeddedControlDelegate(QAbstractItemView* view)',
    'void suppressTextForIndex(const QModelIndex& index)',
    'm_suppressedTextIndexes.contains(QPersistentModelIndex(index))',
    'void suppressPluginEmbeddedControlItemText(QAbstractItemView* view, const QModelIndex& index)',
    'ltzkPluginEmbeddedControlDelegate',
    'ensurePluginEmbeddedControlDelegate(view)',
    'suppressPluginEmbeddedControlItemText(view, index)'
)) {
    if (-not $text.Contains($required)) {
        throw "Plugin embedded-widget cells must suppress backing item text by delegate paint logic: $required"
    }
}

$styleStart = $text.IndexOf('void LTZK::applyPlatformStyleToPluginWindow(QWidget* widget)')
if ($styleStart -lt 0) {
    throw 'Could not locate applyPlatformStyleToPluginWindow.'
}
$styleEnd = $text.IndexOf('void LTZK::normalizePluginTableEmbeddedControls', $styleStart)
if ($styleEnd -lt 0) {
    throw 'Could not locate end of applyPlatformStyleToPluginWindow.'
}
$styleBody = $text.Substring($styleStart, $styleEnd - $styleStart)
foreach ($forbidden in @(
    'QTimer::singleShot(0, widget',
    'QTimer::singleShot(80, widget',
    'QTimer::singleShot(250, widget',
    'normalizePluginTableEmbeddedControls'
)) {
    if ($styleBody.Contains($forbidden)) {
        throw "Plugin table embedded control normalization must not be scheduled for external plugin windows: $forbidden"
    }
}
foreach ($forbidden in @(
    'QStyledItemDelegate::paint(painter, option, index)'
)) {
    if ($text.Contains($forbidden)) {
        throw "Plugin embedded-widget delegate must not call base paint after clearing text because Qt reinitializes the style option: $forbidden"
    }
}

Write-Host 'Plugin tree/table embedded control geometry normalization verified.'






