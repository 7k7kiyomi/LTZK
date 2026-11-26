$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$cppPath = Join-Path $repoRoot 'LTZK.cpp'
$headerPath = Join-Path $repoRoot 'LTZK.h'
$cpp = Get-Content -Raw $cppPath
$header = Get-Content -Raw $headerPath

if ($cpp -match 'kMaxQualityResultLayersPerScan') {
    throw 'Quality result scanning still contains the single-scan layer limit.'
}

if ($cpp -match 'addedCount\s*>=\s*kMaxQualityResultLayersPerScan') {
    throw 'Quality result scanning still stops after 50 added layers.'
}

if ($header -notmatch 'int\s+regroupQualityResultLayersBySourcePath\s*\(\)\s*;') {
    throw 'LTZK.h does not declare the completion-time quality result regroup helper.'
}

$helperStart = $cpp.IndexOf('int LTZK::regroupQualityResultLayersBySourcePath()')
$helperEnd = $cpp.IndexOf('void LTZK::refreshMapCanvasLayerStack()', $helperStart)
if ($helperStart -lt 0 -or $helperEnd -le $helperStart) {
    throw 'LTZK.cpp does not define regroupQualityResultLayersBySourcePath before map canvas refresh.'
}

$helperBody = $cpp.Substring($helperStart, $helperEnd - $helperStart)
foreach ($requiredText in @(
    'project->mapLayers()',
    'localPathFromLayerSource',
    'isQualityResultLayerInSelectedOutputDirs',
    'moveQualityResultLayerToGroupSafely'
)) {
    if ($helperBody.IndexOf($requiredText, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Completion-time regroup helper is missing: $requiredText"
    }
}

$scanInvocation = $cpp.IndexOf('const int loadedCount = syncQualityResultShpLayers()')
if ($scanInvocation -lt 0) {
    throw 'The completion scan callback is missing its quality result scan invocation.'
}

$callbackWindowStart = [Math]::Max(0, $scanInvocation - 800)
$callbackWindowLength = [Math]::Min(1400, $cpp.Length - $callbackWindowStart)
$callbackWindow = $cpp.Substring($callbackWindowStart, $callbackWindowLength)
if ($callbackWindow.IndexOf('regroupQualityResultLayersBySourcePath()', [System.StringComparison]::Ordinal) -lt 0) {
    throw 'The completion scan callback does not regroup loaded result SHP layers.'
}

Write-Output 'Quality result full scan and completion-time path regrouping checks passed.'
