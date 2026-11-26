$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'

if (-not (Test-Path $source)) { throw "Missing required file: $source" }
if (-not (Test-Path $header)) { throw "Missing required file: $header" }

$text = Get-Content -Raw -Encoding UTF8 $source
$headerText = Get-Content -Raw -Encoding UTF8 $header

$actionStart = $text.IndexOf('void LTZK::executePluginFeatureAction')
$actionEnd = $text.IndexOf('void LTZK::bindPluginFeatureActions', $actionStart)
if ($actionStart -lt 0 -or $actionEnd -lt 0) { throw 'Could not locate executePluginFeatureAction body.' }
$actionBody = $text.Substring($actionStart, $actionEnd - $actionStart)

foreach ($forbidden in @(
    'scheduleSeriesContentTablePolish();',
    'binding.featureName == QStringLiteral("图幅接边与系列化表达")',
    'polishSeriesContentTableWindows',
    'polishSeriesContentTableWindow'
)) {
    if ($actionBody.Contains($forbidden)) {
        throw "Series-specific table polishing must not run from plugin action path: $forbidden"
    }
}

foreach ($required in @(
    'installPluginTableEmbeddedWidgetDelegates(widget)',
    'suppressPluginTableEmbeddedWidgetTexts',
    'indexForEmbeddedWidget'
)) {
    if (-not $text.Contains($required) -and -not $headerText.Contains($required)) {
        throw "General table embedded widget text suppression missing: $required"
    }
}

Write-Host 'Series-specific table polishing is replaced by general embedded-widget suppression.'
