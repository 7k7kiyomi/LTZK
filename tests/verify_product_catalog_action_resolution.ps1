$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing platform source: $sourcePath"
}

$source = Get-Content -Raw -Encoding Default $sourcePath

$lookupStart = $source.IndexOf('QAction* findConfiguredPluginAction')
$lookupEnd = $source.IndexOf('QList<QAction*> configuredPluginActionsFromCandidates', $lookupStart)
if ($lookupStart -lt 0 -or $lookupEnd -lt 0) {
    throw 'Could not locate configured plugin action lookup.'
}
$lookupBody = $source.Substring($lookupStart, $lookupEnd - $lookupStart)

if ($lookupBody.Contains('return pluginActions.isEmpty() ? nullptr : pluginActions.first();')) {
    throw 'Plugin action lookup must not invoke the first action when the configured action is missing.'
}
if (-not $lookupBody.Contains('return nullptr;')) {
    throw 'Plugin action lookup must return nullptr when no configured action matches.'
}

$executeStart = $source.IndexOf('void LTZK::executePluginFeatureAction')
$executeEnd = $source.IndexOf('void LTZK::bindPluginFeatureActions', $executeStart)
if ($executeStart -lt 0 -or $executeEnd -lt 0) {
    throw 'Could not locate plugin feature execution body.'
}
$executeBody = $source.Substring($executeStart, $executeEnd - $executeStart)

foreach ($required in @(
    'findProductCatalogOperationAction',
    'binding.featureName == QStringLiteral("成果目录操作")',
    'collectPluginExecutableActions(menuBar(), m_pluginMenu, m_pluginToolBar)'
)) {
    if (-not $executeBody.Contains($required)) {
        throw "Product catalog action recovery is missing: $required"
    }
}

if ($executeBody -match 'findProductDataImportAction\(liveActions\)') {
    throw 'Data import must be resolved from its plugin menu, not a global action list.'
}

foreach ($required in @(
    'binding.featureName == QStringLiteral("数据导入")',
    'findChild<QMenu*>(QStringLiteral("mMapProductMenu"))',
    'QStringLiteral("mActionDataImport")',
    'pluginAction = findProductDataImportAction('
)) {
    if (-not $executeBody.Contains($required) -and -not $source.Contains($required)) {
        throw "Data import action recovery is missing: $required"
    }
}

foreach ($required in @(
    'QList<QAction*> collectPluginExecutableActions(QMenuBar* menuBar, QMenu* pluginMenu, QToolBar* toolbar)',
    'QMenuBar* menuBar',
    'for (QAction* menuAction : menuBar->actions())'
)) {
    if (-not $source.Contains($required)) {
        throw "Plugin action collection is missing: $required"
    }
}

if (-not $source.Contains('mActionDataCatalogOperation')) {
    throw 'Product catalog bridge lookup must recognize the DLL action object name.'
}

Write-Output 'Product catalog action resolution verification passed.'
