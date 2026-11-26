# 数据目录右键菜单 Implementation Plan

> **后续修订：** 本计划中的“不得添加导入动作”已被 [2026-09-09-data-catalog-import-design.md](../specs/2026-09-09-data-catalog-import-design.md) 与 [2026-09-09-data-catalog-import.md](2026-09-09-data-catalog-import.md) 覆盖；当前导入行为按最新计划执行。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task with verification checkpoints.

**Goal:** 为数据库数据树和本地数据树实现统一的六项右键菜单，并保证“重命名”只修改界面显示别名，不修改数据库对象或磁盘路径。

**Architecture:** `DataCatalogWidget` 负责两个树的自定义上下文菜单、动作启用状态和上下文索引映射。数据库 URI 从现有 `QStandardItem` 角色构建，本地 URI 从 `QgsDataItem` 读取；导出共用 vector/raster writer helper，数据库别名保存在会话内哈希表，本地文件夹别名通过 `QgsFavoritesItem::renameFavorite()` 保存。

**Tech Stack:** C++17、Qt 5、QGIS 3.28 API、`QgsBrowserGuiModel`、`QgsMimeDataUtils`、`QgsVectorFileWriter`、`QgsRasterFileWriter`、PowerShell 静态验证、Visual Studio x64 Release。

---

## 文件结构与职责

- Modify: `E:/AAALTZK/DataCatalogWidget.h` — 声明上下文菜单、URI、信息、导出和别名 helper，并保存数据库显示别名。
- Modify: `E:/AAALTZK/DataCatalogWidget.cpp` — 实现菜单、动作、导出、递归收集和显示别名逻辑；保留现有双击加载协议。
- Modify: `E:/AAALTZK/tests/verify_workspace_data_catalog.ps1` — 增加右键菜单、六项动作、writer 和别名边界的静态回归检查。
- Create: `E:/AAALTZK/docs/superpowers/specs/2026-09-08-data-catalog-context-menu-design.md` — 已完成的设计基线。
- Create: `E:/AAALTZK/docs/superpowers/plans/2026-09-08-data-catalog-context-menu.md` — 本实施计划。

### Task 1: 先补充可执行的静态回归检查

**Files:**
- Modify: `E:/AAALTZK/tests/verify_workspace_data_catalog.ps1`

- [x] **Step 1: 添加失败检查，覆盖菜单入口和动作名称**

在脚本现有 `DataCatalogWidget.cpp` 文本检查之后加入以下断言；执行时它们应先因为源代码尚未包含新字符串而失败：

```powershell
Assert-Contains $header 'showDatabaseContextMenu'
Assert-Contains $header 'showLocalContextMenu'
Assert-Contains $source 'Qt::CustomContextMenu'
Assert-Contains $source '添加到地图'
Assert-Contains $source '批量导出数据'
Assert-Contains $source '基本信息'
Assert-Contains $source '重命名'
Assert-Contains $source 'QgsVectorFileWriter::writeAsVectorFormatV3'
Assert-Contains $source 'QgsRasterFileWriter::writeRaster'
Assert-Contains $source 'QgsFavoritesItem::renameFavorite'
Assert-NotContains $source '删除目录'
Assert-NotContains $source '新建目录'
```

- [x] **Step 2: 运行检查并确认它按预期失败**

运行：

```powershell
.\tests\verify_workspace_data_catalog.ps1
```

预期：新上下文菜单断言报告缺失；这证明测试确实覆盖新需求。

### Task 2: 声明上下文菜单状态与导出接口

**Files:**
- Modify: `E:/AAALTZK/DataCatalogWidget.h`

- [x] **Step 1: 增加必要的类型前置声明和状态类型**

在头文件前置声明中加入 `QPoint`、`QMenu` 和 `QgsDataItem`，并在 `private` 中增加：

```cpp
enum class CatalogSource
{
    Database,
    Local
};

struct CatalogContext
{
    CatalogSource source = CatalogSource::Database;
    QModelIndex viewIndex;
    QModelIndex sourceIndex;
    QStandardItem* databaseItem = nullptr;
    QgsDataItem* localItem = nullptr;
};
```

- [x] **Step 2: 声明菜单、URI、信息、导出和别名 helper**

加入以下完整接口，并保留现有公共接口不变：

```cpp
    void showDatabaseContextMenu(const QPoint& position);
    void showLocalContextMenu(const QPoint& position);
    CatalogContext databaseContextAt(const QPoint& position) const;
    CatalogContext localContextAt(const QPoint& position) const;
    void populateContextMenu(QMenu& menu, const CatalogContext& context);
    bool isCatalogLayer(const CatalogContext& context) const;
    bool isCatalogCollection(const CatalogContext& context) const;
    QgsMimeDataUtils::Uri databaseUri(const QStandardItem* item) const;
    QgsMimeDataUtils::UriList localUris(const QgsDataItem* item) const;
    void addContextItemToMap(const CatalogContext& context);
    void refreshContextItem(const CatalogContext& context);
    void showContextItemInfo(const CatalogContext& context);
    void exportContextItem(const CatalogContext& context);
    void batchExportContextItem(const CatalogContext& context);
    void renameContextItem(const CatalogContext& context);
    bool exportVectorUri(const QgsMimeDataUtils::Uri& uri, const QString& outputPath, QString* errorMessage) const;
    bool exportRasterUri(const QgsMimeDataUtils::Uri& uri, const QString& outputPath, QString* errorMessage) const;
    QgsMimeDataUtils::UriList collectLocalUris(QgsDataItem* item, int* unloadedCount) const;
    QgsMimeDataUtils::UriList collectDatabaseUris(const QStandardItem* item) const;
    QString databaseDisplayAliasKey(const QStandardItem* item) const;
    QString displayNameForItem(const CatalogContext& context) const;
```

增加 `QHash<QString, QString> m_databaseDisplayAliases;` 成员，并在类中保存所需的别名状态。

- [x] **Step 3: 检查声明与现有构造关系**

运行 `rg -n "CatalogContext|showDatabaseContextMenu|exportVectorUri" DataCatalogWidget.h`，确认所有声明只出现一次且 `QMenu`、`QgsDataItem` 均为可解析的前置声明。

### Task 3: Wire both trees to one six-action context menu

**Files:**
- Modify: `E:/AAALTZK/DataCatalogWidget.cpp`

- [x] **Step 1: 添加 Qt/QGIS includes**

加入：

```cpp
#include "qgslayeritem.h"
#include "qgsrasterfilewriter.h"
#include "qgsrasterlayer.h"
#include "qgsrasterpipe.h"
#include "qgsvectorfilewriter.h"
#include "qgsvectorlayer.h"

#include <QCoreApplication>
#include <QInputDialog>
#include <QMenu>
#include <QRegularExpression>
```

- [x] **Step 2: 启用自定义菜单并连接信号**

在两个树的编辑/选择配置后加入：

```cpp
m_databaseTree->setContextMenuPolicy(Qt::CustomContextMenu);
m_localTree->setContextMenuPolicy(Qt::CustomContextMenu);
```

在现有双击连接之后加入：

```cpp
connect(m_databaseTree, &QTreeView::customContextMenuRequested,
    this, &DataCatalogWidget::showDatabaseContextMenu);
connect(m_localTree, &QTreeView::customContextMenuRequested,
    this, &DataCatalogWidget::showLocalContextMenu);
```

- [x] **Step 3: 实现上下文映射和固定菜单顺序**

使用 `indexAt(position)`、`setCurrentIndex()` 和本地 `mapToSource()`，创建且只创建以下动作：

```cpp
QAction* addToMap = menu.addAction(QStringLiteral("添加到地图"));
QAction* refresh = menu.addAction(QStringLiteral("刷新"));
QAction* info = menu.addAction(QStringLiteral("基本信息"));
QAction* exportData = menu.addAction(QStringLiteral("导出数据"));
QAction* batchExport = menu.addAction(QStringLiteral("批量导出数据"));
QAction* rename = menu.addAction(QStringLiteral("重命名"));
```

根据节点类型和 `m_loadCallback` 设置启用状态，lambda 捕获 `CatalogContext` 值副本后调用对应处理器，最后用 `menu.exec()` 显示菜单。

- [x] **Step 4: 让双击和“添加到地图”复用同一 URI 路径**

把现有 `activateDatabaseItem()` 中的 URI 构建抽到 `databaseUri()`，让双击和右键动作都只提交 vector/raster URI；本地动作同样复用 `localUris()`。不改变 `LoadCallback` 签名。

### Task 4: Implement refresh, basic information, and display-only rename

**Files:**
- Modify: `E:/AAALTZK/DataCatalogWidget.cpp`

- [x] **Step 1: 实现刷新**

数据库上下文调用 `refreshDatabaseData()`；本地上下文在源索引有效时调用 `m_browserModel->refresh(sourceIndex)`，设置“正在刷新本地数据目录…”状态并排队 `showFavoritesInTree()`。

- [x] **Step 2: 实现只读基本信息**

用 `QStringList` 组装显示名称、来源、provider、路径/URI、数据库角色或 `QFileInfo` 信息，再调用 `QMessageBox::information(this, QStringLiteral("基本信息"), lines.join(QChar('\n')))`。不得调用任何重命名、删除、移动或写入 API。

- [x] **Step 3: 实现数据库显示别名**

用数据库名、模式、表、图层类型和数据列组成稳定键；根/模式使用 `database:`/`schema:` 前缀。`renameContextItem()` 用 `QInputDialog::getText` 获取非空且无换行的新文本，写入 `m_databaseDisplayAliases` 并只调用 `QStandardItem::setText()`。目录重建时应用别名，URI 构建继续读取原始角色值。

- [x] **Step 4: 实现本地 Favorites 别名**

仅对 Favorites 直接子节点且为 `QgsDirectoryItem` 的项启用重命名，调用：

```cpp
favoritesItem->renameFavorite(localDirectory->dirPath(), newName);
```

随后刷新树；不得调用 `QDir::rename`、`QFile::rename` 或文件系统移动 API。

### Task 5: Implement single and batch export

**Files:**
- Modify: `E:/AAALTZK/DataCatalogWidget.cpp`

- [x] **Step 1: 实现矢量 writer helper**

用 `uri.vectorLayer(owner, error)` 解析图层，设置 `SaveVectorOptions.driverName`、`layerName`，调用 `QgsVectorFileWriter::writeAsVectorFormatV3()`，仅当 `owner` 为真时释放临时图层，并返回完整错误。

- [x] **Step 2: 实现栅格 writer helper**

用 `uri.rasterLayer(owner, error)` 创建 `QgsRasterPipe`，再用 `QgsRasterFileWriter(outputPath).writeRaster()` 写 GeoTIFF；同样按 owner 释放临时图层并传播错误。

- [x] **Step 3: 实现单层保存对话框**

矢量使用 `QgsVectorFileWriter::fileFilterString()`，默认 `.gpkg`；栅格使用 GeoTIFF 过滤器，默认 `.tif`。默认名清理 Windows 非法字符，成功/失败都显示结果。

- [x] **Step 4: 实现递归收集和批量导出**

数据库递归遍历 `QStandardItem` 子项；本地递归遍历 `QgsDataItem::children()`，对可 fetch 的节点请求 `fetchMore`、处理一次 pending events，仍未填充则计入 `unloadedCount`。批量选择一个目录，矢量逐层输出独立 `.gpkg`，栅格逐层输出独立 `.tif`，汇总成功/失败/未加载数量。

- [x] **Step 5: 实现安全文件名和冲突后缀**

将 `\\ / : * ? " < > |` 与控制字符替换为 `_`，空名使用 `layer`，已存在时依次追加 `_2`、`_3`；此 helper 只用于输出路径。

### Task 6: Finish regression checks and verify Release output

**Files:**
- Modify: `E:/AAALTZK/tests/verify_workspace_data_catalog.ps1`

- [x] **Step 1: 运行静态与既有环境检查**

运行：

```powershell
.\tests\verify_workspace_data_catalog.ps1
.\tests\verify_bat_qgis_runtime_env.ps1
.\tests\verify_release_plugin_deployment.ps1
```

预期所有脚本退出码为 0，并输出 `Workspace data catalog integration verified.` 等成功信息。

- [x] **Step 2: 构建隔离的 x64 Release**

运行：

```powershell
$taskTemp='C:\Users\孟子腾\.codex\visualizations\2026\09\07\01a07b60-890b-71e1-b3e4-f79b3d925464\tmp_release_catalog_menu'
$env:TEMP=$taskTemp
$env:TMP=$taskTemp
& 'E:\VS 2019\MSBuild\Current\Bin\MSBuild.exe' LTZK.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /p:OutDir="$taskTemp\build_out\" /p:IntDir="$taskTemp\build_int\" /m
```

预期退出码为 0，且 `build_out\LTZK.exe` 存在。

- [x] **Step 3: 保留用户进程并部署测试文件**

始终复制唯一文件名 `E:\AAALTZK\x64\Release\LTZK_catalog_context_menu_fix.exe`。默认 `LTZK.exe` 未锁定时才备份到 `LTZK_before_catalog_context_menu_fix.exe` 并更新默认文件；锁定时保持默认文件不动。

- [x] **Step 4: 启动并停止唯一测试进程**

用 `Start-Process -PassThru` 启动唯一文件，确认短暂存活后只停止返回的测试 PID；不停止用户已有的 LTZK 进程。记录 SHA256、文件大小和启动结果。

### Task 7: Final self-review against the design

- [x] **Step 1: 检查六项动作和禁用动作**

用 `rg` 确认六个标签只在 `populateContextMenu()` 中创建，并确认未添加删除、新建、导入动作。

- [x] **Step 2: 检查重命名写边界**

用 `rg` 确认新重命名路径只包含 `setText`、`m_databaseDisplayAliases` 和 `renameFavorite`；不出现 `QDir::rename`、`QFile::rename`、`ALTER TABLE` 或物理移动。

- [x] **Step 3: 对照设计验收**

逐项对照设计文档的验收标准、静态检查、Release 构建和启动结果；依赖现场数据库或数据文件的行为如果无法在当前环境自动操作，必须明确标记为未现场验证，不宣称已通过。

由于 `E:/AAALTZK` 不是 Git 工作树，无法执行提交；保留设计、计划、源代码和验证结果文件，并在交付时说明这一点。
