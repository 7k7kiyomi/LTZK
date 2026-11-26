$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$cpp = Get-Content -Raw -Encoding UTF8 (Join-Path $repoRoot 'LTZK.cpp')
$header = Get-Content -Raw -Encoding UTF8 (Join-Path $repoRoot 'LTZK.h')

foreach ($forbidden in @(
    'workspaceBottomResultPanel',
    'm_qualityResultPanel',
    'm_qualityResultSummary',
    'm_qualityResultList',
    'm_resultPanelToggleButton',
    'workspaceResultBoardSplitterHandle',
    'resultBoardCollapseButton',
    'actionToggleResultPanel',
    'refreshQualityResultPanel'
)) {
    if ($cpp.Contains($forbidden) -or $header.Contains($forbidden)) {
        throw "Result panel display wiring still exists: $forbidden"
    }
}

foreach ($required in @(
    'syncQualityResultShpLayers()',
    'regroupQualityResultLayersBySourcePath()',
    'addQualityResultLayerToTree(layer);',
    'hideQualityResultCanvasLayers();',
    'm_qualityResultLayers'
)) {
    if (-not ($cpp.Contains($required) -or $header.Contains($required))) {
        throw "Quality result data flow was removed together with the panel: $required"
    }
}

if ($cpp -notmatch 'workspaceSplitter->addWidget\(mapPanel\)') {
    throw 'The map canvas must remain in the workspace after removing the result panel.'
}

Write-Output 'Quality result panel UI is removed while result SHP data flow remains.'
