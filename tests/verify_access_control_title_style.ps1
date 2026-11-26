$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$sourceText = Get-Content -Raw -Encoding UTF8 $source
$title = -join ([char[]](0x6743, 0x9650, 0x7BA1, 0x7406, 0x5B50, 0x5E73, 0x53F0))
$themeFiles = Get-ChildItem (Join-Path $root 'Resources\styles') -Filter '*.qss' | Sort-Object Name

if ($themeFiles.Count -ne 5) {
    throw "Expected five platform theme QSS files, found $($themeFiles.Count)."
}

$accessBlockMatch = [regex]::Match($sourceText, 'QWidget\* LTZK::buildAccessControlPage\(\)[\s\S]*?QWidget\* LTZK::buildSciFiWorkspacePage\(\)')
if (-not $accessBlockMatch.Success) {
    throw 'Access control page block not found.'
}
$accessBlock = $accessBlockMatch.Value

foreach ($needle in @(
    'page->setProperty("ltzkAccessControlBaseStyleSheet", accessControlBaseStyleSheet)',
    'QString accessControlStyle = platformStyleSheet()',
    'if (m_currentTheme == PlatformTheme::SciFi)',
    'page->setStyleSheet(accessControlStyle)',
    "QLabel* titleLabel = new QLabel(QStringLiteral(`"$title`"), topBar)",
    'titleLabel->setObjectName(QStringLiteral("dashboardTitle"))',
    'QGraphicsDropShadowEffect* accessTitleGlow = new QGraphicsDropShadowEffect(titleLabel)',
    'accessTitleGlow->setBlurRadius(scaledInt(titleLabel, 14));',
    'accessTitleGlow->setColor(QColor(86, 185, 220, 105));',
    '#accessControlEditor QTableWidget {'
)) {
    if ($accessBlock -notmatch [regex]::Escape($needle)) {
        throw "Access control title style marker missing: $needle"
    }
 }

if ($sourceText -match [regex]::Escape('workspaceDarkStyleSheet()')) {
    throw 'Permission-management page must not use the fixed Sci-Fi workspaceDarkStyleSheet().'
}

foreach ($forbidden in @(
    '#accessControlEditor QHeaderView {',
    '#accessControlEditor QHeaderView::section {',
    '#accessControlEditor QHeaderView::section:vertical {',
    '#accessControlEditor QTableCornerButton::section {',
    'background: #0b3456;'
)) {
    if ($accessBlock -match [regex]::Escape($forbidden)) {
        throw "Permission-management table header must inherit the active theme instead of using a fixed Sci-Fi override: $forbidden"
    }
}

if ($accessBlock -match [regex]::Escape('background: rgba(7, 28, 49, 245);')) {
    throw 'Permission-management top bar must not override the active theme with a fixed dark background.'
}

foreach ($themeFile in $themeFiles) {
    $qss = Get-Content -Raw -Encoding UTF8 $themeFile.FullName
    if ($qss -notmatch [regex]::Escape('#dashboardTopBar, #accessControlTopBar {')) {
        throw "$($themeFile.Name) must apply one top-bar background rule to both subplatforms."
    }
}

if ($accessBlock -match [regex]::Escape('#dashboardTitle {')) {
    throw 'Access control page must not define its own dashboardTitle style; reuse the platform theme QSS.'
}

if ($sourceText -match [regex]::Escape('#accessControlTitle {')) {
    throw 'Access control title must reuse dashboardTitle style instead of a separate accessControlTitle style.'
}

Write-Host 'Access control title style verified.'


