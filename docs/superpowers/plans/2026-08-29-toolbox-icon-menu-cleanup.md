# 工具箱图标与菜单清理 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 从地图画布工具箱菜单中移除“打开属性表”和“结果面板”，并将工具箱文字按钮替换为圆形 GIS 图标按钮。

**Architecture:** 保留现有 QAction、属性表槽函数、结果面板及其他入口，只改变地图画布工具箱菜单的可见动作集合。新增一个 Qt Resource System SVG 图标，按钮使用 `Qt::ToolButtonIconOnly`、透明控件样式和缩放后的方形尺寸，外层容器预留角框内缩量，继续沿用 `InstantPopup` 弹出菜单。

**Tech Stack:** C++17, Qt 5 Widgets/Resources, QGIS, PowerShell 静态回归脚本, MSBuild/Visual Studio.

---

### Task 1: 写工具箱范围与图标行为的失败回归检查

**Files:**
- Create: `tests/verify_canvas_toolbox_icon_and_menu_scope.ps1`

- [ ] **Step 1: 写出新行为检查**

检查 `LTZK.cpp` 的工具箱构建代码中不再出现 `mapToolboxMenu->addAction(ui.actionOpenAttrTable)`，不再把 `actionToggleResultPanel` 添加到菜单，但仍保留结果面板控制逻辑、图层右键菜单的属性表动作、工具箱其他地图操作，以及图标资源路径和 `Qt::ToolButtonIconOnly`。

```powershell
$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$cpp = Get-Content -LiteralPath (Join-Path $repoRoot "LTZK.cpp") -Raw -Encoding UTF8
$header = Get-Content -LiteralPath (Join-Path $repoRoot "LTZK.h") -Raw -Encoding UTF8
$qrc = Get-Content -LiteralPath (Join-Path $repoRoot "LTZK.qrc") -Raw -Encoding UTF8
$icon = Get-Content -LiteralPath (Join-Path $repoRoot "Resources/toolbox_map_pin.svg") -Raw -Encoding UTF8

function Assert-Contains([string]$Content, [string]$Pattern, [string]$Message) {
    if ($Content -notmatch $Pattern) { throw $Message }
}
function Assert-NotContains([string]$Content, [string]$Pattern, [string]$Message) {
    if ($Content -match $Pattern) { throw $Message }
}

$toolboxStart = $cpp.IndexOf('QMenu* mapToolboxMenu = new QMenu(m_mapCanvasToolboxButton);')
$toolboxEnd = $cpp.IndexOf('m_mapCanvasToolboxButton->setMenu(mapToolboxMenu);', $toolboxStart)
if ($toolboxStart -lt 0 -or $toolboxEnd -lt 0) { throw 'Toolbox menu construction block was not found.' }
$toolboxBlock = $cpp.Substring($toolboxStart, $toolboxEnd - $toolboxStart)

Assert-NotContains $toolboxBlock 'addAction\(ui\.actionOpenAttrTable\)' 'Toolbox menu must not expose Open Attribute Table.'
Assert-NotContains $toolboxBlock 'mapToolboxMenu->addAction\(actionToggleResultPanel\)' 'Toolbox menu must not expose Result Panel.'
Assert-Contains $toolboxBlock 'addAction\(ui\.actionOpenProject\)' 'Toolbox menu must keep Open Project.'
Assert-Contains $toolboxBlock 'addAction\(ui\.actionAddVectorData\)' 'Toolbox menu must keep Add Vector Data.'
Assert-Contains $toolboxBlock 'addAction\(ui\.actionAddRasterData\)' 'Toolbox menu must keep Add Raster Data.'
Assert-Contains $toolboxBlock 'addAction\(ui\.actionZoomIn\)' 'Toolbox menu must keep Zoom In.'
Assert-Contains $toolboxBlock 'addAction\(ui\.actionZoomOut\)' 'Toolbox menu must keep Zoom Out.'
Assert-Contains $toolboxBlock 'addAction\(ui\.actionPan\)' 'Toolbox menu must keep Pan.'
Assert-Contains $toolboxBlock 'addAction\(ui\.actionFullExtent\)' 'Toolbox menu must keep Full Extent.'
Assert-Contains $toolboxBlock 'addAction\(ui\.actionZoomActual\)' 'Toolbox menu must keep Zoom Actual.'

Assert-Contains $cpp 'm_layerTreeView->currentLayer\(\)' 'Layer context behavior must remain available.'
Assert-Contains $cpp 'QAction\* actionOpenAttributeTable = menu\.addAction\(QStringLiteral\("打开属性表"\)\)' 'Layer context menu must keep Open Attribute Table.'
Assert-Contains $cpp 'm_qualityResultPanel->setVisible\(checked\)' 'Result panel control logic must remain available.'
Assert-Contains $cpp 'm_mapCanvasToolboxButton->setIcon\(QIcon\(QStringLiteral\("\:/LTZK/Resources/toolbox_map_pin\.svg"\)\)\)' 'Toolbox button must use the new GIS icon.'
Assert-Contains $cpp 'm_mapCanvasToolboxButton->setToolButtonStyle\(Qt::ToolButtonIconOnly\)' 'Toolbox button must be icon-only.'
Assert-Contains $cpp 'const QSize floatingToolButtonSize = scaledSize\(page, 64, 64\)' 'Toolbox button must use a compact square size.'
Assert-Contains $qrc '<file>Resources/toolbox_map_pin\.svg</file>' 'New toolbox icon must be registered in Qt resources.'
Assert-Contains $icon '地图定位针|toolbox|layer' 'SVG should describe its GIS toolbox purpose.'
Assert-Contains $icon '<circle' 'SVG should contain a circular icon base.'
Assert-Contains $icon '<path' 'SVG should contain vector GIS geometry.'

Write-Host 'Canvas toolbox icon and menu scope verified.'
```

- [ ] **Step 2: Run it before implementation and confirm the expected failure**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tests/verify_canvas_toolbox_icon_and_menu_scope.ps1`

Expected: FAIL because the new SVG does not exist and the current toolbox still uses the text-only 256×42 button and exposes both menu entries.

### Task 2: 添加圆形 GIS 图标资源并登记到 Qt 资源文件

**Files:**
- Create: `Resources/toolbox_map_pin.svg`
- Modify: `LTZK.qrc` near the existing `Resources/LTZK.svg` entry

- [ ] **Step 1: 创建 SVG**

Use a 1024×1024 viewBox with a dark-blue circular base, a cyan map pin, pale latitude/longitude grid strokes, and three cyan layer nodes. Include `<title>地图定位针与图层网格工具箱图标</title>` for resource inspection and accessibility context.

- [ ] **Step 2: 登记 SVG**

Add exactly `<file>Resources/toolbox_map_pin.svg</file>` inside the existing `/LTZK` `<qresource>` block so `:/LTZK/Resources/toolbox_map_pin.svg` resolves at runtime.

### Task 3: 修改工具箱按钮和菜单可见动作

**Files:**
- Modify: `LTZK.cpp:3793-3828`

- [ ] **Step 1: 保留结果面板同步动作但从工具箱菜单移除它**

Construct the result-panel QAction with `new QAction(QStringLiteral("结果面板"), this)` so its existing synchronization with `m_resultPanelToggleButton` remains valid, but do not add it to `mapToolboxMenu`.

- [ ] **Step 2: 从工具箱菜单移除属性表动作**

Delete only the `mapToolboxMenu->addAction(ui.actionOpenAttrTable);` line. Leave `ui.actionOpenAttrTable`'s global connection and the layer tree context-menu action unchanged.

- [ ] **Step 3: 将文字按钮改为圆形图标按钮**

Replace the current button setup with:

```cpp
const QSize floatingToolButtonSize = scaledSize(page, 64, 64);
 const int toolboxInset = scaledInt(page, 12);
 toolboxColumn->setFixedSize(
     floatingToolButtonSize.width() + toolboxInset,
     floatingToolButtonSize.height() + toolboxInset);
m_mapCanvasToolboxButton = new QToolButton(toolboxColumn);
m_mapCanvasToolboxButton->setObjectName(QStringLiteral("workspaceMapToolboxButton"));
m_mapCanvasToolboxButton->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/toolbox_map_pin.svg")));
m_mapCanvasToolboxButton->setIconSize(scaledSize(page, 48, 48));
 m_mapCanvasToolboxButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
 m_mapCanvasToolboxButton->setAttribute(Qt::WA_TranslucentBackground);
 m_mapCanvasToolboxButton->setStyleSheet(QStringLiteral(
     "QToolButton { background: transparent; border: 0; padding: 0; }"
     "QToolButton:hover, QToolButton:pressed { background: transparent; border: 0; }"));
m_mapCanvasToolboxButton->setToolTip(QStringLiteral("打开平台工具箱"));
m_mapCanvasToolboxButton->setAccessibleName(QStringLiteral("工具箱"));
m_mapCanvasToolboxButton->setCursor(Qt::PointingHandCursor);
m_mapCanvasToolboxButton->setAutoRaise(true);
m_mapCanvasToolboxButton->setPopupMode(QToolButton::InstantPopup);
 m_mapCanvasToolboxButton->setFixedSize(floatingToolButtonSize);
 toolboxColumnLayout->addWidget(m_mapCanvasToolboxButton, 0, Qt::AlignLeft | Qt::AlignTop);
```

Keep the existing menu actions for opening projects, adding vector/raster data, and map navigation unchanged.

### Task 4: 运行回归检查并构建验证

**Files:**
- Test: `tests/verify_canvas_toolbox_icon_and_menu_scope.ps1`
- Test: `tests/verify_canvas_toolbox_platform_actions.ps1` (update stale expectations if needed)

- [ ] **Step 1: 运行新回归检查**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tests/verify_canvas_toolbox_icon_and_menu_scope.ps1`

Expected: PASS with `Canvas toolbox icon and menu scope verified.`

- [ ] **Step 2: 更新并运行旧工具箱检查**

Change old assertions that require the two removed menu entries to scope them to the layer context/result-panel logic, then run:

`powershell -NoProfile -ExecutionPolicy Bypass -File tests/verify_canvas_toolbox_platform_actions.ps1`

Expected: PASS with `Canvas toolbox platform actions verified.`

- [ ] **Step 3: 构建 Release x64**

Run: `msbuild LTZK.sln /m /p:Configuration=Release /p:Platform=x64`

Expected: build succeeds and updates `x64\Release\LTZK.exe`; if MSBuild or Qt/QGIS dependencies are unavailable, report the exact command output as an environment limitation.
