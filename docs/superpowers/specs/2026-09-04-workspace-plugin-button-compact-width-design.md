# 子平台顶部动态插件按钮紧凑宽度设计

## 目标

缩小子平台工作区顶部插件栏中各个动态插件按钮的宽度，使更多插件可以在同一行显示；固定的结果面板、退出账号、返回首页等顶部按钮保持现有尺寸。

## 现状

- `LTZK.cpp` 中的 `setTopBarToolButtonResponsiveMetrics()` 同时服务于固定顶部按钮和动态插件按钮。
- 通用按钮当前最小尺寸为 `topBarScaledInt(scaleWidget, 132)` × `topBarScaledInt(scaleWidget, 84)`。
- 动态插件按钮在 `LTZK::setWorkspacePluginActions()` 中创建，并再次调用通用尺寸函数。
- 顶部插件栏已经由 `QScrollArea` 承载，插件超出可视区域时可以横向滚动，因此不需要通过强制固定宽度来隐藏文字。

## 设计方案

新增动态插件专用的紧凑尺寸函数：

1. 先复用通用尺寸函数，保持字体、图标、高度和尺寸策略一致。
2. 仅将动态插件按钮的最小宽度覆盖为 `topBarScaledInt(scaleWidget, 100)`。
3. 不设置固定宽度或最大宽度，较长的插件名称仍可按 `QToolButton` 的内容尺寸适度变宽，避免文字被截断。
4. 在 `setWorkspacePluginActions()` 中只对 `featureButton` 调用该紧凑函数；固定顶部按钮继续调用通用函数。

## 尺寸优先级

| 按钮类型 | 最小宽度 | 高度/图标 | 处理函数 |
| --- | ---: | --- | --- |
| 固定顶部按钮 | 132（缩放后） | 不变 | `setTopBarToolButtonResponsiveMetrics` |
| 动态插件按钮 | 100（缩放后） | 不变 | `setWorkspacePluginToolButtonCompactMetrics` |

## 验收标准

- 动态插件按钮的最小宽度为缩放后的 100。
- 固定顶部按钮仍保留缩放后的 132 最小宽度。
- 动态插件按钮的图标大小、字体大小、最小高度和 `MinimumExpanding` 策略不变。
- 长插件名称不会被强制裁剪，可以自然撑宽并继续使用顶部横向滚动区域。
