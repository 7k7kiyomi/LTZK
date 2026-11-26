$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -LiteralPath (Join-Path $root 'LTZK.cpp') -Raw -Encoding UTF8

$intervalMatch = [regex]::Match(
    $source,
    'constexpr int kQualityResultPostActionScanIntervalMs\s*=\s*(?<value>\d+);')
$attemptMatch = [regex]::Match(
    $source,
    'constexpr int kMaxQualityResultPostActionScanAttempts\s*=\s*(?<value>\d+);')
if (-not $intervalMatch.Success -or -not $attemptMatch.Success) {
    throw 'The long-running quality result scan window is not explicitly configured.'
}

$intervalMs = [int]$intervalMatch.Groups['value'].Value
$attempts = [int]$attemptMatch.Groups['value'].Value
if (($intervalMs * $attempts) -lt 300000) {
    throw "Post-action scanning stops too early for a long-running check: $($intervalMs * $attempts) ms"
}

$timerStart = $source.IndexOf('connect(m_qualityResultScanTimer, &QTimer::timeout')
$timerEnd = $source.IndexOf('m_qualityResultOutputWatcher = new QFileSystemWatcher', $timerStart)
if ($timerStart -lt 0 -or $timerEnd -lt 0) {
    throw 'Could not locate the quality result scan timer callback.'
}
$timerBody = $source.Substring($timerStart, $timerEnd - $timerStart)
if (-not $timerBody.Contains('m_qualityResultScanTimer->start(kQualityResultPostActionScanIntervalMs);')) {
    throw 'Long-running post-action scans must use the configured retry interval.'
}

Write-Host 'Quality result scanning remains active long enough for a multi-minute check to finish.'
