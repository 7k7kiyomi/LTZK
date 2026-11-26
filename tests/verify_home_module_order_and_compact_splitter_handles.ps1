$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$text = Get-Content -Raw -Encoding UTF8 $source

function Assert-Contains {
    param([string]$Content, [string]$Needle, [string]$Message)
    if (-not $Content.Contains($Needle)) { throw $Message }
}

$moduleStart = $text.IndexOf('QWidget* LTZK::buildHomeModulesPanel(QWidget* parent)')
$moduleEnd = $text.IndexOf('QWidget* LTZK::buildAccessControlPage', $moduleStart)
if ($moduleStart -lt 0 -or $moduleEnd -lt 0) { throw 'Could not locate buildHomeModulesPanel body.' }
$moduleBody = $text.Substring($moduleStart, $moduleEnd - $moduleStart)

$expectedOrder = @(
    'QStringLiteral("地图成果管理")',
    'QStringLiteral("地图智能制图")',
    'QStringLiteral("地图成果质检")',
    'QStringLiteral("地图服务管理")'
)
$lastIndex = -1
foreach ($needle in $expectedOrder) {
    $index = $moduleBody.IndexOf($needle)
    if ($index -lt 0) { throw "Home module missing: $needle" }
    if ($index -le $lastIndex) { throw "Home module order is wrong at: $needle" }
    $lastIndex = $index
}

foreach ($required in @(
    'grid->addWidget(achievementButton, 0, 0, Qt::AlignCenter)',
    'grid->addWidget(generalizationButton, 0, 1, Qt::AlignCenter)',
    'grid->addWidget(qualityButton, 1, 0, Qt::AlignCenter)',
    'grid->addWidget(serviceButton, 1, 1, Qt::AlignCenter)',
    'int splitterHandleRailWidth(const QWidget* widget)',
    'int splitterHandleWidth(const QWidget* widget)',
    'return qMax(12, scaledInt(widget, 12));',
    'const int handleWidth = splitterHandleWidth(widget);',
    'const int side = qMax(8, handleWidth - splitterHandleRailWidth(widget));',
    'layerManagerHandleRail->setFixedWidth(splitterHandleRailWidth(page))',
    'resultBoardHandleRail->setFixedHeight(scaledInt(page, 3))',
    'resultButtonRow->setFixedHeight(splitterTriangleButtonSize(page).height() + scaledInt(page, 2))'
)) {
    Assert-Contains $text $required "Requirement missing: $required"
}

foreach ($required in @(
    'QStringLiteral("01")',
    'QStringLiteral("02")',
    'QStringLiteral("03")',
    'QStringLiteral("04")'
)) {
    Assert-Contains $moduleBody $required "Module code missing: $required"
}

foreach ($forbidden in @(
    'qMax(24, scaledInt(widget, 32))',
    'qMax(18, scaledInt(widget, 24))',
    'return splitterTriangleButtonSize(widget).height() + scaledInt(widget, 6)',
    'qMax(14, scaledInt(widget, 14))',
    'return splitterTriangleButtonSize(widget).height() + scaledInt(widget, 2)',
    'layerManagerHandleRail->setFixedWidth(scaledInt(page, 2))',
    'layerManagerHandleRail->setFixedWidth(scaledInt(page, 3))',
    'return splitterTriangleButtonSize(widget).height() + scaledInt(widget, 12)',
    'layerManagerHandleRail->setFixedWidth(scaledInt(page, 4))',
    'resultBoardHandleRail->setFixedHeight(scaledInt(page, 4))',
    'resultButtonRow->setFixedHeight(splitterTriangleButtonSize(page).height() + scaledInt(page, 4))'
)) {
    if ($text.Contains($forbidden)) { throw "Legacy oversized splitter handle remains: $forbidden" }
}

Write-Host 'Home module order and compact splitter handles verified.'
