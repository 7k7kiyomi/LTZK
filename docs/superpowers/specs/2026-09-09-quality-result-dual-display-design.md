# 检查结果 SHP 双通道显示设计

## 目标

同一个检查结果 SHP 同时出现在左侧“图层管理”和下方“结果面板”。左侧只负责列出图层；结果面板继续负责错误要素明细以及地图显示/隐藏。

## 现状与问题

- 主程序扫描输出目录发现的结果 SHP 使用 `addMapLayer(layer, false)`，因此不进入左侧图层树。
- 插件主动调用宿主 `addVectorLayer()` 时，宿主使用默认 `addMapLayer(layer)`，结果进入左侧图层树，但没有进入 `m_qualityResultLayers`，因此结果面板为空。
- 结果面板只遍历 `m_qualityResultLayers`，地图画布只把结果面板勾选的结果层加入画布图层栈。

## 设计

### 统一结果层登记

新增统一登记方法，负责设置 `ltzk/qualityResultShp`、`ltzk/qualityResultPath`、结果层所属模块，并写入 `m_qualityResultLayers` 和 `m_resultLayerModules`。登记方法不改变 `m_qualityResultVisiblePaths`，所以新结果默认出现在面板但不自动显示在地图上。

### 扫描器路径

输出目录扫描成功创建结果层后，使用默认 `QgsProject::addMapLayer(layer)` 加入左侧图层树，再通过统一登记方法加入结果面板注册表。检查动作触发后启动一次延迟扫描并在有限时间内重试，覆盖结果文件覆盖写入时目录监听不触发的情况。

### 插件主动加层路径

监听 `QgsProject::layersAdded` 时，只在当前结果模块已启动结果监控、且新增图层是 SHP 并位于本次选择的输出目录（含子目录）时登记该图层。这样质检任务管理插件主动加到图层树的结果层也会进入结果面板，同时避免把输入 SHP 当成结果层。

### 地图显示控制

`refreshMapCanvasLayerStack()` 继续跳过结果层的图层树节点，只根据结果面板的 `m_qualityResultVisiblePaths` 加入结果层。左侧图层树的勾选状态不参与结果层的地图显隐控制。

## 验证标准

1. 扫描器发现的结果 SHP 同时出现在左侧图层树和结果面板。
2. 插件主动加入输出目录的结果 SHP 同时出现在左侧图层树和结果面板。
3. 结果面板勾选结果层后地图显示，取消勾选后地图隐藏。
4. 输出目录之外的普通输入 SHP 不进入结果面板。
