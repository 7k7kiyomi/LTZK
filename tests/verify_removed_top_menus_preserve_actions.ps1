$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$uiPath = Join-Path $root 'LTZK.ui'

if (-not (Test-Path $uiPath)) {
    throw "Missing required file: $uiPath"
}

$uiText = Get-Content -Raw -Encoding Default $uiPath

$removedTopMenus = @(
    'menu_2',
    'menu_3',
    'menu_5'
)

foreach ($menuName in $removedTopMenus) {
    if ($uiText -match "<addaction name=`"$menuName`"\s*/>") {
        throw "Top menu is still visible in menuBar: $menuName"
    }

    if ($uiText -notmatch "<widget class=`"QMenu`" name=`"$menuName`"") {
        throw "Menu object was removed instead of only hiding it from menuBar: $menuName"
    }
}

$symbolMenuName = 'Self' + 'Stylelib' + 'Mng'
if ($uiText -match "<widget class=`"QMenu`" name=`"$symbolMenuName`"") {
    throw 'Symbol library menu should be fully removed.'
}

$preservedActions = @(
    'actionLayerTreeControl',
    'actionOpenFields',
    'actionOpenAttrTable',
    'actionAddVectorData',
    'actionAddRasterData',
    'actionAddWmsLayer',
    'actionAddWfsLayer',
    'actionAddWcsLayer',
    'actionCoordTransShp',
    'actionCoordTransqgis',
    'actionCoordTranstif',
    'actionCoordTranspoint',
    'actionZoomIn',
    'actionZoomOut',
    'actionFullExtent',
    'actionZoomActual',
    'actionPan'
)

foreach ($actionName in $preservedActions) {
    if ($uiText -notmatch "<action name=`"$actionName`"") {
        throw "Action was removed but should be preserved: $actionName"
    }
}

foreach ($toolbarAction in @(
    'actionAddVectorData',
    'actionAddRasterData',
    'actionOpenFields',
    'actionOpenAttrTable',
    'actionZoomIn',
    'actionZoomOut',
    'actionFullExtent',
    'actionZoomActual',
    'actionPan'
)) {
    if ($uiText -notmatch "<addaction name=`"$toolbarAction`"\s*/>") {
        throw "Toolbar access was removed for action: $toolbarAction"
    }
}

Write-Host 'Removed top menus while preserving actions verified.'
