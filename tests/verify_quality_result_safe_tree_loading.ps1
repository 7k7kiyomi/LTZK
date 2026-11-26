$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -LiteralPath (Join-Path $root 'LTZK.cpp') -Raw -Encoding UTF8
$header = Get-Content -LiteralPath (Join-Path $root 'LTZK.h') -Raw -Encoding UTF8
$interface = Get-Content -LiteralPath (Join-Path $root 'LTZKQgisInterface.cpp') -Raw -Encoding UTF8

function Require-Text([string]$text, [string]$needle, [string]$message) {
    if (-not $text.Contains($needle)) {
        throw $message
    }
}

foreach ($required in @(
    'QgsVectorLayer* addPluginVectorLayer',
    'QgsLayerTreeGroup* ensureQualityResultLayerGroup',
    'void addQualityResultLayerToTree',
    'layerTreeRegistryBridge',
    'm_qualityResultScanReady = false',
    'm_qualityResultFileSignatures',
    'isQualityResultShpFileStable',
    'hasRunningQualityResultWorker',
    'QPointer<QgsVectorLayer>',
    'mapLayer(layer->id())',
    'ltzk/qualityResultFolder',
    'ltzk/qualityResultFolderPath',
    'relativePath',
    'parentGroup->addGroup(folderName)',
    'bridge->setEnabled(false)',
    'currentParent->takeChild(layerNode)',
    'targetGroup->addChildNode(layerNode)'
)) {
    Require-Text ($header + $source) $required "Safe quality-result loading contract is missing: $required"
}

Require-Text $interface 'mApp->addPluginVectorLayer' 'Plugin-created vector layers must use the host result-layer insertion path.'
Require-Text $source 'QgsProject::instance()->addMapLayer(layer, false)' 'Result layers must enter the registry without automatic root insertion.'
Require-Text $source 'targetGroup->addLayer(layer)' 'Result layers must be appended directly to their target folder.'
Require-Text $source 'QStringLiteral("shx")' 'SHP stability checks must include the SHX sidecar.'
Require-Text $source 'QStringLiteral("dbf")' 'SHP stability checks must include the DBF sidecar.'

$addedStart = $source.IndexOf('connect(QgsProject::instance(), &QgsProject::layersAdded')
$addedEnd = $source.IndexOf('m_qualityResultScanTimer = new QTimer(this);', $addedStart)
if ($addedStart -lt 0 -or $addedEnd -lt 0) {
    throw 'Could not locate the layersAdded callback.'
}
$addedBody = $source.Substring($addedStart, $addedEnd - $addedStart)
foreach ($forbidden in @('takeChild', 'addChildNode', 'refreshQualityResultPanel', 'registerQualityResultLayer')) {
    if ($addedBody.Contains($forbidden)) {
        throw "layersAdded must not perform result-tree mutation or panel refresh: $forbidden"
    }
}

$scheduleStart = $source.IndexOf('void LTZK::scheduleQualityResultPostActionScan')
$scheduleEnd = $source.IndexOf('bool LTZK::hasRunningQualityResultWorker', $scheduleStart)
if ($scheduleStart -lt 0 -or $scheduleEnd -lt 0) {
    throw 'Could not locate the post-action quality-result scheduler.'
}
$scheduleBody = $source.Substring($scheduleStart, $scheduleEnd - $scheduleStart)
Require-Text $scheduleBody 'm_qualityResultPostActionScanPending = true;' 'Post-action scheduling must wait for completion.'
if ($scheduleBody.Contains('m_qualityResultScanTimer->start')) {
    throw 'Post-action scheduling must not scan while the plugin may still be writing output files.'
}

$workerStart = $source.IndexOf('void LTZK::handleQualityResultWorkerFinished')
$workerEnd = $source.IndexOf('bool LTZK::isQualityResultLayerInSelectedOutputDirs', $workerStart)
if ($workerStart -lt 0 -or $workerEnd -lt 0) {
    throw 'Could not locate worker-completion handling.'
}
$workerBody = $source.Substring($workerStart, $workerEnd - $workerStart)
Require-Text $workerBody 'hasRunningQualityResultWorker()' 'Worker completion must wait for the last monitored worker.'
Require-Text $workerBody 'armQualityResultScanAfterCompletion()' 'Worker completion must arm the deferred scan.'

Write-Host 'Quality result loading is completion-gated, sidecar-stable, and safe for the layer tree.'
