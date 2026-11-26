$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -LiteralPath (Join-Path $root 'LTZK.cpp') -Raw -Encoding UTF8
$header = Get-Content -LiteralPath (Join-Path $root 'LTZK.h') -Raw -Encoding UTF8

foreach ($required in @(
    'void scheduleQualityResultPostActionScan();',
    'bool m_qualityResultPostActionScanActive = false;',
    'int m_qualityResultPostActionScanAttempts = 0;'
)) {
    if (-not $header.Contains($required)) {
        throw "Post-action quality result scan state is missing: $required"
    }
}

$executeStart = $source.IndexOf('void LTZK::executePluginFeatureAction')
$executeEnd = $source.IndexOf('void LTZK::bindPluginFeatureActions', $executeStart)
if ($executeStart -lt 0 -or $executeEnd -lt 0) {
    throw 'Could not locate executePluginFeatureAction.'
}
$executeBody = $source.Substring($executeStart, $executeEnd - $executeStart)
$triggerIndex = $executeBody.IndexOf('pluginAction->trigger();')
if ($triggerIndex -lt 0) {
    throw 'The configured plugin feature action must still execute.'
}
$afterTrigger = $executeBody.Substring($triggerIndex)
if (-not $afterTrigger.Contains('scheduleQualityResultPostActionScan();')) {
    throw 'A result-producing feature action must schedule a scan after it is triggered.'
}

$scheduleStart = $source.IndexOf('void LTZK::scheduleQualityResultPostActionScan')
$scheduleEnd = $source.IndexOf('bool LTZK::hasRunningQualityResultWorker', $scheduleStart)
if ($scheduleStart -lt 0 -or $scheduleEnd -lt 0) {
    throw 'Could not locate the post-action quality result scan scheduler.'
}
$scheduleBody = $source.Substring($scheduleStart, $scheduleEnd - $scheduleStart)
foreach ($required in @(
    'm_qualityResultPostActionScanPending = true;',
    'm_qualityResultPostActionScanActive = false;',
    'm_qualityResultPostActionScanAttempts = 0;'
)) {
    if (-not $scheduleBody.Contains($required)) {
        throw "Post-action scan scheduler is incomplete: $required"
    }
}
if ($scheduleBody.Contains('m_qualityResultScanTimer->start')) {
    throw 'Post-action scan scheduler must wait for worker completion before starting the timer.'
}

$timerStart = $source.IndexOf('connect(m_qualityResultScanTimer, &QTimer::timeout')
$timerEnd = $source.IndexOf('m_qualityResultOutputWatcher = new QFileSystemWatcher', $timerStart)
if ($timerStart -lt 0 -or $timerEnd -lt 0) {
    throw 'Could not locate the quality result scan timer callback.'
}
$timerBody = $source.Substring($timerStart, $timerEnd - $timerStart)
foreach ($required in @(
    'm_qualityResultPostActionScanActive',
    'm_qualityResultScanReady',
    'm_qualityResultPostActionScanAttempts',
    'm_qualityResultScanTimer->start(kQualityResultPostActionScanIntervalMs);'
)) {
    if (-not $timerBody.Contains($required)) {
        throw "The post-action scan must remain bounded and retry asynchronously: $required"
    }
}

Write-Host 'Quality result scanning is scheduled after the check action and retries asynchronously while output is being generated.'
