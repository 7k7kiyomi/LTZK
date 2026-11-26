# 质检方案真实表头匹配与三列宽度修复设计

## 已确认根因

外部插件 DLL 中的质检方案控件是 `QTreeWidget`，对象名为 `treeWidget_TaskModelData`。其六个真实表头依次为：`比例尺 / id`、`质检项`、`质检规则名称`、`质检规则描述`、`质检规则数值`、`质检规则单位`。

宿主 `normalizeQualityWindowTableColumnWidths(QWidget*)` 已支持从 `QTreeView` 获取表头，但当前查找的是不存在的“质检规则数量”。因此 `hasQualitySchemeHeaders` 为 `false`；而位置兜底又只接受 `QTableView`，这个 `QTreeWidget` 被跳过，三列 250 像素设置从未执行。

## 目标与范围

- 只修正真实表头“质检规则数值”的识别。
- 继续将规则名称、规则描述、规则数值三列设为 250 像素和 `QHeaderView::Interactive`。
- 将规则单位列设为 `QHeaderView::Stretch`，填充固定列宽之外的剩余表头空间。
- 不修改插件 DLL、控件对象名、列顺序、数据、滚动或重试逻辑。
- 不影响其他插件表格。

## 方案

将宿主中的表头查找从“质检规则数量”改为“质检规则数值”，并将局部变量命名从 `qualityCountColumn` 改为 `qualityValueColumn`。`hasQualitySchemeHeaders` 随后能够识别 `treeWidget_TaskModelData`，现有 `QTreeView` 表头处理会直接给名称、描述、数值三列应用 `{ 250, 250, 250 }`，再将“质检规则单位”列设为 `Stretch`，使表头填满表格可视宽度。

## 验证

- 新增源码回归检查，要求代码匹配“质检规则数值”，并拒绝旧的“质检规则数量”。
- 新增源码回归检查，要求“质检规则单位”列使用 `QHeaderView::Stretch`。
- 先运行检查确认当前代码因旧表头失败，再实施单点修复并确认通过。
- 运行既有质检表头回归检查。
- 构建 x64 Release，确认 0 个编译错误。
