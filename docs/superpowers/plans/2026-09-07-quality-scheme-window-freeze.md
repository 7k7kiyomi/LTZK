# 质检方案配置窗口卡顿与标题对齐修复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 阻止“质检方案配置”错误启动结果目录监控，避免在 GUI 线程递归注册用户目录，并让窗口完成既有的组框标题左对齐初始化。

**Architecture:** 在现有插件级能力开关下面增加功能级意图开关，执行功能时两者同时为真才启动结果监控。“质检方案配置”显式关闭该开关；文件系统监视器只注册所选结果根目录，实际结果发现仍由既有受限递归扫描完成。

**Tech Stack:** C++17、Qt 5 Widgets、QFileSystemWatcher、PowerShell 源码回归检查、MSBuild x64 Release。

---

### Task 1: 建立失败回归检查

**Files:**
- Create: `E:\AAALTZK\tests\verify_quality_scheme_window_monitoring_isolation.ps1`

- [x] **Step 1: 编写检查**

检查 `FeatureBinding` 包含默认开启的 `watchResultOutputs`，JSON 解析功能级覆盖，执行逻辑使用插件级与功能级双重判断，“质检方案配置”在所有配置副本中显式关闭监控，目录监视函数中不存在 `QDirIterator::Subdirectories`，并保留标题覆盖标签的左边缘定位。

- [x] **Step 2: 运行检查确认红灯**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_quality_scheme_window_monitoring_isolation.ps1`

Expected: FAIL，首先报告 `FeatureBinding` 缺少功能级结果监控开关。

### Task 2: 增加功能级结果监控开关

**Files:**
- Modify: `E:\AAALTZK\PluginBindingConfig.h`
- Modify: `E:\AAALTZK\PluginBindingConfig.cpp`
- Modify: `E:\AAALTZK\LTZK.cpp`

- [x] **Step 1: 扩展绑定结构和解析器**

在 `FeatureBinding` 中添加 `bool watchResultOutputs = true;`，并用 `bindingObject.value(QStringLiteral("watchResultOutputs")).toBool(true)` 解析，保持旧配置默认行为。

- [x] **Step 2: 收紧执行条件**

把执行路径改为 `const bool watchesResultOutputs = plugin.watchResultOutputs && binding.watchResultOutputs;`。

- [x] **Step 3: 运行新增检查**

Expected: 此时仍因配置尚未关闭和递归监视仍存在而失败，证明检查覆盖完整链路。

### Task 3: 配置隔离与根目录监视

**Files:**
- Modify: `E:\AAALTZK\config\plugin_bindings.json`
- Modify: `E:\AAALTZK\LTZK_Developer_Package\01_Source\config\plugin_bindings.json`
- Modify: `E:\AAALTZK\package_build\out\config\plugin_bindings.json`
- Modify: `E:\AAALTZK\x64\Release\config\plugin_bindings.json`
- Modify: `E:\AAALTZK\x64\Debug\config\plugin_bindings.json`
- Modify: `E:\AAALTZK\x64\Release\verify-build\config\plugin_bindings.json`
- Modify: `E:\LTZK\config\plugin_bindings.json`
- Modify: `E:\AAALTZK\LTZK.cpp`

- [x] **Step 1: 关闭方案配置监控**

在每个配置副本的“质检方案配置”绑定中加入 `"watchResultOutputs": false`，不修改其他功能。

- [x] **Step 2: 删除递归监视注册**

`updateQualityResultOutputWatcher()` 仅把 `m_qualityResultOutputDirs` 中存在的根目录加入 `directoriesToWatch`，删除在 GUI 线程执行的子目录 `QDirIterator`。

- [x] **Step 3: 运行新增检查确认绿灯**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\verify_quality_scheme_window_monitoring_isolation.ps1`

Expected: PASS。

### Task 4: 同步源码副本并回归

**Files:**
- Modify: `E:\AAALTZK\LTZK_Developer_Package\01_Source\LTZK.cpp`
- Modify: `E:\AAALTZK\LTZK_Developer_Package\01_Source\LTZK.h`
- Modify: `E:\AAALTZK\LTZK_Developer_Package\01_Source\PluginBindingConfig.cpp`
- Modify: `E:\AAALTZK\LTZK_Developer_Package\01_Source\PluginBindingConfig.h`

- [x] **Step 1: 同步本次实际变更的三个源码文件**

将根源码中本次变更同步到 Developer Package 对应副本；`LTZK.h` 不涉及本次类接口变更，因此保持不动。

- [x] **Step 2: 运行相关回归检查**

运行 `verify_config_driven_plugin_bindings.ps1`、`verify_result_output_scan_event_driven.ps1`、`verify_quality_result_output_path_semantic_binding.ps1`、`verify_quality_result_scan_after_action_only.ps1`、`verify_quality_scheme_*`、`verify_quality_window_groupbox_margin_override.ps1` 和 `verify_plugin_title_transparency_and_tree_header_font.ps1`。

### Task 5: 构建与部署验证

**Files:**
- Build: `E:\AAALTZK\LTZK.sln`
- Deploy: `E:\LTZK\bin\LTZK.exe`

- [x] **Step 1: 构建 Release**

Run: `msbuild .\LTZK.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m`

Expected: exit code 0，零编译错误。

- [x] **Step 2: 部署运行文件**

将 `E:\AAALTZK\x64\Release\LTZK.exe` 复制到 `E:\LTZK\bin\LTZK.exe`。

- [x] **Step 3: 最终复验**

重新运行新增检查和核心回归，核对两个 EXE 的 SHA-256 一致，并确认实际配置中只有“质检方案配置”的功能级监控为 `false`。
