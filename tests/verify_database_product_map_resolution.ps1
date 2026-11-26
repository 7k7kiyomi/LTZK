$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'DataCatalogWidget.cpp')
$start = $source.IndexOf('void DataCatalogWidget::addProductIdsToMap(')
$end = $source.IndexOf('void DataCatalogWidget::addProductItemToMap(', $start)
if ($start -lt 0 -or $end -le $start) {
    throw 'Cannot locate platform database map-loading implementation.'
}
$method = $source.Substring($start, $end - $start)

foreach ($required in @(
    'gCurrentUserSession.isLoggedIn',
    'DataCatalogProductCatalog catalog(database)',
    'catalog.product(id, &errorMessage)',
    'product.layerTableName',
    'isProductVectorType(product.productType)',
    'isProductRasterType(product.productType)',
    'm_loadCallback(uri)'
)) {
    if (-not $method.Contains($required)) {
        throw "Database map loading does not resolve product records by ID: $required"
    }
}
if ($method.Contains('findProduct(') -or $method.Contains('m_databaseModel->item(')) {
    throw 'Database map loading still depends on product nodes in the UI tree.'
}
Write-Output 'Database product ID resolution contract passed.'
