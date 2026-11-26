$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$style = Join-Path $root 'Resources\styles\ltzk_sci_fi.qss'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}
if (-not (Test-Path $style)) {
    throw "Missing required file: $style"
}

$text = Get-Content -Raw -Encoding Default $source
$styleText = Get-Content -Raw -Encoding UTF8 $style
$combinedText = $text + "`n" + $styleText

foreach ($required in @(
    'class SplitterTriangleButton final : public QToolButton',
    'void setTriangleDirection(Qt::ArrowType direction)',
    'int splitterHandleRailWidth(const QWidget* widget)',
    'QSize splitterTriangleButtonSize(const QWidget* widget)',
    'int splitterHandleWidth(const QWidget* widget)',
    'return qMax(6, scaledInt(widget, 6));',
    'const int triangleWidth = splitterHandleWidth(widget);',
    'const int triangleHeight = qCeil(triangleWidth / equilateralAltitudeRatio);',
    'const QRectF bounds(0.0, 0.0, rect.width(), rect.height());',
    'void paintEvent(QPaintEvent*) override',
    'QPolygonF triangle = trianglePolygon(rect(), m_direction)',
    'const qreal outlineWidth = qMax<qreal>(0.5, width() * 0.15);',
    'bool hitButton(const QPoint& pos) const override',
    'QWidget* layerManagerHandle = workspaceSplitter->handle(1)',
    'workspaceLayerManagerCollapseButton',
    'setProperty("panelCollapseButton", true)',
    'layerManagerCollapseButton->setFixedSize(splitterTriangleButtonSize(page))',
    'layerManagerCollapseButton->raise();',
    'QFrame* layerManagerHandleRail = new QFrame(layerManagerHandle)',
    'setProperty("splitterHandleRail", true)',
    'layerManagerHandleRail->setFixedWidth(splitterHandleRailWidth(page))',
    'layerHandleLayout->setSpacing(0)',
    'layerHandleLayout->addWidget(layerManagerHandleRail, 0, 0)',
    'layerHandleLayout->addWidget(layerManagerCollapseButton, 0, 0, Qt::AlignCenter)',
    'QToolButton[panelCollapseButton="true"] { background: transparent; border: 0; padding: 0; min-width: 0; min-height: 0; }',
    'setTriangleDirection(Qt::LeftArrow)',
    'setTriangleDirection(Qt::RightArrow)',
    'layerManagerPanel->setMaximumWidth(collapsedLayerWidth)',
    'layerManagerPanel->setMaximumWidth(QWIDGETSIZE_MAX)',
    'connect(layerManagerCollapseButton, &QToolButton::clicked'
)) {
    if (-not $combinedText.Contains($required)) {
        throw "Workspace panel triangle toggle requirement missing: $required"
    }
}

foreach ($forbidden in @(
    'workspaceLayerManagerCollapseRail',
    'workspaceResultBoardCollapseRail',
    'workspaceSplitter->addWidget(layerCollapseRail)',
    'rightSplitter->addWidget(resultCollapseRail)',
    'QToolButton* collapseButton = new QToolButton(titleLabel)',
    'titleButtonLayout->addWidget(collapseButton'
,
    'new QToolButton(layerManagerHandle)',
    'new QToolButton(resultBoardHandle)',
    'setStyleSheet(QStringLiteral("QToolButton { color: #f8feff; background: #0f5b8f;',
    'QSplitter#workspaceSplitter::handle, QSplitter#workspaceRightSplitter::handle { background: rgba(38, 181, 228, 155);',
    'layerManagerCollapseButton->setFixedSize(scaledSize(page, 20, 38))',
    'resultBoardCollapseButton->setFixedSize(scaledSize(page, 38, 20))',
    'QHBoxLayout* resultButtonRowLayout = new QHBoxLayout()',
    'resultHandleLayout->addLayout(resultButtonRowLayout)',
    'qMax(24, scaledInt(widget, 32))'
,
    'qMax(18, scaledInt(widget, 24))',
    'return splitterTriangleButtonSize(widget).height() + scaledInt(widget, 6)',
    'qMax(14, scaledInt(widget, 14))',
    'return splitterTriangleButtonSize(widget).height() + scaledInt(widget, 2)',
    'layerManagerHandleRail->setFixedWidth(scaledInt(page, 3))',
    'layerManagerHandleRail->setFixedWidth(scaledInt(page, 2))'
)) {
    if ($combinedText.Contains($forbidden)) {
        throw "Triangle buttons must use existing splitter handles, not added bars/title labels: $forbidden"
    }
}

Write-Host 'Workspace splitter-handle outside triangle collapse/expand controls verified.'
