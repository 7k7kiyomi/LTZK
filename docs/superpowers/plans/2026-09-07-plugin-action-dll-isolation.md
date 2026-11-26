# Plugin Action DLL Isolation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ensure every configured feature resolves and triggers actions only from its configured plugin DLL, including after that DLL is already loaded.

**Architecture:** Capture the real `QAction` objects added during each DLL's first successful load and cache them under a normalized canonical DLL path. Subsequent feature invocations read only that DLL-scoped cache; there is no fallback to the global action pool.

**Tech Stack:** C++17, Qt `QAction`/`QPointer`/`QMap`, PowerShell regression checks, MSBuild.

---

### Task 1: Add the failing DLL-isolation regression test

**Files:**
- Create: `tests/verify_plugin_action_dll_isolation.ps1`

- [ ] **Step 1: Write the failing test**

Create a source-level regression check that requires both the main source and developer-package mirror to contain a DLL-keyed `QPointer<QAction>` cache, cache helpers, proxy-action filtering, and no `pluginActions = afterActions;` global fallback inside `executePluginFeatureAction`.

```powershell
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$sourcePairs = @(
    @((Join-Path $root 'LTZK.h'), (Join-Path $root 'LTZK.cpp')),
    @((Join-Path $root 'LTZK_Developer_Package\01_Source\LTZK.h'), (Join-Path $root 'LTZK_Developer_Package\01_Source\LTZK.cpp'))
)

foreach ($pair in $sourcePairs) {
    $headerText = Get-Content -Raw -Encoding Default $pair[0]
    $sourceText = Get-Content -Raw -Encoding Default $pair[1]
    foreach ($required in @(
        'QMap<QString, QList<QPointer<QAction>>> m_pluginActionsByDllPath;',
        'void cachePluginActionsForDll(const QString& pluginPath, const QList<QAction*>& actions);',
        'QList<QAction*> pluginActionsForDll(const QString& pluginPath) const;'
    )) {
        if (-not $headerText.Contains($required)) { throw "Missing DLL action cache declaration: $required" }
    }
    foreach ($required in @(
        'QString pluginActionCacheKey(const QString& pluginPath)',
        'void LTZK::cachePluginActionsForDll',
        'QList<QAction*> LTZK::pluginActionsForDll',
        'action->objectName().startsWith(QStringLiteral("pluginFeature_"))',
        'cachePluginActionsForDll(pluginPath, actionsAddedByPlugin(beforeActions, afterActions));',
        'pluginActions = pluginActionsForDll(pluginPath);'
    )) {
        if (-not $sourceText.Contains($required)) { throw "Missing DLL action isolation implementation: $required" }
    }
    $executeStart = $sourceText.IndexOf('void LTZK::executePluginFeatureAction')
    $executeEnd = $sourceText.IndexOf('void LTZK::bindPluginFeatureActions', $executeStart)
    if ($executeStart -lt 0 -or $executeEnd -lt 0) { throw 'Could not locate plugin feature execution body.' }
    $executeBody = $sourceText.Substring($executeStart, $executeEnd - $executeStart)
    if ($executeBody.Contains('pluginActions = afterActions;')) { throw 'Plugin execution still falls back to the global action pool.' }
}

Write-Host 'Plugin actions remain isolated to their configured DLL.'
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File tests\verify_plugin_action_dll_isolation.ps1`

Expected: FAIL because `m_pluginActionsByDllPath` and the DLL cache helpers do not exist.

### Task 2: Implement DLL-scoped action caching

**Files:**
- Modify: `LTZK.h`
- Modify: `LTZK.cpp`
- Modify: `LTZK_Developer_Package/01_Source/LTZK.h`
- Modify: `LTZK_Developer_Package/01_Source/LTZK.cpp`

- [ ] **Step 1: Add private cache API and storage**

Add next to the plugin binding methods and members:

```cpp
void cachePluginActionsForDll(const QString& pluginPath, const QList<QAction*>& actions);
QList<QAction*> pluginActionsForDll(const QString& pluginPath) const;
QMap<QString, QList<QPointer<QAction>>> m_pluginActionsByDllPath;
```

- [ ] **Step 2: Add a stable DLL cache key**

```cpp
QString pluginActionCacheKey(const QString& pluginPath)
{
    QString normalizedPath = QFileInfo(pluginPath).canonicalFilePath();
    if (normalizedPath.isEmpty())
    {
        normalizedPath = QFileInfo(pluginPath).absoluteFilePath();
    }
    return QDir::cleanPath(normalizedPath).toLower();
}
```

- [ ] **Step 3: Cache only live real plugin actions**

```cpp
void LTZK::cachePluginActionsForDll(const QString& pluginPath, const QList<QAction*>& actions)
{
    QList<QPointer<QAction>> cachedActions;
    for (QAction* action : actions)
    {
        if (!action || action->isSeparator()
            || action->objectName().startsWith(QStringLiteral("pluginFeature_")))
        {
            continue;
        }
        if (!cachedActions.contains(QPointer<QAction>(action)))
        {
            cachedActions.append(QPointer<QAction>(action));
        }
    }
    m_pluginActionsByDllPath.insert(pluginActionCacheKey(pluginPath), cachedActions);
}

QList<QAction*> LTZK::pluginActionsForDll(const QString& pluginPath) const
{
    QList<QAction*> actions;
    const QList<QPointer<QAction>> cachedActions = m_pluginActionsByDllPath.value(pluginActionCacheKey(pluginPath));
    for (const QPointer<QAction>& action : cachedActions)
    {
        if (action && !actions.contains(action.data()))
        {
            actions.append(action.data());
        }
    }
    return actions;
}
```

- [ ] **Step 4: Remove the global fallback**

Replace the current post-load action selection with:

```cpp
QList<QAction*> pluginActions;
if (loadedNow)
{
    const QList<QAction*> afterActions = collectPluginExecutableActions(m_pluginMenu, m_pluginToolBar);
    cachePluginActionsForDll(pluginPath, actionsAddedByPlugin(beforeActions, afterActions));
}
pluginActions = pluginActionsForDll(pluginPath);

if (loadedNow)
{
    applyConfiguredPluginActionIcons(plugin.actionIcons, pluginActions);
}
```

- [ ] **Step 5: Run the isolation test to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File tests\verify_plugin_action_dll_isolation.ps1`

Expected: PASS with `Plugin actions remain isolated to their configured DLL.`

### Task 3: Verify regressions and build

**Files:**
- Verify: `tests/*.ps1`
- Build: `LTZK.sln`

- [ ] **Step 1: Run focused plugin-binding checks**

Run the new isolation test plus `verify_config_driven_plugin_bindings.ps1`, `verify_plugins_lazy_loaded_on_action.ps1`, `verify_plugin_manager_initialized_before_binding.ps1`, `verify_quality_module_loads_plugin_toolbar_icons.ps1`, and `verify_workspace_static_top_actions_survive_plugin_reload.ps1`.

Expected: all exit with code 0.

- [ ] **Step 2: Build Release**

Run: `msbuild LTZK.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m`

Expected: build exit code 0 with zero compile/link errors.

- [ ] **Step 3: Inspect the final diff manually**

Confirm no JSON menu names, keywords, or fallback indexes changed and no unrelated source areas were modified. This workspace has no Git metadata, so report changed files directly instead of committing.
