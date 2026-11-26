$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePairs = @(
    @((Join-Path $root 'LTZK.h'), (Join-Path $root 'LTZK.cpp')),
    @((Join-Path $root 'LTZK_Developer_Package\01_Source\LTZK.h'), (Join-Path $root 'LTZK_Developer_Package\01_Source\LTZK.cpp'))
)

foreach ($pair in $sourcePairs) {
    foreach ($path in $pair) {
        if (-not (Test-Path -LiteralPath $path)) {
            throw "Missing required source file: $path"
        }
    }

    $headerText = Get-Content -Raw -Encoding Default -LiteralPath $pair[0]
    $sourceText = Get-Content -Raw -Encoding Default -LiteralPath $pair[1]

    foreach ($required in @(
        'QMap<QString, QList<QPointer<QAction>>> m_pluginActionsByDllPath;',
        'void cachePluginActionsForDll(const QString& pluginPath, const QList<QAction*>& actions);',
        'QList<QAction*> pluginActionsForDll(const QString& pluginPath) const;'
    )) {
        if (-not $headerText.Contains($required)) {
            throw "Missing DLL action cache declaration: $required"
        }
    }

    foreach ($required in @(
        'QString pluginActionCacheKey(const QString& pluginPath)',
        'void LTZK::cachePluginActionsForDll',
        'QList<QAction*> LTZK::pluginActionsForDll',
        'cachePluginActionsForDll(pluginPath, actionsAddedByPlugin(beforeActions, afterActions));',
        'pluginActions = pluginActionsForDll(pluginPath);'
    )) {
        if (-not $sourceText.Contains($required)) {
            throw "Missing DLL action isolation implementation: $required"
        }
    }

    $cacheStart = $sourceText.IndexOf('void LTZK::cachePluginActionsForDll')
    $cacheEnd = $sourceText.IndexOf('QList<QAction*> LTZK::pluginActionsForDll', $cacheStart)
    if ($cacheStart -lt 0 -or $cacheEnd -lt 0) {
        throw 'Could not locate DLL action cache implementation.'
    }
    $cacheBody = $sourceText.Substring($cacheStart, $cacheEnd - $cacheStart)
    if (-not $cacheBody.Contains('action->objectName().startsWith(QStringLiteral("pluginFeature_"))')) {
        throw 'Configured feature proxy actions must not enter a DLL action cache.'
    }

    $executeStart = $sourceText.IndexOf('void LTZK::executePluginFeatureAction')
    $executeEnd = $sourceText.IndexOf('void LTZK::bindPluginFeatureActions', $executeStart)
    if ($executeStart -lt 0 -or $executeEnd -lt 0) {
        throw 'Could not locate plugin feature execution body.'
    }
    $executeBody = $sourceText.Substring($executeStart, $executeEnd - $executeStart)
    if ($executeBody.Contains('pluginActions = afterActions;')) {
        throw 'Plugin execution still falls back to the global action pool.'
    }
}

Write-Host 'Plugin actions remain isolated to their configured DLL.'
