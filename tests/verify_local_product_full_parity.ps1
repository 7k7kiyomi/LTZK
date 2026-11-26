$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$catalogHeader = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'DataCatalogWidget.h')
$catalogSource = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'DataCatalogWidget.cpp')
$hostSource = Get-Content -Raw -Encoding Default (Join-Path $root 'LTZK.cpp')

function Assert-Contains([string]$text, [string]$token, [string]$message) {
    if (-not $text.Contains($token)) { throw $message }
}

Assert-Contains $catalogHeader 'LocalProductOperationCallback' 'Missing local product operation callback contract.'
Assert-Contains $catalogHeader 'setLocalProductOperationCallback' 'Missing local product operation callback setter.'

$proxyStart = $catalogSource.IndexOf('class CatalogBrowserProxyModel final')
$proxyEnd = $catalogSource.IndexOf('void appendUniqueUri(', $proxyStart)
if ($proxyStart -lt 0 -or $proxyEnd -le $proxyStart) { throw 'Local proxy boundary not found.' }
$proxy = $catalogSource.Substring($proxyStart, $proxyEnd - $proxyStart)
Assert-Contains $proxy 'Qt::DecorationRole' 'Local proxy must override item icons.'
Assert-Contains $proxy 'drawProductLayerIcon' 'Local files must reuse database product layer icons.'
Assert-Contains $proxy 'catalogDirectoryIcon' 'Local folders must reuse database directory icons.'

if ($catalogSource -notmatch 'isCatalogCollection\(context\)\s*\|\|\s*isCatalogLayer\(context\)') {
    throw 'Local layers must use the same full menu layout as database layer-directory nodes.'
}
Assert-Contains $catalogSource 'm_localProductOperationCallback(operation, localUris)' `
    'Local range/main-area/theme actions must dispatch selected URIs.'
Assert-Contains $hostSource 'setLocalProductOperationCallback' 'Host must wire the local product callback.'
Assert-Contains $hostSource 'QStringLiteral("localSources")' 'Host must serialize local source descriptors.'
Assert-Contains $hostSource 'buildProductCatalogApiRequest(' `
    'Database product catalog requests must continue to use the existing C ABI dispatch path.'

$leafStart = $catalogSource.IndexOf('QAction* exportData = menu.addAction(QStringLiteral("导出所选数据"));')
$leafEnd = $catalogSource.IndexOf('void DataCatalogWidget::populateContextMenu(', $leafStart)
if ($leafStart -lt 0 -or $leafEnd -le $leafStart) { throw 'Product leaf context-menu boundary not found.' }
$leafMenu = $catalogSource.Substring($leafStart, $leafEnd - $leafStart)
Assert-Contains $leafMenu 'rangeExport->setEnabled(canProductOperation);' `
    'Local product leaf nodes must enable the same range-export action as database products.'
Assert-Contains $leafMenu 'mainAreaExport->setEnabled(canProductOperation);' `
    'Local product leaf nodes must enable the same cartographic-export action as database products.'
Assert-Contains $leafMenu 'addToTheme->setEnabled(canProductOperation);' `
    'Local product leaf nodes must enable the same theme action as database products.'
Assert-Contains $leafMenu 'selectedProductIds, selectedLocalUris]()' `
    'Local leaf export actions must capture the selected local URI list.'
Assert-Contains $leafMenu 'selectedLocalUris);' `
    'Local leaf range/cartographic actions must dispatch the selected local URI list.'

$pluginChanges = rg -n 'localSources|LocalProductOperation|localProductsFromRequest' `
    (Join-Path $root 'map_product_manager_tools') `
    --glob '!**/GeneratedFiles/**' --glob '!**/node_modules/**' 2>$null
if ($LASTEXITCODE -eq 0 -and $pluginChanges) {
    throw "Product-manager plugin source must remain unchanged:`n$pluginChanges"
}

Write-Output 'Local product full parity verification passed.'
