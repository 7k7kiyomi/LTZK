$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding Default $source

$pluginLabel = -join ([char[]](0x63D2, 0x4EF6))
$mapGeneralization = -join ([char[]](0x667A, 0x80FD, 0x5236, 0x56FE))
$quality = -join ([char[]](0x5730, 0x56FE, 0x6210, 0x679C, 0x8D28, 0x68C0))
$achievementManagement = -join ([char[]](0x5730, 0x56FE, 0x6210, 0x679C, 0x7BA1, 0x7406))
$serviceManagement = -join ([char[]](0x5730, 0x56FE, 0x670D, 0x52A1, 0x7BA1, 0x7406))

if ($text -match 'menuBar\(\)->addMenu\s*\(\s*QStringLiteral\("' + [regex]::Escape($pluginLabel) + '"\)\s*\)') {
    throw 'The visible top-level Plugin menu is still added to the menu bar.'
}

if ($text -notmatch 'm_pluginMenu\s*=\s*new\s+QMenu\s*\(\s*QStringLiteral\("' + [regex]::Escape($pluginLabel) + '"\)\s*,\s*this\s*\)') {
    throw 'Plugin menu should remain only as a hidden internal container for plugin compatibility.'
}

foreach ($label in @($mapGeneralization, $quality, $achievementManagement, $serviceManagement)) {
    if ($text -notmatch 'menuBar\(\)->addMenu\s*\(\s*QStringLiteral\("' + [regex]::Escape($label) + '"\)\s*\)') {
        throw "Missing business top-level menu: $label"
    }
}



if ($text -match 'm_pluginMenu->addAction\s*\(\s*actionLoadPlugin\s*\)') {
    throw 'The old Load Plugin action is still visible under the plugin menu.'
}

Write-Host 'Business main menu structure verified.'
