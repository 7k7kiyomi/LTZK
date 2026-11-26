# 数据列表复用菜单栏数据库连接 Implementation Plan

> **已废弃：** 本计划对应的插件桥接方案已撤回，不应继续执行。当前平台侧实现计划见 `2026-09-22-data-catalog-platform-settings.md`。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (recommended) or superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 隐藏数据列表自己的数据库连接入口，让菜单栏数据库连接成功后自动驱动数据列表刷新、重连切换和断开清空。

**Architecture:** 保留 `DataCatalogWidget` 现有 QPSQL 目录查询实现，但将连接参数来源改为插件菜单栏。`PostgisConnector` 在插件内部导出当前连接参数，`CMapProductManagerPlugin` 通过 Qt 元对象调用主窗口桥接槽，`LTZK` 再把参数转发给数据列表。空参数表示断开，非空参数表示替换目录查询连接并刷新。

**Tech Stack:** C++17、Qt 5.15 Widgets/Meta Object、Qt SQL QPSQL、libpq、QGIS C++ 插件、PowerShell 静态回归脚本、MSBuild x64 Release。

---

## 文件结构与职责

- Modify: `tests/verify_data_catalog_manual_connection.ps1` — 将旧的手动连接断言改为菜单栏连接同步、桥接和清空行为断言，保持现有验证入口可用。
- Modify: `DataCatalogWidget.h` — 移除数据列表内部连接入口声明，增加菜单栏连接参数同步接口，并把底层连接辅助函数改为接收 `QVariantMap`。
- Modify: `DataCatalogWidget.cpp` — 移除手动连接按钮/对话框调用；实现空参数清空、非空参数重连刷新和新的提示文本。
- Modify: `LTZK.h` — 声明主窗口 Qt 元对象桥接槽并引入 `QVariantMap`。
- Modify: `LTZK.cpp` — 将插件通知转发到 `DataCatalogWidget`。
- Modify: `map_product_manager_tools/database/postgis_connector.h` — 声明读取当前连接参数的插件内部接口。
- Modify: `map_product_manager_tools/database/postgis_connector.cpp` — 从当前 `PGconn` 组装连接参数并返回 Schema。
- Modify: `map_product_manager_tools/map_product_manager_tools.h` — 声明通知主程序数据列表的私有辅助函数。
- Modify: `map_product_manager_tools/map_product_manager_tools.cpp` — 监听断开、在自动连接及菜单连接成功后发送最终连接参数。
- Modify: `map_product_manager_tools/ui_class/db_config_dialog.cpp` — 连接时同步记录最终 Schema，Schema 初始化失败时断开，确保失败状态能通知主程序。
- No change: `DataCatalogConnectionDialog.h/.cpp` — 保留文件和工程条目，但从数据列表 UI 移除可达路径，避免本次需求扩大到删除旧组件。

### Task 1: 更新菜单栏连接同步回归检查（先写失败测试）

**Files:**
- Modify: `tests/verify_data_catalog_manual_connection.ps1`

- [ ] **Step 1: Replace manual-connection assertions with the new contract**

保留脚本的 UTF-8、路径检查和 `$ErrorActionPreference = 'Stop'`，将核心断言改为下面的内容。脚本仍使用原文件名，保证已有调用入口不变：

```powershell
$catalogHeader = Join-Path $root 'DataCatalogWidget.h'
$catalogSource = Join-Path $root 'DataCatalogWidget.cpp'
$ltzkHeader = Join-Path $root 'LTZK.h'
$ltzkSource = Join-Path $root 'LTZK.cpp'
$pluginHeader = Join-Path $root 'map_product_manager_tools\map_product_manager_tools.h'
$pluginSource = Join-Path $root 'map_product_manager_tools\map_product_manager_tools.cpp'
$connectorHeader = Join-Path $root 'map_product_manager_tools\database\postgis_connector.h'
$connectorSource = Join-Path $root 'map_product_manager_tools\database\postgis_connector.cpp'
$dbDialogSource = Join-Path $root 'map_product_manager_tools\ui_class\db_config_dialog.cpp'

foreach ($path in @(
    $catalogHeader, $catalogSource, $ltzkHeader, $ltzkSource,
    $pluginHeader, $pluginSource, $connectorHeader, $connectorSource,
    $dbDialogSource
)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing menu database connection component: $path"
    }
}

$catalogHeaderText = Get-Content -Raw -Encoding UTF8 $catalogHeader
$catalogText = Get-Content -Raw -Encoding UTF8 $catalogSource
$ltzkHeaderText = Get-Content -Raw -Encoding UTF8 $ltzkHeader
$ltzkText = Get-Content -Raw -Encoding UTF8 $ltzkSource
$pluginHeaderText = Get-Content -Raw -Encoding UTF8 $pluginHeader
$pluginText = Get-Content -Raw -Encoding UTF8 $pluginSource
$connectorHeaderText = Get-Content -Raw -Encoding UTF8 $connectorHeader
$connectorText = Get-Content -Raw -Encoding UTF8 $connectorSource
$dbDialogText = Get-Content -Raw -Encoding UTF8 $dbDialogSource

foreach ($required in @(
    'void applyMenuDatabaseConnection(const QVariantMap& parameters);',
    'void refreshDatabaseData();'
)) {
    if (-not $catalogHeaderText.Contains($required)) {
        throw "Data catalog menu-connection contract is missing: $required"
    }
}

foreach ($required in @(
    'void DataCatalogWidget::applyMenuDatabaseConnection(',
    'm_databaseModel->removeRows(0, m_databaseModel->rowCount());',
    '请先通过菜单栏“数据库连接配置”连接数据库',
    '菜单栏数据库连接同步失败',
    'QList<QToolButton*>() << refreshDatabaseButton'
)) {
    if (-not $catalogText.Contains($required)) {
        throw "Data catalog menu-connection behavior is missing: $required"
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

foreach ($required in @(
    'void applyMenuDatabaseConnection(const QVariantMap& parameters);',
    '#include <QVariantMap>'
)) {
    if (-not $ltzkHeaderText.Contains($required)) {
        throw "Main-window database bridge contract is missing: $required"
    }
}
if (-not $ltzkText.Contains('m_dataCatalogWidget->applyMenuDatabaseConnection(parameters);')) {
    throw 'Main-window database bridge must forward parameters to DataCatalogWidget.'
}

foreach ($required in @(
    'QVariantMap connectionParameters() const;',
    'void notifyDataCatalogConnection();',
    'connectionStatusChanged'
)) {
    if (-not $connectorHeaderText.Contains($required) -and
        -not $pluginHeaderText.Contains($required) -and
        -not $pluginText.Contains($required)) {
        throw "Plugin database bridge contract is missing: $required"
    }
}

foreach ($required in @(
    'QVariantMap PostgisConnector::connectionParameters() const',
    'QMetaObject::invokeMethod',
    'applyMenuDatabaseConnection',
    'notifyDataCatalogConnection()',
    'if (!connected)'
)) {
    if (-not $connectorText.Contains($required) -and -not $pluginText.Contains($required)) {
        throw "Plugin database bridge behavior is missing: $required"
    }
}

foreach ($required in @(
    'db->setSearchPath(schemaName);',
    'db->disconnect();',
    'Schema初始化失败'
)) {
    if (-not $dbDialogText.Contains($required)) {
        throw "Database configuration failure handling is missing: $required"
    }
}

Write-Output 'Data catalog menu database connection verification passed.'
```

- [ ] **Step 2: Run the updated test before implementation**

Run:

```powershell
.\tests\verify_data_catalog_manual_connection.ps1
```

Expected: non-zero exit code with a missing-contract message such as `Data catalog menu-connection contract is missing: void applyMenuDatabaseConnection(...)`. This confirms the test detects the requested behavior before implementation.

### Task 2: Add the main-program data-list synchronization entry

**Files:**
- Modify: `DataCatalogWidget.h`
- Modify: `DataCatalogWidget.cpp`
- Modify: `LTZK.h`
- Modify: `LTZK.cpp`

- [ ] **Step 1: Change the DataCatalogWidget public/private contract**

In `DataCatalogWidget.h`, keep the existing public callbacks and add:

```cpp
void applyMenuDatabaseConnection(const QVariantMap& parameters);
```

Remove `void connectDatabase();` and change the private helper declaration to:

```cpp
bool openDatabaseConnection(
    const QVariantMap& parameters,
    QString* errorMessage);
```

Remove the forward declaration of `DataCatalogConnectionParameters`; `QVariantMap` is already included by the header.

- [ ] **Step 2: Remove the independent data-list connection dialog path**

In `DataCatalogWidget.cpp`:

- Remove `#include "DataCatalogConnectionDialog.h"`.
- Remove the complete `DataCatalogWidget::connectDatabase()` implementation.
- Change `openDatabaseConnection` to read `host`, `port`, `database`, `user`, `password`, and `schema` from the passed map; retain the existing connection name prefix, QPSQL setup, schema search-path SQL, error propagation, and old-connection cleanup.

- [ ] **Step 3: Implement parameter synchronization with clear-before-replace semantics**

Add this method after `closeDatabaseConnection()`:

```cpp
void DataCatalogWidget::applyMenuDatabaseConnection(const QVariantMap& parameters)
{
    closeDatabaseConnection();
    if (m_databaseModel)
    {
        m_databaseModel->removeRows(0, m_databaseModel->rowCount());
    }

    if (parameters.isEmpty())
    {
        showDatabaseMessage(
            QStringLiteral("请先通过菜单栏“数据库连接配置”连接数据库。"),
            false);
        return;
    }

    QString errorMessage;
    if (!openDatabaseConnection(parameters, &errorMessage))
    {
        showDatabaseMessage(
            QStringLiteral("菜单栏数据库连接同步失败：%1")
                .arg(errorMessage.isEmpty()
                    ? QStringLiteral("无法打开数据库查询连接。")
                    : errorMessage),
            true);
        return;
    }

    refreshDatabaseData();
}
```

The method must leave no old tree visible after a replacement or QPSQL open failure.

- [ ] **Step 4: Update refresh messages and hide the manual button**

In `refreshDatabaseData()` replace the two manual-connection messages with menu-connection wording:

```cpp
showDatabaseMessage(QStringLiteral("请先通过菜单栏“数据库连接配置”连接数据库。"), false);
```

and:

```cpp
showDatabaseMessage(
    QStringLiteral("菜单栏数据库连接尚未打开，请通过“数据库连接配置”重新连接。"),
    true);
```

In `buildUi()` remove `connectDatabaseButton`, pass only `refreshDatabaseButton` to the database section header, and remove the `connect(connectDatabaseButton, ...)` binding. The database title bar must still contain the refresh button.

- [ ] **Step 5: Add the host bridge slot**

In `LTZK.h`, add `#include <QVariantMap>` and declare this public slot near the other host/plugin slots:

```cpp
void applyMenuDatabaseConnection(const QVariantMap& parameters);
```

In `LTZK.cpp`, add:

```cpp
void LTZK::applyMenuDatabaseConnection(const QVariantMap& parameters)
{
    if (m_dataCatalogWidget)
    {
        m_dataCatalogWidget->applyMenuDatabaseConnection(parameters);
    }
}
```

Place it near the existing catalog-facing methods so the bridge has a single host-level entry point.

### Task 3: Export plugin connection parameters and notify the host

**Files:**
- Modify: `map_product_manager_tools/database/postgis_connector.h`
- Modify: `map_product_manager_tools/database/postgis_connector.cpp`
- Modify: `map_product_manager_tools/map_product_manager_tools.h`
- Modify: `map_product_manager_tools/map_product_manager_tools.cpp`

- [ ] **Step 1: Add a connection-parameter accessor to PostgisConnector**

In `postgis_connector.h`, add after `lastError()`:

```cpp
QVariantMap connectionParameters() const;
```

Implement it in `postgis_connector.cpp`:

```cpp
QVariantMap PostgisConnector::connectionParameters() const
{
    QVariantMap parameters;
    if (!m_conn || !isConnected())
    {
        return parameters;
    }

    const auto valueOrEmpty = [](const char* value)
    {
        return value ? QString::fromUtf8(value) : QString();
    };

    parameters.insert(QStringLiteral("host"), valueOrEmpty(PQhost(m_conn)));
    parameters.insert(QStringLiteral("port"), valueOrEmpty(PQport(m_conn)));
    parameters.insert(QStringLiteral("database"), valueOrEmpty(PQdb(m_conn)));
    parameters.insert(
        QStringLiteral("schema"),
        m_searchPath.trimmed().isEmpty()
            ? QStringLiteral("public")
            : m_searchPath.trimmed());
    parameters.insert(QStringLiteral("user"), valueOrEmpty(PQuser(m_conn)));
    parameters.insert(QStringLiteral("password"), valueOrEmpty(PQpass(m_conn)));
    return parameters;
}
```

Do not persist these values or expose `PGconn*` outside the plugin.

- [ ] **Step 2: Add the plugin-side host notification helper**

In `map_product_manager_tools.h`, declare in the private section:

```cpp
void notifyDataCatalogConnection();
```

In `map_product_manager_tools.cpp`, include `<QMetaObject>` and add:

```cpp
void CMapProductManagerPlugin::notifyDataCatalogConnection()
{
    if (!mQGisIface || !mQGisIface->mainWindow())
    {
        return;
    }

    const QVariantMap parameters = PostgisConnector::instance()->connectionParameters();
    QMetaObject::invokeMethod(
        mQGisIface->mainWindow(),
        "applyMenuDatabaseConnection",
        Qt::QueuedConnection,
        Q_ARG(QVariantMap, parameters));
}
```

The empty map returned by a disconnected connector is the clear signal.

- [ ] **Step 3: Notify on disconnect and initialize the current state**

In the plugin constructor, connect `PostgisConnector::connectionStatusChanged` to a lambda with `this` as context:

```cpp
connect(
    PostgisConnector::instance(),
    &PostgisConnector::connectionStatusChanged,
    this,
    [this](bool connected)
    {
        if (!connected)
        {
            notifyDataCatalogConnection();
        }
    });
```

After the automatic connection attempt in `initGui()` and before `updateActions()`, call:

```cpp
notifyDataCatalogConnection();
```

This covers both an existing saved connection and the no-connection state when the plugin is first loaded.

- [ ] **Step 4: Notify after a successful menu connection**

In `CMapProductManagerPlugin::DbConfig()`, after `dlg.exec()` returns `QDialog::Accepted`, call `notifyDataCatalogConnection()` after `updateActions()`:

```cpp
if (dlg.exec() == QDialog::Accepted)
{
    updateActions();
    notifyDataCatalogConnection();
}
```

This sends the final parameters after `DbConfigDialog` has completed Schema initialization, rather than relying on the earlier low-level connection signal.

### Task 4: Make menu connection failure and Schema state accurate

**Files:**
- Modify: `map_product_manager_tools/ui_class/db_config_dialog.cpp`

- [ ] **Step 1: Always synchronize the selected Schema after connecting**

In `DbConfigDialog::onConnect()`, after reading `schemaName`, keep the existing `CREATE SCHEMA IF NOT EXISTS` behavior for non-public schemas, then call `db->setSearchPath(schemaName)` for both public and custom schemas:

```cpp
QString schemaName = ui.mSchemaCombo->currentText().trimmed();
if (!schemaName.isEmpty() && schemaName != "public")
{
    db->executeNonQuery(QString("CREATE SCHEMA IF NOT EXISTS %1").arg(schemaName));
}
db->setSearchPath(schemaName);
```

This prevents a previous custom Schema from leaking into a later public connection.

- [ ] **Step 2: Disconnect when Schema initialization fails**

Inside the `!schemaMgr.initializeSchema()` failure branch, call `db->disconnect();` before re-enabling buttons and returning:

```cpp
if (!schemaMgr.initializeSchema())
{
    QString errDetail = schemaMgr.lastError();
    if (errDetail.isEmpty()) {
        errDetail = "未知错误";
    }
    db->disconnect();
    setStatus("Schema初始化失败: " + errDetail, "red");
    ui.mTestBtn->setEnabled(true);
    ui.mConnectBtn->setEnabled(true);
    return;
}
```

The existing plugin status listener then clears the data list for this failure path.

### Task 5: Run regression tests and build both binaries

**Files:**
- Verify: `tests/verify_data_catalog_manual_connection.ps1`
- Verify: `tests/verify_workspace_data_catalog.ps1`
- Build: `LTZK.sln`
- Build: `map_product_manager_tools/map_product_manager_tools.vcxproj`

- [ ] **Step 1: Run the new static regression checks**

Run:

```powershell
.\tests\verify_data_catalog_manual_connection.ps1
.\tests\verify_workspace_data_catalog.ps1
```

Expected: both exit with code `0`; the first prints `Data catalog menu database connection verification passed.` and the second retains its existing success message.

- [ ] **Step 2: Build the host application**

Run:

```powershell
msbuild LTZK.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m
```

Expected: exit code `0`, including successful compilation of `DataCatalogWidget.cpp` and `LTZK.cpp`.

- [ ] **Step 3: Build the成果管理 plugin**

Run:

```powershell
msbuild map_product_manager_tools\map_product_manager_tools.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m
```

Expected: exit code `0`, including successful compilation of `PostgisConnector`, `map_product_manager_tools.cpp`, and the existing generated Qt MOC files.

- [ ] **Step 4: Inspect scope and report runtime limitation honestly**

Use `git status` if available; because this workspace has no `.git`, report that no diff/commit can be generated from Git metadata. If the QGIS runtime can be launched safely, perform a smoke check for success, reconnection, failure, and test-disconnect states. If runtime launch is unavailable, report static-test and build evidence separately without claiming GUI verification.
