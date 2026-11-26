$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$bindingHeader = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'PluginBindingConfig.h')
$bindingSource = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'PluginBindingConfig.cpp')
$configText = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'config\plugin_bindings.json')
$catalogHeader = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'DataCatalogWidget.h')
$catalogSource = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'DataCatalogWidget.cpp')
$ltzkHeader = Get-Content -Raw -Encoding Default (Join-Path $root 'LTZK.h')
$ltzkSource = Get-Content -Raw -Encoding Default (Join-Path $root 'LTZK.cpp')

foreach ($required in @(
    'bool showInMenu = true;',
    'binding.showInMenu = object.value(QStringLiteral("showInMenu")).toBool(true);'
)) {
    if (-not $bindingHeader.Contains($required) -and -not $bindingSource.Contains($required)) {
        throw "Feature binding menu visibility contract is missing: $required"
    }
}

$dataClipConfig = [regex]::Match(
    $configText,
    '(?s)"featureName"\s*:\s*"数据裁剪".*?(?=\},\s*\{|\]\s*\})')
if (-not $dataClipConfig.Success -or $dataClipConfig.Value -notmatch '"showInMenu"\s*:\s*false') {
    throw '数据裁剪 binding must be retained for action lookup but hidden from the plugin menu.'
}

foreach ($required in @(
    '!binding.showInMenu',
    'using DataClipCallback',
    'void setDataClipCallback',
    'DataClipCallback m_dataClipCallback;',
    'QAction* dataClip = menu.addAction(QStringLiteral("数据裁剪"))',
    'dataClip->setEnabled(layer && static_cast<bool>(m_dataClipCallback));',
    'connect(dataClip, &QAction::triggered',
    'isCatalogLayer(context)'
)) {
    if (-not $bindingSource.Contains($required) -and
        -not $catalogHeader.Contains($required) -and
        -not $catalogSource.Contains($required) -and
        -not $ltzkSource.Contains($required)) {
        throw "Data clip context action implementation is missing: $required"
    }
}

foreach ($required in @(
    'void executeConfiguredPluginFeature(const QString& pluginId, const QString& featureName);',
    'm_dataCatalogWidget->setDataClipCallback',
    'QStringLiteral("map_generalization_tools")',
    'QStringLiteral("数据裁剪")',
    'executePluginFeatureAction(plugin, binding)'
)) {
    if (-not $ltzkHeader.Contains($required) -and -not $ltzkSource.Contains($required)) {
        throw "Main-window data clip plugin trigger is missing: $required"
    }
}

Write-Host 'Data clip context action wiring verified.'
