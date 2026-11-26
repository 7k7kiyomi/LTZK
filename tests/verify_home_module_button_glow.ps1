$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$sourceText = Get-Content -Raw -Encoding Default $source

foreach ($required in @(
    'QGraphicsDropShadowEffect',
    'setBlurRadius(28.0)',
    'setColor(QColor(76, 226, 255, 190))',
    'setOffset(0, 0)',
    'setProperty("homeModuleButton", true)',
    'QToolButton[homeModuleButton=\"true\"]',
    'font-size: 13px',
    'padding: 13px',
    'button->setIconSize(QSize(29, 29))',
    'button->setMinimumSize(QSize(168, 78))',
    'button->setMaximumSize(QSize(216, 90))',
    'QSizePolicy::Fixed, QSizePolicy::Fixed'
)) {
    if (-not $sourceText.Contains($required)) {
        throw "Homepage module button glow/60-percent sizing missing: $required"
    }
}

foreach ($homeButton in @(
    'moduleButton_mapGeneralization',
    'moduleButton_mapQuality',
    'moduleButton_mapAchievementManagement',
    'moduleButton_mapServiceManagement'
)) {
    if (-not $sourceText.Contains("QStringLiteral(`"$homeButton`")")) {
        throw "Homepage module button is not explicitly configured by object name: $homeButton"
    }
}

if ($sourceText.Contains('workspaceModuleButton_mapGeneralization"), QIcon') -and
    $sourceText.Contains('workspaceModuleButton_mapGeneralization");' + "`r`n" + '    button->setProperty("homeModuleButton", true)')) {
    throw 'Workspace module buttons must not receive homepage-only glow sizing.'
}

Write-Host 'Homepage module button glow and 60-percent sizing verified.'
