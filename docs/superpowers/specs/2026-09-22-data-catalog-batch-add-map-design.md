# 数据目录多选批量添加到地图设计

日期：2026-09-22

## 根因

数据库成果和本地数据的产品菜单在计算“添加到地图”参数时，只读取当前右键上下文的产品 ID 或 URI。本地目录路径传入当前目录的全部 URI，数据库目录路径没有传递产品 ID；因此跨节点勾选的其它数据不会进入地图加载回调。

## 目标与范围

当用户在本地数据或数据库数据列表中勾选多个可加载数据，并在其中一个已勾选项上执行“添加到地图”时，一次添加全部已勾选且受支持的数据。未勾选当前项时，保留当前项/当前目录的原有操作语义。只修改产品菜单的地图参数聚合，不改变通用菜单、导出、主题、刷新或数据库业务回调。

## 设计

- 复用 `checkedContextsFor(context)` 获取当前操作范围；该方法已经实现“当前项已勾选才扩展到全局勾选集合”的语义。
- 数据库路径遍历范围内上下文，通过 `productIdsForContext()` 聚合产品 ID，并去重后传给现有 `dispatchProductOperation("map", ...)`。
- 本地路径遍历范围内上下文，通过 `localProductUrisForContext(mapContext, true)` 收集 URI，并使用现有 `appendUniqueUri()` 去重。
- 当前项未勾选时，数据库目录使用当前目录全部产品 ID，单个成果使用当前成果 ID；本地使用当前上下文已有的全部 URI。
- `dispatchProductOperation()`、`m_loadCallback` 和 `m_productOperationCallback` 的接口不变；不新增加载错误分支。

## 验证

- 先在 `verify_data_catalog_checked_operations.ps1` 增加批量地图操作静态契约并确认旧代码失败。
- 实现聚合后运行该脚本及数据目录相关回归脚本。
- 检查数据库和本地聚合都存在、地图回调同时接收 `mapProductIds` 与 `mapLocalUris`，且旧的单项逻辑没有被删除。
