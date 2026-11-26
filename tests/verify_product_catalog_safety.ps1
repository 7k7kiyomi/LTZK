$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$pluginHeaderPath = Join-Path $root 'map_product_manager_tools\map_product_manager_tools.h'
$pluginSourcePath = Join-Path $root 'map_product_manager_tools\map_product_manager_tools.cpp'
$connectorSourcePath = Join-Path $root 'map_product_manager_tools\database\postgis_connector.cpp'
$apiSourcePath = Join-Path $root 'map_product_manager_tools\product_catalog_plugin_api.cpp'

foreach ($path in @($pluginHeaderPath, $pluginSourcePath, $connectorSourcePath, $apiSourcePath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing product catalog safety source: $path"
    }
}

$pluginHeader = Get-Content -Raw -Encoding UTF8 $pluginHeaderPath
$pluginSource = Get-Content -Raw -Encoding UTF8 $pluginSourcePath
$connectorSource = Get-Content -Raw -Encoding UTF8 $connectorSourcePath
$apiSource = Get-Content -Raw -Encoding UTF8 $apiSourcePath

foreach ($required in @(
    '#include <QPointer>',
    'QPointer<ProductStorageDialog> mProductStorageDialog'
)) {
    if (-not $pluginHeader.Contains($required)) {
        throw "Product storage dialog lifecycle tracking is missing: $required"
    }
}

foreach ($required in @(
    'setAttribute(Qt::WA_DeleteOnClose)',
    'if (mProductStorageDialog)',
    'delete mProductStorageDialog.data()'
)) {
    if (-not $pluginSource.Contains($required)) {
        throw "Product storage dialog cleanup is missing: $required"
    }
}

$syncStart = $pluginSource.IndexOf('static bool syncExternalCatalogConnection')
$syncEnd = $pluginSource.IndexOf('static void tryAutoLoginFromConfigFile', $syncStart)
if ($syncStart -lt 0 -or $syncEnd -lt 0) {
    throw 'Could not locate external catalog connection synchronization.'
}
$syncBody = $pluginSource.Substring($syncStart, $syncEnd - $syncStart)
if ($syncBody.Contains('db->disconnect()')) {
    throw 'External catalog connection synchronization must preserve the old connection when the new connection fails.'
}

foreach ($required in @(
    'PGconn* newConnection',
    'PQfinish(newConnection)'
)) {
    if (-not $connectorSource.Contains($required)) {
        throw "Transactional PostgreSQL reconnect is missing: $required"
    }
}

foreach ($required in @(
    'QMetaObject::invokeMethod(',
    '"executeProductCatalogRequest"',
    'Q_ARG(QVariantMap, requestMap)'
)) {
    if (-not $apiSource.Contains($required)) {
        throw "Safe queued product catalog dispatch is missing: $required"
    }
}

Write-Output 'Product catalog lifecycle, reconnect, and queued-dispatch safety verification passed.'
