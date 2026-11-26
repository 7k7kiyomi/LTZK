# 子平台顶部动态插件按钮紧凑宽度 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or **superpowers:executing-plans** to implement this plan task-by-task.

**Goal:** 将子平台顶部动态插件按钮的最小宽度从 132 缩小到 100，同时保持固定顶部按钮和插件按钮的高度、字体、图标及滚动行为不变。

**Architecture:** 保留通用顶部按钮尺寸函数不变，增加一个只覆盖最小宽度的动态插件专用函数，并在 `setWorkspacePluginActions()` 中使用它。

**Tech Stack:** C++17、Qt `QToolButton`/`QSizePolicy`、PowerShell 源码回归检查。

### Task 1：建立失败回归检查

**Files:**
- Create: `E:/AAALTZK/tests/verify_workspace_plugin_button_compact_width.ps1`

- [x] 检查通用按钮仍保留 132 宽度。
- [x] 检查动态插件专用函数设置 100 最小宽度且复用通用函数。
- [x] 检查动态插件按钮调用专用函数，固定按钮仍调用通用函数。
- [x] 运行检查，确认当前源码因缺少专用函数而失败。

### Task 2：实现动态插件按钮紧凑宽度

**Files:**
- Modify: `E:/AAALTZK/LTZK.cpp`

- [x] 在顶部按钮通用函数后新增 `setWorkspacePluginToolButtonCompactMetrics()`。
- [x] 在 `setWorkspacePluginActions()` 将动态插件按钮的尺寸调用替换为专用函数。
- [x] 保持通用函数的 132 最小宽度和其他静态按钮调用不变。

### Task 3：验证

**Files:**
- Verify: `E:/AAALTZK/tests/verify_workspace_plugin_button_compact_width.ps1`
- Verify: `E:/AAALTZK/tests/verify_workspace_all_top_actions_share_scroll.ps1`
- Verify: `E:/AAALTZK/tests/verify_workspace_plugin_overflow_scroll.ps1`
- Verify: `E:/AAALTZK/tests/verify_workspace_topbar_text_under_icons.ps1`

- [x] 运行新增回归检查。
- [x] 运行顶部插件滚动和共用滚动区域检查。
- [x] 已检查构建环境；当前未找到 `msbuild`，因此未执行 Release x64 构建。
