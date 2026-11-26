# 本地数据树与数据库成果树展开一致化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 隐藏本地图层的 QGIS 内部子节点，使本地图层成为与数据库成果节点一致的叶节点，并禁止本地内部节点回退到旧通用右键菜单。

**Architecture:** 保留现有 `QgsBrowserGuiModel` 和目录懒加载，仅在 `CatalogBrowserProxyModel` 视图边界过滤图层子节点。右键入口增加产品菜单上下文守卫，数据库树和质检任务逻辑不变。

**Tech Stack:** C++17、Qt 5、QGIS 3.28 Browser Model、PowerShell 静态回归脚本、MSBuild Release x64。

---

### Task 1: 添加失败回归检查

**Files:**
- Create: `E:\AAALTZK\tests\verify_local_catalog_layer_leaf.ps1`
- Test: `E:\AAALTZK\DataCatalogWidget.cpp`

- [x] **Step 1: Write the failing test**

创建静态检查，要求 `CatalogBrowserProxyModel` 同时覆盖 `hasChildren()` 与 `filterAcceptsRow()`，对 `QgsLayerItem` 返回叶节点语义，并要求 `showLocalContextMenu()` 在调用 `populateContextMenu()` 前拒绝非产品菜单上下文。

- [x] **Step 2: Run test to verify it fails**

Run:

```powershell
.\tests\verify_local_catalog_layer_leaf.ps1
```

Expected: FAIL，报告缺少 `filterAcceptsRow` 或本地菜单上下文守卫。

### Task 2: 让本地图层成为叶节点

**Files:**
- Modify: `E:\AAALTZK\DataCatalogWidget.cpp`
- Test: `E:\AAALTZK\tests\verify_local_catalog_layer_leaf.ps1`

- [x] **Step 1: Override layer child visibility**

在 `CatalogBrowserProxyModel` 中加入：

```cpp
bool hasChildren(const QModelIndex& parent = QModelIndex()) const override
{
    if (qobject_cast<QgsLayerItem*>(dataItem(parent)))
    {
        return false;
    }
    return QgsBrowserProxyModel::hasChildren(parent);
}

bool filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const override
{
    if (mModel &&
        qobject_cast<QgsLayerItem*>(mModel->dataItem(sourceParent)))
    {
        return false;
    }
    return QgsBrowserProxyModel::filterAcceptsRow(
        sourceRow,
        sourceParent);
}
```

- [x] **Step 2: Add the local context-menu guard**

把 `showLocalContextMenu()` 的入口条件改为：

```cpp
if (!context.localItem || !m_localTree ||
    !isProductMenuContext(context))
{
    return;
}
```

不得修改 `showDatabaseContextMenu()`。

- [x] **Step 3: Run the focused test**

Run:

```powershell
.\tests\verify_local_catalog_layer_leaf.ps1
```

Expected: PASS。

### Task 3: 回归与编译验证

**Files:**
- Modify: `E:\AAALTZK\DataCatalogWidget.cpp`
- Create: `E:\AAALTZK\tests\verify_local_catalog_layer_leaf.ps1`

- [x] **Step 1: Run catalog regression scripts**

Run:

```powershell
.\tests\verify_local_catalog_layer_leaf.ps1
.\tests\verify_local_data_context_menu.ps1
.\tests\verify_data_catalog_product_tree.ps1
.\tests\verify_workspace_data_catalog.ps1
.\tests\verify_data_catalog_checked_operations.ps1
```

Expected: all scripts exit 0。

- [x] **Step 2: Build Release x64 in an isolated output directory**

使用 Visual Studio MSBuild，设置独立 `OutDir`、`IntDir` 和可写 `TEMP/TMP`。Expected: exit 0 and generated `LTZK.exe`，不得覆盖正在运行的 `E:\LTZK\bin\LTZK.exe`。

- [x] **Step 3: Verify scope boundaries**

确认生产代码只修改 `DataCatalogWidget.cpp` 的本地代理模型和本地菜单入口；`LTZK.cpp`、数据库回调、质检任务管理导出逻辑没有改动。当前目录不是 Git 工作区，因此不执行提交。

## Self-review

- 计划覆盖了截图中的 `Fields` 节点、展开箭头、旧通用菜单回退和数据库/质检边界。
- 采用 QGIS 现有代理模型扩展点，不重复实现本地目录扫描。
- 测试先失败再实现，编译输出隔离，不覆盖运行程序。
