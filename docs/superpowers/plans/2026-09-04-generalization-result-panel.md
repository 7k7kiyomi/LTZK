# 地图智能综合结果面板接入 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (\`- [ ]\`) syntax for tracking.

**Goal:** Make \`plugin_map_generalization.dll\` feed newly generated error-result SHP files into the existing result panel using the same user-selected output-directory scan as map quality checking.

**Architecture:** Extend the existing plugin binding configuration with an opt-in result-output monitor flag. Generalize the host's result pipeline to both result modules, record the owning module for each loaded result layer, and filter the panel/map canvas by the active result module. Keep the existing directory discovery, recursive SHP scan, field fallback, feature limits, visibility, and locate behavior unchanged.

**Tech Stack:** C++/Qt/QGIS host application, JSON plugin binding configuration, PowerShell static verification scripts, MSBuild.

---

### Task 1: Add a failing regression test for generalization result binding

**Files:**
- Create: \`tests/verify_generalization_result_panel_binding.ps1\`
- Test: \`LTZK.cpp\`, \`LTZK.h\`, \`PluginBindingConfig.h\`, \`PluginBindingConfig.cpp\`, all deployed \`plugin_bindings.json\` copies

- [ ] **Step 1: Write the failing test**

Create a PowerShell verification script that loads the source/config files and fails until all of the following are present:

~~~~powershell
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -Raw -Encoding Default (Join-Path $root 'LTZK.cpp')
$header = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'PluginBindingConfig.h')
$parser = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'PluginBindingConfig.cpp')

if (-not $header.Contains('watchResultOutputs')) {
    throw 'PluginDefinition must expose watchResultOutputs.'
}
if (-not $parser.Contains('watchResultOutputs')) {
    throw 'PluginBindingConfig parser must read watchResultOutputs.'
}

$generalizationTitle = -join ([char[]](0x5730, 0x56FE, 0x667A, 0x80FD, 0x7EFC, 0x5408))
foreach ($required in @(
    'const bool isResultModule =',
    $generalizationTitle,
    'plugin.watchResultOutputs',
    'm_resultLayerModules',
    'm_currentBusinessModuleTitle'
)) {
    if (-not $source.Contains($required)) {
        throw "Generalization result panel wiring is missing: $required"
    }
}

$configPaths = @(
    'config\plugin_bindings.json',
    'dist\LTZK\config\plugin_bindings.json',
    'x64\Debug\config\plugin_bindings.json',
    'x64\Release\config\plugin_bindings.json',
    'x64\Release\verify-build\config\plugin_bindings.json',
    'LTZK_Developer_Package\01_Source\config\plugin_bindings.json',
    'LTZK_Developer_Package\02_Runtime\LTZK\config\plugin_bindings.json'
)
foreach ($relativePath in $configPaths) {
    $path = Join-Path $root $relativePath
    if (-not (Test-Path $path)) {
        throw "Missing deployed plugin config: $path"
    }
    $json = Get-Content -Raw -Encoding UTF8 $path | ConvertFrom-Json
    foreach ($dll in @('plugin_map_quality_check_tools.dll', 'plugin_map_generalization.dll')) {
        $plugin = @($json.plugins | Where-Object { $_.dll -eq $dll }) | Select-Object -First 1
        if (-not $plugin -or $plugin.watchResultOutputs -ne $true) {
            throw "$relativePath must enable watchResultOutputs for $dll."
        }
    }
}

Write-Host 'Generalization result panel binding verified.'
~~~~

- [ ] **Step 2: Run it to verify it fails**

Run:

~~~~powershell
.\tests\verify_generalization_result_panel_binding.ps1
~~~~

Expected: FAIL because \`watchResultOutputs\` and \`m_resultLayerModules\` do not yet exist.

### Task 2: Make plugin result monitoring configuration-driven

**Files:**
- Modify: \`PluginBindingConfig.h:24-32\`
- Modify: \`PluginBindingConfig.cpp:84-90\`
- Modify: \`config/plugin_bindings.json\`
- Modify: \`dist/LTZK/config/plugin_bindings.json\`
- Modify: \`x64/Debug/config/plugin_bindings.json\`
- Modify: \`x64/Release/config/plugin_bindings.json\`
- Modify: \`x64/Release/verify-build/config/plugin_bindings.json\`
- Modify: \`LTZK_Developer_Package/01_Source/config/plugin_bindings.json\`
- Modify: \`LTZK_Developer_Package/02_Runtime/LTZK/config/plugin_bindings.json\`

- [ ] **Step 1: Add the default-off field to the plugin definition**

In \`PluginBindingConfig.h\`, add the field to \`PluginDefinition\`:

~~~~cpp
struct PluginDefinition
{
    bool enabled = true;
    bool watchResultOutputs = false;
    QString id;
    QString dll;
    QVector<PluginActionIcon> actionIcons;
    QVector<FeatureBinding> bindings;
};
~~~~

- [ ] **Step 2: Parse the field**

In \`loadPluginBindingConfig\`, immediately after parsing \`enabled\`, add:

~~~~cpp
plugin.watchResultOutputs = pluginObject.value(QStringLiteral("watchResultOutputs")).toBool(false);
~~~~

- [ ] **Step 3: Enable monitoring for both result-producing plugins in every deployed config**

Add \`"watchResultOutputs": true\` after \`"enabled": true\` in the plugin objects whose \`dll\` is \`plugin_map_quality_check_tools.dll\` or \`plugin_map_generalization.dll\`. Leave unrelated plugin entries absent/default-off.

- [ ] **Step 4: Run the new test**

Run:

~~~~powershell
.\tests\verify_generalization_result_panel_binding.ps1
~~~~

Expected: FAIL only on the not-yet-implemented host wiring (\`const bool isResultModule\`, \`m_resultLayerModules\`, or \`plugin.watchResultOutputs\`).

### Task 3: Generalize result-module state and action-triggered scanning

**Files:**
- Modify: \`LTZK.h:217-234,279-284\`
- Modify: \`LTZK.cpp:2570-2600,4032-4046,4579-4620,4687-4764,4767-4859,4884-4895,6944-6957\`

- [ ] **Step 1: Add module ownership state**

In \`LTZK.h\`, add the following declaration with the existing result state:

~~~~cpp
QMap<QString, QString> m_resultLayerModules;
~~~~

Keep the existing method and member names for source compatibility with current tests; the state map records the active business module for each canonical result path.

- [ ] **Step 2: Add result-module predicates**

In the anonymous namespace near the existing result-path helpers, add:

~~~~cpp
bool isResultModuleTitle(const QString& moduleTitle)
{
    return moduleTitle == QStringLiteral("地图成果质检")
        || moduleTitle == QStringLiteral("地图智能综合");
}
~~~~

Add a private host method declaration in \`LTZK.h\` if needed by the implementation, or keep the predicate local to \`LTZK.cpp\` if no member access is required.

- [ ] **Step 3: Make timer status text generic**

Change the timer status message from \`已加载 %1 个质检 shp 结果图层。\` to \`已加载 %1 个检查结果 shp 图层。\` so both platforms report accurately.

- [ ] **Step 4: Record the active module when loading a result layer**

In \`syncQualityResultShpLayers\`, after inserting the canonical path into \`m_qualityResultLayers\`, add:

~~~~cpp
m_resultLayerModules.insert(canonicalPath, m_currentBusinessModuleTitle);
~~~~

When an existing result layer is removed from the QGIS project, remove its path from \`m_resultLayerModules\` wherever the existing result registry cleanup handles removals; do not clear the entire registry on a module switch.

- [ ] **Step 5: Trigger monitoring from configuration instead of an ID substring**

In \`executePluginFeatureAction\`, replace:

~~~~cpp
const bool watchesQualityResults = plugin.id.contains(QStringLiteral("quality"), Qt::CaseInsensitive);
if (watchesQualityResults)
~~~~

with:

~~~~cpp
const bool watchesResultOutputs = plugin.watchResultOutputs;
if (watchesResultOutputs)
~~~~

Use \`watchesResultOutputs\` for the post-action refresh block as well. This preserves the existing before-action timestamp, post-action immediate scan, and timer behavior for quality checking and adds the same behavior to generalization.

- [ ] **Step 6: Run the new test**

Run:

~~~~powershell
.\tests\verify_generalization_result_panel_binding.ps1
~~~~

Expected: PASS for configuration and action-trigger wiring; the module display/filter assertions may still fail until Task 4 is complete.

### Task 4: Show smart-comprehensive results in the shared panel without cross-module mixing

**Files:**
- Modify: \`LTZK.cpp:4034-4045,4579-4620,4767-4857,4884-4895\`

- [ ] **Step 1: Use shared result wording**

Update the result panel hint, initial summary, empty row, and summary text to use “检查结果 shp” / “检查结果” instead of “质检图层 shp” / “质检输出 shp”. Keep the panel object names unchanged so existing theme and layout behavior remains compatible.

- [ ] **Step 2: Treat both business modules as result modules**

Replace the single-module activation condition with:

~~~~cpp
const bool isResultModule = isResultModuleTitle(moduleTitle);
if (isResultModule)
~~~~

The existing non-result branch continues to stop the timer, hide result layers, update the waiting summary, and clear the visible list.

- [ ] **Step 3: Filter panel rows by owner module**

Inside \`refreshQualityResultPanel\`, before incrementing \`resultLayerCount\`, skip layers whose recorded owner does not equal \`m_currentBusinessModuleTitle\`:

~~~~cpp
if (m_resultLayerModules.value(resultPath) != m_currentBusinessModuleTitle)
{
    continue;
}
~~~~

Keep the existing field discovery and feature-row rendering unchanged.

- [ ] **Step 4: Filter canvas result layers by owner module**

Replace the quality-only canvas condition with \`isResultModuleTitle(m_currentBusinessModuleTitle)\`, and inside the visible-path loop skip paths whose owner does not equal the active module:

~~~~cpp
if (m_resultLayerModules.value(resultPath) != m_currentBusinessModuleTitle)
{
    continue;
}
~~~~

This keeps each module’s result layers out of the other module’s map canvas while preserving checkbox state.

- [ ] **Step 5: Run the focused regression suite**

Run:

~~~~powershell
.\tests\verify_generalization_result_panel_binding.ps1
.\tests\verify_quality_result_scan_after_action_only.ps1
.\tests\verify_quality_result_layers_hidden_on_module_switch.ps1
.\tests\verify_quality_result_panel_autoload.ps1
.\tests\verify_quality_result_board_registry_layers.ps1
~~~~

Expected: all commands exit 0 and print their verification success message.

### Task 5: Build and run the complete relevant verification suite

**Files:**
- Test: \`tests\verify_*.ps1\` related to quality/result/module/config behavior
- Build: \`LTZK.sln\`

- [ ] **Step 1: Run all result/module/config static checks**

Run:

~~~~powershell
$checks = @(
    '.\tests\verify_generalization_result_panel_binding.ps1',
    '.\tests\verify_config_driven_plugin_bindings.ps1',
    '.\tests\verify_quality_result_panel_autoload.ps1',
    '.\tests\verify_quality_result_scan_after_action_only.ps1',
    '.\tests\verify_quality_result_scan_uses_selected_output_dirs.ps1',
    '.\tests\verify_quality_result_scan_ignores_input_layer_dirs.ps1',
    '.\tests\verify_quality_result_layers_hidden_on_module_switch.ps1',
    '.\tests\verify_quality_result_board_registry_layers.ps1',
    '.\tests\verify_quality_result_panel_grouped_message.ps1',
    '.\tests\verify_quality_workspace_result_board_layout.ps1',
    '.\tests\verify_quality_module_loads_plugin_toolbar_icons.ps1',
    '.\tests\verify_module_display_names.ps1'
)
foreach ($check in $checks) { & $check }
~~~~

Expected: every script prints a success message and exits with code 0.

- [ ] **Step 2: Build the Release configuration**

Run:

~~~~powershell
msbuild .\LTZK.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m
~~~~

Expected: \`Build succeeded\` with zero errors.

- [ ] **Step 3: Inspect the final diff/status**

Run:

~~~~powershell
git diff -- LTZK.cpp LTZK.h PluginBindingConfig.h PluginBindingConfig.cpp config tests docs
git status --short
~~~~

Expected: only the result-panel design/plan, new regression test, host/config changes, and synchronized deployment config copies are touched. If Git metadata remains unavailable, record that status inspection/commit cannot be performed in this workspace.
