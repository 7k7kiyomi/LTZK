# 质检方案配置窗口卡顿与标题对齐修复设计

## 问题与证据

`plugin_map_quality_check_tools` 在插件级启用了 `watchResultOutputs`，因此“质检方案配置”也会进入结果目录监控。该窗口的方案保存控件名为 `lineEdit_OutputDataPath`，当前语义识别仅因对象名含 `output` 就把它当成检查结果目录。默认值指向用户主目录时，`updateQualityResultOutputWatcher()` 会在 GUI 线程递归遍历全部子目录并逐个注册监视，阻塞 Qt 事件循环。窗口样式初始化因此不能稳定完成，原生 `QGroupBox::title` 的左侧偏移仍然可见。

## 方案比较

1. **推荐：插件能力与功能意图双层开关。** 保留插件级 `watchResultOutputs`，在 `FeatureBinding` 增加默认开启的功能级同名开关，只对“质检方案配置”配置为 `false`。这能在源头排除无结果功能，又不改变真正执行质检的功能。
2. **仅按窗口标题排除。** 在监控绑定函数里硬编码“质检方案配置”。改动较小，但业务名称与基础设施耦合，重命名后容易失效。
3. **只把递归扫描搬到后台。** 可以减轻卡顿，但仍会错误监视方案保存目录，且会浪费大量系统监视句柄，未修复语义根因。

采用方案 1，并增加防御性改进：文件监视器只注册用户明确选择的结果根目录，不在窗口打开阶段递归遍历整个目录树。生成完成后的既有 SHP 扫描仍可递归查找结果文件。

## 数据流

点击功能时，`executePluginFeatureAction()` 只有在 `plugin.watchResultOutputs && binding.watchResultOutputs` 为真时才启动结果监控。“质检方案配置”在配置文件中显式关闭功能级开关，因此其 `lineEdit_OutputDataPath` 不会进入结果监控。真正的质检功能保持现有监控流程；选定结果目录后只给根目录注册 `QFileSystemWatcher`，目录变化或工作线程完成后再触发现有的受限 SHP 扫描。

## 标题行为

保留现有 `polishPluginGroupBoxTitles()`：它清空原生组框标题，并把覆盖标签放到 `groupBox->rect().left()`。解除主线程阻塞后，“质检方案配置”可以完成同一条样式初始化路径，从而与“质检任务管理”一致。测试同时锁定窗口标题归类、覆盖标签左边缘和无递归监视三项契约。

## 配置与部署

同步修改工作区、Developer Package、Release 构建目录以及实际运行目录 `E:\LTZK\config` 中的配置，避免源码修复与运行配置不一致。构建完成后将新的 Release 可执行文件部署到 `E:\LTZK\bin\LTZK.exe`。

## 验证

- 新增源码级回归检查，先确认旧代码失败，再确认实现后通过。
- 运行插件绑定、结果监控、质检方案表格和组框标题相关回归检查。
- 构建 x64 Release，并核对生成物与实际部署文件的时间和哈希。

