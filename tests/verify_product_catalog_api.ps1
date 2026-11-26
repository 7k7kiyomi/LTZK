$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$apiHeader = Join-Path $root 'product_catalog_plugin_api.h'
$apiSource = Join-Path $root 'map_product_manager_tools\product_catalog_plugin_api.cpp'
$pluginHeader = Join-Path $root 'map_product_manager_tools\map_product_manager_tools.h'
$pluginSource = Join-Path $root 'map_product_manager_tools\map_product_manager_tools.cpp'
$pluginProject = Join-Path $root 'map_product_manager_tools\map_product_manager_tools.vcxproj'
$platformManagerHeader = Join-Path $root 'LTZKPluginManager.h'
$platformManagerSource = Join-Path $root 'LTZKPluginManager.cpp'
$platformSource = Join-Path $root 'LTZK.cpp'

foreach ($path in @(
    $apiHeader,
    $apiSource,
    $pluginHeader,
    $pluginSource,
    $pluginProject,
    $platformManagerHeader,
    $platformManagerSource,
    $platformSource
)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing required API integration file: $path"
    }
}

$apiText = Get-Content -Raw -Encoding UTF8 $apiHeader
foreach ($required in @(
    'LTZK_PRODUCT_CATALOG_API_VERSION',
    'LTZKProductCatalogOperationRequest',
    'operationUtf8',
    'directoryId',
    'productIds',
    'connectionJsonUtf8',
    'LTZKProductCatalogOpenOperationFn',
    'ltzk_product_catalog_open_operation'
)) {
    if (-not $apiText.Contains($required)) {
        throw "Shared product catalog API header is missing: $required"
    }
}

$apiSourceText = Get-Content -Raw -Encoding UTF8 $apiSource
foreach ($required in @(
    'product_catalog_plugin_api.h',
    'Q_DECL_EXPORT',
    'ltzk_product_catalog_open_operation',
    'CMapProductManagerPlugin::currentInstance',
    'QMetaObject::invokeMethod',
    'LTZK_PRODUCT_CATALOG_INVALID_ARGUMENT'
)) {
    if (-not $apiSourceText.Contains($required)) {
        throw "DLL API implementation is missing: $required"
    }
}

$pluginHeaderText = Get-Content -Raw -Encoding UTF8 $pluginHeader
foreach ($required in @(
    'static CMapProductManagerPlugin* currentInstance()',
    'executeProductCatalogRequest(const QVariantMap& request)',
    's_instance'
)) {
    if (-not $pluginHeaderText.Contains($required)) {
        throw "Plugin instance/request entry is missing: $required"
    }
}

$pluginSourceText = Get-Content -Raw -Encoding UTF8 $pluginSource
foreach ($required in @(
    'CMapProductManagerPlugin* CMapProductManagerPlugin::s_instance',
    'CMapProductManagerPlugin::currentInstance()',
    'CMapProductManagerPlugin::executeProductCatalogRequest',
    'executeProductCatalogRequest(request)'
)) {
    if (-not $pluginSourceText.Contains($required)) {
        throw "Plugin request routing is missing: $required"
    }
}

$projectText = Get-Content -Raw -Encoding UTF8 $pluginProject
foreach ($required in @(
    'product_catalog_plugin_api.cpp',
    'product_catalog_plugin_api.h'
)) {
    if (-not $projectText.Contains($required)) {
        throw "Plugin project does not compile/include API file: $required"
    }
}

$managerHeaderText = Get-Content -Raw -Encoding UTF8 $platformManagerHeader
$managerSourceText = Get-Content -Raw -Encoding UTF8 $platformManagerSource
foreach ($required in @(
    'invokeProductCatalogOperation',
    'ltzk_product_catalog_open_operation',
    'resolve'
)) {
    if (-not ($managerHeaderText.Contains($required) -or $managerSourceText.Contains($required))) {
        throw "Platform plugin manager API invocation is missing: $required"
    }
}

$platformText = Get-Content -Raw -Encoding UTF8 $platformSource
foreach ($required in @(
    'LTZKProductCatalogOperationRequest',
    'invokeProductCatalogOperation',
    '成果目录操作'
)) {
    if (-not $platformText.Contains($required)) {
        throw "Platform data catalog API call is missing: $required"
    }
}

Write-Host 'Product catalog DLL API integration is present.'
