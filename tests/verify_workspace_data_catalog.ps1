$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$catalogHeader = Join-Path $root 'DataCatalogWidget.h'
$catalogSource = Join-Path $root 'DataCatalogWidget.cpp'
$ltzkHeader = Join-Path $root 'LTZK.h'
$ltzkSource = Join-Path $root 'LTZK.cpp'
$interfaceSource = Join-Path $root 'LTZKQgisInterface.cpp'
$projectFile = Join-Path $root 'LTZK.vcxproj'
$filterFile = Join-Path $root 'LTZK.vcxproj.filters'
$qrcFile = Join-Path $root 'LTZK.qrc'
$dataClipVerification = Join-Path $PSScriptRoot 'verify_data_clip_context_action.ps1'

$databaseDataLabel = -join [char[]]@(0x6570, 0x636E, 0x5E93, 0x6570, 0x636E)
$localDataLabel = -join [char[]]@(0x672C, 0x5730, 0x6570, 0x636E)
$connectFolderLabel = -join [char[]]@(0x8FDE, 0x63A5, 0x6587, 0x4EF6, 0x5939)
$disconnectLabel = -join [char[]]@(0x65AD, 0x5F00, 0x8FDE, 0x63A5)
$refreshLabel = -join [char[]]@(0x5237, 0x65B0)
$dataListLabel = -join [char[]]@(0x6570, 0x636E, 0x5217, 0x8868)
$layerManagementLabel = -join [char[]]@(0x56FE, 0x5C42, 0x7BA1, 0x7406)
$workspaceSidebarTooltip = -join [char[]]@(0x9690, 0x85CF, 0x2F, 0x5C55, 0x5F00, 0x5DE5, 0x4F5C, 0x533A, 0x4FA7, 0x8FB9, 0x680F)
$obsoleteLayerManagerTooltip = -join [char[]]@(0x9690, 0x85CF, 0x2F, 0x5C55, 0x5F00, 0x56FE, 0x5C42, 0x7BA1, 0x7406, 0x5668)

foreach ($path in @($catalogHeader, $catalogSource)) {
    if (-not (Test-Path $path)) {
        throw "Missing data catalog component: $path"
    }
}
if (-not (Test-Path -LiteralPath $qrcFile)) {
    throw "Missing Qt resource collection: $qrcFile"
}
if (-not (Test-Path -LiteralPath $dataClipVerification)) {
    throw "Missing data clip context-action verification: $dataClipVerification"
}

$headerText = Get-Content -Raw -Encoding UTF8 $catalogHeader
$catalogText = Get-Content -Raw -Encoding UTF8 $catalogSource
$ltzkHeaderText = Get-Content -Raw -Encoding Default $ltzkHeader
$ltzkText = Get-Content -Raw -Encoding Default $ltzkSource
$interfaceText = Get-Content -Raw -Encoding Default $interfaceSource
$projectText = Get-Content -Raw -Encoding UTF8 $projectFile
$filterText = Get-Content -Raw -Encoding UTF8 $filterFile
$qrcText = Get-Content -Raw -Encoding UTF8 $qrcFile

foreach ($required in @(
    'class DataCatalogWidget',
    'QgsBrowserGuiModel* browserModel() const;',
    'void refreshDatabaseData();',
    'void setLoadCallback'
)) {
    if (-not $headerText.Contains($required)) {
        throw "Data catalog public contract missing: $required"
    }
}

$refreshStart = $catalogText.IndexOf('void DataCatalogWidget::refreshDatabaseData()')
$refreshEnd = $catalogText.IndexOf('void DataCatalogWidget::', $refreshStart + 1)
if ($refreshStart -lt 0) {
    throw 'Database catalog refresh boundary was not found.'
}
if ($refreshEnd -lt 0) {
    $refreshEnd = $catalogText.Length
}
$refreshText = $catalogText.Substring($refreshStart, $refreshEnd - $refreshStart)
$loadStart = $catalogText.IndexOf('void DataCatalogWidget::loadDatabaseData()')
$loadEnd = $catalogText.IndexOf('void DataCatalogWidget::', $loadStart + 1)
if ($loadStart -lt 0) {
    throw 'Database catalog loading boundary was not found.'
}
if ($loadEnd -lt 0) {
    $loadEnd = $catalogText.Length
}
$loadText = $catalogText.Substring($loadStart, $loadEnd - $loadStart)
$refreshFlowText = $refreshText + $loadText
$refreshGuard = 'if (!gCurrentUserSession.isLoggedIn)'
$refreshGuardIndex = $refreshFlowText.IndexOf($refreshGuard)
$connectionStateIndex = $refreshFlowText.IndexOf('m_databaseConnectionName')
if ($refreshGuardIndex -lt 0 -or $connectionStateIndex -lt 0 -or $refreshGuardIndex -gt $connectionStateIndex) {
    throw 'Database catalog refresh must reject unauthenticated requests before using the synchronized PostGIS connection.'
}
if ($catalogText.Contains('ensureConnectedAndInitialized') -or
    $catalogText.Contains('LTZKAuthPostgisConnection')) {
    throw 'Database catalog must not reuse the automatic login connection.'
}

foreach ($required in @(
    'if (layerType == QStringLiteral("table"))',
    'table->setEnabled(false);',
    'item->data(DatabaseLayerTypeRole).toString() == QStringLiteral("table")'
)) {
    if (-not $catalogText.Contains($required)) {
        throw "Ordinary database tables must remain visible but not be treated as loadable spatial layers: $required"
    }
}

foreach ($required in @(
    'showDatabaseContextMenu',
    'showLocalContextMenu',
    'Qt::CustomContextMenu',
    'using DataImportCallback = std::function<void(int)>;',
    'void setDataImportCallback(DataImportCallback callback);',
    'void setProductOperationCallback(ProductOperationCallback callback);',
    'void importContextItem(const CatalogContext& context);',
    'QAction* importData = menu.addAction(QStringLiteral("导入数据"));',
    'm_dataImportCallback',
    'importContextItem(context)',
    'QFileDialog::getOpenFileNames',
    'QFile::copy',
    'copyLocalImportDataset',
    'uniqueLocalImportPath',
    'QgsDirectoryItem* targetDirectory',
    'm_browserModel->refresh(targetIndex)',
    '添加到地图',
    '刷新',
    '数据裁剪',
    '基本信息',
    '导出数据',
    '批量导出数据',
    '重命名',
    'QgsVectorFileWriter::writeAsVectorFormatV3',
    'writeRaster(',
    'renameFavorite(',
    'm_databaseDisplayAliases'
)) {
    if (-not $catalogText.Contains($required) -and -not $headerText.Contains($required)) {
        throw "Data catalog context-menu feature is missing: $required"
    }
}

foreach ($required in @(
    'setDataImportCallback',
    'executeConfiguredPluginFeature',
    'QStringLiteral("map_product_manager_tools")',
    'QStringLiteral("map_generalization_tools")',
    'QStringLiteral("数据导入")'
)) {
    if (-not $ltzkText.Contains($required)) {
        throw "Database catalog import must delegate to the configured plugin action: $required"
    }
}

$importMethodStart = $catalogText.IndexOf('void DataCatalogWidget::importContextItem(')
$importMethodEnd = $catalogText.IndexOf(
    'void DataCatalogWidget::',
    $importMethodStart + 'void DataCatalogWidget::importContextItem('.Length)
if ($importMethodStart -lt 0 -or $importMethodEnd -lt $importMethodStart) {
    throw 'Data catalog import dispatch boundary was not found.'
}
$importMethodText = $catalogText.Substring($importMethodStart, $importMethodEnd - $importMethodStart)
if ($importMethodText.Contains('QSqlQuery') -or
    $importMethodText.Contains('ALTER TABLE') -or
    $importMethodText.Contains('CREATE TABLE') -or
    $importMethodText.Contains('INSERT INTO')) {
    throw 'Database catalog import must not implement database writes.'
}

foreach ($forbidden in @(
    '批量导入',
    'QDir::rename',
    'QFile::rename',
    'ALTER TABLE'
)) {
    if ($catalogText.Contains($forbidden)) {
        throw "Data catalog context menu contains a forbidden destructive operation: $forbidden"
    }
}

$catalogQueryStart = $catalogText.IndexOf('QString databaseCatalogQuery()')
$catalogQueryEnd = $catalogText.IndexOf('QString databaseItemSuffix', $catalogQueryStart)
if ($catalogQueryStart -lt 0 -or $catalogQueryEnd -lt $catalogQueryStart) {
    throw 'Database catalog base-query boundary was not found.'
}
$catalogQueryText = $catalogText.Substring($catalogQueryStart, $catalogQueryEnd - $catalogQueryStart)
if ($catalogQueryText -match 'FROM\s+(geometry_columns|raster_columns)') {
    throw 'The base catalog query must not directly reference optional PostGIS metadata views.'
}

foreach ($required in @(
    'databaseMetadataRelationExists',
    "to_regclass(:relation_name)",
    'databaseVectorMetadataQuery',
    'databaseRasterMetadataQuery',
    'QStringLiteral("geometry_columns")',
    'QStringLiteral("raster_columns")',
    'metadataWarnings'
)) {
    if (-not $catalogText.Contains($required)) {
        throw "Optional PostGIS metadata fallback is missing: $required"
    }
}

foreach ($required in @(
    'if (query.lastError().isValid())',
    'if (metadata.lastError().isValid())',
    'databaseItemSuffix(layerType, geometryType, srid, geometryColumn)',
    ' · %1',
    '栅格 · %1'
)) {
    if (-not $catalogText.Contains($required)) {
        throw "Database catalog diagnostics or geometry-column labeling is missing: $required"
    }
}

foreach ($required in @(
    'QgsBrowserGuiModel',
    'QgsBrowserProxyModel',
    'QgsBrowserTreeView',
    'QgsDirectoryItem',
    'addFavoriteDirectory',
    'removeFavorite',
    'information_schema.tables',
    'geometry_columns',
    'raster_columns',
    'r_raster_column',
    'user_permissions',
    'spatial_ref_sys',
    'geography_columns',
    'raster_overviews',
    'QgsMimeDataUtils::Uri',
    'localCatalogStatusLabel',
    'showFavoritesInTree',
    $databaseDataLabel,
    $localDataLabel,
    $connectFolderLabel,
    $disconnectLabel,
    $refreshLabel
)) {
    if (-not $catalogText.Contains($required)) {
        throw "Data catalog implementation missing: $required"
    }
}

$favoritesStart = $catalogText.IndexOf('QModelIndex DataCatalogWidget::favoritesRootIndex()')
$favoritesEnd = $catalogText.IndexOf('void DataCatalogWidget::connectLocalFolder()', $favoritesStart)
if ($favoritesStart -lt 0 -or $favoritesEnd -lt $favoritesStart) {
    throw 'Local favorites-root lookup boundary was not found.'
}
$favoritesText = $catalogText.Substring($favoritesStart, $favoritesEnd - $favoritesStart)
if (-not $favoritesText.Contains('findPath(QStringLiteral("favorites:"))')) {
    throw 'Local catalog must resolve the QGIS Favorites root through its canonical item path.'
}

$localViewStart = $catalogText.IndexOf('void DataCatalogWidget::showFavoritesInTree()')
$localViewEnd = $catalogText.IndexOf('void DataCatalogWidget::connectLocalFolder()', $localViewStart)
if ($localViewStart -lt 0 -or $localViewEnd -lt $localViewStart) {
    throw 'Local catalog view binding boundary was not found.'
}
$localRefreshText = $catalogText.Substring($localViewStart, $localViewEnd - $localViewStart)
foreach ($required in @(
    'm_localProxyModel->mapFromSource(favoritesIndex)',
    'm_localTree->setRootIndex(favoritesViewIndex);',
    'm_localTree->collapseAll();',
    'm_localTree->expand(favoritesViewIndex);',
    'm_browserModel->fetchMore(favoritesIndex);'
)) {
    if (-not $localRefreshText.Contains($required)) {
        throw "Local catalog refresh must rebind and load the Favorites root after a folder is selected: $required"
    }
}

$connectStart = $catalogText.IndexOf('void DataCatalogWidget::connectLocalFolder()')
$disconnectStart = $catalogText.IndexOf('void DataCatalogWidget::disconnectSelectedLocalFolder()', $connectStart)
if ($connectStart -lt 0 -or $disconnectStart -le $connectStart) {
    throw 'Local folder connection boundary was not found.'
}
$connectText = $catalogText.Substring($connectStart, $disconnectStart - $connectStart)
if ($connectText.Contains('m_localTree->expandPath(path)')) {
    throw 'Newly connected local folders must remain collapsed by default.'
}

foreach ($required in @(
    '&QgsBrowserModel::stateChanged',
    'QTimer::singleShot(0, this, &DataCatalogWidget::showFavoritesInTree)',
    'm_localProxyModel->mapToSource(index)',
    '已连接 %1 个本地数据文件夹',
    '正在连接本地数据文件夹'
)) {
    if (-not $catalogText.Contains($required)) {
        throw "Local catalog must recover from asynchronous Favorites population: $required"
    }
}

$buildUiStart = $catalogText.IndexOf('void DataCatalogWidget::buildUi()')
$buildUiEnd = $catalogText.IndexOf('void DataCatalogWidget::initializeLocalBrowser()', $buildUiStart)
if ($buildUiStart -lt 0 -or $buildUiEnd -lt $buildUiStart) {
    throw 'Data catalog UI construction boundary was not found.'
}
$buildUiText = $catalogText.Substring($buildUiStart, $buildUiEnd - $buildUiStart)

if (-not $catalogText.Contains('QToolButton** titleButton') -or
    -not $catalogText.Contains('catalogSectionTitle')) {
    throw 'Data catalog section-header helper does not expose a clickable title button.'
}
foreach ($required in @(
    'QLabel* titleLabel = new QLabel(text, title);',
    'titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);',
    'titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);',
    'title->setText(QString());'
)) {
    if (-not $catalogText.Contains($required)) {
        throw "Data catalog section titles must render through a left-aligned label: $required"
    }
}

foreach ($required in @(
    'databaseCatalogTitleButton',
    'localCatalogTitleButton',
    'databaseCatalogContent',
    'localCatalogContent',
    'databaseContent->setVisible(true);',
    'localContent->setVisible(true);',
    'databaseTitleButton, &QToolButton::clicked',
    'localTitleButton, &QToolButton::clicked',
    'databaseContent->setVisible(showDatabase)',
    'localContent->setVisible(!showDatabase)',
    'sectionSplitter->setSizes(targetSizes)'
)) {
    if (-not $buildUiText.Contains($required)) {
        throw "Data catalog source-switch implementation is missing: $required"
    }
}

foreach ($required in @(
    'QFrame* databaseHeader = createSectionHeader(',
    'QFrame* localHeader = createSectionHeader(',
    'databaseHeader->sizeHint().height()',
    'localHeader->sizeHint().height()'
)) {
    if (-not $buildUiText.Contains($required)) {
        throw "Data catalog collapsed source height must be based on the header itself: $required"
    }
}

foreach ($required in @(
    'button->setIcon(QIcon(iconPath));',
    'button->setIconSize(QSize(18, 18));',
    'button->setToolButtonStyle(Qt::ToolButtonIconOnly);',
    'button->setFixedSize(28, 28);',
    'button->setAccessibleName(accessibleName);',
    'button->setText(QString());',
    ':/LTZK/Resources/mActionRefresh.svg',
    ':/LTZK/Resources/mIconFolderLink.svg',
    ':/LTZK/Resources/mActionUnlink.svg'
)) {
    if (-not $catalogText.Contains($required)) {
        throw "Data catalog icon button contract is missing: $required"
    }
}

if ($catalogText.Contains('compactCatalogButton(') -or
    $catalogText.Contains('fontMetrics().horizontalAdvance(button->text())')) {
    throw 'Data catalog icon buttons must not use text-width sizing.'
}

foreach ($resource in @(
    'Resources/mActionRefresh.svg',
    'Resources/mIconFolderLink.svg',
    'Resources/mActionUnlink.svg'
)) {
    if (-not $qrcText.Contains($resource)) {
        throw "Catalog icon resource is missing from LTZK.qrc: $resource"
    }
}

if ($buildUiText.Contains('parentWidget()->minimumSizeHint().height()')) {
    throw 'Data catalog collapsed source height must not use the entire section minimum height.'
}

if ($buildUiText.Contains('databaseSection->setVisible(false)') -or
    $buildUiText.Contains('localSection->setVisible(false)')) {
    throw 'Source switching must retain both section headers instead of hiding whole sections.'
}

foreach ($theme in @(
    'ltzk_sci_fi.qss',
    'ltzk_black.qss',
    'ltzk_white.qss',
    'ltzk_field_notes.qss',
    'ltzk_steel_warm_white.qss'
)) {
    $themePath = Join-Path $root (Join-Path 'Resources\styles' $theme)
    $themeText = Get-Content -Raw -Encoding UTF8 $themePath
    if (-not $themeText.Contains('QToolButton[catalogSectionTitle="true"]')) {
        throw "$theme is missing clickable data catalog title styling."
    }
}

if (-not $ltzkText.Contains(('layerManagerCollapseButton->setToolTip(QStringLiteral("' + $workspaceSidebarTooltip + '"))'))) {
    throw 'The workspace collapse tooltip must describe the whole side bar, not only the layer manager.'
}
if ($ltzkText.Contains(('layerManagerCollapseButton->setToolTip(QStringLiteral("' + $obsoleteLayerManagerTooltip + '"))'))) {
    throw 'The obsolete layer-manager-only collapse tooltip must not remain.'
}

foreach ($required in @(
    'DataCatalogWidget* m_dataCatalogWidget = nullptr;',
    'QTabWidget* m_workspaceSideTabs = nullptr;',
    'QgsBrowserGuiModel* browserModel() const;',
    'void loadCatalogDataSource(const QgsMimeDataUtils::Uri& uri);'
)) {
    if (-not $ltzkHeaderText.Contains($required)) {
        throw "Workspace catalog declaration missing: $required"
    }
}

foreach ($required in @(
    'm_workspaceSideTabs->setTabPosition(QTabWidget::South)',
    ('m_workspaceSideTabs->addTab(m_dataCatalogWidget, QStringLiteral("' + $dataListLabel + '"))'),
    ('m_workspaceSideTabs->addTab(m_layerTreePanelWidget, QStringLiteral("' + $layerManagementLabel + '"))'),
    'm_workspaceSideTabs->setCurrentWidget(m_layerTreePanelWidget)',
    'void LTZK::loadCatalogDataSource',
    'uri.vectorLayer',
    'uri.rasterLayer',
    'm_dataCatalogWidget->refreshDatabaseData()'
)) {
    if (-not $ltzkText.Contains($required)) {
        throw "Workspace catalog integration missing: $required"
    }
}

$dataCatalogVisibleModuleTitles = @('地图成果管理', '地图智能制图')
$dataCatalogHiddenModuleTitles = @('地图成果质检', '地图服务管理')
foreach ($moduleTitle in @($dataCatalogVisibleModuleTitles + $dataCatalogHiddenModuleTitles)) {
    if (-not $ltzkText.Contains($moduleTitle)) {
        throw "Workspace module title is missing from the module activation flow: $moduleTitle"
    }
}
if (-not $ltzkHeaderText.Contains('void updateDataCatalogVisibility(const QString& moduleTitle);')) {
    throw 'Workspace catalog visibility update declaration is missing.'
}

$visibilityHelperStart = $ltzkText.IndexOf('bool isDataCatalogVisibleModuleTitle(const QString& moduleTitle)')
$visibilityHelperEnd = $ltzkText.IndexOf('bool isResultModuleTitle', $visibilityHelperStart)
if ($visibilityHelperStart -lt 0 -or $visibilityHelperEnd -lt $visibilityHelperStart) {
    throw 'Data catalog module visibility helper boundary was not found.'
}
$visibilityHelperText = $ltzkText.Substring($visibilityHelperStart, $visibilityHelperEnd - $visibilityHelperStart)
foreach ($moduleTitle in $dataCatalogVisibleModuleTitles) {
    if (-not $visibilityHelperText.Contains(('QStringLiteral("' + $moduleTitle + '")'))) {
        throw "Data catalog visibility helper must allow: $moduleTitle"
    }
}
foreach ($moduleTitle in $dataCatalogHiddenModuleTitles) {
    if ($visibilityHelperText.Contains(('QStringLiteral("' + $moduleTitle + '")'))) {
        throw "Data catalog visibility helper must not allow: $moduleTitle"
    }
}

$visibilityMethodStart = $ltzkText.IndexOf('void LTZK::updateDataCatalogVisibility(const QString& moduleTitle)')
$visibilityMethodEnd = $ltzkText.IndexOf('void LTZK::activateBusinessModule', $visibilityMethodStart)
if ($visibilityMethodStart -lt 0 -or $visibilityMethodEnd -lt $visibilityMethodStart) {
    throw 'Data catalog visibility update method boundary was not found.'
}
$visibilityMethodText = $ltzkText.Substring($visibilityMethodStart, $visibilityMethodEnd - $visibilityMethodStart)
foreach ($required in @(
    'm_workspaceSideTabs->indexOf(m_dataCatalogWidget)',
    'm_workspaceSideTabs->setTabVisible(dataCatalogTabIndex, visible)',
    'm_workspaceSideTabs->setTabPosition(visible ? QTabWidget::South : QTabWidget::North);',
    'm_workspaceSideTabs->currentWidget() == m_dataCatalogWidget',
    'm_workspaceSideTabs->setCurrentWidget(m_layerTreePanelWidget)',
    'm_workspaceSideTabs->setCurrentWidget(m_layerTreeView)'
)) {
    if (-not $visibilityMethodText.Contains($required)) {
        throw "Data catalog visibility protection is missing: $required"
    }
}
if (-not $ltzkText.Contains('updateDataCatalogVisibility(moduleTitle);')) {
    throw 'Module activation does not update data catalog visibility.'
}
if (-not $ltzkText.Contains('updateDataCatalogVisibility(m_currentBusinessModuleTitle);')) {
    throw 'Workspace tab construction does not apply the initial hidden data catalog policy.'
}

$logoutStart = $ltzkText.IndexOf('void LTZK::logoutCurrentUser()')
$logoutEnd = $ltzkText.IndexOf('bool LTZK::ensureAuthenticatedForModule', $logoutStart)
if ($logoutStart -lt 0 -or $logoutEnd -lt $logoutStart) {
    throw 'Logout handler boundary was not found.'
}
$logoutText = $ltzkText.Substring($logoutStart, $logoutEnd - $logoutStart)
if (-not $logoutText.Contains('gCurrentUserSession.clear()') -or
    -not $logoutText.Contains('m_dataCatalogWidget->applyMenuDatabaseConnection(QVariantMap());')) {
    throw 'Logout must clear the database catalog after invalidating the user session.'
}

if (-not $interfaceText.Contains('return mApp ? mApp->browserModel() : nullptr;')) {
    throw 'QGIS interface does not expose the workspace browser model.'
}

foreach ($required in @(
    'ClCompile Include="DataCatalogWidget.cpp"',
    'ClInclude Include="DataCatalogWidget.h"'
)) {
    if (-not $projectText.Contains($required)) {
        throw "Visual Studio project entry missing: $required"
    }
    if (-not $filterText.Contains($required)) {
        throw "Visual Studio filter entry missing: $required"
    }
}

$themes = @(
    'ltzk_sci_fi.qss',
    'ltzk_black.qss',
    'ltzk_white.qss',
    'ltzk_field_notes.qss',
    'ltzk_steel_warm_white.qss'
)

foreach ($theme in $themes) {
    $themePath = Join-Path $root (Join-Path 'Resources\styles' $theme)
    $themeText = Get-Content -Raw -Encoding UTF8 $themePath
    foreach ($selector in @('#workspaceSideTabs', '#databaseCatalogTree', '#localCatalogTree', '#catalogToolbarButton')) {
        if (-not $themeText.Contains($selector)) {
            throw "$theme is missing scoped data catalog style: $selector"
        }
    }
}

Write-Host 'Workspace data catalog integration verified.'
