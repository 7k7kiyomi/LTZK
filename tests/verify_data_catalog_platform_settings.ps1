$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$catalogHeader = Join-Path $root 'DataCatalogWidget.h'
$catalogSource = Join-Path $root 'DataCatalogWidget.cpp'
$ltzkSource = Join-Path $root 'LTZK.cpp'
$pluginSource = Join-Path $root 'map_product_manager_tools\map_product_manager_tools.cpp'
$connectorSource = Join-Path $root 'map_product_manager_tools\database\postgis_connector.cpp'
$dbDialogSource = Join-Path $root 'map_product_manager_tools\ui_class\db_config_dialog.cpp'

foreach ($path in @(
    $catalogHeader, $catalogSource, $ltzkSource,
    $pluginSource, $connectorSource, $dbDialogSource
)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing data catalog platform component: $path"
    }
}

$catalogHeaderText = Get-Content -Raw -Encoding UTF8 $catalogHeader
$catalogText = Get-Content -Raw -Encoding UTF8 $catalogSource
$ltzkText = Get-Content -Raw -Encoding UTF8 $ltzkSource
$pluginText = Get-Content -Raw -Encoding UTF8 $pluginSource
$connectorText = Get-Content -Raw -Encoding UTF8 $connectorSource
$dbDialogText = Get-Content -Raw -Encoding UTF8 $dbDialogSource

$logoutStart = $ltzkText.IndexOf('void LTZK::logoutCurrentUser()')
$logoutEnd = $ltzkText.IndexOf('bool LTZK::ensureAuthenticatedForModule', $logoutStart)
if ($logoutStart -lt 0 -or $logoutEnd -le $logoutStart) {
    throw 'Platform logout boundary was not found.'
}
$logoutText = $ltzkText.Substring($logoutStart, $logoutEnd - $logoutStart)
if (-not $logoutText.Contains('"ltzkProductCatalogRequest", QVariant()') -or
    -not $logoutText.Contains('"ltzkProductImportTargetDirId", QVariant()')) {
    throw 'Platform logout must clear temporary plugin connection parameters.'
}

foreach ($required in @(
    'void refreshDatabaseData();',
    'void applyMenuDatabaseConnection(const QVariantMap& parameters);',
    'void applySuccessfulMenuDatabaseConnection(',
    'QVariantMap m_liveMenuConnectionParameters;',
    'QVariantMap menuDatabaseConnectionParametersFromSettings() const;',
    'QString menuDatabaseConnectionSettingsSignature(const QVariantMap& parameters) const;',
    'bool synchronizeMenuDatabaseConnection(bool forceRetry);',
    'void loadDatabaseData();'
)) {
    if (-not $catalogHeaderText.Contains($required)) {
        throw "Data catalog platform synchronization contract is missing: $required"
    }
}

foreach ($required in @(
    '#include <QSettings>',
    '#include <QCryptographicHash>',
    'QSettings settings(',
    'QStringLiteral("GarMap")',
    'QStringLiteral("MapProductManager")',
    'settings.value(QStringLiteral("db/host")',
    'settings.value(QStringLiteral("db/port")',
    'QStringLiteral("db/database")',
    'QStringLiteral("db/schema")',
    'QStringLiteral("db/user")',
    'QStringLiteral("db/password")',
    'QStringLiteral("db/savePassword")',
    'db/password',
    'db/savePassword',
    'QCryptographicHash::Sha256',
    'PlatformMenuDatabaseConnectionObserver::isConnectionDialogVisible()',
    'if (!m_liveMenuConnectionParameters.isEmpty())',
    'm_liveMenuConnectionParameters = remembered ? QVariantMap() : parameters;',
    '菜单栏上次数据库配置未保存密码，请重新连接数据库；可不勾选“记住密码”。'
)) {
    if (-not $catalogText.Contains($required)) {
        throw "Platform-side menu database settings synchronization is missing: $required"
    }
}

foreach ($required in @(
    'QTimer* m_menuDatabaseSettingsTimer = nullptr;',
    'QString m_menuDatabaseSettingsSignature;'
)) {
    if (-not $catalogHeaderText.Contains($required)) {
        throw "Platform-side settings polling state is missing: $required"
    }
}

foreach ($required in @(
    'm_menuDatabaseSettingsTimer->setInterval(500);',
    'm_menuDatabaseSettingsTimer->start();',
    'synchronizeMenuDatabaseConnection(false)',
    'synchronizeMenuDatabaseConnection(true)',
    'void DataCatalogWidget::loadDatabaseData()'
)) {
    if (-not $catalogText.Contains($required)) {
        throw "Platform-side settings synchronization behavior is missing: $required"
    }
}

$refreshStart = $catalogText.IndexOf('void DataCatalogWidget::refreshDatabaseData()')
$refreshEnd = $catalogText.IndexOf('void DataCatalogWidget::loadDatabaseData()', $refreshStart)
if ($refreshStart -lt 0 -or $refreshEnd -le $refreshStart) {
    throw 'Data catalog refresh wrapper boundary was not found.'
}
$refreshText = $catalogText.Substring($refreshStart, $refreshEnd - $refreshStart)
if (-not $refreshText.Contains('synchronizeMenuDatabaseConnection(true)') -or
    -not $refreshText.Contains('loadDatabaseData();')) {
    throw 'Manual refresh must synchronize the menu settings before loading database rows.'
}

foreach ($required in @(
    'm_menuDatabaseSettingsSignature = menuDatabaseConnectionSettingsSignature(parameters);',
    'openDatabaseConnection(parameters, &errorMessage)',
    'loadDatabaseData();',
    'm_databaseModel->removeRows(0, m_databaseModel->rowCount());',
    '请先通过菜单栏“数据库连接配置”连接数据库。'
)) {
    if (-not $catalogText.Contains($required)) {
        throw "Data catalog menu-settings connection lifecycle is missing: $required"
    }
}

foreach ($forbidden in @(
    'DataCatalogConnectionDialog.h',
    'connectDatabaseButton',
    'DataCatalogConnectionDialog dialog(this)',
    'void DataCatalogWidget::connectDatabase()'
)) {
    if ($catalogText.Contains($forbidden) -or $catalogHeaderText.Contains($forbidden)) {
        throw "Data catalog must not retain an interactive manual connection path: $forbidden"
    }
}

foreach ($forbidden in @(
    'notifyDataCatalogConnection',
    'connectionParameters() const',
    'QMetaObject::invokeMethod'
)) {
    if ($pluginText.Contains($forbidden) -or
        $connectorText.Contains($forbidden) -or
        $dbDialogText.Contains($forbidden)) {
        throw "The platform-side solution must not require plugin bridge code: $forbidden"
    }
}

if (-not $ltzkText.Contains('m_dataCatalogWidget->refreshDatabaseData();')) {
    throw 'User login must keep the data catalog refresh entry point.'
}

Write-Output 'Data catalog platform-side menu database settings verification passed.'
