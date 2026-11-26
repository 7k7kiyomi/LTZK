$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -Raw -Encoding Default (Join-Path $root 'LTZK.cpp')

foreach ($required in @(
    'QString businessModuleMenuTitle(const QString& moduleTitle)',
    'QString businessModuleDisplayTitle(const QString& menuTitle)',
    'moduleTitle == QStringLiteral("地图智能制图")',
    'QStringLiteral("智能制图")',
    'businessModuleMenuTitle(binding.menuPath.first()) == businessModuleMenuTitle(moduleTitle)',
    'businessModuleMenuTitle(existingMenu->title()) == canonicalTitle',
    'const QString canonicalTitle = businessModuleMenuTitle(title)',
    'businessModuleDisplayTitle(binding.menuPath.first())'
)) {
    if (-not $source.Contains($required)) {
        throw "Smart-comprehensive display/menu alias is missing: $required"
    }
}

Write-Host 'Smart-comprehensive display and menu names are explicitly mapped.'
