$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'

foreach ($path in @($source, $header)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$sourceText = Get-Content -LiteralPath $source -Raw -Encoding Default
$headerText = Get-Content -LiteralPath $header -Raw -Encoding Default

if (-not $headerText.Contains('LTZKPluginManager* m_pluginManager = nullptr;')) {
    throw 'LTZK.h must keep a plugin manager member for plugin lifetime management.'
}

$ctorStart = $sourceText.IndexOf('LTZK::LTZK(QWidget *parent)')
$ctorEnd = $sourceText.IndexOf('LTZK::~LTZK()', $ctorStart)
if ($ctorStart -lt 0 -or $ctorEnd -lt 0) {
    throw 'Could not locate LTZK constructor body.'
}

$ctorBody = $sourceText.Substring($ctorStart, $ctorEnd - $ctorStart)
$managerInit = $ctorBody.IndexOf('m_pluginManager = new LTZKPluginManager(this')
$bindingLoad = $ctorBody.IndexOf('loadPluginBindings(&m_pluginBindingConfig')
$bindingBind = $ctorBody.IndexOf('bindPluginFeatureActions(m_pluginBindingConfig)')

if ($managerInit -lt 0) {
    throw 'The plugin manager is never initialized in the LTZK constructor.'
}

if ($bindingLoad -lt 0 -or $bindingBind -lt 0) {
    throw 'Plugin binding load/bind calls are missing from the LTZK constructor.'
}

if ($managerInit -gt $bindingLoad -or $managerInit -gt $bindingBind) {
    throw 'The plugin manager must be initialized before plugin bindings are loaded and bound.'
}

Write-Host 'Plugin manager is initialized before config-driven plugin binding.'
