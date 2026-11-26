$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$sourceText = Get-Content -Raw -Encoding UTF8 $source
$qualityModule = -join ([char[]](0x5730, 0x56FE, 0x6210, 0x679C, 0x8D28, 0x68C0))
$accessModule = -join ([char[]](0x6743, 0x9650, 0x7BA1, 0x7406))
$subplatformSuffix = -join ([char[]](0x5B50, 0x5E73, 0x53F0))

$expectedInitialTitle = 'QStringLiteral("' + $qualityModule + $subplatformSuffix + '")'
$expectedDynamicTitle = 'QStringLiteral("%1' + $subplatformSuffix + '").arg(moduleTitle)'
$expectedAccessTitle = 'QStringLiteral("' + $accessModule + $subplatformSuffix + '")'

foreach ($needle in @(
    $expectedInitialTitle,
    $expectedDynamicTitle,
    $expectedAccessTitle,
    'm_activeModuleTitle->setObjectName(QStringLiteral("dashboardTitle"))',
    'titleLabel->setObjectName(QStringLiteral("dashboardTitle"))',
    'titleGlow->setBlurRadius(14);',
    'titleGlow->setColor(QColor(86, 185, 220, 105));',
    'accessTitleGlow->setBlurRadius(14);',
    'accessTitleGlow->setColor(QColor(86, 185, 220, 105));'
)) {
    if ($sourceText -notmatch [regex]::Escape($needle)) {
        throw "Subplatform title marker missing: $needle"
    }
}

foreach ($oldNeedle in @(
    'QStringLiteral("陇图智控平台 · 地图成果质检平台")',
    'QStringLiteral("陇图智控平台 · %1平台").arg(moduleTitle)',
    'QStringLiteral("陇图智控平台 · 权限管理")',
    'QStringLiteral("陇图智控平台——地图成果质检子平台")',
    'QStringLiteral("陇图智控平台——%1子平台").arg(moduleTitle)',
    'QStringLiteral("陇图智控平台——权限管理子平台")'
)) {
    if ($sourceText -match [regex]::Escape($oldNeedle)) {
        throw "Old subplatform title format remains: $oldNeedle"
    }
}

Write-Host 'All subplatform titles verified.'
