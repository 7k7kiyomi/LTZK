$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root "LTZK.cpp"
$ui = Join-Path $root "LTZK.ui"

foreach ($path in @($source, $ui)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$sourceText = Get-Content $source -Raw -Encoding Default
$uiText = Get-Content $ui -Raw -Encoding UTF8
$attributeTableLabel = -join ([char[]](0x6253, 0x5F00, 0x5C5E, 0x6027, 0x8868))
$fieldsLabel = -join ([char[]](0x6253, 0x5F00, 0x5B57, 0x6BB5, 0x7BA1, 0x7406, 0x5668))

if ($sourceText -match "QgsRasterLayer\*\s+rasterLayer\s*=\s*qobject_cast<QgsRasterLayer\*>\(layer\);\s*m_curMapLayer\s*=\s*rasterLayer;") {
    throw "onCurrentLayerChanged still overwrites vector layers with a null raster pointer"
}

if ($sourceText -notmatch "m_curMapLayer\s*=\s*layer;") {
    throw "onCurrentLayerChanged does not preserve the selected map layer directly"
}

if ($sourceText -notmatch "setContextMenuPolicy\(Qt::CustomContextMenu\)") {
    throw "Layer tree does not enable a custom context menu"
}

if ($sourceText -notmatch "customContextMenuRequested") {
    throw "Layer tree does not handle right-click context menu requests"
}

if ($sourceText -notmatch "QAction\*\s+actionOpenAttributeTable") {
    throw "Layer tree context menu does not create an Open Attribute Table action"
}

if ($sourceText -notmatch "on_actionOpenAttrTable_triggered") {
    throw "Layer tree context menu is not wired to the attribute table slot"
}

$attributeBlock = [regex]::Match($uiText, '(?s)<action name="actionOpenAttrTable">.*?</action>').Value
if (-not $attributeBlock.Contains($attributeTableLabel)) {
    throw "The real attribute table action has the wrong label"
}

$fieldsBlock = [regex]::Match($uiText, '(?s)<action name="actionOpenFields">.*?</action>').Value
if (-not $fieldsBlock.Contains($fieldsLabel)) {
    throw "The fields action has the wrong label"
}

Write-Host "Attribute table action wiring verified."
