$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$cppPath = Join-Path $repoRoot "LTZK.cpp"
$headerPath = Join-Path $repoRoot "LTZK.h"

$cpp = Get-Content -LiteralPath $cppPath -Raw -Encoding UTF8
$header = Get-Content -LiteralPath $headerPath -Raw -Encoding UTF8

function Assert-Contains {
    param(
        [string]$Content,
        [string]$Pattern,
        [string]$Message
    )

    if ($Content -notmatch $Pattern) {
        throw $Message
    }
}

function Assert-NotContains {
    param(
        [string]$Content,
        [string]$Pattern,
        [string]$Message
    )

    if ($Content -match $Pattern) {
        throw $Message
    }
}
Assert-Contains $cpp '#include <QColorDialog>' "LTZK.cpp should include QColorDialog for color picking."
Assert-Contains $cpp 'actionSetLayerColor' "Layer tree context menu should expose a set-layer-color action."
Assert-Contains $cpp '设置图层颜色' "Layer tree context menu should label the color action in Chinese."
Assert-Contains $cpp 'QColor chooseLocalizedColor' "Color dialogs should use a localized helper instead of QColorDialog::getColor."
Assert-Contains $cpp 'QColorDialog::DontUseNativeDialog' "Color dialogs should force the Qt dialog so internal text can be localized."
Assert-Contains $cpp 'localizeColorDialog' "Color dialogs should localize built-in Qt labels."
foreach ($cn in @('基础颜色', '自定义颜色', '拾取屏幕颜色', '添加到自定义颜色', '色调', '饱和度', '亮度', '红色', '绿色', '蓝色', '十六进制', '确定', '取消')) {
    Assert-Contains $cpp ([regex]::Escape($cn)) "Color dialog should contain Chinese text: $cn"
}
if ($cpp -match 'QColorDialog::getColor') {
    throw 'Color dialogs should not use QColorDialog::getColor because the static API cannot localize the built-in English labels before showing.'
}Assert-Contains $cpp 'void LTZK::setShpLayerDisplayColor\(QgsVectorLayer\* layer\)' "LTZK.cpp should implement setShpLayerDisplayColor."
Assert-Contains $cpp 'QgsWkbTypes::PointGeometry' "Layer color implementation should handle point shp layers."
Assert-Contains $cpp 'QgsWkbTypes::LineGeometry' "Layer color implementation should handle line shp layers."
Assert-Contains $cpp 'QgsWkbTypes::PolygonGeometry' "Layer color implementation should handle polygon shp layers."
Assert-Contains $cpp 'QgsSingleSymbolRenderer' "Layer color implementation should apply a single-symbol renderer."
Assert-Contains $cpp 'void LTZK::chooseMapCanvasColor\(\)' "LTZK.cpp should implement chooseMapCanvasColor."
Assert-Contains $cpp 'contextMenuAboutToShow' "QgsMapCanvas should use its native context menu extension signal."
Assert-Contains $cpp 'QMenu\* menu, QgsMapMouseEvent\* event' "QGIS native context menu callback should receive the menu and mouse event."
Assert-Contains $cpp '选择画布颜色' "Map canvas context menu should label the canvas color action in Chinese."
Assert-Contains $cpp 'chooseMapCanvasColor' "Map canvas context menu should reuse chooseMapCanvasColor."
Assert-NotContains $cpp 'workspaceMapCanvasColorButton' "The old floating canvas color button should be removed."
Assert-NotContains $header 'm_mapCanvasColorButton' "The old floating canvas color member should be removed."
Assert-Contains $cpp 'm_hasCustomCanvasColor' "Canvas background logic should track a user-selected canvas color."
Assert-Contains $cpp 'm_customCanvasColor' "Canvas background logic should store a user-selected canvas color."
Assert-Contains $cpp 'if \(m_hasCustomCanvasColor\)' "Canvas background logic should prefer the custom canvas color."
Assert-Contains $cpp 'setCanvasColor\(m_customCanvasColor\)' "Canvas background should apply the custom canvas color when present."

Assert-Contains $header 'void setShpLayerDisplayColor\(QgsVectorLayer\* layer\);' "LTZK.h should declare setShpLayerDisplayColor."
Assert-Contains $header 'void chooseMapCanvasColor\(\);' "LTZK.h should declare chooseMapCanvasColor."

Assert-Contains $header 'QColor m_customCanvasColor;' "LTZK.h should store the custom canvas color."
Assert-Contains $header 'bool m_hasCustomCanvasColor = false;' "LTZK.h should track whether custom canvas color is active."

Write-Host "SHP layer and canvas color controls verified."
