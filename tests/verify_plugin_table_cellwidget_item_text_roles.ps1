$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding UTF8 $source

foreach ($required in @(
    '#include <QTableWidget>',
    '#include <QComboBox>',
    'class PluginEmbeddedControlDelegate final : public QStyledItemDelegate',
    'explicit PluginEmbeddedControlDelegate(QAbstractItemView* view, QAbstractItemDelegate* sourceDelegate)',
    'm_sourceDelegate(sourceDelegate)',
    'QPointer<QAbstractItemDelegate> m_sourceDelegate;',
    'view->indexWidget(index)',
    'bool isDrawingLayoutTreeEmbeddedColumn(QAbstractItemView* view, const QModelIndex& index)',
    'view->objectName() == QStringLiteral("treeWidget_csv")',
    'index.column() >= 1',
    'index.column() <= 3',
    'isDrawingLayoutTreeEmbeddedColumn(m_view, index)',
    'option.text.clear()',
    'm_sourceDelegate->paint(painter, sourceOption, index);',
    'new PluginEmbeddedControlDelegate(view, delegate);',
    'void polishDrawingLayoutTreeEmbeddedCombos(LTZK* owner, QWidget* widget, bool isDarkTheme, bool isBlackTheme, QComboBox* onlyCombo = nullptr)',
    'findChildren<QAbstractItemView*>()',
    'const QList<QComboBox*> combos = onlyCombo ? (QList<QComboBox*>() << onlyCombo) : widget->findChildren<QComboBox*>();',
    'combo->lineEdit()->setTextMargins(0, 0, 24, 0);',
    'ensurePluginEmbeddedControlDelegate(view);',
    'suppressPluginTableEmbeddedWidgetTexts(view);',
    'void suppressDrawingLayoutTreeEmbeddedItemText(QTreeWidget* tree)',
    'const QBrush transparentTextBrush(QColor(255, 255, 255, 0));',
    'item->setForeground(column, transparentTextBrush);',
    'tree->itemWidget(item, column)',
    'suppressDrawingLayoutTreeEmbeddedItemText(tree);',
    'combo->setAttribute(Qt::WA_StyledBackground, true);',
    'combo->setAutoFillBackground(true);',
    'combo->lineEdit()->setAttribute(Qt::WA_StyledBackground, true);',
    'combo->lineEdit()->setAutoFillBackground(true);',
    'combo->setEditable(true);',
    'combo->lineEdit()->setReadOnly(true);',
    'combo->lineEdit()->setFrame(false);',
    'combo->setFont(font);'
    'combo->lineEdit()->setFont(font);',
    'combo->lineEdit()->setTextMargins(0, 0, 24, 0);',
    'combo->lineEdit()->setStyleSheet(comboLineEditStyle);',
    'QPalette lineEditPalette = combo->lineEdit()->palette();',
    'lineEditPalette.setColor(QPalette::Text, comboTextPaletteColor);',
    'lineEditPalette.setColor(QPalette::WindowText, comboTextPaletteColor);',
    'combo->lineEdit()->setPalette(lineEditPalette);',
    '#include <QPalette>',
    'combo->view()->viewport()->setAttribute(Qt::WA_StyledBackground, true);',
    'combo->view()->viewport()->setAutoFillBackground(true);'
    'polishDrawingLayoutTreeEmbeddedCombos(this, widget, m_currentTheme != PlatformTheme::White, m_currentTheme == PlatformTheme::Black);'
)) {
    if (-not $text.Contains($required)) {
        throw "Missing QTreeWidget/QComboBox embedded-control text suppression behavior: $required"
    }
}

foreach ($forbidden in @(
    'item->setData(Qt::UserRole',
    'tree->topLevelItem(topLevelIndex)->setText(',
    'cellWidget->findChild<QLabel*>(QStringLiteral("pathLabel"))',
    'cellWidget->findChild<QPushButton*>(QStringLiteral("loadButton"))',
    'if (delegate && QString::fromLatin1(delegate->metaObject()->className()) != QStringLiteral("QStyledItemDelegate"))',
    'model->setData(index, QString(), Qt::DisplayRole);',
    'model->setData(index, QString(), Qt::EditRole);',
    'qobject_cast<QComboBox*>(lineEdit->parentWidget())->setGeometry(editRect)'
)) {
    if ($text.Contains($forbidden)) {
        throw "Embedded-control cleanup must not mutate saved data, depend on path widgets, or skip custom delegates: $forbidden"
    }
}


$helperStart = $text.IndexOf('void suppressDrawingLayoutTreeEmbeddedItemText(QTreeWidget* tree)')
$helperEnd = $text.IndexOf('void polishDrawingLayoutTreeEmbeddedCombos(LTZK* owner, QWidget* widget, bool isDarkTheme, bool isBlackTheme, QComboBox* onlyCombo = nullptr)', $helperStart)
if ($helperStart -lt 0 -or $helperEnd -lt 0) {
    throw 'Could not locate suppressDrawingLayoutTreeEmbeddedItemText helper body.'
}
$helperBody = $text.Substring($helperStart, $helperEnd - $helperStart)
foreach ($forbidden in @(
    'item->setText(column, QString())',
    'item->setData(column, Qt::DisplayRole, QString())',
    'item->setData(column, Qt::EditRole, QString())'
)) {
    if ($helperBody.Contains($forbidden)) {
        throw "Drawing layout tree text suppression must not mutate saved item text: $forbidden"
    }
}
Write-Host 'QTreeWidget treeWidget_csv embedded QComboBox text suppression verified.'
















