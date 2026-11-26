# 数据列表模块可见性设计

**日期：** 2026-09-08  
**范围：** `LTZK` 工作台左侧 `数据列表` 页签

## 1. 目标

仅在以下业务模块显示“数据列表”页签：

- 地图成果管理（用户称“地图成果仓”）
- 地图智能综合

以下模块隐藏“数据列表”页签：

- 地图成果质检
- 地图服务管理

“图层管理”页签在四个模块中都保持可用。

## 2. 设计方案

工作台仍只创建一个 `DataCatalogWidget` 实例和一个 `QTabWidget`，不在模块切换时销毁或重新创建数据目录。新增一个模块可见性判断 helper，在 `activateBusinessModule()` 设置 `m_currentBusinessModuleTitle` 后调用：

1. 用 `m_workspaceSideTabs->indexOf(m_dataCatalogWidget)` 找到“数据列表”页签。
2. 对“地图成果管理”和“地图智能综合”调用 `setTabVisible(index, true)`。
3. 对“地图成果质检”和“地图服务管理”调用 `setTabVisible(index, false)`。
4. 页签可见时保持 `QTabWidget::South`，页签隐藏时切换为 `QTabWidget::North`，让质检和服务管理模块中的“图层管理”标题位于容器顶部。
5. 如果隐藏前当前页正是数据列表，先切换到“图层管理”，再隐藏页签，避免工作台出现空白或当前 widget 已不可见的状态。

数据目录实例本身不清空、不断开数据库、不删除本地收藏夹；重新进入允许显示的模块时，原连接状态和目录内容仍可用。未知或空模块标题按隐藏处理，保证数据列表不会意外暴露到未授权模块。

## 3. 接口与调用边界

### `LTZK.h`

增加私有方法：

```cpp
void updateDataCatalogVisibility(const QString& moduleTitle);
```

### `LTZK.cpp`

在匿名命名空间增加：

```cpp
bool isDataCatalogVisibleModuleTitle(const QString& moduleTitle);
```

该 helper 只匹配 `地图成果管理` 和 `地图智能综合`。`updateDataCatalogVisibility()` 只操作页签可见性、页签位置和当前页签，不改变插件动作、地图图层、质检结果监控或数据库刷新流程。`activateBusinessModule()` 在设置当前模块标题后调用该 helper，确保所有入口（首页按钮、菜单动作等）行为一致。

## 4. 测试与验收

- 静态回归脚本检查允许模块标题集合、禁止模块标题集合、动态 `South`/`North` 页签位置、`setTabVisible` 调用和隐藏前切换到图层管理的保护逻辑。
- x64 Release 编译成功。
- 默认 Release 程序启动冒烟成功。
- 四个模块切换逻辑不修改 `DataCatalogWidget` 内部数据；现场操作时应观察：成果管理/智能综合可见数据列表，质检/服务管理不可见数据列表但仍可见图层管理。

## 5. 非目标

- 不删除 `DataCatalogWidget`，不改变右键菜单、数据库连接、本地文件夹连接和导出功能。
- 不修改模块名称、不调整模块权限、不改变地图画布或插件工具栏显示。
