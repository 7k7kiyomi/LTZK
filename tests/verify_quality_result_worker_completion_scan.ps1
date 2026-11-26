$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -LiteralPath (Join-Path $root 'LTZK.cpp') -Raw -Encoding UTF8
$header = Get-Content -LiteralPath (Join-Path $root 'LTZK.h') -Raw -Encoding UTF8

foreach ($required in @(
    '#include <QThread>',
    'class QThread;',
    'void handleQualityResultWorkerFinished();',
    'QThread::finished',
    'handleQualityResultWorkerFinished()',
    'hasRunningQualityResultWorker()',
    'armQualityResultScanAfterCompletion()',
    'changedPath.startsWith(selectedOutputDirPath',
    'm_qualityResultScanTimer->start()'
)) {
    if (-not ($source.Contains($required) -or $header.Contains($required))) {
        throw "Quality result completion/subdirectory monitoring is missing: $required"
    }
}

$workerStart = $source.IndexOf('void LTZK::handleQualityResultWorkerFinished')
$workerEnd = $source.IndexOf('QStringList LTZK::collectQualityResultSearchRoots', $workerStart)
if ($workerStart -lt 0 -or $workerEnd -lt 0) {
    throw 'Could not locate quality worker-completion handling.'
}
$workerBody = $source.Substring($workerStart, $workerEnd - $workerStart)
if ($workerBody -notmatch '(?s)refreshQualityResultOutputDirectories\(\).*?armQualityResultScanAfterCompletion\(\)') {
    throw 'Worker completion must refresh the selected output directory and arm the final scan.'
}

$watchStart = $source.IndexOf('void LTZK::updateQualityResultOutputWatcher')
$watchEnd = $source.IndexOf('void LTZK::handleQualityResultDirectoryChanged', $watchStart)
if ($watchStart -lt 0 -or $watchEnd -lt 0) {
    throw 'Could not locate output watcher update method.'
}
$watchBody = $source.Substring($watchStart, $watchEnd - $watchStart)
if ($watchBody.Contains('QDirIterator directoryIterator') -or $watchBody.Contains('QDirIterator::Subdirectories')) {
    throw 'The GUI thread must not recursively register every result subdirectory.'
}
$keepsSelectedRoot = $watchBody.Contains('appendUniqueDir(directoriesToWatch, selectedOutputDir);') -and
    $watchBody.Contains('m_qualityResultOutputWatcher->addPath(selectedOutputDir)')
if (-not $keepsSelectedRoot) {
    throw 'The watcher must keep observing the explicitly selected result root.'
}

$changeStart = $source.IndexOf('void LTZK::handleQualityResultDirectoryChanged')
$changeEnd = $source.IndexOf('QStringList LTZK::collectQualityResultSearchRoots', $changeStart)
$changeBody = $source.Substring($changeStart, $changeEnd - $changeStart)
if ($changeBody -notmatch 'startsWith\(selectedOutputDirPath') {
    throw 'A changed result subdirectory must be treated as belonging to the selected output root.'
}

Write-Host 'Quality SHP results scan after worker completion while the GUI watcher remains root-only.'
