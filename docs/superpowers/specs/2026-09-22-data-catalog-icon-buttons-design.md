# 数据目录标题栏图标按钮设计

日期：2026-09-22

## 目标

将数据目录中“数据库数据”和“本地数据”两个列表标题栏的文字操作按钮改为紧凑纯图标按钮，减少标题栏占用空间，同时保留可发现性、无障碍名称和原有业务行为。

## 范围

本次只处理 `DataCatalogWidget` 标题栏中的 4 个操作按钮：

| 区域 | 当前操作 | 图标资源 |
| --- | --- | --- |
| 数据库数据 | 刷新 | `Resources/mActionRefresh.svg` |
| 本地数据 | 连接文件夹 | `Resources/mIconFolderLink.svg` |
| 本地数据 | 断开连接 | `Resources/mActionUnlink.svg` |
| 本地数据 | 刷新 | `Resources/mActionRefresh.svg` |

右键菜单、数据库连接弹窗及其它业务模块中的文字操作不在范围内。

## 交互与视觉规范

- 使用 `QToolButton` 的 `Qt::ToolButtonIconOnly` 模式，不在标题栏显示文字。
- 每个按钮固定为 `28×28 px`，图标固定为 `18×18 px`，按钮间距沿用现有标题栏布局。
- tooltip 保留现有完整中文说明：刷新数据库数据、连接一个本地数据文件夹、断开当前选中的文件夹连接、刷新本地数据。
- 设置无障碍名称，沿用原按钮短名称“刷新 / 连接文件夹 / 断开连接”，使纯图标按钮仍可被屏幕阅读器识别。
- 保留现有 hover、pressed、disabled 和主题配色语义，只微调 `catalogToolbarButton` 的内边距以适配固定尺寸。

## 实现方案

1. 在 `LTZK.qrc` 中注册 3 个已有 SVG 资源。
2. 调整 `DataCatalogWidget.cpp` 的目录按钮工厂，使其接收图标资源路径、无障碍名称和 tooltip，创建固定尺寸的纯图标按钮。
3. 在 `buildUi()` 中为数据库刷新、本地连接、本地断开、本地刷新传入对应图标；不改变已有 signal/slot 连接及业务方法。
4. 将原先按文字宽度计算的本地按钮压缩逻辑改为统一的 28×28 固定尺寸逻辑，数据库刷新按钮同样使用该逻辑。
5. 在 5 套主题 QSS 中将该类按钮 padding 调整为适合图标的值，不改变主题颜色和 hover 状态。

## 数据流与错误处理

按钮点击仍调用现有的 `refreshDatabaseData()`、`connectLocalFolder()`、`disconnectSelectedLocalFolder()` 和 `refreshLocalFolders()`。不新增数据流、数据库操作或错误分支；现有状态提示、禁用状态和错误消息保持不变。

## 验证

- 更新并运行数据目录静态验证脚本，确认资源已注册、按钮使用 icon-only 模式、尺寸为 28×28、图标为 18×18、tooltip 和无障碍名称存在。
- 确认 4 个按钮的原有槽函数绑定未改变。
- 检查 5 套主题 QSS 的 `catalogToolbarButton` 规则仍存在且无文字按钮宽度依赖。
- 若构建环境可用，执行项目对应的增量构建或至少进行资源与源码静态检查。

## 取舍

本方案使用项目中已有的 QGIS 风格 SVG，而不是依赖系统 `QStyle` 标准图标，原因是跨主题和不同 Windows 环境下的外观更稳定；同时不新增图标绘制代码，降低维护成本。
