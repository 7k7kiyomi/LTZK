$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$header = Get-Content -Raw (Join-Path $root 'LTZK.h')
$source = Get-Content -Raw (Join-Path $root 'LTZK.cpp')
$qrc = Get-Content -Raw (Join-Path $root 'LTZK.qrc')

function Assert-Contains([string]$text, [string]$needle, [string]$message) {
    if (-not $text.Contains($needle)) { throw $message }
}

function Assert-NotContains([string]$text, [string]$needle, [string]$message) {
    if ($text.Contains($needle)) { throw $message }
}

function Assert-File([string]$relativePath) {
    $path = Join-Path $root $relativePath
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing file: $relativePath" }
}

Assert-Contains $header 'FieldNotes' 'PlatformTheme must contain FieldNotes.'
Assert-Contains $header 'SteelWarmWhite' 'PlatformTheme must contain SteelWarmWhite.'
Assert-File 'Resources/styles/ltzk_field_notes.qss'
Assert-File 'Resources/styles/ltzk_steel_warm_white.qss'
Assert-Contains $qrc 'Resources/styles/ltzk_field_notes.qss' 'Field Notes QSS must be registered in LTZK.qrc.'
Assert-Contains $qrc 'Resources/styles/ltzk_steel_warm_white.qss' 'Steel & Warm White QSS must be registered in LTZK.qrc.'
Assert-Contains $source 'field-notes' 'Field Notes key mapping is missing.'
Assert-Contains $source 'steel-warm-white' 'Steel & Warm White key mapping is missing.'
Assert-Contains $source '绿色系' 'Green theme label is missing.'
Assert-Contains $source '灰色系' 'Gray theme label is missing.'
Assert-NotContains $source '自然野外图册' 'The old Field Notes label must be removed from runtime source.'
Assert-NotContains $source '钢灰暖白' 'The old Steel & Warm White label must be removed from runtime source.'
Assert-Contains $source 'PlatformTheme::FieldNotes' 'Field Notes selection entry or branch is missing.'
Assert-Contains $source 'PlatformTheme::SteelWarmWhite' 'Steel & Warm White selection entry or branch is missing.'

$fieldQss = Get-Content -Raw (Join-Path $root 'Resources/styles/ltzk_field_notes.qss')
$steelQss = Get-Content -Raw (Join-Path $root 'Resources/styles/ltzk_steel_warm_white.qss')
Assert-Contains $fieldQss '#3D5540' 'Field Notes forest green palette is missing.'
Assert-Contains $fieldQss '#F4EBD7' 'Field Notes paper palette is missing.'
Assert-Contains $steelQss '#34464B' 'Steel & Warm White steel palette is missing.'
Assert-Contains $steelQss '#F4F2EB' 'Steel & Warm White warm-white palette is missing.'
Assert-Contains $steelQss '#C6D8D2' 'Steel & Warm White title color is missing.'

# Theme-specific homepage and workspace overrides must not leave the new themes
# with the old white/cyan card text or dark-on-dark workspace titles.
Assert-Contains $source 'color: #B86F4F; background: transparent; border: none;' 'Field Notes card code color is missing.'
Assert-Contains $source 'color: #3D5540; background: transparent; border: none;' 'Field Notes card title color is missing.'
Assert-Contains $source 'color: #66725D; background: transparent; border: none;' 'Field Notes card subtitle color is missing.'
Assert-Contains $source 'color: #668B8C; background: transparent; border: none;' 'Steel & Warm White card code color is missing.'
Assert-Contains $source 'color: #2F5960; background: transparent; border: none;' 'Steel & Warm White card title color is missing.'
Assert-Contains $source 'color: #687775; background: transparent; border: none;' 'Steel & Warm White card subtitle color is missing.'
Assert-Contains $source 'setThemeAwareIconPixmap' 'Theme-aware icon treatment is missing.'
Assert-Contains $fieldQss '#qualityResultLayerHint { color: #3E4D3D; background: #E4DFC8;' 'Field Notes institution footer style is missing.'
Assert-Contains $steelQss '#qualityResultLayerHint { color: #34464B; background: #D5DEDA;' 'Steel & Warm White institution footer style is missing.'
Assert-Contains $fieldQss '#dashboardTitle { color: #F4EBD7;' 'Field Notes workspace title contrast is missing.'
Assert-Contains $steelQss '#dashboardTitle { color: #C6D8D2;' 'Steel & Warm White workspace title contrast is missing.'
Assert-Contains $source 'titleLabel->setAttribute(Qt::WA_TranslucentBackground);' 'Permission-management title must explicitly allow a translucent background.'
Assert-Contains $source 'titleLabel->setAutoFillBackground(false);' 'Permission-management title must disable auto-filled background.'

Write-Output 'Platform theme expansion static checks passed.'
