$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding UTF8 $source

foreach ($required in @(
    'int splitterHandleWidth(const QWidget* widget)',
    'return qMax(6, scaledInt(widget, 6));',
    'int splitterHandleRailWidth(const QWidget* widget)',
    'return splitterHandleWidth(widget);',
    'const int triangleWidth = splitterHandleWidth(widget);',
    'const int triangleHeight = qCeil(triangleWidth / equilateralAltitudeRatio);',
    'return QSize(triangleWidth, triangleHeight);',
    'const QRectF bounds(0.0, 0.0, rect.width(), rect.height());',
    'QGridLayout* layerHandleLayout = new QGridLayout(layerManagerHandle);',
    'layerHandleLayout->addWidget(layerManagerCollapseButton, 0, 0, Qt::AlignCenter);',
    'layerHandleLayout->setSpacing(0)',
    'layerManagerHandleRail->setFixedWidth(splitterHandleRailWidth(page))'
)) {
    if (-not $text.Contains($required)) {
        throw "Adaptive workspace rail requirement missing: $required"
    }
}

$baseHandleWidth = 6
$baseRailWidth = 6
$baseTriangleWidth = $baseHandleWidth
$baseTriangleHeight = $baseTriangleWidth / ([Math]::Sqrt(3.0) / 2.0)
if ($baseHandleWidth -ne 6 -or $baseRailWidth -ne 6 -or $baseTriangleWidth -ne 6 -or [Math]::Abs($baseTriangleHeight - 6.92820323027551) -gt 0.0001) {
    throw 'The workspace handle and triangle must both span 6px, with an equilateral triangle height of about 6.93px.'
}

foreach ($forbidden in @(
    'qMax(14, scaledInt(widget, 14))',
    'return splitterTriangleButtonSize(widget).height() + scaledInt(widget, 2)',
    'layerManagerHandleRail->setFixedWidth(scaledInt(page, 2))'
)) {
    if ($text.Contains($forbidden)) {
        throw "Fixed 16px workspace rail implementation remains: $forbidden"
    }
}

Write-Host 'Workspace splitter rail and triangle 6px width verified.'
