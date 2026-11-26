# 数据列表数据源互斥显示 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (recommended) or superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `DataCatalogWidget` 中保留两个标题栏，并让点击一个标题栏时只显示对应数据内容、隐藏另一个数据内容。

**Architecture:** 保留现有垂直 `QSplitter`、数据库树、本地 QGIS 浏览树和标题栏操作按钮；在两个 section 中新增独立的内容容器。标题栏标题使用可点击的 `QToolButton`，点击回调只切换两个内容容器的可见性，并重新分配 splitter 尺寸；标题栏本身始终保留。

**Tech Stack:** C++17、Qt 5.15 Widgets、QGIS browser model、PowerShell 静态回归脚本、Visual Studio/MSBuild x64 Release。

---

## 文件范围

- Modify: `tests/verify_workspace_data_catalog.ps1` — 增加标题点击、内容容器和 splitter 互斥显示的失败优先回归断言。
- Modify: `DataCatalogWidget.cpp` — 将标题改为可点击控件，拆分内容容器，并实现互斥显示和尺寸重分配。
- Modify: `Resources/styles/ltzk_sci_fi.qss` — 为可点击数据目录标题按钮保留科幻主题标题样式。
- Modify: `Resources/styles/ltzk_white.qss` — 为可点击数据目录标题按钮保留白色主题标题样式。
- Modify: `Resources/styles/ltzk_black.qss` — 为可点击数据目录标题按钮保留黑色主题标题样式。
- Modify: `Resources/styles/ltzk_field_notes.qss` — 为可点击数据目录标题按钮保留田野笔记主题标题样式。
- Modify: `Resources/styles/ltzk_steel_warm_white.qss` — 为可点击数据目录标题按钮保留暖白钢主题标题样式。
- No change: `DataCatalogWidget.h` — 所有切换状态都局限在 `buildUi()` 的局部控件和 lambda，不增加持久化状态或公共接口。

### Task 1: Add the failing source-switch regression checks

**Files:**
- Modify: `tests/verify_workspace_data_catalog.ps1`

- [x] **Step 1: Add source-switch assertions before implementation**

在现有数据目录实现断言之后、工作区模块可见性断言之前，加入以下检查。检查只截取 `DataCatalogWidget::buildUi()`，避免将无关的数据库/本地业务实现误判为 UI 行为：

```powershell
$buildUiStart = $catalogText.IndexOf('void DataCatalogWidget::buildUi()')
$buildUiEnd = $catalogText.IndexOf('void DataCatalogWidget::initializeLocalBrowser()', $buildUiStart)
if ($buildUiStart -lt 0 -or $buildUiEnd -lt $buildUiStart) {
    throw 'Data catalog UI construction boundary was not found.'
}
$buildUiText = $catalogText.Substring($buildUiStart, $buildUiEnd - $buildUiStart)

foreach ($required in @(
    'QToolButton** titleButton',
    'catalogSectionTitle',
    'databaseCatalogTitleButton',
    'localCatalogTitleButton',
    'databaseCatalogContent',
    'localCatalogContent',
    'databaseContent->setVisible(true);',
    'localContent->setVisible(true);',
    'databaseTitleButton, &QToolButton::clicked',
    'localTitleButton, &QToolButton::clicked',
    'databaseContent->setVisible(showDatabase)',
    'localContent->setVisible(!showDatabase)',
    'sectionSplitter->setSizes(targetSizes)'
)) {
    if (-not $buildUiText.Contains($required)) {
        throw "Data catalog source-switch implementation is missing: $required"
    }
}

if ($buildUiText.Contains('databaseSection->setVisible(false)') -or
    $buildUiText.Contains('localSection->setVisible(false)')) {
    throw 'Source switching must retain both section headers instead of hiding whole sections.'
}

foreach ($theme in @(
    'ltzk_sci_fi.qss',
    'ltzk_black.qss',
    'ltzk_white.qss',
    'ltzk_field_notes.qss',
    'ltzk_steel_warm_white.qss'
)) {
    $themePath = Join-Path $root (Join-Path 'Resources\styles' $theme)
    $themeText = Get-Content -Raw -Encoding UTF8 $themePath
    if (-not $themeText.Contains('QToolButton[catalogSectionTitle="true"]')) {
        throw "$theme is missing clickable data catalog title styling."
    }
}
```

- [x] **Step 2: Run the regression script and verify the expected RED state**

Run:

```powershell
.\tests\verify_workspace_data_catalog.ps1
```

Expected: the script exits non-zero with `Data catalog source-switch implementation is missing: QToolButton** titleButton` (or the first missing source-switch marker). This confirms the regression checks fail because the requested behavior is not implemented yet, rather than because of an unrelated test error.

### Task 2: Implement clickable headers and independent content containers

**Files:**
- Modify: `DataCatalogWidget.cpp`

- [x] **Step 1: Change the section-header helper to expose a styled clickable title button**

Replace the existing `createSectionHeader()` signature and title label construction with the following shape, while keeping the existing frame property, margins, spacing, and right-side button loop:

```cpp
QFrame* createSectionHeader(
    const QString& text,
    QWidget* parent,
    const QList<QToolButton*>& buttons,
    QToolButton** titleButton)
{
    QFrame* header = new QFrame(parent);
    header->setProperty("catalogSectionHeader", true);
    QHBoxLayout* layout = new QHBoxLayout(header);
    layout->setContentsMargins(8, 4, 4, 4);
    layout->setSpacing(4);

    QToolButton* title = new QToolButton(header);
    title->setProperty("catalogSectionTitle", true);
    title->setText(text);
    title->setToolButtonStyle(Qt::ToolButtonTextOnly);
    title->setAutoRaise(true);
    title->setCursor(Qt::PointingHandCursor);
    title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(title, 1);
    for (QToolButton* button : buttons)
    {
        layout->addWidget(button, 0, Qt::AlignVCenter);
    }

    if (titleButton)
    {
        *titleButton = title;
    }
    return header;
}
```

The helper must have exactly one new output parameter, and both call sites in `buildUi()` must pass a local `QToolButton*` variable.

- [x] **Step 2: Wrap the database tree in `databaseCatalogContent`**

In `buildUi()`, create the title button before adding the header, set its object name, then create a zero-margin `QWidget` content container and move the database tree into it:

```cpp
QToolButton* databaseTitleButton = nullptr;
databaseLayout->addWidget(createSectionHeader(
    QStringLiteral("数据库数据"),
    databaseSection,
    QList<QToolButton*>() << refreshDatabaseButton,
    &databaseTitleButton));
databaseTitleButton->setObjectName(QStringLiteral("databaseCatalogTitleButton"));

QWidget* databaseContent = new QWidget(databaseSection);
databaseContent->setObjectName(QStringLiteral("databaseCatalogContent"));
QVBoxLayout* databaseContentLayout = new QVBoxLayout(databaseContent);
databaseContentLayout->setContentsMargins(0, 0, 0, 0);
databaseContentLayout->setSpacing(0);

m_databaseModel = new QStandardItemModel(this);
m_databaseModel->setHorizontalHeaderLabels(QStringList() << QStringLiteral("数据库数据"));
m_databaseTree = new QTreeView(databaseContent);
// Keep the existing tree properties unchanged.
databaseContentLayout->addWidget(m_databaseTree, 1);
databaseLayout->addWidget(databaseContent, 1);
```

All existing model and `itemChanged` connection code remains after this construction and before the local section is built. Do not hide `databaseSection` itself.

- [x] **Step 3: Wrap local status and tree in `localCatalogContent`**

Build the local header with an output title button, set its object name, and place the existing status label and local tree inside a zero-margin content container:

```cpp
QToolButton* localTitleButton = nullptr;
localLayout->addWidget(createSectionHeader(
    QStringLiteral("本地数据"),
    localSection,
    QList<QToolButton*>() << connectFolderButton << m_disconnectFolderButton << refreshLocalButton,
    &localTitleButton));
localTitleButton->setObjectName(QStringLiteral("localCatalogTitleButton"));

QWidget* localContent = new QWidget(localSection);
localContent->setObjectName(QStringLiteral("localCatalogContent"));
QVBoxLayout* localContentLayout = new QVBoxLayout(localContent);
localContentLayout->setContentsMargins(0, 0, 0, 0);
localContentLayout->setSpacing(0);

m_localStatusLabel = new QLabel(localContent);
// Keep the existing status-label properties and text unchanged.
localContentLayout->addWidget(m_localStatusLabel);

m_localTree = new QgsBrowserTreeView(localContent);
// Keep the existing local-tree properties unchanged.
localContentLayout->addWidget(m_localTree, 1);
localLayout->addWidget(localContent, 1);
```

The title bar and its action buttons stay outside `localContent`, so they remain visible when local data content is hidden.

- [x] **Step 4: Add the mutually exclusive content-selection handler**

After `sectionSplitter->setSizes(...)` and before the existing action-button connections, add this handler:

```cpp
auto selectCatalogSource = [sectionSplitter, databaseContent, localContent](bool showDatabase)
{
    databaseContent->setVisible(showDatabase);
    localContent->setVisible(!showDatabase);

    QTimer::singleShot(0, sectionSplitter, [sectionSplitter, databaseContent, localContent, showDatabase]()
    {
        const QList<int> currentSizes = sectionSplitter->sizes();
        const int totalSize = currentSizes.value(0) + currentSizes.value(1);
        if (totalSize <= 0)
        {
            return;
        }

        const int collapsedSize = qMax(
            databaseContent->parentWidget()->minimumSizeHint().height(),
            localContent->parentWidget()->minimumSizeHint().height());
        const int expandedSize = qMax(collapsedSize, totalSize - collapsedSize);
        const QList<int> targetSizes = showDatabase
            ? (QList<int>() << expandedSize << collapsedSize)
            : (QList<int>() << collapsedSize << expandedSize);
        sectionSplitter->setSizes(targetSizes);
    });
};

databaseContent->setVisible(true);
localContent->setVisible(true);
connect(databaseTitleButton, &QToolButton::clicked, this, [selectCatalogSource]()
{
    selectCatalogSource(true);
});
connect(localTitleButton, &QToolButton::clicked, this, [selectCatalogSource]()
{
    selectCatalogSource(false);
});
```

The handler intentionally does not toggle the clicked side off. Selecting a side always leaves that side visible and hides only the other content container. The `QTimer::singleShot` defers size calculation until Qt has processed the visibility/layout request.

### Task 3: Preserve title-button appearance in every theme

**Files:**
- Modify: `Resources/styles/ltzk_sci_fi.qss`
- Modify: `Resources/styles/ltzk_white.qss`
- Modify: `Resources/styles/ltzk_black.qss`
- Modify: `Resources/styles/ltzk_field_notes.qss`
- Modify: `Resources/styles/ltzk_steel_warm_white.qss`

- [x] **Step 1: Extend each title selector to cover `QToolButton`**

For each theme, change the existing `QLabel[catalogSectionTitle="true"]` rule to include the new button selector, preserving the theme's existing color and font-weight values. Add transparent button metrics so the title control does not look like a toolbar action:

```css
QLabel[catalogSectionTitle="true"], QToolButton[catalogSectionTitle="true"] {
    /* keep the existing theme-specific color and font-weight */
    background: transparent;
    border: 0;
    padding: 0;
    text-align: left;
}
QToolButton[catalogSectionTitle="true"]:hover,
QToolButton[catalogSectionTitle="true"]:pressed {
    background: transparent;
    border: 0;
}
```

Use the existing one-line QSS style used by each file; do not change the section-header background, tree colors, toolbar button styles, or unrelated selectors.

### Task 4: Run the regression checks and build

**Files:**
- Verify: `tests/verify_workspace_data_catalog.ps1`
- Build: `LTZK.vcxproj`

- [x] **Step 1: Run the full data-catalog static regression**

Run:

```powershell
.\tests\verify_workspace_data_catalog.ps1
```

Expected: exit code `0` and output `Workspace data catalog integration verified.`

- [x] **Step 2: Inspect the final diff and confirm scope**

Run:

```powershell
git diff -- DataCatalogWidget.cpp tests/verify_workspace_data_catalog.ps1 Resources/styles/ltzk_sci_fi.qss Resources/styles/ltzk_white.qss Resources/styles/ltzk_black.qss Resources/styles/ltzk_field_notes.qss Resources/styles/ltzk_steel_warm_white.qss
```

Expected: only the title-control/content-container/splitter behavior and corresponding regression/style checks are changed; database queries, QGIS model operations, context-menu handlers, and workspace module visibility code remain unchanged. The workspace has no `.git` metadata, so if the command reports that it is not a repository, use `git diff --no-index` only for individual before/after copies if available and report that no commit could be created.

- [x] **Step 3: Build an isolated x64 Release output**

Run:

```powershell
msbuild LTZK.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /p:OutDir=x64\Release\catalog_source_switch\ /p:IntDir=x64\Release\catalog_source_switch\obj\ /m
```

Expected: MSBuild exits `0`, `DataCatalogWidget.cpp` compiles successfully, and the isolated output contains `LTZK.exe` plus the copied plugin/config runtime files.

- [x] **Step 4: Perform a startup smoke check when the isolated runtime is complete**

Run the generated isolated executable through the existing QGIS startup environment, keep it alive for the existing smoke interval, and terminate only that smoke-test process. Confirm that opening the data list shows both content areas, clicking either title leaves both title bars visible, and the opposite data content disappears. If the local GUI runtime cannot be launched in the current session, report the build and static-test evidence separately instead of claiming GUI verification.

## Verification checklist

- [x] The regression script failed before the implementation and passes after it.
- [x] Both title controls are present and retain their section headers.
- [x] Initial database/local content visibility is explicitly enabled.
- [x] Database title click shows database content and hides local content.
- [x] Local title click shows local content and hides database content.
- [x] Existing data-tree actions and data models are not recreated or cleared.
- [x] All five themes style the new clickable title control.
- [x] x64 Release build succeeds, or any environment limitation is reported with the exact failing command/output.
