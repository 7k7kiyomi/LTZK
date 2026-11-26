$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$hostSource = Get-Content -Raw -Encoding Default (Join-Path $root 'LTZK.cpp')
$managerHeader = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'LTZKPluginManager.h')
$managerSource = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'LTZKPluginManager.cpp')

function Assert-Contains([string]$text, [string]$token, [string]$message) {
    if (-not $text.Contains($token)) { throw $message }
}

Assert-Contains $hostSource 'LocalProductOperation::dispatch' `
    'Local source requests must be handled by the platform adapter.'
if ($hostSource.Contains('Local product catalog action dispatched through plugin slot.')) {
    throw 'Local source requests must not be dispatched through the database plugin slot.'
}
if ($hostSource.Contains('setProperty("ltzkProductCatalogRequest", request)') -and
    $hostSource.Contains('setLocalProductOperationCallback')) {
    $callbackStart = $hostSource.IndexOf('setLocalProductOperationCallback(')
    $callbackEnd = $hostSource.IndexOf('m_workspaceSideTabs->addTab', $callbackStart)
    if ($callbackStart -ge 0 -and $callbackEnd -gt $callbackStart) {
        $localCallback = $hostSource.Substring($callbackStart, $callbackEnd - $callbackStart)
        if ($localCallback.Contains('ltzkProductCatalogRequest')) {
            throw 'The local callback must not create a database product catalog request.'
        }
    }
}
Assert-Contains $managerHeader 'invokeProductCatalogAction(' `
    'The platform plugin manager legacy bridge must remain source-compatible.'
Assert-Contains $managerSource 'QMetaObject::invokeMethod' `
    'The platform plugin manager legacy bridge must remain source-compatible.'
Assert-Contains $managerSource 'DataCatalogOperation' `
    'The plugin manager legacy bridge must remain source-compatible.'

$pluginChanges = rg -n 'localSources|LocalProductOperation|localProductsFromRequest' `
    (Join-Path $root 'map_product_manager_tools') `
    --glob '!**/GeneratedFiles/**' --glob '!**/node_modules/**' 2>$null
if ($LASTEXITCODE -eq 0 -and $pluginChanges) {
    throw "Product-manager plugin source must remain unchanged:`n$pluginChanges"
}

Write-Output 'Local product dispatch fallback verification passed.'
