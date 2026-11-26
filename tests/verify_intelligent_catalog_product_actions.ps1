$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$hostSource = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'LTZK.cpp')
$importStart = $hostSource.IndexOf('m_dataCatalogWidget->setDataImportCallback')
$operationStart = $hostSource.IndexOf('m_dataCatalogWidget->setProductOperationCallback')
$localStart = $hostSource.IndexOf('m_dataCatalogWidget->setLocalProductOperationCallback')
if ($importStart -lt 0 -or $operationStart -le $importStart -or $localStart -le $operationStart) {
    throw 'Cannot locate the data-catalog callbacks.'
}
$importCallback = $hostSource.Substring($importStart, $operationStart - $importStart)
$operationCallback = $hostSource.Substring($operationStart, $localStart - $operationStart)
foreach ($callback in @($importCallback, $operationCallback)) {
    if (-not $callback.Contains('"ltzkProductCatalogRequest"') -or
        -not $callback.Contains('executeConfiguredPluginFeature(') -or
        -not $callback.Contains('QStringLiteral("map_product_manager_tools")')) {
        throw 'A data-catalog callback no longer passes its request to the product manager.'
    }
}
if ($hostSource -notmatch 'catalogProductAction\s*=\s*!catalogRequest\.isEmpty\(\)\s*&&\s*plugin\.id\s*==\s*QStringLiteral\("map_product_manager_tools"\)') {
    throw 'Cross-module dispatch must be limited to data-catalog product-manager requests.'
}
if ($hostSource -notmatch 'catalogRequest\.contains\(QStringLiteral\("operation"\)\)' -or
    $hostSource -notmatch 'catalogRequest\.contains\(QStringLiteral\("connection"\)\)') {
    throw 'Cross-module dispatch must require a complete catalog request.'
}
if ($hostSource -notmatch '!catalogProductAction\s*&&\s*businessModuleMenuTitle') {
    throw 'Normal plugin actions must keep the active-module guard.'
}
if ($hostSource -notmatch '!catalogProductAction\s*&&\s*activatePluginFeatureWindows') {
    throw 'A catalog request must dispatch even when an earlier plugin window is open.'
}
if ($hostSource -notmatch 'ensureAuthenticatedForModule\(bindingModuleTitle\)') {
    throw 'The permission check must remain in place.'
}

Write-Output 'Intelligent catalog product action routing contract passed.'
