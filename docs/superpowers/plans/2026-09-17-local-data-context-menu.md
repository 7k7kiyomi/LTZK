# 本地数据右键菜单与数据库成果目录一致化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (or inline execution with the current session) to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让本地数据右键菜单完整复用当前数据库成果目录的菜单内容和分发入口，同时保持数据库与质检任务管理导出逻辑不变。

**Architecture:** 在 `DataCatalogWidget` 内抽取一个共享的成果目录菜单构建器。数据库上下文继续使用原有成果 ID 回调，本地上下文使用本地 URI 和现有本地操作适配器；不跨数据源伪造 ID，也不引入数据库写入。

**Tech Stack:** C++17、Qt 5、QGIS Browser Model、`QgsMimeDataUtils::Uri`、PowerShell 静态回归脚本、MSBuild Release x64。

---

### Task 1: 添加失败回归检查

**Files:**
- Create: `E:\AAALTZK\tests\verify_local_data_context_menu.ps1`
- Test: `E:\AAALTZK\DataCatalogWidget.h`, `E:\AAALTZK\DataCatalogWidget.cpp`, `E:\AAALTZK\LTZK.cpp`

- [x] **Step 1: Write the failing static test**

创建 PowerShell 检查，读取三份源文件，验证共享构建器、数据源判断、本地 URI 适配器、统一分发入口和两棵树的调用入口。脚本必须包含以下断言内容：

```powershell
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$header = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'DataCatalogWidget.h')
$source = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'DataCatalogWidget.cpp')
$ltzk = Get-Content -Raw -Encoding Default (Join-Path $root 'LTZK.cpp')

function Assert-Contains([string]$text, [string]$token, [string]$message) {
    if (-not $text.Contains($token)) { throw $message }
}

foreach ($token in @(
    'populateProductContextMenu',
    'isProductMenuContext',
    'isProductMenuDirectory',
    'localProductUrisForContext',
    'setLocalProductSubtreeCheck',
    'dispatchProductOperation',
    'batchExportUris',
    'CatalogSource::Local',
    'isCatalogLayer(context)',
    'isCatalogCollection(context)',
    '勾选本节点下全部成果',
    '取消勾选本节点下全部成果',
    '导出所选数据',
    '按范围导出',
    '按制图导出',
    '添加到主题'
)) {
    Assert-Contains ($header + $source) $token "Missing local/database product menu contract: $token"
}

Assert-Contains $source 'if (isProductMenuContext(context))' 'Context menu must route both sources through the product menu builder.'
Assert-Contains $source 'dispatchProductOperation(QStringLiteral("map")' 'Product menu map action must use the shared dispatcher.'
Assert-Contains $source 'context.source == CatalogSource::Local' 'Shared dispatcher must contain a local-data adapter branch.'
Assert-Contains $ltzk 'm_productOperationCallback' 'The existing database product callback must remain wired.'
Assert-Contains $ltzk 'QStringLiteral("成果目录操作")' 'The existing database product operation feature must remain unchanged.'

$genericStart = $source.IndexOf('QAction* addToMap = menu.addAction(QStringLiteral("添加到地图"));')
$genericEnd = $source.IndexOf('bool DataCatalogWidget::isCatalogLayer', $genericStart)
if ($genericStart -lt 0 -or $genericEnd -lt $genericStart) {
    throw 'Generic context-menu boundary was not found.'
}
$genericMenu = $source.Substring($genericStart, $genericEnd - $genericStart)
if ($genericMenu.Contains('新建目录') -or $genericMenu.Contains('按范围导出')) {
    throw 'Generic menu must not own the database product-directory menu actions.'
}

Write-Output 'Local/database product context menu verification passed.'
```

- [x] **Step 2: Run the test and verify the failure is expected**

运行：

```powershell
.\tests\verify_local_data_context_menu.ps1
```

预期：脚本以非零状态失败，首先报告当前实现缺少 `populateProductContextMenu` 或 `isProductMenuContext`。如果脚本因路径、编码或 PowerShell 语法错误失败，先修正脚本，不得先改生产代码。

### Task 2: 声明共享菜单与本地适配器接口

**Files:**
- Modify: `E:\AAALTZK\DataCatalogWidget.h`

- [x] **Step 1: Add the exact private declarations**

在现有菜单和导出声明附近加入以下接口，保留公共回调签名不变：

```cpp
    void populateProductContextMenu(
        QMenu& menu,
        const CatalogContext& context);
    bool isProductMenuContext(const CatalogContext& context) const;
    bool isProductMenuDirectory(const CatalogContext& context) const;
    bool isProductMenuRoot(const CatalogContext& context) const;
    QgsMimeDataUtils::UriList localProductUrisForContext(
        const CatalogContext& context,
        bool checkedOnly) const;
    QVector<CatalogContext> checkedLocalContextsUnder(
        const CatalogContext& context) const;
    void setLocalProductSubtreeCheck(
        const CatalogContext& context,
        Qt::CheckState state);
    void exportUri(const QgsMimeDataUtils::Uri& uri);
    void batchExportUris(
        const QgsMimeDataUtils::UriList& uris,
        int unloadedCount = 0,
        const QString& title = QStringLiteral("批量导出数据"));
```

扩展现有 `dispatchProductOperation()` 声明，使同一入口可以携带本地 URI，但原数据库参数和默认调用保持兼容：

```cpp
    void dispatchProductOperation(
        const QString& operation,
        const CatalogContext& context,
        const QList<int>& productIds = QList<int>(),
        const QgsMimeDataUtils::UriList& localUris = QgsMimeDataUtils::UriList());
```

- [x] **Step 2: Check declaration uniqueness**

运行：

```powershell
rg -n "populateProductContextMenu|isProductMenuContext|localProductUrisForContext|batchExportUris|dispatchProductOperation" .\DataCatalogWidget.h
```

预期：每个新接口在头文件中只声明一次，原 `ProductOperationCallback` 类型和 `setProductOperationCallback()` 公共接口仍各出现一次。

### Task 3: Implement shared menu construction and source-specific targets

**Files:**
- Modify: `E:\AAALTZK\DataCatalogWidget.cpp`

- [x] **Step 1: Route both product sources to the shared builder**

在 `populateContextMenu()` 开头使用：

```cpp
    if (isProductMenuContext(context))
    {
        populateProductContextMenu(menu, context);
        return;
    }
```

把原数据库成果目录/成果节点分支整体移入 `populateProductContextMenu()`，数据库动作文字和顺序保持以下调用不变：

```cpp
menu.addAction(QStringLiteral("新建目录"));
menu.addAction(QStringLiteral("删除目录"));
menu.addAction(QStringLiteral("重命名"));
menu.addAction(QStringLiteral("勾选本节点下全部成果"));
menu.addAction(QStringLiteral("取消勾选本节点下全部成果"));
menu.addAction(QStringLiteral("导入数据"));
menu.addAction(QStringLiteral("导出所选数据"));
menu.addAction(QStringLiteral("按范围导出"));
menu.addAction(QStringLiteral("按制图导出"));
menu.addAction(QStringLiteral("添加到地图"));
menu.addAction(QStringLiteral("添加到主题"));
```

成果/图层节点保留五个动作和原分隔线位置。

- [x] **Step 2: Implement context classification**

使用以下规则，不改变普通数据库表的通用菜单：

```cpp
bool DataCatalogWidget::isProductMenuContext(
    const CatalogContext& context) const
{
    if (context.source == CatalogSource::Database)
    {
        return isProductCatalogDirectory(context.databaseItem) ||
               isProductCatalogProduct(context.databaseItem);
    }
    return isCatalogCollection(context) || isCatalogLayer(context);
}

bool DataCatalogWidget::isProductMenuDirectory(
    const CatalogContext& context) const
{
    return context.source == CatalogSource::Database
        ? isProductCatalogDirectory(context.databaseItem)
        : isCatalogCollection(context);
}
```

`isProductMenuRoot()` 对数据库继续调用 `isProductCatalogRoot()`；本地只识别不可变的 Favorites 根语义，不把数据库 ID 传给本地操作。

- [x] **Step 3: Implement local selected/all URI collection**

`localProductUrisForContext(context, true)` 收集当前本地目录树范围内已勾选的可加载 URI；对本地图层收集当前图层 URI。`checkedLocalContextsUnder()` 必须只递归 `context.viewIndex` 子树，使用当前代理索引和现有 `m_checkedLocalPaths`，不扫描另一棵数据库树。

`localProductUrisForContext(context, false)` 对目录复用 `collectLocalUris()`，对图层复用 `localUris()`，通过 `appendUniqueUri()` 去重。

- [x] **Step 4: Implement local subtree check actions**

递归当前代理节点，在 `Qt::ItemIsUserCheckable` 的索引上调用：

```cpp
m_localProxyModel->setData(
    index,
    state,
    Qt::CheckStateRole);
```

不得直接改写文件系统；状态仍由 `CatalogBrowserProxyModel::setData()` 写入 `m_checkedLocalPaths`。

### Task 4: Unify dispatch while preserving database behavior

**Files:**
- Modify: `E:\AAALTZK\DataCatalogWidget.cpp`

- [x] **Step 1: Preserve the database dispatch block**

`dispatchProductOperation()` 对数据库上下文继续调用原有：

```cpp
m_productOperationCallback(
    operation,
    productDirectoryId(context.databaseItem),
    productId(context.databaseItem),
    productIds);
```

不得修改 `LTZK.cpp` 的产品请求属性、操作字符串、连接参数或插件动作名。

- [x] **Step 2: Add the local adapter branch**

本地分支按操作使用已有本地能力：

```cpp
if (operation == QStringLiteral("map"))
{
    for (const QgsMimeDataUtils::Uri& uri : localUris)
    {
        if (m_loadCallback && isSupportedCatalogUri(uri))
            m_loadCallback(uri);
    }
}
else if (operation == QStringLiteral("export"))
{
    if (isProductMenuDirectory(context))
        batchExportUris(localUris, 0, QStringLiteral("导出所选数据"));
    else if (!localUris.isEmpty())
        exportUri(localUris.first());
}
else if (operation == QStringLiteral("rename-directory"))
{
    renameContextItem(context);
}
```

范围导出、按制图导出、添加到主题以及本地新建/删除目录没有安全等价实现时不执行任何数据库操作；对应动作在菜单中禁用。

- [x] **Step 3: Refactor existing export helpers without changing generic behavior**

把 `exportContextItem()` 中对单个 URI 的保存对话框和 writer 逻辑移入 `exportUri()`，原函数只负责从数据库或本地上下文取 URI 后调用 `exportUri()`。

把 `batchExportContextItems()` 中“输出目录选择、逐 URI 写出、汇总消息”移入 `batchExportUris()`，原函数只负责收集 URI 和未加载数量后调用它。数据库普通菜单的批量导出结果和消息保持原样。

### Task 5: Wire the shared menu actions and run the red test

**Files:**
- Modify: `E:\AAALTZK\DataCatalogWidget.cpp`

- [x] **Step 1: Wire directory actions with source-specific targets**

目录动作使用同一套动作创建顺序；数据库保持原启用条件，本地按以下规则启用：

```cpp
const QgsMimeDataUtils::UriList selectedLocalUris =
    localProductUrisForContext(context, true);
const QgsMimeDataUtils::UriList allLocalUris =
    localProductUrisForContext(context, false);

checkAll->setEnabled(!allLocalUris.isEmpty());
uncheckAll->setEnabled(!allLocalUris.isEmpty());
importData->setEnabled(canImportLocalDirectory(context));
exportData->setEnabled(!selectedLocalUris.isEmpty());
rangeExport->setEnabled(false);
mainAreaExport->setEnabled(false);
addToMap->setEnabled(!allLocalUris.isEmpty() && static_cast<bool>(m_loadCallback));
addToTheme->setEnabled(false);
```

本地导入继续调用 `importContextItem(context)`；勾选动作调用本地子树 helper；地图和导出调用 `dispatchProductOperation()`。

- [x] **Step 2: Wire layer actions with the same five-item layout**

本地图层的 `导出所选数据` 使用 `localUris(context.localItem)`，`添加到地图` 使用同一 dispatcher；范围、按制图和主题动作保留但禁用。数据库成果节点的原有启用条件和 lambda 不变。

- [x] **Step 3: Run the regression test and inspect exact menu ownership**

运行：

```powershell
.\tests\verify_local_data_context_menu.ps1
rg -n "populateContextMenu|populateProductContextMenu|新建目录|按范围导出|批量导出数据" .\DataCatalogWidget.cpp
```

预期：回归脚本通过；成果目录动作只由共享 builder 创建；旧通用菜单只保留其自身动作，不再包含成果目录专用动作。

### Task 6: Compile and verify boundaries

**Files:**
- Modify: `E:\AAALTZK\DataCatalogWidget.h`
- Modify: `E:\AAALTZK\DataCatalogWidget.cpp`
- Create: `E:\AAALTZK\tests\verify_local_data_context_menu.ps1`

- [x] **Step 1: Run focused static checks**

运行：

```powershell
.\tests\verify_local_data_context_menu.ps1
.\tests\verify_data_catalog_product_tree.ps1
.\tests\verify_workspace_data_catalog.ps1
```

预期：三个脚本均输出 passed；产品树检查中的数据库成果目录动作、ID 回调和插件动作名继续存在。

- [x] **Step 2: Build Release x64 in an isolated verification directory**

使用已有 MSBuild 环境，不覆盖当前运行程序：

```powershell
$verifyRoot = 'E:\AAALTZK\x64\Release\verify-local-context-menu'
New-Item -ItemType Directory -Force "$verifyRoot\out" | Out-Null
New-Item -ItemType Directory -Force "$verifyRoot\int" | Out-Null
msbuild .\LTZK.vcxproj /m /t:Build /p:Configuration=Release /p:Platform=x64 /p:OutDir="$verifyRoot\out\" /p:IntDir="$verifyRoot\int\"
```

预期：构建退出码为 0，输出目录生成 `LTZK.exe`；不得覆盖 `E:\LTZK\bin\LTZK.exe`。

- [x] **Step 3: Check the final diff boundary**

运行：

```powershell
rg -n "quality|质检任务|Quality|m_productOperationCallback|成果目录操作" .\DataCatalogWidget.cpp .\DataCatalogWidget.h .\LTZK.cpp
```

确认此次修改只增加数据目录共享菜单/本地适配器和回归检查，没有改动质检任务管理导出路径或逻辑。当前目录不是可用 Git worktree，因此不执行提交操作，改用文件 diff 和验证脚本作为交付证据。

## Self-review

- 覆盖了菜单文字/顺序、数据库保留、本地适配、勾选状态、单/批量导出和质检导出边界。
- 没有添加物理删除、物理重命名或数据库写入。
- 所有新增接口在头文件、实现和静态检查中的名称保持一致。
- 计划先运行失败检查，再写生产代码，符合 TDD 的 red-green 顺序。
