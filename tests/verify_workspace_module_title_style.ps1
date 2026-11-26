$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$theme = Join-Path $root 'Resources\styles\ltzk_sci_fi.qss'
if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}
if (-not (Test-Path $theme)) {
    throw "Missing required file: $theme"
}

$sourceText = Get-Content -Raw -Encoding UTF8 $source
$themeText = Get-Content -Raw -Encoding UTF8 $theme

$required = @(
    'font-family: "Microsoft YaHei UI", "Microsoft YaHei", sans-serif;',
    'font-size: 36px;',
    'font-weight: 700;',
    'color: #D9F7FF;',
    'background: transparent;',
    'border-left: 4px solid #55D9FF;',
    'padding-left: 14px;',
    'qproperty-alignment: AlignLeft;'
)

foreach ($item in $required) {
    if (-not $themeText.Contains($item)) {
        throw "Workspace module title QSS style missing: $item"
    }
}

foreach ($item in @(
    'titleGlow->setColor(QColor(86, 185, 220, 105));',
    'titleGlow->setBlurRadius(14);',
    'm_activeModuleTitle->setObjectName(QStringLiteral("dashboardTitle"))'
)) {
    if (-not $sourceText.Contains($item)) {
        throw "Workspace module title runtime style missing: $item"
    }
}

if ($sourceText -notmatch 'titleContainer->setObjectName\(QStringLiteral\("workspaceTitleContainer"\)\)') {
    throw 'Workspace module title container must use the explicit transparent-style object name.'
}

Write-Host 'Workspace module title has distinct sci-fi typography style.'

