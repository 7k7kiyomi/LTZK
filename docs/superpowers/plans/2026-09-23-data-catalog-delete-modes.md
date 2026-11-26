# 数据目录删除双模式 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为数据库列表和本地列表的数据节点增加统一的“重命名/删除数据”入口，并在删除弹窗中区分“仅平台侧移除”和“永久删除源数据”，且移除独立“隐藏”菜单。

**Architecture:** `DataCatalogWidget` 继续负责两个树的右键菜单、平台侧持久化移除状态、删除模式对话框和来源分流。数据库成果的永久删除由 `DataCatalogProductCatalog` 提供事务接口；旧式数据库空间表和本地文件由宿主侧分别执行。`CatalogBrowserProxyModel` 负责本地移除路径过滤和会话内显示别名。

**Tech Stack:** Qt 5 C++, QGIS Browser model/proxy, QSqlDatabase/QSqlQuery, QSettings, PowerShell 静态回归脚本, MSBuild x64 Release。

---

## 文件结构与职责

- Modify: `DataCatalogWidget.h` — 删除模式、平台移除键、显示别名和数据库/本地删除处理器声明。
- Modify: `DataCatalogWidget.cpp` — 菜单动作、双模式确认对话框、QSettings 持久化、本地代理过滤、数据库/本地软删除与永久删除。
- Modify: `DataCatalogProductCatalog.h` — 暴露单成果/多成果事务删除接口。
- Modify: `DataCatalogProductCatalog.cpp` — 将既有私有事务删除逻辑复用到公开批量入口。
- Create: `tests/verify_data_catalog_delete_modes.ps1` — 静态验证菜单文案、删除模式、持久化和来源分流契约。
- Create: `docs/superpowers/specs/2026-09-23-data-catalog-delete-mode-design.md` — 已确认的交互设计；本计划按该文档执行。

## Task 1: 先写删除双模式回归契约

**Files:**
- Create: `tests/verify_data_catalog_delete_modes.ps1`
- Test: `DataCatalogWidget.h`, `DataCatalogWidget.cpp`, `DataCatalogProductCatalog.h`, `DataCatalogProductCatalog.cpp`

- [ ] **Step 1: 写静态失败测试**

创建 PowerShell 脚本，读取四个源码文件并要求以下字符串存在：

```powershell
$root = Split-Path -Parent $PSScriptRoot
$widgetHeader = Join-Path $root 'DataCatalogWidget.h'
$widgetSource = Join-Path $root 'DataCatalogWidget.cpp'
$catalogHeader = Join-Path $root 'DataCatalogProductCatalog.h'
$catalogSource = Join-Path $root 'DataCatalogProductCatalog.cpp'

foreach ($path in @($widgetHeader, $widgetSource, $catalogHeader, $catalogSource)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing source: $path" }
}

$header = Get-Content -Raw -Encoding UTF8 $widgetHeader
$source = Get-Content -Raw -Encoding UTF8 $widgetSource
$serviceHeader = Get-Content -Raw -Encoding UTF8 $catalogHeader
$serviceSource = Get-Content -Raw -Encoding UTF8 $catalogSource
$all = "$header`n$source`n$serviceHeader`n$serviceSource"

foreach ($required in @(
    '删除数据',
    '仅从平台列表移除（保留源数据）',
    '永久删除源数据',
    'QSettings',
    'filterAcceptsRow',
    'deleteProducts',
    'lo_unlink',
    'QFile::remove',
    'localImportCompanionFiles',
    'QSqlDatabase::transaction'
)) {
    if (-not $all.Contains($required)) { throw "Missing delete-mode contract: $required" }
}

if ($source -match 'addAction\(QStringLiteral\("隐藏"\)\)') {
    throw 'Standalone 隐藏 context-menu action must not exist.'
}
if (-not $source.Contains('m_removedDatabaseKeys') -or
    -not $source.Contains('m_removedLocalPaths')) {
    throw 'Persistent platform-removal state is not wired in the widget.'
}

Write-Output 'Data catalog delete-mode static contract passed.'
```

- [ ] **Step 2: 运行测试确认当前实现失败**

Run: `pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_data_catalog_delete_modes.ps1`

Expected: FAIL，因为当前代码没有删除双模式文案、平台移除集合和 `deleteProducts` 公开接口。

## Task 2: 增加数据库成果事务删除接口

**Files:**
- Modify: `DataCatalogProductCatalog.h` — 在 `deleteDirectory` 前声明 `deleteProduct` 和 `deleteProducts`。
- Modify: `DataCatalogProductCatalog.cpp` — 实现事务包装，复用现有 `deleteProductInTransaction`。

- [ ] **Step 1: 写接口失败检查**

先运行 Task 1 的静态测试，记录 `deleteProducts` 缺失作为本任务的失败基线。

- [ ] **Step 2: 添加最小接口**

在头文件添加：

```cpp
bool deleteProduct(
    int productId,
    QString* errorMessage = nullptr) const;
bool deleteProducts(
    const QList<int>& productIds,
    QString* errorMessage = nullptr) const;
```

在源文件中实现 `deleteProducts`：去重并过滤非正 ID；打开事务；逐个调用已有 `deleteProductInTransaction`；任一失败回滚；提交失败回滚并返回数据库错误。`deleteProduct` 调用单元素 `deleteProducts`。

- [ ] **Step 3: 运行服务层静态测试**

Run: `pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_data_catalog_product_tree.ps1`

Expected: PASS，且源码仍包含 `lo_unlink`、`layer_registry` 和 `product_metadata` 删除路径。

## Task 3: 增加平台侧移除状态和本地代理过滤

**Files:**
- Modify: `DataCatalogWidget.h` — 增加 `m_removedDatabaseKeys`、`m_removedLocalPaths`、`m_localDisplayAliases` 及对应 helper 声明。
- Modify: `DataCatalogWidget.cpp` — 读取/保存 `QSettings`，扩展 `CatalogBrowserProxyModel`。

- [ ] **Step 1: 添加失败测试契约**

运行 Task 1 脚本，确认 `m_removedDatabaseKeys` 和 `m_removedLocalPaths` 缺失。

- [ ] **Step 2: 实现状态加载与保存**

使用 `QSettings(QStringLiteral("GarMap"), QStringLiteral("LTZK"))`，保存以下键：

```text
dataCatalog/removedDatabaseKeys
dataCatalog/removedLocalPaths
```

在构造函数进入 UI 初始化前加载状态；提供 `savePlatformRemovalState()`，每次集合变更后 `sync()`，失败时保留内存状态并显示警告。数据库键使用无密码的连接身份、产品 ID 或 Schema/表/类型/几何列；本地键使用规范化绝对路径，无法取得文件路径时回退到 `QgsDataItem::path()`。

- [ ] **Step 3: 扩展 `CatalogBrowserProxyModel`**

增加：

```cpp
void setRemovedPaths(const QSet<QString>* paths);
void setDisplayAliases(const QHash<QString, QString>* aliases);
```

`filterAcceptsRow()` 对本地图层项计算稳定路径键，命中移除集合时返回 `false`；`data()` 在 `Qt::DisplayRole` 下对本地图层应用会话内别名；状态变更后调用 `invalidateFilter()` 和 `invalidate()`。目录节点不因自身没有 URI 而被误过滤。

- [ ] **Step 4: 在数据库/本地刷新路径应用过滤**

数据库目录构建时跳过已移除的产品或旧式空间表；本地 Browser 初始化时把两个集合地址传给代理；完成软移除或配置切换后刷新模型。清理被永久删除节点对应的移除键、勾选缓存和别名。

- [ ] **Step 5: 运行静态测试**

Run: `pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_data_catalog_delete_modes.ps1`

Expected: 仍可能因菜单动作和弹窗实现缺失失败，但 `QSettings`、`filterAcceptsRow`、两个集合契约通过。

## Task 4: 实现双模式删除确认对话框

**Files:**
- Modify: `DataCatalogWidget.h` — 添加 `DataRemovalMode`、删除确认和删除处理器声明。
- Modify: `DataCatalogWidget.cpp` — 在匿名命名空间实现轻量 `QDialog`，并连接来源分流。

- [ ] **Step 1: 添加失败测试契约**

运行静态测试，确认两个中文选项和删除模式符号缺失。

- [ ] **Step 2: 实现选择对话框**

使用 `QDialog`、两个 `QRadioButton`、`QLabel` 和 `QDialogButtonBox`。默认选中“仅从平台列表移除（保留源数据）”；切换到“永久删除源数据”时显示数据来源、目标路径/数据库对象和“此操作不可恢复”警告。关闭或取消返回 `false`。

- [ ] **Step 3: 实现统一入口**

增加 `removeData(const CatalogContext&)`：先收集当前上下文的稳定键和永久删除目标摘要，显示对话框；平台模式调用持久化集合并刷新；永久模式根据 `CatalogSource` 调用数据库或本地处理器。确认按钮文字保持明确，不使用含糊的“删除/是”作为唯一说明。

- [ ] **Step 4: 运行测试确认弹窗契约通过**

Run: `pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_data_catalog_delete_modes.ps1`

Expected: PASS 通过双模式文案、确认入口和无独立“隐藏”菜单检查。

## Task 5: 接入数据库和本地永久删除

**Files:**
- Modify: `DataCatalogWidget.h` — 添加数据库旧式图层删除、本地数据集路径收集和刷新 helper。
- Modify: `DataCatalogWidget.cpp` — 实现数据库成果/空间表删除、本地主文件与伴随文件删除。

- [ ] **Step 1: 实现数据库成果删除**

成果节点用 `productId`，图层目录用 `allProductIdsUnderItem()` 收集全部成果，调用 `DataCatalogProductCatalog::deleteProducts()`。成功后调用 `refreshDatabaseData()`；失败显示数据库错误，不清理移除键。

- [ ] **Step 2: 实现旧式数据库空间表删除**

只接受 `DatabaseLayerRole` 为 `vector` 或 `raster` 且 Schema/表名非空的节点。使用现有 `quotedSqlIdentifier()` 生成 `DROP TABLE IF EXISTS "schema"."table" CASCADE`，执行失败显示 `QSqlError`；成功后刷新数据库模型。普通表、Schema、根节点直接禁用或拒绝。

- [ ] **Step 3: 实现本地数据集删除**

从 `localUris(context.localItem)` 收集 `filePath`；普通文件复用 `localImportCompanionFiles()`，目录型数据集使用 `QDir::removeRecursively()`。禁止 Favorites 根目录和连接根目录；路径为空、非本地路径或不在数据项上下文中时拒绝。删除结果按成功/失败路径汇总，失败项不静默忽略。

- [ ] **Step 4: 刷新与清理状态**

数据库删除后重建数据库树；本地删除后刷新 `sourceIndex` 或 Browser model，并调用 `showFavoritesInTree()`。同时移除已删除数据的平台移除键、勾选路径和显示别名。

- [ ] **Step 5: 运行静态回归**

Run: `pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_data_catalog_delete_modes.ps1`

Expected: PASS，且脚本确认事务删除、伴随文件删除和刷新路径存在。

## Task 6: 接入两个树的右键菜单和重命名

**Files:**
- Modify: `DataCatalogWidget.cpp` — 修改 `populateProductContextMenu()` 和 `populateContextMenu()`。

- [ ] **Step 1: 为产品/图层数据节点添加动作**

在成果节点和旧式空间图层菜单加入：

```cpp
QAction* rename = menu.addAction(QStringLiteral("重命名"));
QAction* deleteData = menu.addAction(QStringLiteral("删除数据"));
```

连接 `renameContextItem(context)` 和 `removeData(context)`；删除动作只对可识别的数据节点启用。删除目录动作继续使用原有 `delete-directory`，不改成 `删除数据`。

- [ ] **Step 2: 扩展重命名上下文**

数据库成果/旧式空间图层沿用 `m_databaseDisplayAliases`；本地数据节点写入代理使用的会话别名表。所有重命名继续拒绝空文本和换行，不调用 `QFile::rename` 或数据库 `ALTER TABLE`。

- [ ] **Step 3: 删除独立隐藏入口**

确认两个菜单构建器没有 `addAction(QStringLiteral("隐藏"))`，并且任何软移除只从 `删除数据` 弹窗触发。

- [ ] **Step 4: 运行菜单静态回归**

Run: `pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_data_catalog_delete_modes.ps1`

Expected: PASS，且输出 `Data catalog delete-mode static contract passed.`。

## Task 7: 完整验证和构建

**Files:**
- Test: `tests/verify_data_catalog_delete_modes.ps1`
- Test: `tests/verify_workspace_data_catalog.ps1`
- Test: `tests/verify_data_catalog_product_tree.ps1`
- Build: `LTZK.vcxproj`

- [ ] **Step 1: 运行新增静态测试**

Run: `pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_data_catalog_delete_modes.ps1`

Expected: PASS。

- [ ] **Step 2: 运行既有数据目录回归测试**

Run: `pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_workspace_data_catalog.ps1`

Expected: PASS；若旧脚本把原有菜单动作数量写死，只调整其断言以允许新增 `删除数据`、保留既有动作，并继续拒绝独立 `隐藏`。

- [ ] **Step 3: 运行成果目录回归测试**

Run: `pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_data_catalog_product_tree.ps1`

Expected: PASS；目录创建/删除/重命名、导入、导出、主题和地图加载契约保持不变。

- [ ] **Step 4: 构建 x64 Release**

Run: `msbuild .\LTZK.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m`

Expected: exit code 0，生成 LTZK Release x64 目标；若机器缺少 QGIS/Qt 构建环境，保留完整错误并至少完成全部静态测试。

- [ ] **Step 5: 做源码验收扫描**

Run: `rg -n "删除数据|仅从平台列表移除|永久删除源数据|隐藏|deleteProducts|m_removedDatabaseKeys|m_removedLocalPaths" DataCatalogWidget.h DataCatalogWidget.cpp DataCatalogProductCatalog.h DataCatalogProductCatalog.cpp tests\verify_data_catalog_delete_modes.ps1`

Expected: “隐藏”只出现在明确的非菜单上下文或旧文档中；右键菜单构建器不存在独立隐藏动作，双模式删除和持久化状态均可定位。

由于 `E:\AAALTZK` 当前没有 Git 元数据，不执行 `git commit`；完成后报告修改文件、测试输出和构建结果。

