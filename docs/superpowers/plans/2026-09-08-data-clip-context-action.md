# 数据裁剪右键入口实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将“数据裁剪”从地图智能综合插件菜单移到数据库/本地具体图层的右键菜单，并复用现有插件窗口触发流程。

**Architecture:** 通过 `FeatureBinding.showInMenu` 保留配置绑定但隐藏菜单动作；`DataCatalogWidget` 只负责展示和触发一个注入的裁剪回调；`LTZK` 回调按插件 ID 与功能名找到绑定并调用现有 `executePluginFeatureAction()`。目录节点不启用右键动作。

**Tech Stack:** C++17、Qt 5、QGIS API、JSON 配置、PowerShell 静态验证、MSBuild x64 Release。

---

### Task 1: 添加失败回归验证

**Files:**
- Modify: `tests/verify_workspace_data_catalog.ps1`
- Create: `tests/verify_data_clip_context_action.ps1`

- [ ] **Step 1: 写出目标行为验证**

  在 `verify_data_clip_context_action.ps1` 中读取 `PluginBindingConfig.h/.cpp`、`config/plugin_bindings.json`、`DataCatalogWidget.h/.cpp` 和 `LTZK.cpp`，断言：
  - `FeatureBinding` 声明/解析 `showInMenu`；
  - 数据裁剪配置包含 `showInMenu: false`；
  - `bindPluginFeatureActions()` 检查该标志；
  - 数据列表公开 `setDataClipCallback` 并创建“数据裁剪”动作；
  - 动作启用条件包含 `isCatalogLayer(context)`；
  - 主窗口为数据列表注入回调，并通过 `map_generalization_tools`/`数据裁剪` 调用现有执行函数。

- [ ] **Step 2: 运行验证并确认按预期失败**

  Run: `.	ests\verify_data_clip_context_action.ps1`

  Expected: FAIL，指出上述新接口或配置尚不存在；该失败证明回归验证不是对现有代码的空验证。

- [ ] **Step 3: 将测试纳入数据列表验证入口**

  在 `tests/verify_workspace_data_catalog.ps1` 中增加对新验证脚本存在性的检查，但不复制其全部断言，避免两份规则漂移。

### Task 2: 扩展插件绑定配置并隐藏菜单入口

**Files:**
- Modify: `PluginBindingConfig.h:10-24`
- Modify: `PluginBindingConfig.cpp:35-48`
- Modify: `config/plugin_bindings.json:388-397`
- Modify: `LTZK.cpp:7460-7478`

- [ ] **Step 1: 增加兼容字段**

  在 `FeatureBinding` 增加：

  ```cpp
  bool showInMenu = true;
  ```

  在 `readFeatureBinding()` 中读取：

  ```cpp
  binding.showInMenu = object.value(QStringLiteral("showInMenu")).toBool(true);
  ```

- [ ] **Step 2: 标记数据裁剪为仅上下文入口**

  在 `config/plugin_bindings.json` 的“数据裁剪”绑定中加入：

  ```json
  "showInMenu": false,
  ```

- [ ] **Step 3: 过滤菜单动作创建**

  在 `bindPluginFeatureActions()` 的绑定循环中，将跳过条件扩展为：

  ```cpp
  if (!binding.enabled || !binding.showInMenu || binding.menuPath.isEmpty() || binding.featureName.isEmpty())
  {
      continue;
  }
  ```

- [ ] **Step 4: 运行回归验证**

  Run: `.	ests\verify_data_clip_context_action.ps1`

  Expected: 仍可能因右键回调尚未实现而失败，但配置字段和菜单隐藏断言应通过；记录剩余失败点后进入 Task 3。

### Task 3: 增加数据列表右键动作和主窗口回调

**Files:**
- Modify: `DataCatalogWidget.h:24-86`
- Modify: `DataCatalogWidget.cpp:306-324,1048-1107`
- Modify: `LTZK.h:145-170`
- Modify: `LTZK.cpp:3998-4005,7390-7494`

- [ ] **Step 1: 增加数据裁剪回调接口**

  在 `DataCatalogWidget` 中增加：

  ```cpp
  using DataClipCallback = std::function<void()>;
  void setDataClipCallback(DataClipCallback callback);
  ```

  增加成员 `DataClipCallback m_dataClipCallback;`，并在 `.cpp` 中保存回调。

- [ ] **Step 2: 构造右键动作**

  在 `populateContextMenu()` 中创建 `QAction* dataClip = menu.addAction(QStringLiteral("数据裁剪"));`，设置：

  ```cpp
  dataClip->setEnabled(layer && static_cast<bool>(m_dataClipCallback));
  connect(dataClip, &QAction::triggered, this, [this]() {
      if (m_dataClipCallback)
      {
          m_dataClipCallback();
      }
  });
  ```

  `layer` 使用现有 `isCatalogLayer(context)`，因此目录节点保持禁用。

- [ ] **Step 3: 提供主窗口绑定查找入口**

  在 `LTZK` 私有方法中增加 `void executeConfiguredPluginFeature(const QString& pluginId, const QString& featureName);`。实现遍历 `m_pluginBindingConfig.plugins` 和 `plugin.bindings`，匹配插件 ID、功能名和 `binding.enabled`，找到后调用 `executePluginFeatureAction(plugin, binding)`；找不到时显示现有“插件动作未找到”警告。

- [ ] **Step 4: 注入回调**

  在创建 `m_dataCatalogWidget` 并设置 `setLoadCallback()` 后增加：

  ```cpp
  m_dataCatalogWidget->setDataClipCallback([this]() {
      executeConfiguredPluginFeature(
          QStringLiteral("map_generalization_tools"),
          QStringLiteral("数据裁剪"));
  });
  ```

- [ ] **Step 5: 运行回归验证**

  Run: `.	ests\verify_data_clip_context_action.ps1`

  Expected: PASS，确认菜单隐藏、右键动作、图层启用条件和主窗口插件触发链均存在。

### Task 4: 编译、部署和验证

**Files:**
- Modify: none beyond Tasks 1-3
- Output: `x64/Release/LTZK.exe`

- [ ] **Step 1: 运行静态验证集合**

  Run:

  ```powershell
  .\tests\verify_data_clip_context_action.ps1
  .\tests\verify_workspace_data_catalog.ps1
  .\tests\verify_plugin_action_dll_isolation.ps1
  .\tests\verify_release_plugin_deployment.ps1
  ```

  Expected: 每个脚本退出码为 0。

- [ ] **Step 2: 隔离目录编译 Release**

  使用 MSBuild：

  ```powershell
  & 'E:\VS 2019\MSBuild\Current\Bin\MSBuild.exe' LTZK.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /p:OutDir='<isolated>\out\' /p:IntDir='<isolated>\int\' /m /v:minimal /nologo
  ```

  Expected: exit code 0，生成 `LTZK.exe`。

- [ ] **Step 3: 部署并冒烟启动**

  先备份现有 `x64/Release/LTZK.exe`，再复制隔离构建结果到默认 Release 目录和唯一文件名；用配置的 QGIS 环境启动 5 秒，确认进程保持运行，再只结束冒烟测试进程。

- [ ] **Step 4: 复核行为证据**

  用 `rg` 确认 `showInMenu: false`、右键“数据裁剪”和 `executeConfiguredPluginFeature` 存在；确认默认 Release 与唯一副本 SHA256 相同，旧备份哈希不同。
