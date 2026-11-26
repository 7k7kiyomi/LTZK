# 五套主题工作区地图画布默认颜色设计

## 目标

进入子平台工作区后，让没有可见 SHP 图层时的地图画布使用当前平台主题的默认颜色；有可见 SHP 图层时继续使用白色，以保持地图数据的可读性。

## 现状与约束

- `LTZK::m_mapCanvas` 在主窗口构造时创建。
- `applyWorkspaceShpCanvasBackground(bool useLightCanvas)` 会在工作区刷新、图层栈刷新和主题切换时设置画布颜色。
- 当前用户自定义颜色由 `m_customCanvasColor` 和 `m_hasCustomCanvasColor` 管理，必须继续优先于主题默认色。
- `useLightCanvas` 由可见 SHP 图层状态传入；方案 B 要保留 `true` 时的白色画布行为。
- 方案 B 中的“可见 SHP 图层”沿用现有语义，指业务 SHP 图层；标记为 `ltzk/systemBasemap` 的系统底图不计入该判断，避免改变既有系统底图主题逻辑。

## 设计

增加 `defaultMapCanvasColor()` 统一返回主题默认颜色：

| 主题 | 默认颜色 |
| --- | --- |
| White | `#FFFFFF` |
| Black | `#050505` |
| SciFi | `#DCEFFA`（浅蓝色） |
| FieldNotes | `#E4F0E2`（浅绿色） |
| SteelWarmWhite | `#E4E7E3`（浅灰色） |

`applyWorkspaceShpCanvasBackground()` 按以下优先级设置颜色：

1. 有用户自定义颜色：使用自定义颜色。
2. 有可见 SHP 图层：使用白色。
3. 其他情况：使用 `defaultMapCanvasColor()`。

颜色选择器的当前颜色计算复用同一优先级，确保打开选择器时显示的颜色与工作区实际画布一致。

## 验收标准

- 五套主题首次进入工作区且没有可见 SHP 图层时，画布分别呈现白色、黑色、浅蓝色、浅绿色和浅灰色。
- 有可见 SHP 图层时，五套主题仍使用白色画布。
- 主题切换会重新应用对应默认颜色。
- 用户手动选择的自定义颜色不被主题切换或图层刷新覆盖。
