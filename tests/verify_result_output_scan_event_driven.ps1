$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -LiteralPath (Join-Path $root 'LTZK.cpp') -Raw -Encoding UTF8
$header = Get-Content -LiteralPath (Join-Path $root 'LTZK.h') -Raw -Encoding UTF8

foreach ($required in @(
    'class QFileSystemWatcher;',
    'QFileSystemWatcher* m_qualityResultOutputWatcher',
    'QPointer<QWidget> m_qualityResultMonitoringWindow',
    'void bindQualityResultOutputWindow(QWidget* window);',
    'void updateQualityResultOutputWatcher();',
    'void handleQualityResultDirectoryChanged(const QString& path);',
    '#include <QFileSystemWatcher>',
    '&QFileSystemWatcher::directoryChanged',
    'm_qualityResultScanTimer->setSingleShot(true)',
    'm_qualityResultOutputWatcher->addPath(selectedOutputDir)',
    'm_qualityResultScanTimer->start()'
)) {
    if (-not ($source.Contains($required) -or $header.Contains($required))) {
        throw "Event-driven result monitoring is missing: $required"
    }
}

$beginStart = $source.IndexOf('void LTZK::beginQualityResultMonitoring')
$beginEnd = $source.IndexOf('void LTZK::bindQualityResultOutputWindow', $beginStart)
if ($beginStart -lt 0 -or $beginEnd -lt 0) {
    throw 'Could not locate result-monitor arming method.'
}
$beginBody = $source.Substring($beginStart, $beginEnd - $beginStart)
foreach ($forbidden in @(
    'm_qualityResultScanTimer->start()',
    'syncQualityResultShpLayers()'
)) {
    if ($beginBody.Contains($forbidden)) {
        throw "Arming a feature window must not scan its output directory: $forbidden"
    }
}

$refreshStart = $source.IndexOf('void LTZK::refreshQualityResultOutputDirectories')
$refreshEnd = $source.IndexOf('void LTZK::updateQualityResultOutputWatcher', $refreshStart)
if ($refreshStart -lt 0 -or $refreshEnd -lt 0) {
    throw 'Could not locate selected-output-directory refresh method.'
}
$refreshBody = $source.Substring($refreshStart, $refreshEnd - $refreshStart)
foreach ($required in @(
    'm_qualityResultMonitoringWindow',
    'lineEdit->text().trimmed()',
    'QFileInfo(text).isDir()',
    'isQualityResultOutputPathControl(lineEdit, monitoredWindow)'
)) {
    if (-not $refreshBody.Contains($required)) {
        throw "The monitor must use the actual selected output-directory control: $required"
    }
}
foreach ($forbidden in @(
    'QApplication::topLevelWidgets()',
    'QgsProject::instance()->mapLayers()',
    'localPathFromLayerSource('
)) {
    if ($refreshBody.Contains($forbidden)) {
        throw "The selected output directory must not be inferred from unrelated state: $forbidden"
    }
}

$executeStart = $source.IndexOf('void LTZK::executePluginFeatureAction')
$executeEnd = $source.IndexOf('void LTZK::bindPluginFeatureActions', $executeStart)
if ($executeStart -lt 0 -or $executeEnd -lt 0) {
    throw 'Could not locate plugin feature execution method.'
}
$executeBody = $source.Substring($executeStart, $executeEnd - $executeStart)
if (-not $executeBody.Contains('pluginAction->trigger();')) {
    throw 'Plugin action trigger is missing.'
}
if (-not $executeBody.Contains('scheduleQualityResultPostActionScan();')) {
    throw 'A result-producing plugin action must schedule a post-action scan.'
}
foreach ($forbidden in @(
    'syncQualityResultShpLayers()',
    'refreshQualityResultPanel()'
)) {
    if ($executeBody.Contains($forbidden)) {
        throw "Opening or returning from a feature action must not scan immediately: $forbidden"
    }
}

$directoryChangedStart = $source.IndexOf('void LTZK::handleQualityResultDirectoryChanged')
$directoryChangedEnd = $source.IndexOf('QStringList LTZK::collectQualityResultSearchRoots', $directoryChangedStart)
if ($directoryChangedStart -lt 0 -or $directoryChangedEnd -lt 0) {
    throw 'Could not locate directory-change handler.'
}
$directoryChangedBody = $source.Substring($directoryChangedStart, $directoryChangedEnd - $directoryChangedStart)
if (-not $directoryChangedBody.Contains('m_qualityResultScanTimer->start()')) {
    throw 'A real output-directory change must start the debounced SHP scan.'
}

Write-Host 'Result SHP scanning is event-driven or post-action polled and scoped to the user-selected output directory.'
