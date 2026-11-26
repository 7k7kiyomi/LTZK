$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$hostSource = Get-Content -Raw -Encoding Default (Join-Path $root 'LTZK.cpp')
$project = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'LTZK.vcxproj')
$adapterHeaderPath = Join-Path $root 'LocalProductOperation.h'
$adapterSourcePath = Join-Path $root 'LocalProductOperation.cpp'

function Assert-Contains([string]$text, [string]$token, [string]$message) {
    if (-not $text.Contains($token)) { throw $message }
}

if (-not (Test-Path -LiteralPath $adapterHeaderPath) -or
    -not (Test-Path -LiteralPath $adapterSourcePath)) {
    throw 'Platform local product adapter files are missing.'
}

$adapterHeader = Get-Content -Raw -Encoding UTF8 $adapterHeaderPath
$adapterSource = Get-Content -Raw -Encoding UTF8 $adapterSourcePath

Assert-Contains $adapterHeader 'class LocalProductOperation final' `
    'Missing platform local product operation boundary.'
Assert-Contains $adapterHeader 'static bool dispatch(' `
    'The local operation boundary must expose a dispatch entry point.'
Assert-Contains $adapterSource 'QgsVectorFileWriter::writeAsVectorFormatV3' `
    'Local vector range export must be implemented by the platform adapter.'
Assert-Contains $adapterSource 'QgsRasterFileWriter' `
    'Local raster range export must be implemented by the platform adapter.'
Assert-Contains $adapterSource 'transformBoundingBox' `
    'Local range export must transform the requested extent into source CRS.'
Assert-Contains $adapterSource 'main-area' `
    'The platform adapter must own the local main-area operation path.'

$callbackStart = $hostSource.IndexOf('setLocalProductOperationCallback(')
$callbackEnd = $hostSource.IndexOf('m_workspaceSideTabs->addTab', $callbackStart)
if ($callbackStart -lt 0 -or $callbackEnd -le $callbackStart) {
    throw 'Local product callback boundary not found.'
}
$localCallback = $hostSource.Substring($callbackStart, $callbackEnd - $callbackStart)
Assert-Contains $localCallback 'LocalProductOperation::dispatch' `
    'Local catalog operations must be handled by the platform adapter.'
if ($localCallback.Contains('executeConfiguredPluginFeature') -or
    $localCallback.Contains('ltzkProductCatalogRequest')) {
    throw 'Local catalog operations must not build or dispatch a database plugin request.'
}

if ($hostSource.Contains('Local product catalog action dispatched through plugin slot.')) {
    throw 'The obsolete local plugin-slot dispatch path is still present.'
}
Assert-Contains $hostSource 'buildProductCatalogApiRequest(' `
    'Database product catalog API dispatch must remain available.'
Assert-Contains $project 'LocalProductOperation.cpp' `
    'The platform adapter source must be part of the host project.'
Assert-Contains $project 'LocalProductOperation.h' `
    'The platform adapter header must be part of the host project.'

$pluginChanges = rg -n 'localSources|LocalProductOperation|localProductsFromRequest' `
    (Join-Path $root 'map_product_manager_tools') `
    --glob '!**/GeneratedFiles/**' --glob '!**/node_modules/**' 2>$null
if ($LASTEXITCODE -eq 0 -and $pluginChanges) {
    throw "Product-manager plugin source must remain unchanged:`n$pluginChanges"
}

Write-Output 'Local product platform adapter verification passed.'
