# 五套主题工作区地图画布默认颜色 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or **superpowers:executing-plans** to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让五套主题在无可见 SHP 图层的工作区使用各自默认画布颜色，并保留有可见 SHP 图层时的白色画布规则。

**Architecture:** 在 `LTZK` 中增加单一的主题默认画布颜色函数，工作区背景应用逻辑和颜色选择器都调用它。保留“自定义颜色优先、业务可见 SHP 使用白色”的现有状态优先级；系统底图按现有 `includeSystemBasemap=false` 语义不参与该判断，并用 PowerShell 源码检查验证所有主题映射和优先级。

**Tech Stack:** C++17、Qt QColor/QgsMapCanvas、PowerShell、MSBuild（若可用）。

---

### Task 1: 建立失败回归检查

**Files:**
- Create: `E:/AAALTZK/tests/verify_theme_canvas_default_colors.ps1`

- [ ] **Step 1: 编写主题映射和优先级断言**

检查 `LTZK.h` 声明 `defaultMapCanvasColor()`，检查 `LTZK.cpp` 为五套主题返回指定颜色，并检查工作区背景与颜色选择器都保留自定义颜色优先、可见 SHP 使用白色、其他情况使用主题默认色的顺序。

- [ ] **Step 2: 运行当前代码确认失败**

运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_theme_canvas_default_colors.ps1
```

预期：失败，因为当前源码没有统一默认画布颜色函数，科幻系和绿色系也没有目标默认颜色映射。

### Task 2: 统一主题默认画布颜色

**Files:**
- Modify: `E:/AAALTZK/LTZK.h`
- Modify: `E:/AAALTZK/LTZK.cpp`

- [ ] **Step 1: 声明并实现 `defaultMapCanvasColor()`**

按设计表返回五套主题颜色，未知主题回退到科幻系浅蓝色。

- [ ] **Step 2: 修改工作区背景应用逻辑**

保留 `m_hasCustomCanvasColor` 分支；将有可见 SHP 图层时的颜色固定为白色；无可见 SHP 图层时调用 `defaultMapCanvasColor()`。

- [ ] **Step 3: 修改颜色选择器当前色逻辑**

保留自定义色和可见 SHP 白色优先级，其他情况调用 `defaultMapCanvasColor()`。

### Task 3: 验证主题和已有画布功能

**Files:**
- Verify: `E:/AAALTZK/tests/verify_theme_canvas_default_colors.ps1`
- Verify: `E:/AAALTZK/tests/verify_theme_aware_startup_workspace.ps1`
- Verify: `E:/AAALTZK/tests/verify_workspace_theme_switches_for_visible_shp.ps1`
- Verify: `E:/AAALTZK/tests/verify_shp_layer_and_canvas_color_controls.ps1`

- [ ] **Step 1: 运行新增回归检查**

确认五套颜色和方案 B 的优先级全部通过。

- [ ] **Step 2: 运行既有主题/画布检查**

确认启动、主题切换、可见 SHP 图层和自定义画布颜色逻辑没有回归。

- [ ] **Step 3: 尝试 Release x64 构建**

若 `msbuild` 可用，运行：

```powershell
msbuild .\LTZK.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m
```

否则记录环境缺少构建工具。
