# SHP 地图画布右键选择画布颜色 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将画布颜色选择从浮动按钮迁移到中间 SHP 地图画布的右键菜单。

**Architecture:** 复用现有 `LTZK::chooseMapCanvasColor()` 和自定义颜色状态，仅把用户入口改为 `QgsMapCanvas::contextMenuAboutToShow(QMenu*, QgsMapMouseEvent*)`。在 QGIS 已创建的原生菜单中追加动作，并使用现有 `visibleLayersContainShp()` 判断当前画布是否有可见 SHP 图层；不新增画布子类或全局事件过滤器。

**Tech Stack:** C++、Qt Widgets、QGIS `QgsMapCanvas`、PowerShell 静态验证脚本、Visual Studio/MSBuild。

---

### Task 1: 为地图画布右键菜单增加失败验证

**Files:**
- Modify: `E:/AAALTZK/tests/verify_shp_layer_and_canvas_color_controls.ps1`

- [ ] **Step 1: 替换旧浮动按钮断言**

保留颜色选择函数、颜色状态和 SHP 图层颜色断言；将旧断言
`workspaceMapCanvasColorButton` 改为以下右键菜单结构断言：

```powershell
Assert-Contains $cpp 'm_mapCanvas->setContextMenuPolicy\(Qt::CustomContextMenu\)' "Map canvas should expose a custom context menu."
Assert-Contains $cpp 'm_mapCanvas, &QWidget::customContextMenuRequested' "Map canvas context menu signal should be connected."
Assert-Contains $cpp '选择画布颜色' "Map canvas context menu should label the canvas color action in Chinese."
Assert-Contains $cpp 'chooseMapCanvasColor' "Map canvas context menu should reuse chooseMapCanvasColor."
Assert-NotContains $cpp 'workspaceMapCanvasColorButton' "The old floating canvas color button should be removed."
Assert-NotContains $header 'm_mapCanvasColorButton' "The old floating canvas color member should be removed."
```

同时在脚本中加入与现有 `Assert-Contains` 对应的 `Assert-NotContains` 函数；该函数在内容匹配时抛出传入错误信息。

- [ ] **Step 2: 运行验证脚本确认当前实现失败**

Run:

```powershell
./tests/verify_shp_layer_and_canvas_color_controls.ps1
```

Expected: FAIL，原因是当前源码仍然使用浮动颜色按钮且没有地图画布右键菜单入口。

### Task 2: 将画布颜色入口迁移到地图画布右键菜单

**Files:**
- Modify: `E:/AAALTZK/LTZK.h:269-270`
- Modify: `E:/AAALTZK/LTZK.cpp:2242-2247`
- Modify: `E:/AAALTZK/LTZK.cpp:3761-3771`

- [ ] **Step 1: 删除旧按钮成员**

从 `LTZK.h` 删除：

```cpp
QToolButton* m_mapCanvasColorButton = nullptr;
```

保留 `chooseMapCanvasColor()`、`m_customCanvasColor` 和 `m_hasCustomCanvasColor`。

- [ ] **Step 2: 在画布初始化处注册自定义右键菜单**

在 `m_mapCanvas = new QgsMapCanvas(this);` 之后、背景初始化前后均可，但必须在对象创建后添加以下连接：

```cpp
m_mapCanvas->setContextMenuPolicy(Qt::CustomContextMenu);
connect(m_mapCanvas, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
    if (!m_mapCanvas)
    {
        return;
    }

    const QList<QgsMapLayer*> canvasLayers = m_mapCanvas->layers();
    if (!visibleLayersContainShp(canvasLayers))
    {
        return;
    }

    QMenu menu(m_mapCanvas);
    QAction* actionChooseCanvasColor = menu.addAction(QStringLiteral("选择画布颜色"));
    connect(actionChooseCanvasColor, &QAction::triggered, this, &LTZK::chooseMapCanvasColor);
    menu.exec(m_mapCanvas->mapToGlobal(pos));
    });
```

该回调只负责右键入口和 SHP 可见性过滤，颜色读取、对话框、本地化和刷新继续由既有函数负责。

- [ ] **Step 3: 删除工作区浮动按钮创建逻辑**

从 `buildSciFiWorkspacePage()` 中删除 `m_mapCanvasColorButton` 的创建、属性设置、信号连接和 `toolboxColumnLayout->addWidget(...)`，保留工具箱按钮及其布局，不改变地图画布容器的布局结构。

- [ ] **Step 4: 保持现有颜色状态逻辑不变**

不要修改以下行为：

```cpp
if (m_hasCustomCanvasColor)
{
    m_mapCanvas->setCanvasColor(m_customCanvasColor);
    return;
}
```

以及 `chooseMapCanvasColor()` 中仅在有效颜色返回后设置 `m_customCanvasColor` 和 `m_hasCustomCanvasColor` 的逻辑。

### Task 3: 运行静态验证并检查构建影响

**Files:**
- Test: `E:/AAALTZK/tests/verify_shp_layer_and_canvas_color_controls.ps1`
- Test: 相关 `tests/verify_*.ps1` 中不应再引用已删除的 `workspaceMapCanvasColorButton` 或 `m_mapCanvasColorButton`。

- [ ] **Step 1: 搜索旧入口残留**

Run:

```powershell
rg -n "workspaceMapCanvasColorButton|m_mapCanvasColorButton" LTZK.cpp LTZK.h tests docs
```

Expected: 除设计文档中描述“旧入口已移除”的文字外，不存在源码或测试残留；如有其他测试残留，按同样语义更新为右键菜单断言。

- [ ] **Step 2: 运行 SHP 颜色功能验证**

Run:

```powershell
./tests/verify_shp_layer_and_canvas_color_controls.ps1
```

Expected: 输出 `SHP layer and canvas color controls verified.`。

- [ ] **Step 3: 运行与工作区和主题相关的回归验证**

Run:

```powershell
./tests/verify_workspace_theme_switches_for_visible_shp.ps1
./tests/verify_theme_aware_startup_workspace.ps1
./tests/verify_local_gansu_map_interaction_stable.ps1
```

Expected: 每个脚本均成功退出，并输出对应的 verified 信息。

- [ ] **Step 4: 执行项目构建验证**

Run：使用仓库已有的 Visual Studio 解决方案构建 Release 配置，例如：

```powershell
msbuild LTZK.sln /t:Build /p:Configuration=Release /p:Platform=x64
```

Expected: 构建成功；若环境缺少 QGIS/Qt 或构建工具导致失败，记录原始错误并至少完成全部静态验证。

- [ ] **Step 5: 手工交互验收**

启动 `x64/Release/LTZK.exe`，进入包含 SHP 图层的工作区，在地图画布空白区域右键：

1. 菜单显示“选择画布颜色”；
2. 点击后打开颜色对话框；
3. 确认颜色后画布立即改变；
4. 取消对话框后画布颜色不变；
5. 刷新图层或切换主题后自定义颜色仍保持；
6. 没有可见 SHP 图层时右键不显示该菜单项。
