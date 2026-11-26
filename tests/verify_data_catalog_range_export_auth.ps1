$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$ltzkPath = Join-Path $root 'LTZK.cpp'
$ltzkHeader = Join-Path $root 'LTZK.h'
$pluginManagerHeader = Join-Path $root 'LTZKPluginManager.h'
$pluginManagerSource = Join-Path $root 'LTZKPluginManager.cpp'

foreach ($path in @($ltzkPath, $ltzkHeader, $pluginManagerHeader, $pluginManagerSource)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing shared-session component: $path"
    }
}

$ltzkText = Get-Content -Raw -Encoding Default $ltzkPath
$ltzkHeaderText = Get-Content -Raw -Encoding UTF8 $ltzkHeader
$pluginManagerHeaderText = Get-Content -Raw -Encoding UTF8 $pluginManagerHeader
$pluginManagerText = Get-Content -Raw -Encoding UTF8 $pluginManagerSource

foreach ($required in @(
    'resetProductManagerPluginSession',
    'saveUserLog(userName, password)',
    'clearUserLog()',
    '../config/user_login.ini',
    'unloadPluginIfLoaded'
)) {
    if (-not $ltzkText.Contains($required) -and
        -not $ltzkHeaderText.Contains($required) -and
        -not $pluginManagerHeaderText.Contains($required) -and
        -not $pluginManagerText.Contains($required)) {
        throw "Platform-side plugin session handoff contract is missing: $required"
    }
}

foreach ($required in @(
    'bool unloadPluginIfLoaded',
    'unloadPluginIfLoaded(const QString &pluginPath)'
)) {
    if (-not $pluginManagerHeaderText.Contains($required) -and
        -not $pluginManagerText.Contains($required)) {
        throw "Platform plugin lifecycle handoff is missing: $required"
    }
}

foreach ($required in @(
    'resetProductManagerPluginSession();',
    'saveUserLog(userName, password);'
)) {
    if (-not $ltzkText.Contains($required)) {
        throw "Platform login flow is missing plugin session refresh: $required"
    }
}

if ($ltzkText.Contains('LTZKSessionBridge') -or $ltzkText.Contains('publishMainUserSessionToSharedProperty')) {
    throw 'Platform-side fix must not depend on a new plugin session bridge in the template.'
}

Write-Output 'Platform-side plugin session handoff verification passed.'
