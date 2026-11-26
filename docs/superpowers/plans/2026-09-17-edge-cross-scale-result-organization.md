# 图幅接边与跨比例尺结果组织 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 仅让图幅接边和跨比例尺从地图名下的“质检错误结果”目录加载并组织 SHP，保持质检任务管理导出逻辑不变。

**Architecture:** 在 `FeatureBinding` 中增加可选的结果组织标记，只有两个目标功能配置该标记。宿主保存当前标记并在专用模式下把用户选中的输出目录解析为可达的“质检错误结果”目录；通用模式完全保留现有搜索根和分组行为。专用模式只影响结果加载/图层树，不移动或写入文件。

**Tech Stack:** C++/Qt/QGIS 宿主、JSON 插件绑定配置、PowerShell 静态回归测试、现有 MSBuild 构建链。

---

### Task 1: Add the failing regression contract

**Files:**
- Create: `tests/verify_edge_cross_scale_result_organization.ps1`

- [x] **Step 1: Write the failing test**

创建一个静态回归测试，读取 `config/plugin_bindings.json`、`PluginBindingConfig.h/.cpp`、`LTZK.h/.cpp`，验证以下行为：

```powershell
$config = Get-Content -Raw (Join-Path $root 'config\plugin_bindings.json') | ConvertFrom-Json
$qualityPlugin = @($config.plugins | Where-Object id -eq 'map_quality_check_tools')[0]
$edge = @($qualityPlugin.bindings | Where-Object featureName -eq '图幅接边检查')[0]
$cross = @($qualityPlugin.bindings | Where-Object featureName -eq '跨比例尺检查')[0]
$task = @($qualityPlugin.bindings | Where-Object featureName -eq '质检任务管理')[0]

if ($edge.resultOrganization -ne 'mapQualityError' -or
    $cross.resultOrganization -ne 'mapQualityError') { throw '...' }
if ($null -ne $task.resultOrganization) { throw '...' }
if ($header -notmatch 'QString\s+resultOrganization') { throw '...' }
if ($parser -notmatch 'resultOrganization') { throw '...' }
if ($source -notmatch 'mapQualityError') { throw '...' }
if ($source -notmatch '质检错误结果') { throw '...' }
if ($source -notmatch 'merged_index') { throw '...' }
```

测试还应截取 `collectQualityResultSearchRoots()` 的函数体，确认其中仍直接返回 `m_qualityResultOutputDirs`，以锁定通用（包括质检任务管理）路径不变。

- [x] **Step 2: Run test to verify it fails**

运行：

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File .\tests\verify_edge_cross_scale_result_organization.ps1
```

预期：FAIL，原因是当前配置、绑定结构和宿主代码不存在 `resultOrganization` 专用模式。

### Task 2: Add binding metadata without changing task-manager export behavior

**Files:**
- Modify: `PluginBindingConfig.h:15-26`
- Modify: `PluginBindingConfig.cpp:43-57`
- Modify: `config/plugin_bindings.json:120-143`

- [x] **Step 1: Add the optional binding field**

在 `FeatureBinding` 中加入：

```cpp
QString resultOrganization;
```

在 `readFeatureBinding()` 中读取：

```cpp
binding.resultOrganization = object.value(QStringLiteral("resultOrganization")).toString().trimmed();
```

没有该 JSON 字段的绑定得到空字符串；不要改变 `watchResultOutputs` 的默认值和质检任务管理绑定内容。

- [x] **Step 2: Mark only the two requested features**

只在“图幅接边检查”和“跨比例尺检查”的 JSON 绑定对象中增加：

```json
"resultOrganization": "mapQualityError"
```

不要给“质检任务管理”绑定增加该字段，也不要修改其导出相关字段或代码。

- [x] **Step 3: Run the regression test**

运行同一个 PowerShell 测试。预期仍 FAIL，但失败点应从配置/绑定字段转移到宿主尚未实现专用解析逻辑。

### Task 3: Implement bounded canonical-result-root resolution

**Files:**
- Modify: `LTZK.h:230-324`
- Modify: `LTZK.cpp:5112-5368, 5795-5905, 5924-5964, 8177-8345`

- [x] **Step 1: Store the active result organization**

增加宿主状态：

```cpp
QString m_qualityResultOrganization;
```

把 `beginQualityResultMonitoring()` 改成接收可选的组织模式，并在重置监控状态时清空它；普通调用使用空字符串。`executePluginFeatureAction()` 只把 `binding.resultOrganization` 传入。现有 `watchResultOutputs` 判断保持不变。

- [x] **Step 2: Add helpers for the special mode**

在 `LTZK.h` 声明并在 `LTZK.cpp` 实现以下宿主私有辅助函数：

```cpp
bool usesMapQualityErrorResultOrganization() const;
QStringList collectMapQualityErrorSearchRoots() const;
QStringList collectQualityErrorDirectories(const QString& searchBase) const;
bool isQualityResultLayerInSearchRoots(const QString& sourcePath,
                                       const QStringList& searchRoots) const;
```

实现约束：

1. 目录名比较使用 `Qt::CaseInsensitive`，同时兼容 Windows 原生分隔符。
2. `collectQualityErrorDirectories()` 先接受自身名为 `质检错误结果` 的目录，再递归查找该目录名；只从用户选择目录及其有限层级祖先开始查找，禁止从盘符根目录或无界的输入数据路径搜索。
3. 每个候选目录只保留真实目录，并去重；如果输出目录尚未创建，返回空列表，后续现有轮询再次解析。
4. 专用搜索根只包含 `质检错误结果` 目录，不包含“系列图”等中间目录。
5. `isQualityResultLayerInSelectedOutputDirs()` 在专用模式下改为使用专用根判断；通用模式继续使用原实现。

- [x] **Step 3: Scan only canonical error-result roots in special mode**

让 `collectQualityResultSearchRoots()` 在专用模式下返回 `collectMapQualityErrorSearchRoots()`，否则仍返回 `m_qualityResultOutputDirs`。保留现有 SHP 后缀、sidecar 稳定性和完成后扫描机制；不要通过文件名关键词替代目录约束。

明确过滤中间产物：专用模式的 `syncQualityResultShpLayers()` 只能遍历专用根目录，因此位于其外部的 `merged_index.shp` 不会被加载。不要删除、移动或覆盖该文件。

- [x] **Step 4: Keep grouping based on the actual canonical path**

专用根下的相对路径仍由 `ensureQualityResultLayerGroup()` 计算。因为 SHP 源路径真实位于 `<地图名>/质检错误结果/<严重程度>/`，现有路径分组自然建立地图名、质检错误结果、严重程度节点；不要为通用模式增加固定目录名称推断。

- [x] **Step 5: Run the regression test**

运行：

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File .\tests\verify_edge_cross_scale_result_organization.ps1
```

预期：PASS，并确认测试检测到专用分支、`质检错误结果` 根目录约束、`merged_index.shp` 不在专用扫描范围，以及通用搜索根仍返回原始选择目录。

### Task 4: Verify no regression in the existing result pipeline

**Files:**
- No additional production files.

- [x] **Step 1: Run focused static checks**

运行：

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File .\tests\verify_edge_cross_scale_result_organization.ps1
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File .\tests\verify_quality_result_scan_uses_selected_output_dirs.ps1
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File .\tests\verify_quality_result_full_regrouping.ps1
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File .\tests\verify_quality_result_safe_tree_loading.ps1
```

预期：全部 PASS；若通用扫描测试失败，只修复专用分支，不放宽或改写质检任务管理路径。

- [x] **Step 2: Build the host**

使用仓库现有的 MSBuild 配置执行宿主构建，确认 `LTZK.cpp`、`LTZK.h` 和绑定解析代码能通过编译。构建前读取现有解决方案/项目配置，使用当前仓库已配置的 Release x64 目标，不修改插件 DLL。

- [x] **Step 3: Review the final diff**

确认变更只涉及：两个绑定的可选标记、宿主专用结果加载分支、专用回归测试、设计/计划文档；确认没有修改质检任务管理导出实现、输出目录写入代码或任何插件 DLL。

- [x] **Step 4: Run the final verification set**

重新运行 focused static checks 和构建命令，记录退出码及完整输出；只有全部通过后再报告修复结果。
