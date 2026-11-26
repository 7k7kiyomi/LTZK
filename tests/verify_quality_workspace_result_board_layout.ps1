$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'

foreach ($path in @($source, $header)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$sourceText = Get-Content -Raw -Encoding UTF8 $source
$headerText = Get-Content -Raw -Encoding UTF8 $header

$platformTitle = -join ([char[]](0x9647, 0x56FE, 0x667A, 0x63A7, 0x5E73, 0x53F0, 0x2014, 0x2014))
$qualitySubplatform = -join ([char[]](0x5B50, 0x5E73, 0x53F0))

foreach ($required in @(
    'workspaceLayerManagerPanel',
    'workspaceMapPanel',
    'workspaceTopActionStrip',
    'createTopBarToolButton',
    'm_qualityResultVisiblePaths',
    $platformTitle,
    $qualitySubplatform
)) {
    if (-not $sourceText.Contains($required) -and -not $headerText.Contains($required)) {
        throw "Quality workspace layout wiring missing: $required"
    }
}

foreach ($forbidden in @(
    'workspaceFeatureContainer',
    'createDashboardPanel(QStringLiteral("功能工具")',
    'workspaceRightSplitter',
    'workspaceBottomResultPanel',
    'workspaceResultToggleButton',
    'm_resultPanelToggleButton',
    'm_qualityResultPanel',
    'workspaceSplitter->addWidget(resultPanel);',
    'workspaceSplitter->setSizes(QList<int>() << 300 << 1400 << 360)'
)) {
    if ($sourceText.Contains($forbidden)) {
        throw "Old right-side/feature-rail workspace layout remains: $forbidden"
    }
}

if ($sourceText -notmatch 'topLayout->addWidget\s*\(\s*titleContainer\s*,\s*0\s*,\s*Qt::AlignLeft') {
    throw 'Workspace title should be placed at the left side of the single-row top bar.'
}

if ($sourceText -notmatch 'topLayout->addStretch\s*\(\s*1\s*\)') {
    throw 'Workspace top bar should push plugin/map actions and return home to the right.'
}

Write-Host 'Quality workspace result-board layout verified.'

