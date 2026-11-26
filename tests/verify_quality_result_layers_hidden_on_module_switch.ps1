$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'

foreach ($path in @($source, $header)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$sourceText = Get-Content -Raw -Encoding Default $source
$headerText = Get-Content -Raw -Encoding Default $header

foreach ($required in @(
    'void hideQualityResultCanvasLayers();',
    'void LTZK::hideQualityResultCanvasLayers()',
    'QString m_currentBusinessModuleTitle;',
    'm_currentBusinessModuleTitle = moduleTitle;',
    'const bool showResultModule = isResultModuleTitle(m_currentBusinessModuleTitle);',
    'if (showResultModule)',
    'hideQualityResultCanvasLayers();'
)) {
    if (-not $sourceText.Contains($required) -and -not $headerText.Contains($required)) {
        throw "Quality result module isolation wiring missing: $required"
    }
}

$switchPattern = 'else\s*\{(?<body>.*?)\n\s*\}\s*\n\s*const int layerCount = QgsProject::instance\(\)->layerTreeRoot\(\)->findLayers\(\)\.size\(\);'
$match = [regex]::Match($sourceText, $switchPattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)
if (-not $match.Success) {
    throw 'Could not locate non-quality module switch block.'
}

$body = $match.Groups['body'].Value
if (-not $body.Contains('hideQualityResultCanvasLayers();')) {
    throw 'Switching to a non-quality module must hide result shp layers from the map canvas.'
}

foreach ($forbidden in @(
    'm_qualityResultVisiblePaths.clear()',
    'm_qualityResultLayers.clear()'
)) {
    if ($body.Contains($forbidden) -or $sourceText.Contains($forbidden)) {
        throw "Switching modules must preserve quality result state: $forbidden"
    }
}

$refreshStart = $sourceText.IndexOf('void LTZK::refreshMapCanvasLayerStack()')
$refreshEnd = $sourceText.IndexOf('void LTZK::hideQualityResultCanvasLayers()', $refreshStart)
if ($refreshStart -lt 0 -or $refreshEnd -lt 0) {
    throw 'Could not locate refreshMapCanvasLayerStack before hideQualityResultCanvasLayers.'
}

$refreshBody = $sourceText.Substring($refreshStart, $refreshEnd - $refreshStart)
foreach ($required in @(
    'const bool showResultModule = isResultModuleTitle(m_currentBusinessModuleTitle);',
    'if (showResultModule)',
    'for (const QString& resultPath : m_qualityResultVisiblePaths)'
)) {
    if (-not $refreshBody.Contains($required)) {
        throw "Map canvas must add result shp layers only for their owning module: $required"
    }
}

Write-Host 'Quality result shp canvas visibility is scoped to the owning module without clearing state.'
