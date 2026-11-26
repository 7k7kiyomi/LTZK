# 数据目录导入 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task with verification checkpoints. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为数据目录右键菜单增加“导入数据”，让数据库导入委托现有插件，让本地导入按方案 B 复制到选中的已连接文件夹并刷新。

**Architecture:** `DataCatalogWidget` 只负责上下文判定和本地文件复制；数据库上下文通过无参 `DataImportCallback` 委托主窗口。`LTZK` 根据当前工作台把该回调路由到现有配置插件的“数据导入”动作，不新增数据库写入路径。

**Tech Stack:** C++17、Qt 5 Widgets、QGIS 3.28 browser model、PowerShell 静态回归检查、Visual Studio x64 Release。

---

## 文件结构与职责

- Modify: `E:/AAALTZK/DataCatalogWidget.h` — 增加数据库导入回调、本地复制 helper 声明。
- Modify: `E:/AAALTZK/DataCatalogWidget.cpp` — 加入右键动作、数据库委托、本地数据集复制、冲突命名和刷新。
- Modify: `E:/AAALTZK/LTZK.cpp` — 设置数据库导入回调并调用现有配置插件动作。
- Modify: `E:/AAALTZK/tests/verify_workspace_data_catalog.ps1` — 覆盖导入分派与本地复制边界的静态回归断言。
- Create: `E:/AAALTZK/docs/superpowers/specs/2026-09-09-data-catalog-import-design.md` — 已确认的设计基线。

### Task 1: 保持 RED 回归检查

**Files:**
- Modify: `E:/AAALTZK/tests/verify_workspace_data_catalog.ps1`

- [x] **Step 1: 检查失败断言**

验证脚本必须要求以下契约：

```powershell
'using DataImportCallback = std::function<void()>;'
'QAction* importData = menu.addAction(QStringLiteral("导入数据"));'
'QFileDialog::getOpenFileNames'
'QFile::copy'
'copyLocalImportDataset'
'uniqueLocalImportPath'
```

- [x] **Step 2: 运行 RED 检查**

运行：

```powershell
.\tests\verify_workspace_data_catalog.ps1
```

预期：以退出码 1 失败，并报告缺少 `using DataImportCallback = std::function<void()>;`，证明测试不是空通过。

### Task 2: 声明导入回调与本地复制接口

**Files:**
- Modify: `E:/AAALTZK/DataCatalogWidget.h`

- [x] **Step 1: 扩展公共回调协议**

在现有 `DataClipCallback` 后加入：

```cpp
using DataImportCallback = std::function<void()>;
```

在 `setDataClipCallback` 后加入：

```cpp
void setDataImportCallback(DataImportCallback callback);
```

- [x] **Step 2: 声明动作和复制 helper**

在现有右键动作声明中加入：

```cpp
void importContextItem(const CatalogContext& context);
bool canImportLocalDirectory(const CatalogContext& context) const;
QString localImportTargetPath(const CatalogContext& context) const;
QString uniqueLocalImportPath(const QString& targetDirectory, const QString& fileName) const;
QStringList localImportCompanionFiles(const QString& sourcePath) const;
bool copyLocalImportDataset(const QString& sourcePath, const QString& targetDirectory, QString* errorMessage) const;
void refreshLocalImportTarget(const CatalogContext& context, const QString& targetDirectory);
```

在成员变量中加入：

```cpp
DataImportCallback m_dataImportCallback;
```

- [x] **Step 3: 做声明检查**

运行：

```powershell
rg -n "DataImportCallback|importContextItem|copyLocalImportDataset|refreshLocalImportTarget" DataCatalogWidget.h
```

预期：每个声明只出现一次，并且没有修改既有 `LoadCallback`、`DataClipCallback` 签名。

### Task 3: 接入统一右键菜单并实现数据库委托

**Files:**
- Modify: `E:/AAALTZK/DataCatalogWidget.cpp`

- [x] **Step 1: 实现回调设置器**

在 `setDataClipCallback` 后加入：

```cpp
void DataCatalogWidget::setDataImportCallback(DataImportCallback callback)
{
    m_dataImportCallback = std::move(callback);
}
```

- [x] **Step 2: 在菜单中加入导入动作**

在 `populateContextMenu()` 的“刷新”动作之后创建：

```cpp
QAction* importData = menu.addAction(QStringLiteral("导入数据"));
```

数据库节点的启用条件是数据库 item 有 `DatabaseNameRole` 且不是错误/提示行并且存在 `m_dataImportCallback`；本地节点的启用条件由 `canImportLocalDirectory(context)` 给出。动作连接到：

```cpp
connect(importData, &QAction::triggered, this,
    [this, context]() { importContextItem(context); });
```

- [x] **Step 3: 实现数据库分派**

`importContextItem()` 的数据库分支只能调用回调并返回：

```cpp
if (context.source == CatalogSource::Database)
{
    if (m_dataImportCallback && context.databaseItem &&
        context.databaseItem->data(DatabaseNameRole).isValid())
    {
        m_dataImportCallback();
    }
    return;
}
```

该分支不得出现 `QSqlQuery`、`ALTER TABLE`、`CREATE TABLE` 或 `INSERT INTO`。

### Task 4: 实现本地方案 B 的复制与刷新

**Files:**
- Modify: `E:/AAALTZK/DataCatalogWidget.cpp`

- [x] **Step 1: 解析合法目标目录**

`canImportLocalDirectory()` 和 `localImportTargetPath()` 只接受 `CatalogSource::Local` 下的 `QgsDirectoryItem`，并检查 `dirPath()` 对应目录存在；Favorites 虚拟根、文件和图层返回 false/空路径。

- [x] **Step 2: 生成不覆盖目标的路径**

`uniqueLocalImportPath()` 先返回目标目录中的原文件名；若存在，则按 `_2`、`_3` 递增，并保留原后缀：

```cpp
QString stem = info.completeBaseName();
const QString suffix = info.completeSuffix();
QString candidate = info.fileName();
int sequence = 2;
while (QFileInfo::exists(targetDirectory + QDir::separator() + candidate))
{
    candidate = stem + QStringLiteral("_") + QString::number(sequence++);
    if (!suffix.isEmpty())
    {
        candidate += QStringLiteral(".") + suffix;
    }
}
return QDir(targetDirectory).filePath(candidate);
```

- [x] **Step 3: 收集数据集配套文件**

`localImportCompanionFiles()` 至少覆盖 Shapefile 的 `.shp/.shx/.dbf/.prj/.qpj/.cpg/.sbn/.sbx/.qix` 和栅格的 `.tfw/.wld/.aux.xml/.ovr`；总是包含用户选择的源文件，其他不存在的配套文件不加入结果。

- [x] **Step 4: 复制数据集**

`copyLocalImportDataset()` 对源文件及其配套文件统一计算目标基名，使用 `QFile::copy()` 复制，源目标相同或复制失败则返回 false 和具体错误文本。配套文件复制到同一目标基名，不能覆盖已有文件。

- [x] **Step 5: 实现导入交互**

`importContextItem()` 的本地分支：

1. 解析目标目录；
2. 调用 `QFileDialog::getOpenFileNames()`，默认目录为目标目录，过滤矢量/栅格数据和“所有文件”；
3. 对每个源文件调用 `copyLocalImportDataset()`；
4. 汇总成功和失败，并用 `QMessageBox` 显示；
5. 成功数大于 0 时调用 `refreshLocalImportTarget()`。

取消选择时直接返回，不显示成功提示。

- [x] **Step 6: 刷新 QGIS 本地树**

`refreshLocalImportTarget()` 通过 `m_browserModel->findPath(targetDirectory)` 找到源索引，调用 `m_browserModel->refresh(targetIndex)`，然后用 `QTimer::singleShot(0, ...)` 执行 `showFavoritesInTree()` 并 `expandPath(targetDirectory)`。

### Task 5: 从主窗口路由到现有插件动作

**Files:**
- Modify: `E:/AAALTZK/LTZK.cpp`

- [x] **Step 1: 设置数据导入回调**

在现有 `setDataClipCallback` 后加入：

```cpp
m_dataCatalogWidget->setDataImportCallback([this]() {
    const QString pluginId = m_currentBusinessModuleTitle == QStringLiteral("地图智能综合")
        ? QStringLiteral("map_generalization_tools")
        : QStringLiteral("map_product_manager_tools");
    executeConfiguredPluginFeature(pluginId, QStringLiteral("数据导入"));
});
```

这样数据库右键动作只重用现有插件加载/动作查找链路，不复制插件内部导入逻辑。

- [x] **Step 2: 检查回调位置**

运行：

```powershell
rg -n -C 5 "setDataImportCallback|map_product_manager_tools|map_generalization_tools|数据导入" LTZK.cpp
```

预期：回调在 `DataCatalogWidget` 创建后设置，调用的是 `executeConfiguredPluginFeature()`。

### Task 6: GREEN、回归与构建验证

**Files:**
- Verify: `E:/AAALTZK/tests/verify_workspace_data_catalog.ps1`
- Verify: `E:/AAALTZK/tests/verify_data_clip_context_action.ps1`
- Build: `E:/AAALTZK/LTZK.sln`

- [x] **Step 1: 运行聚焦检查**

运行：

```powershell
.\tests\verify_workspace_data_catalog.ps1
.\tests\verify_data_clip_context_action.ps1
```

预期：两个脚本均退出码 0；第一个输出 `Workspace data catalog integration verified.`。

- [ ] **Step 2: 运行相关既有检查**

运行：

```powershell
.\tests\verify_workspace_layer_tree_reparent_safe.ps1
.\tests\verify_workspace_resizable_panels.ps1
.\tests\verify_login_permissions_postgis.ps1
```

预期：退出码全部为 0。

- [x] **Step 3: 构建 x64 Release**

运行已有工程对应的 MSBuild Release 命令：

```powershell
msbuild LTZK.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m
```

预期：编译和链接退出码 0；若工具链或运行库路径阻断，记录实际错误而不把静态检查结果表述为构建通过。

- [x] **Step 4: 终检代码边界**

运行：

```powershell
rg -n "QFile::copy|QSqlQuery|ALTER TABLE|CREATE TABLE|INSERT INTO|setDataImportCallback|导入数据" DataCatalogWidget.cpp LTZK.cpp
```

确认 `QFile::copy` 只位于本地导入 helper，数据库导入只调用现有插件执行器；没有新增数据库写入语句，也没有删除/覆盖本地源文件。

由于 `E:/AAALTZK` 没有 Git 元数据，交付时报告实际修改文件和验证结果，不执行提交。
