$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$themeFiles = @(
    'Resources\styles\ltzk_sci_fi.qss',
    'Resources\styles\ltzk_black.qss',
    'Resources\styles\ltzk_white.qss'
)

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$sourceText = Get-Content -Raw -Encoding UTF8 $source

foreach ($required in @(
    'void setTopBarToolButtonResponsiveMetrics(QToolButton* button, const QWidget* scaleWidget)',
    'QFont topBarFont = button->font();',
    'topBarFont.setPixelSize(topBarScaledInt(scaleWidget, 13));',
    'button->setFont(topBarFont);'
)) {
    if (-not $sourceText.Contains($required)) {
        throw "Workspace plugin top-bar buttons must keep a scoped compact font: $required"
    }
}

foreach ($relativePath in $themeFiles) {
    $path = Join-Path $root $relativePath
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }

    $text = Get-Content -Raw -Encoding UTF8 $path
    if ($text -notmatch 'QToolButton\[topBarTool="true"\]\s*\{[^}]*font-size:\s*13px') {
        throw "Top-bar tool buttons must define a scoped 13px QSS font-size in ${relativePath}."
    }
    if ($text -match 'QToolButton\[topBarTool="true"\]\s*\{[^}]*font-size:\s*(1[6-9]|[2-9][0-9])px') {
        throw "Top-bar tool buttons must not use oversized QSS font-size in ${relativePath}."
    }
}

Write-Host 'Workspace plugin top-bar buttons keep scoped compact fonts.'
