$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$ltzkCpp = Join-Path $root 'LTZK.cpp'
$pluginConfig = Join-Path $root 'config\plugin_bindings.json'
$businessMenuTest = Join-Path $root 'tests\verify_business_main_menus.ps1'
$setupDoc = Join-Path $root 'docs\postgis_login_permissions_setup.md'

foreach ($path in @($ltzkCpp, $pluginConfig, $businessMenuTest, $setupDoc)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$ltzkText = Get-Content -Raw -Encoding Default $ltzkCpp
$configText = Get-Content -Raw -Encoding UTF8 $pluginConfig
$businessTestText = Get-Content -Raw -Encoding Default $businessMenuTest
$docText = Get-Content -Raw -Encoding UTF8 $setupDoc

$oldGeneralization = -join ([char[]](0x5730, 0x56FE, 0x7F29, 0x7F16))
$homeGeneralization = -join ([char[]](0x5730, 0x56FE, 0x667A, 0x80FD, 0x5236, 0x56FE))
$internalGeneralization = -join ([char[]](0x667A, 0x80FD, 0x5236, 0x56FE))
$mapService = -join ([char[]](0x5730, 0x56FE, 0x670D, 0x52A1))
$mapServiceManagement = -join ([char[]](0x5730, 0x56FE, 0x670D, 0x52A1, 0x7BA1, 0x7406))

if ($ltzkText -notmatch [regex]::Escape('QStringLiteral("' + $homeGeneralization + '")')) {
    throw 'Home module source should use 地图智能制图.'
}

if ($configText -notmatch [regex]::Escape('"' + $homeGeneralization + '"')) {
    throw 'Plugin binding config should map generalization tools to 智能制图.'
}

if ($businessTestText -notmatch '0x667A,\s*0x80FD,\s*0x5236,\s*0x56FE') {
    throw 'Business menu verification should expect 智能制图.'
}

if ($docText -notmatch [regex]::Escape($internalGeneralization)) {
    throw 'PostGIS setup doc should describe 数据使用者 access to 智能制图.'
}

if ($ltzkText -match [regex]::Escape('QStringLiteral("' + $oldGeneralization + '")')) {
    throw 'LTZK.cpp still contains the old 地图缩编 module display/mapping name.'
}

if ($configText -match [regex]::Escape('"' + $oldGeneralization + '"')) {
    throw 'plugin_bindings.json still contains the old 地图缩编 menuPath.'
}

if ($ltzkText -notmatch [regex]::Escape('QStringLiteral("' + $mapServiceManagement + '")')) {
    throw 'LTZK.cpp should show 地图服务管理 for the service module.'
}

if ($ltzkText -match [regex]::Escape('createModuleLaunchButton(') -and
    $ltzkText -match [regex]::Escape('QStringLiteral("' + $mapService + '"),') -and
    $ltzkText -notmatch [regex]::Escape('QStringLiteral("' + $mapServiceManagement + '"),')) {
    throw 'Home service module still appears to use 地图服务.'
}

Write-Host 'Module display names verified.'


