$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$catalog = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'DataCatalogWidget.cpp')
$platform = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'LTZK.cpp')
$catalogHeader = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'DataCatalogWidget.h')
$productCatalogHeader = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'DataCatalogProductCatalog.h')
$productCatalog = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'DataCatalogProductCatalog.cpp')

$catalogTree = $catalog.Substring(
    $catalog.IndexOf('void DataCatalogWidget::appendProductDirectoryChildren('),
    $catalog.IndexOf('bool DataCatalogWidget::isProductCatalogDirectory(') -
        $catalog.IndexOf('void DataCatalogWidget::appendProductDirectoryChildren('))
$importBridge = $platform.Substring(
    $platform.IndexOf('m_dataCatalogWidget->setDataImportCallback('),
    $platform.IndexOf('m_dataCatalogWidget->setProductOperationCallback(') -
        $platform.IndexOf('m_dataCatalogWidget->setDataImportCallback('))
$catalogLoad = $catalog.Substring(
    $catalog.IndexOf('void DataCatalogWidget::loadDatabaseData()'),
    $catalog.IndexOf('QStandardItem* DataCatalogWidget::schemaItem(') -
        $catalog.IndexOf('void DataCatalogWidget::loadDatabaseData()'))

if ($catalogTree -notmatch 'if \(directory\.nodeType != 1\)[\s\S]*?for \(const ProductRecord& product : visibleProducts\)') {
    throw 'Database tree must show products in every ordinary directory, including custom directories.'
}
if ($importBridge -notmatch 'executeConfiguredPluginFeature\([\s\S]*?QStringLiteral\("map_product_manager_tools"\)[\s\S]*?QStringLiteral\("数据导入"\)') {
    throw 'Database import must invoke the product manager plugin that understands directory IDs.'
}
if ($importBridge -notmatch 'executeConfiguredPluginFeature\([\s\S]*?refreshDatabaseData\(\)') {
    throw 'Database tree must reload after the modal import wizard returns.'
}
if ($catalogLoad -notmatch 'isExpanded\([\s\S]*?expandedDirectoryIds[\s\S]*?m_databaseTree->expand\(') {
    throw 'Reloading database data must restore expanded directories.'
}
if ($catalogHeader -notmatch 'void importProductData\(int targetDirectoryId\)' -or
    $catalog -notmatch 'captureImportSnapshot\([\s\S]*?m_dataImportCallback\(targetDirectoryId\)[\s\S]*?relocateImportedProducts\(') {
    throw 'Platform import must compare database state before and after the import callback.'
}
if ($productCatalogHeader -notmatch 'struct ProductImportSnapshot' -or
    $productCatalog -notmatch 'SELECT id, parent_id, name FROM product_directory' -or
    $productCatalog -notmatch 'SELECT id, parent_dir_id FROM product_metadata' -or
    $productCatalog -notmatch 'UPDATE product_directory SET parent_id = :target_id' -or
    $productCatalog -notmatch 'UPDATE product_metadata SET parent_dir_id = :target_id') {
    throw 'Platform catalog must relocate only products and directories created by this import.'
}

Write-Output 'Database import visibility verification passed.'
