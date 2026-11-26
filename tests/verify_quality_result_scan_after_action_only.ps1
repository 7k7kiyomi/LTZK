$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$sourceText = Get-Content -LiteralPath $sourcePath -Raw

$qualityModuleTitle = -join ([char[]](0x5730, 0x56FE, 0x6210, 0x679C, 0x8D28, 0x68C0))
$generalizationModuleTitle = -join ([char[]](0x5730, 0x56FE, 0x667A, 0x80FD, 0x7EFC, 0x5408))
$qualityBlockPattern = 'const bool isResultModule = isResultModuleTitle\(moduleTitle\);\s*if \(isResultModule\)\s*\{(?<body>.*?)\}\s*else'
$match = [regex]::Match($sourceText, $qualityBlockPattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)
if (-not $match.Success) {
    throw 'Could not locate the result-module entry block.'
}

$body = $match.Groups['body'].Value
foreach ($forbidden in @('beginQualityResultMonitoring();', 'syncQualityResultShpLayers();')) {
    if ($body.Contains($forbidden)) {
        throw "Entering the quality module must not start result scanning before a check action runs: $forbidden"
    }
}

foreach ($moduleTitle in @($qualityModuleTitle, $generalizationModuleTitle)) {
    if (-not $sourceText.Contains($moduleTitle)) {
        throw "Result module title is not registered: $moduleTitle"
    }
}

$executeStart = $sourceText.IndexOf('void LTZK::executePluginFeatureAction')
$executeEnd = $sourceText.IndexOf('void LTZK::bindPluginFeatureActions', $executeStart)
if ($executeStart -lt 0 -or $executeEnd -lt 0) {
    throw 'Could not locate executePluginFeatureAction.'
}
$executeBody = $sourceText.Substring($executeStart, $executeEnd - $executeStart)
if ($executeBody -notmatch 'beginQualityResultMonitoring\s*\(\s*resultModuleTitle\s*\)') {
    throw 'Result monitoring must be armed for the configured action module.'
}
foreach ($required in @('pluginAction->trigger();')) {
    if (-not $executeBody.Contains($required)) {
        throw "The configured plugin feature action must still execute: $required"
    }
}
if (-not $executeBody.Contains('scheduleQualityResultPostActionScan();')) {
    throw 'A result-producing feature action must schedule a post-action scan.'
}
foreach ($forbidden in @('syncQualityResultShpLayers();', 'refreshQualityResultPanel();')) {
    if ($executeBody.Contains($forbidden)) {
        throw "Opening or returning from a plugin feature must not scan before output changes: $forbidden"
    }
}

$changeStart = $sourceText.IndexOf('void LTZK::handleQualityResultDirectoryChanged')
$changeEnd = $sourceText.IndexOf('QStringList LTZK::collectQualityResultSearchRoots', $changeStart)
if ($changeStart -lt 0 -or $changeEnd -lt 0) {
    throw 'Could not locate result output directory-change handling.'
}
$changeBody = $sourceText.Substring($changeStart, $changeEnd - $changeStart)
if (-not $changeBody.Contains('m_qualityResultScanTimer->start();')) {
    throw 'A real output-directory change must trigger the debounced SHP scan.'
}

Write-Host 'Result scanning is armed by the feature action and runs after the action or a selected output-directory change.'
