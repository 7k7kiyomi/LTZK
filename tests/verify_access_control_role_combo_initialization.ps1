$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$accessSourcePath = Join-Path $root 'auth\LTZKAccessControlDialog.cpp'
$sourcePath = Join-Path $root 'LTZK.cpp'
$styleDirectory = Join-Path $root 'Resources\styles'

foreach ($path in @($accessSourcePath, $sourcePath, $styleDirectory)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing required path: $path"
    }
}

$accessSource = Get-Content -Raw -Encoding UTF8 -LiteralPath $accessSourcePath
$source = Get-Content -Raw -Encoding UTF8 -LiteralPath $sourcePath
$roleItems = ([regex]::Matches($accessSource, 'mRoleCombo->addItem\(')).Count

if ($roleItems -ne 3) {
    throw "Expected three role items in the access-control combo, found $roleItems."
}

if ($accessSource -match 'mRoleCombo->clear\(') {
    throw 'The access-control role combo must not clear its role model during reload.'
}

$eventFilterMatch = [regex]::Match($source, 'bool LTZK::eventFilter\(QObject\* watched, QEvent\* event\)[\s\S]*?void LTZK::schedulePluginWindowPolish')
if (-not $eventFilterMatch.Success) {
    throw 'Could not locate LTZK::eventFilter.'
}

$eventFilter = $eventFilterMatch.Value
if ($eventFilter -notmatch 'window->windowType\(\) != Qt::Popup') {
    throw 'The global window-polish path must exclude Qt::Popup windows.'
}

if ($eventFilter -notmatch 'window->windowType\(\) != Qt::ToolTip') {
    throw 'The global window-polish path must exclude Qt::ToolTip windows.'
}

$darkThemes = @('ltzk_sci_fi.qss', 'ltzk_black.qss')
$lightThemes = @('ltzk_white.qss', 'ltzk_field_notes.qss', 'ltzk_steel_warm_white.qss')

foreach ($theme in $darkThemes) {
    $path = Join-Path $styleDirectory $theme
    $qss = Get-Content -Raw -Encoding UTF8 -LiteralPath $path
    if ($qss -notmatch 'QComboBox::down-arrow[\s\S]*combo_arrow_dark\.svg') {
        throw "$theme must use the dark combo arrow resource."
    }
}

foreach ($theme in $lightThemes) {
    $path = Join-Path $styleDirectory $theme
    $qss = Get-Content -Raw -Encoding UTF8 -LiteralPath $path
    if ($qss -notmatch 'QComboBox::down-arrow[\s\S]*combo_arrow_light\.svg') {
        throw "$theme must use the light combo arrow resource."
    }
}

Write-Host 'Access-control role combo initialization verified.'
