$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$headerPath = Join-Path $root 'LTZK.h'
$sourcePath = Join-Path $root 'LTZK.cpp'
foreach ($path in @($headerPath, $sourcePath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing required file: $path"
    }
}

$header = Get-Content -LiteralPath $headerPath -Raw -Encoding UTF8
$source = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8

if ($header -notmatch 'QColor\s+defaultMapCanvasColor\(\)\s+const\s*;') {
    throw 'LTZK must declare one shared theme-default map canvas color helper.'
}

$defaultStart = $source.IndexOf('QColor LTZK::defaultMapCanvasColor() const')
if ($defaultStart -lt 0) {
    throw 'Could not locate the theme-default map canvas color helper.'
}
$defaultEnd = $source.IndexOf('void LTZK::applyWorkspaceShpCanvasBackground(', $defaultStart)
if ($defaultEnd -lt 0) {
    throw 'Could not locate the end of the theme-default map canvas color helper.'
}
$defaultBody = $source.Substring($defaultStart, $defaultEnd - $defaultStart)

foreach ($mapping in @(
    @{ Theme = 'White'; Color = 'QColor\(Qt::white\)' },
    @{ Theme = 'Black'; Color = 'QColor\(5, 5, 5\)' },
    @{ Theme = 'SciFi'; Color = 'QColor\(QStringLiteral\("#DCEFFA"\)\)' },
    @{ Theme = 'FieldNotes'; Color = 'QColor\(QStringLiteral\("#E4F0E2"\)\)' },
    @{ Theme = 'SteelWarmWhite'; Color = 'QColor\(QStringLiteral\("#E4E7E3"\)\)' }
)) {
    $mappingPattern = "case\s+PlatformTheme::$($mapping.Theme):\s*return\s+$($mapping.Color)\s*;"
    if ($defaultBody -notmatch $mappingPattern) {
        throw "Missing theme canvas color mapping: $($mapping.Theme) -> $($mapping.Color)"
    }
}

$applyStart = $source.IndexOf('void LTZK::applyWorkspaceShpCanvasBackground(')
$applyEnd = $source.IndexOf('void LTZK::setShpLayerDisplayColor(', $applyStart)
if ($applyStart -lt 0 -or $applyEnd -lt 0) {
    throw 'Could not locate workspace canvas background logic.'
}
$applyBody = $source.Substring($applyStart, $applyEnd - $applyStart)
if ($applyBody -notmatch 'if\s*\(m_hasCustomCanvasColor\)[\s\S]*?setCanvasColor\(m_customCanvasColor\)[\s\S]*?setCanvasColor\(useLightCanvas\s*\?\s*QColor\(Qt::white\)\s*:\s*defaultMapCanvasColor\(\)\)\s*;') {
    throw 'Workspace canvas color priority must be custom color, visible-SHP white, then theme default.'
}

$chooseStart = $source.IndexOf('void LTZK::chooseMapCanvasColor()')
$chooseEnd = $source.IndexOf('void LTZK::setQualityResultLayerVisible(', $chooseStart)
if ($chooseStart -lt 0) {
    throw 'Could not locate canvas color chooser logic.'
}
if ($chooseEnd -lt 0) {
    throw 'Could not locate the end of canvas color chooser logic.'
}
$chooseBody = $source.Substring($chooseStart, $chooseEnd - $chooseStart)
if ($chooseBody -notmatch 'm_workspaceUsesLightShpCanvas\s*\?\s*QColor\(Qt::white\)\s*:\s*defaultMapCanvasColor\(\)') {
    throw 'Canvas color chooser must preview the same B-scheme default as the workspace.'
}

if ($source -notmatch 'const bool hasVisibleShp\s*=\s*visibleLayersContainShp\(canvasLayers\);') {
    throw 'Workspace canvas background must use the existing business-SHP visibility check.'
}
if ($source -notmatch 'applyWorkspaceShpCanvasBackground\(m_workspaceUsesLightShpCanvas\);') {
    throw 'Theme refresh and basemap refresh must reapply the current canvas background policy.'
}

Write-Host 'Theme-aware default map canvas colors verified.'
