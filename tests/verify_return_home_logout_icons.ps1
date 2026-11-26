$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$qrcPath = Join-Path $root 'LTZK.qrc'
$returnIconPath = Join-Path (Join-Path $root 'Resources') 'template_icon_return_home.svg'
$logoutIconPath = Join-Path (Join-Path $root 'Resources') 'template_icon_logout.svg'

$source = Get-Content -Raw -Encoding UTF8 $sourcePath
$qrc = Get-Content -Raw -Encoding UTF8 $qrcPath

foreach ($path in @($returnIconPath, $logoutIconPath)) {
    if (-not (Test-Path $path)) { throw "Missing icon file: $path" }
    $svg = Get-Content -Raw -Encoding UTF8 $path
    foreach ($required in @('<svg', 'viewBox="0 0 24 24"', 'stroke="#4ae7ff"', 'stroke-width="2.35"', 'fill="none"', 'stroke-linecap="round"', 'stroke-linejoin="round"')) {
        if (-not $svg.Contains($required)) { throw "Icon $path is not a simple platform line SVG: $required" }
    }
}

foreach ($required in @(
    '<file>Resources/template_icon_return_home.svg</file>',
    '<file>Resources/template_icon_logout.svg</file>'
)) {
    if (-not $qrc.Contains($required)) { throw "QRC missing icon resource: $required" }
}

foreach ($required in @(
    'backButton->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/template_icon_return_home.svg")))',
    'logoutButton->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/template_icon_logout.svg")))',
    'backButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon)',
    'logoutButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon)',

    'm_workspaceLogoutButton->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/template_icon_logout.svg")))',
    'backHomeButton->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/template_icon_return_home.svg")))'
)) {
    if (-not $source.Contains($required)) { throw "Navigation action icon binding missing: $required" }
}

foreach ($forbidden in @(
    'backHomeButton->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/mActionFileOpen.svg")))',
    'm_homeLogoutButton->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/mActionFileExit.svg")))',
    'm_homeLogoutButton->setIcon(',
    'm_homeLogoutButton->setIconSize(',
    'm_workspaceLogoutButton->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/mActionFileExit.svg")))'
)) {
    if ($source.Contains($forbidden)) { throw "Old generic action icon still used: $forbidden" }
}

Write-Host 'Return-home and logout action icons verified.'