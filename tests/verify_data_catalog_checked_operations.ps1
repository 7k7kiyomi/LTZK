$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$headerPath = Join-Path $root 'DataCatalogWidget.h'
$sourcePath = Join-Path $root 'DataCatalogWidget.cpp'
$headerText = Get-Content -Raw $headerPath
$sourceText = Get-Content -Raw $sourcePath

$requiredSourceTokens = @(
    'm_checkedDatabaseKeys',
    'm_checkedLocalPaths',
    'Qt::ItemIsUserCheckable',
    'Qt::CheckStateRole',
    'checkedContextsFor',
    'hasRefreshTarget',
    'addContextItemsToMap',
    'refreshContextItems',
    'batchExportContextItems',
    'collectCheckedLocalContexts',
    'item->checkState() == Qt::Checked',
    'collectCheckedCatalogIndexes(m_localProxyModel, parent)'
)
foreach ($token in $requiredSourceTokens) {
    if (-not ($headerText.Contains($token) -or $sourceText.Contains($token))) {
        throw "Missing catalog checked-operation contract: $token"
    }
}

$requiredBehaviorTokens = @(
    'const QVector<CatalogContext> operationContexts = checkedContextsFor(context);',
    '[this, context]() { showContextItemInfo(context); }',
    '[this, context]() { exportContextItem(context); }',
    'QgsBrowserProxyModel'
)
foreach ($token in $requiredBehaviorTokens) {
    if (-not $sourceText.Contains($token)) {
        throw "Missing catalog operation-scope contract: $token"
    }
}

$requiredBatchMapTokens = @(
    'const QVector<CatalogContext> checkedMapContexts =',
    'const QVector<CatalogContext> mapContexts = hasCheckedMapContexts',
    'QList<int> mapProductIds;',
    'QgsMimeDataUtils::UriList mapLocalUris;',
    'for (const CatalogContext& mapContext : mapContexts)',
    'productIdsForContext(mapContext)',
    'localProductUrisForContext(mapContext, true)',
    'appendUniqueUri(&mapLocalUris, uri)',
    'dispatchProductOperation(',
    'QStringLiteral("map"),',
    'mapProductIds,',
    'mapLocalUris);'
)
foreach ($token in $requiredBatchMapTokens) {
    if (-not $sourceText.Contains($token)) {
        throw "Missing batch add-to-map contract: $token"
    }
}

$requiredLocalBatchLookupTokens = @(
    'collectCheckedLocalContexts(m_localTree->rootIndex(), &contexts)',
    'collectCheckedCatalogIndexes(m_localProxyModel, parent)',
    'm_localProxyModel->dataItem(viewIndex)',
    'm_checkedLocalPaths.contains(context.localItem->path())',
    'if (!contexts.isEmpty())'
)
foreach ($token in $requiredLocalBatchLookupTokens) {
    if (-not $sourceText.Contains($token)) {
        throw "Missing checked-view local batch lookup contract: $token"
    }
}

$requiredProductSelectionTokens = @(
    'synchronizeProductCheckCache',
    '[this, context, allProductIds]()',
    'synchronizeProductCheckCache(context.databaseItem, allProductIds, Qt::Checked);',
    'synchronizeProductCheckCache(context.databaseItem, allProductIds, Qt::Unchecked);',
    'product-check:%1\\x1f%2'
)
foreach ($token in $requiredProductSelectionTokens) {
    if (-not $sourceText.Contains($token)) {
        throw "Missing product selection cache synchronization contract: $token"
    }
}

$requiredPlatformMapBatchTokens = @(
    'const QVector<CatalogContext> checkedMapContexts =',
    'const bool hasCheckedMapContexts = !checkedMapContexts.isEmpty();',
    'if (hasCheckedMapContexts)',
    'void DataCatalogWidget::addProductIdsToMap(',
    'addProductIdsToMap(productIds);',
    'context.source == CatalogSource::Database &&',
    'operation == QStringLiteral("map")',
    'hasMapData && static_cast<bool>(m_loadCallback)'
)
foreach ($token in $requiredPlatformMapBatchTokens) {
    if (-not $sourceText.Contains($token)) {
        throw "Missing platform-side batch map contract: $token"
    }
}

$themeFiles = @(
    'Resources/styles/ltzk_sci_fi.qss',
    'Resources/styles/ltzk_black.qss',
    'Resources/styles/ltzk_white.qss',
    'Resources/styles/ltzk_field_notes.qss',
    'Resources/styles/ltzk_steel_warm_white.qss'
)
$qrcText = Get-Content -Raw (Join-Path $root 'LTZK.qrc')
if (-not $qrcText.Contains('Resources/tree_checkbox_checked.svg')) {
    throw 'Checked catalog indicator SVG is missing from LTZK.qrc.'
}
foreach ($relativePath in $themeFiles) {
    $themePath = Join-Path $root $relativePath
    $themeText = Get-Content -Raw $themePath
    if ($themeText -notmatch 'QTreeView::indicator:checked') {
        throw "Theme is missing checked catalog indicator styling: $relativePath"
    }
    if ($themeText -notmatch 'QTreeView::indicator:unchecked') {
        throw "Theme is missing unchecked catalog indicator styling: $relativePath"
    }
    if ($themeText -notmatch 'tree_checkbox_checked\.svg') {
        throw "Theme is missing visible checked catalog indicator icon: $relativePath"
    }
}

Write-Output 'Data catalog checked-operation contracts passed.'
