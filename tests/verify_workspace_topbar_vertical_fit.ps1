$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$source = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8

foreach ($required in @(
    '#include <QWheelEvent>',
    'class WorkspaceTopBarGradientScrollArea final : public QScrollArea',
    'void setBaseContentHeight(int height)',
    'horizontalScrollBar()->maximum() > 0',
    'horizontalScrollBar()->sizeHint().height()',
    'void wheelEvent(QWheelEvent* event) override',
    'event->accept();',
    'scrollContent->setFixedHeight(contentHeight)',
    'pluginStrip->setFixedHeight(contentHeight)',
    'scrollArea->setBaseContentHeight(contentHeight)',
    'refreshWorkspaceScaledMetrics();'
)) {
    if (-not $source.Contains($required)) {
        throw "Top-bar vertical-fit requirement missing: $required"
    }
}

foreach ($forbidden in @(
    'topActionScrollBarReserveHeight',
    'topActionContentHeight + topActionScrollBarReserveHeight',
    'topActionHoverOverflow'
)) {
    if ($source.Contains($forbidden)) {
        throw "Permanent top-bar gap or obsolete scroll reserve remains: $forbidden"
    }
}

Write-Host 'Workspace top-bar content height and vertical wheel behavior verified.'
