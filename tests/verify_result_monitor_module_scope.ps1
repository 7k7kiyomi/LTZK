$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -Raw -Encoding Default (Join-Path $root 'LTZK.cpp')
$header = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'LTZK.h')

if (-not $header.Contains('m_qualityResultMonitoringModuleTitle')) {
    throw 'Result monitoring must retain the module that started the scan.'
}

$activateStart = $source.IndexOf('void LTZK::activateBusinessModule')
$activateEnd = $source.IndexOf('void LTZK::beginQualityResultMonitoring', $activateStart)
if ($activateStart -lt 0 -or $activateEnd -lt 0) {
    throw 'Could not locate business-module activation and result-monitoring methods.'
}
$activateBody = $source.Substring($activateStart, $activateEnd - $activateStart)
foreach ($required in @(
    'm_qualityResultScanTimer->stop()',
    'm_qualityResultOutputDirs.clear()',
    'm_qualityResultMonitoringModuleTitle.clear()'
)) {
    if (-not $activateBody.Contains($required)) {
        throw "Switching business modules must reset result monitoring state: $required"
    }
}

$beginStart = $source.IndexOf('void LTZK::beginQualityResultMonitoring')
$beginEnd = $source.IndexOf('void LTZK::refreshQualityResultOutputDirectories', $beginStart)
$beginBody = $source.Substring($beginStart, $beginEnd - $beginStart)
if ($source -notmatch 'void LTZK::beginQualityResultMonitoring\s*\(\s*const QString& moduleTitle\s*\)') {
    throw 'Result monitoring must receive the module that owns the plugin action.'
}
if (-not $beginBody.Contains('m_qualityResultMonitoringModuleTitle = normalizedModuleTitle')) {
    throw 'Result monitoring must record its owning module before scanning.'
}

$executeStart = $source.IndexOf('void LTZK::executePluginFeatureAction')
$executeEnd = $source.IndexOf('void LTZK::bindPluginFeatureActions', $executeStart)
$executeBody = $source.Substring($executeStart, $executeEnd - $executeStart)
if ($executeBody -notmatch 'const QString bindingModuleTitle\s*=\s*binding\.menuPath\.isEmpty\(\)\s*\?\s*QString\(\)\s*:\s*businessModuleDisplayTitle\(\s*binding\.menuPath\.first\(\)\s*\)') {
    throw 'Plugin actions must derive the owning module from their configured module.'
}
if ($executeBody -notmatch 'const QString resultModuleTitle\s*=\s*bindingModuleTitle') {
    throw 'Plugin actions must reuse the normalized configured module for result monitoring.'
}
if ($executeBody -notmatch 'beginQualityResultMonitoring\s*\(\s*resultModuleTitle\s*\)') {
    throw 'Plugin actions must start result monitoring for their configured module.'
}

$refreshStart = $source.IndexOf('void LTZK::refreshQualityResultOutputDirectories')
$refreshEnd = $source.IndexOf('QStringList LTZK::collectQualityResultSearchRoots', $refreshStart)
$refreshBody = $source.Substring($refreshStart, $refreshEnd - $refreshStart)
if ($refreshBody -notmatch 'm_qualityResultMonitoringModuleTitle\s*!=\s*m_currentBusinessModuleTitle') {
    throw 'Result-directory refresh must reject a monitor owned by another module.'
}

Write-Host 'Result monitoring is scoped to the active business module.'
