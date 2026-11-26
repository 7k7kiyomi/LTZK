# SHP 地图画布右键选择画布颜色设计

## 目标

将地图画布颜色功能从工作区右下角的“画布颜色”浮动按钮迁移到中间 SHP 地图画布的右键菜单中。用户在地图画布区域右键时，可以通过“选择画布颜色”打开现有颜色选择对话框，并保持当前主题、自定义颜色保存和画布刷新行为。

## 当前上下文

- 地图画布由 `LTZK::m_mapCanvas` 持有，并在 `buildSciFiWorkspacePage()` 中嵌入工作区。
- 当前浮动按钮 `m_mapCanvasColorButton` 的点击信号连接到 `chooseMapCanvasColor()`。
- `chooseMapCanvasColor()` 已负责读取当前颜色、打开本地化颜色对话框、保存 `m_customCanvasColor`、设置 `m_hasCustomCanvasColor`，并刷新画布。
- `applyWorkspaceShpCanvasBackground()` 在工作区刷新、主题切换和图层栈刷新时应用自定义颜色；自定义颜色优先于主题默认颜色。
- 左侧图层树已有独立的 `.shp` 图层右键菜单，不属于本次入口迁移范围。

## 交互设计

1. 用户在中间地图画布区域按下鼠标右键。
2. 当当前地图画布中存在可见的 `.shp` 图层时，在 QGIS 原生右键菜单中追加该功能。
3. 菜单包含“选择画布颜色”动作。
4. 用户点击动作后复用 `chooseMapCanvasColor()`。
5. 用户取消颜色对话框时，画布颜色和状态保持不变；确认颜色后立即刷新画布。
6. 原“画布颜色”浮动按钮及其连接被移除，避免同一功能保留两个入口。

如果地图画布没有可见 `.shp` 图层，则不显示该功能菜单，避免把 SHP 专属操作暴露在无 SHP 的工作区中。

## 实现方案

使用 QGIS `QgsMapCanvas::contextMenuAboutToShow(QMenu*, QgsMapMouseEvent*)` 原生扩展信号：

- 连接 `contextMenuAboutToShow` 信号，在回调中检查当前可见图层是否包含 SHP。
- 在 QGIS 已创建的原生 `QMenu` 中追加“选择画布颜色”动作。
- 菜单动作的触发函数直接连接到 `LTZK::chooseMapCanvasColor()`，不复制颜色选择逻辑。
- 在 QGIS 已创建的原生 `QMenu` 中追加动作，不替换 QGIS 自带的 CRS 菜单。
- 使用已有 `visibleLayersContainShp()` 作为 SHP 可见性判断，保证判断规则与画布图层栈一致。

不新增 `QgsMapCanvas` 子类，也不使用全局事件过滤器，避免扩大修改范围和改变其他控件的右键行为。

## 文件影响

- 修改 `LTZK.h`：删除不再使用的 `m_mapCanvasColorButton` 成员声明；保留 `chooseMapCanvasColor()` 和自定义颜色状态。
- 修改 `LTZK.cpp`：删除浮动颜色按钮的创建与信号连接；为 `m_mapCanvas` 连接 QGIS 原生右键菜单扩展信号并追加菜单动作；保留现有颜色选择实现。
- 修改 `tests/verify_shp_layer_and_canvas_color_controls.ps1`：将验证重点从浮动按钮改为QGIS 原生地图画布右键菜单，同时继续验证颜色选择状态和主题优先级。

## 错误与边界行为

- `m_mapCanvas` 为空时不注册菜单，也不执行颜色选择。
- 当前没有可见 SHP 图层时，右键不弹出“选择画布颜色”。
- 颜色对话框返回无效颜色时直接返回，不修改 `m_customCanvasColor`、`m_hasCustomCanvasColor` 或画布颜色。
- 主题切换和图层刷新继续调用现有背景应用逻辑；自定义颜色存在时必须优先保留。

## 验证标准

- 静态验证能确认地图画布连接 `contextMenuAboutToShow` 并向原生菜单追加动作。
- 静态验证能确认右键动作文本为“选择画布颜色”，且连接到 `chooseMapCanvasColor()`。
- 静态验证能确认旧的 `workspaceMapCanvasColorButton`、`m_mapCanvasColorButton` 浮动按钮入口已移除。
- 运行验证脚本后，现有 SHP 图层颜色功能、本地化颜色对话框和自定义画布颜色状态检查全部通过。
- 手工验证：加载 SHP 后在画布空白区域右键，菜单出现；选择颜色后画布改变；取消对话框不改变颜色；刷新图层或切换主题后自定义颜色仍保持。
