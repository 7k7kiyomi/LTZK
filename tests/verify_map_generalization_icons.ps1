$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$qrc = Join-Path $root 'LTZK.qrc'
$source = Join-Path $root 'LTZK.cpp'
$config = Join-Path $root 'config\plugin_bindings.json'

foreach ($requiredFile in @($qrc, $source, $config)) {
    if (-not (Test-Path $requiredFile)) {
        throw "Missing required file: $requiredFile"
    }
}

$icons = @(
    'icon_intelligent_database.svg'
    'icon_intelligent_import.svg'
    'icon_intelligent_metadata.svg'
    'icon_intelligent_download.svg'
    'icon_intelligent_backup.svg'
    'icon_intelligent_convert.svg'
    'icon_intelligent_merge.svg'
    'icon_intelligent_edge.svg'
    'icon_intelligent_generalization.svg'
    'icon_intelligent_quality.svg'
)

$qrcText = Get-Content -Raw -Encoding UTF8 $qrc
$sourceText = Get-Content -Raw -Encoding UTF8 $source
$configText = Get-Content -Raw -Encoding UTF8 $config
$configObject = $configText | ConvertFrom-Json

foreach ($icon in $icons) {
    $iconPath = Join-Path $root "Resources\$icon"
    if (-not (Test-Path $iconPath)) {
        throw "Missing map intelligent comprehensive icon: Resources\$icon"
    }

    if (-not $qrcText.Contains("<file>Resources/$icon</file>")) {
        throw "Icon is not registered in LTZK.qrc: $icon"
    }

    $svgText = Get-Content -Raw -Encoding UTF8 $iconPath
    [xml]$svg = $svgText
    if ($svg.svg.width -ne '24' -or $svg.svg.height -ne '24' -or $svg.svg.viewBox -ne '0 0 24 24') {
        throw "Map intelligent icon must use the 24x24 SVG format: Resources\$icon"
    }

    foreach ($token in @('{{PRIMARY}}', '{{SECONDARY}}', '{{ACCENT}}')) {
        if (-not $svgText.Contains($token)) {
            throw "Map intelligent icon is missing theme token ${token}: Resources\$icon"
        }
    }
}

$generalization = @($configObject.plugins | Where-Object id -eq 'map_generalization_tools')[0]
if ($null -eq $generalization) {
    throw 'Missing map_generalization_tools plugin configuration.'
}

$visibleBindings = @($generalization.bindings | Where-Object { $_.showInMenu -ne $false })
$visibleBindingIcons = @(
    'Resources/icon_intelligent_generalization.svg'
    'Resources/icon_intelligent_quality.svg'
    'Resources/icon_intelligent_convert.svg'
)
if ($visibleBindings.Count -ne $visibleBindingIcons.Count) {
    throw "Expected $($visibleBindingIcons.Count) visible map intelligent bindings, got $($visibleBindings.Count)"
}
for ($index = 0; $index -lt $visibleBindingIcons.Count; $index++) {
    $expectedIcon = $visibleBindingIcons[$index]
    if ($visibleBindings[$index].icon -ne $expectedIcon) {
        throw "Unexpected map intelligent icon at binding index $index. Expected $expectedIcon, got $($visibleBindings[$index].icon)"
    }
}

$unchangedBindings = @{
    'map_quality_check_tools' = @(
        'Resources/mActionMapDataDownload.svg'
        'Resources/mActionMapDataPreprocess.svg'
        'Resources/mActionQualitySchemeConfig.svg'
        'Resources/mActionQualityTaskManager.svg'
        'Resources/mActionQualityReportExport.svg'
        'Resources/mActionMapLayoutConfig.svg'
        'Resources/mActionSheetEdgeSeries.svg'
        'Resources/mActionScaleAccuracy.svg'
    )
    'map_product_manager_tools' = @(
        'Resources/icon_db_config.svg'
        'Resources/icon_data_import.svg'
        'Resources/icon_product_storage.svg'
        'Resources/icon_metadata.svg'
        'Resources/icon_product_search.svg'
        'Resources/icon_version_diff.svg'
        'Resources/icon_theme_manager.svg'
        'Resources/icon_map_service.svg'
    )
}

foreach ($pluginId in $unchangedBindings.Keys) {
    $plugin = @($configObject.plugins | Where-Object id -eq $pluginId)[0]
    if ($null -eq $plugin) {
        throw "Missing unchanged plugin configuration: $pluginId"
    }

    $actualIcons = @($plugin.bindings | Where-Object { $_.showInMenu -ne $false } | ForEach-Object { $_.icon })
    $expectedIcons = @($unchangedBindings[$pluginId])
    if ($actualIcons.Count -ne $expectedIcons.Count) {
        throw "Existing icon binding count changed unexpectedly: $pluginId"
    }
    for ($index = 0; $index -lt $expectedIcons.Count; $index++) {
        if ($actualIcons[$index] -ne $expectedIcons[$index]) {
            throw "Existing icon binding changed unexpectedly: $pluginId at index $index"
        }
    }
}

foreach ($requiredSourceMarker in @(
    'struct PluginIconPalette',
    'pluginIconPaletteForTheme',
    'renderThemeAwareSvgIcon',
    'QSvgRenderer',
    'refreshConfiguredPluginActionIcons',
    'setConfiguredMenuIconSize(QMenu* menu)',
    'PM_SmallIconSize',
    'setIconSize(QSize(topBarScaledInt(scaleWidget, 34), topBarScaledInt(scaleWidget, 34)))',
    'applyConfiguredPluginActionIcons(plugin.actionIcons, pluginActions, platformThemeKey(m_currentTheme))'
)) {
    if (-not $sourceText.Contains($requiredSourceMarker)) {
        throw "Missing map intelligent icon implementation detail: $requiredSourceMarker"
    }
}

foreach ($themeKey in @('white', 'black', 'sci-fi', 'field-notes', 'steel-warm-white')) {
    $themeMarker = 'QStringLiteral("' + $themeKey + '")'
    if (-not $sourceText.Contains($themeMarker)) {
        throw "Missing map intelligent icon palette theme key: $themeKey"
    }
}

foreach ($paletteMarker in @(
    '#C5E987', '#6FD8D0', '#FFC45E',
    '#4E9FA3', '#80B8AA', '#D38A63'
)) {
    if (-not $sourceText.Contains($paletteMarker)) {
        throw "Missing bright map intelligent icon palette color: $paletteMarker"
    }
}

Write-Host 'Map intelligent comprehensive icons are configured, theme-aware, and sized.'
