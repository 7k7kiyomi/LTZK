# 本地数据树与数据库成果树展开一致化设计

## 目标

让数据列表中的本地图层与数据库成果节点保持相同的树形语义：目录可以展开，具体数据图层是叶节点；本地右键菜单只对目录和可加载图层生效，并继续复用数据库成果目录菜单。

## 根因

本地树直接使用 `QgsBrowserGuiModel`。矢量图层被展开时，QGIS 会自动创建 `Qgis::BrowserItemType::Fields` 和 `Field` 等内部节点。现有 `CatalogBrowserProxyModel` 只增加勾选状态，没有过滤图层的内部子节点，因此界面显示了 `Fields`。右键点击该节点时，`isProductMenuContext()` 返回 false，随后进入旧的通用菜单，所以截图中仍出现“刷新、数据裁剪、基本信息、批量导出数据”等内容。

数据库成果树由 `QStandardItemModel` 构建，成果节点本身没有字段子节点，因此不会出现上述问题。

## 方案比较

1. **代理模型过滤并强制图层为叶节点（采用）**：在 `CatalogBrowserProxyModel` 中拒绝图层节点的所有子行，并让图层的 `hasChildren()` 返回 false。保留 QGIS 对目录、GeoPackage 等集合节点的懒加载和 URI 解析能力，改动范围最小。
2. **仅按 `Fields`/`Field` 类型过滤**：能处理截图场景，但其他 provider 生成的自定义元数据子节点仍可能泄漏，不足以保证“图层即叶节点”。
3. **重建本地 `QStandardItemModel`**：外观最接近数据库树，但会重复实现目录刷新、Favorites、provider URI 和懒加载，风险和维护成本过高。

## 设计

1. `CatalogBrowserProxyModel::hasChildren()` 对 `QgsLayerItem` 返回 false，使本地图层不再显示展开箭头。
2. `CatalogBrowserProxyModel::filterAcceptsRow()` 拒绝父节点为 `QgsLayerItem` 的所有源模型子行，防止已经生成的 `Fields`、`Field` 或其他 provider 内部节点进入视图。
3. `showLocalContextMenu()` 只允许 `isProductMenuContext(context)` 为 true 的节点创建菜单。目录和图层继续进入 `populateProductContextMenu()`；异常或内部节点不再回退到旧通用菜单。
4. `showDatabaseContextMenu()`、数据库 `m_productOperationCallback`、普通数据库表通用菜单以及质检任务管理导出逻辑保持不变。

## 验收标准

- 本地目录仍可展开；本地矢量和栅格图层不显示展开箭头。
- 本地图层下不再显示 `Fields`、`Field` 或其他内部子节点。
- 本地目录/图层右键继续显示与数据库成果目录/成果节点一致的菜单。
- 非目录/非图层本地内部节点即使由 provider 生成，也不显示旧通用菜单。
- 现有数据目录静态回归检查通过，Release x64 编译通过。

## 非目标

- 不重建本地浏览器模型，不改变本地文件或目录。
- 不修改数据库连接、成果目录回调和普通数据库表行为。
- 不修改质检任务管理、图幅接边或跨比例尺的导出与结果组织逻辑。
