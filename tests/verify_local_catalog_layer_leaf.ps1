$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'DataCatalogWidget.cpp'
if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing data catalog source: $sourcePath"
}

$source = Get-Content -Raw -Encoding UTF8 $sourcePath

function Assert-Contains([string]$text, [string]$token, [string]$message) {
    if (-not $text.Contains($token)) {
        throw $message
    }
}

$proxyStart = $source.IndexOf('class CatalogBrowserProxyModel final')
$proxyEnd = $source.IndexOf('void appendUniqueUri(', $proxyStart)
if ($proxyStart -lt 0 -or $proxyEnd -le $proxyStart) {
    throw 'CatalogBrowserProxyModel boundary was not found.'
}
$proxy = $source.Substring($proxyStart, $proxyEnd - $proxyStart)

Assert-Contains $proxy 'bool hasChildren(' `
    'Local catalog layers must override hasChildren and behave as leaf nodes.'
Assert-Contains $proxy 'bool filterAcceptsRow(' `
    'Local catalog proxy must filter browser children below layer nodes.'
Assert-Contains $proxy 'qobject_cast<QgsLayerItem*>(dataItem(parent))' `
    'hasChildren must identify local layer proxy indexes.'
Assert-Contains $proxy 'mModel->dataItem(sourceParent)' `
    'filterAcceptsRow must inspect the source parent item.'
Assert-Contains $proxy 'QgsBrowserProxyModel::hasChildren(parent)' `
    'Non-layer nodes must keep the QGIS browser expansion behavior.'
Assert-Contains $proxy 'QgsBrowserProxyModel::filterAcceptsRow(' `
    'Non-layer rows must keep the QGIS browser filtering behavior.'

$localMenuStart = $source.IndexOf(
    'void DataCatalogWidget::showLocalContextMenu')
$localMenuEnd = $source.IndexOf(
    'void DataCatalogWidget::populateProductContextMenu',
    $localMenuStart)
if ($localMenuStart -lt 0 -or $localMenuEnd -le $localMenuStart) {
    throw 'Local context-menu boundary was not found.'
}
$localMenu = $source.Substring(
    $localMenuStart,
    $localMenuEnd - $localMenuStart)
$guardIndex = $localMenu.IndexOf('!isProductMenuContext(context)')
$populateIndex = $localMenu.IndexOf('populateContextMenu(menu, context)')
if ($guardIndex -lt 0) {
    throw 'Local context menu must reject Fields and other non-product browser nodes.'
}
if ($populateIndex -lt 0 -or $guardIndex -gt $populateIndex) {
    throw 'Local context-menu guard must run before menu population.'
}

$databaseMenuStart = $source.IndexOf(
    'void DataCatalogWidget::showDatabaseContextMenu')
$databaseMenuEnd = $source.IndexOf(
    'void DataCatalogWidget::showLocalContextMenu',
    $databaseMenuStart)
if ($databaseMenuStart -lt 0 -or $databaseMenuEnd -le $databaseMenuStart) {
    throw 'Database context-menu boundary was not found.'
}
$databaseMenu = $source.Substring(
    $databaseMenuStart,
    $databaseMenuEnd - $databaseMenuStart)
if ($databaseMenu.Contains('!isProductMenuContext(context)')) {
    throw 'Database context menu must keep ordinary database-table behavior.'
}

Write-Output 'Local catalog layer-leaf verification passed.'
