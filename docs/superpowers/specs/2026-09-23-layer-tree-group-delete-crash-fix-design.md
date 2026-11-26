# 图层树组删除闪退修复设计

## 问题

组节点在右键菜单和删除流程中被直接传给 `QgsLayerTree::toLayer()`。QGIS 3.28 的 `toLayer()` / `toGroup()` 只是 `static_cast`，不会验证节点类型，因此组对象会被按照图层对象布局读取并触发访问冲突。

## 设计

- 所有根据节点类型分支的代码必须先调用 `QgsLayerTree::isLayer()` 或 `QgsLayerTree::isGroup()`，仅在判断成立后调用对应的 `toLayer()` / `toGroup()`。
- 修复右键菜单节点识别、`removeLayer()` 批量删除、质量结果组路径递归，以及同文件中两个相同模式的质量结果树遍历。
- 删除前不主动调用 `selectionModel()->clear()`；由 QGIS 图层树模型在移除节点时维护选择状态，避免额外的同步信号重入。
- 组删除继续只删除图层树节点，不删除磁盘数据；图层节点删除继续从 `QgsProject` 移除图层。

## 验证

- 源码回归检查禁止用 `if (Type* x = toLayer/toGroup(...))` 充当类型判断。
- 检查右键菜单和删除函数中存在显式 `isLayer()` / `isGroup()` 守卫。
- Release x64 编译通过，并运行现有图层树与数据目录回归检查。

