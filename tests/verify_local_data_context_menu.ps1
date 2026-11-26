$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$headerPath = Join-Path $root 'DataCatalogWidget.h'
$sourcePath = Join-Path $root 'DataCatalogWidget.cpp'
$ltzkPath = Join-Path $root 'LTZK.cpp'

foreach ($path in @($headerPath, $sourcePath, $ltzkPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing local/database product menu source: $path"
    }
}

$header = Get-Content -Raw -Encoding UTF8 $headerPath
$source = Get-Content -Raw -Encoding UTF8 $sourcePath
$ltzk = Get-Content -Raw -Encoding Default $ltzkPath

function Assert-Contains([string]$text, [string]$token, [string]$message) {
    if (-not $text.Contains($token)) {
        throw $message
    }
}

foreach ($token in @(
    'populateProductContextMenu',
    'isProductMenuContext',
    'isProductMenuDirectory',
    'localProductUrisForContext',
    'setLocalProductSubtreeCheck',
    'dispatchProductOperation',
    'batchExportUris',
    'CatalogSource::Local',
    'isCatalogLayer(context)',
    'isCatalogCollection(context)',
    '导出所选数据',
    '按范围导出',
    '按制图导出',
    '添加到主题'
)) {
    Assert-Contains ($header + $source) $token "Missing local/database product menu contract: $token"
}

Assert-Contains $source 'if (isProductMenuContext(context))' `
    'Context menu must route both sources through the product menu builder.'
if ($source -notmatch 'dispatchProductOperation\(\s*QStringLiteral\("map"\)') {
    throw 'Product menu map action must use the shared dispatcher.'
}
Assert-Contains $source 'context.source == CatalogSource::Local' `
    'Shared dispatcher must contain a local-data adapter branch.'
$productMenuStart = $source.IndexOf('void DataCatalogWidget::populateProductContextMenu(')
$productMenuEnd = $source.IndexOf('void DataCatalogWidget::populateContextMenu(', $productMenuStart)
if ($productMenuStart -lt 0 -or $productMenuEnd -le $productMenuStart) {
    throw 'Shared product context-menu boundary was not found.'
}
$productMenu = $source.Substring($productMenuStart, $productMenuEnd - $productMenuStart)
if ($productMenu.Contains('勾选本节点下全部成果') -or
    $productMenu.Contains('取消勾选本节点下全部成果')) {
    throw 'Shared product context menu must hide bulk-check actions.'
}
Assert-Contains $ltzk 'setProductOperationCallback' `
    'The existing database product callback must remain wired.'
Assert-Contains $ltzk 'QStringLiteral("成果目录操作")' `
    'The existing database product operation feature must remain unchanged.'

$populateStart = $source.IndexOf('void DataCatalogWidget::populateContextMenu')
$genericStart = $source.IndexOf(
    'QAction* addToMap = menu.addAction(QStringLiteral("添加到地图"));',
    $populateStart)
$genericEnd = $source.IndexOf('bool DataCatalogWidget::isCatalogLayer', $genericStart)
if ($populateStart -lt 0 -or $genericStart -lt $populateStart -or
    $genericEnd -lt $genericStart) {
    throw 'Generic context-menu boundary was not found.'
}
$genericMenu = $source.Substring($genericStart, $genericEnd - $genericStart)
if ($genericMenu.Contains('新建目录') -or $genericMenu.Contains('按范围导出')) {
    throw 'Generic menu must not own the database product-directory menu actions.'
}

Write-Output 'Local/database product context menu verification passed.'
