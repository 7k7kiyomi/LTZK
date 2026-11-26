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
$headerStart = $sourceText.IndexOf('class SciFiHomeHeader')
$headerEnd = $sourceText.IndexOf('QStringList pluginBindingConfigPathCandidates', $headerStart)
if ($headerStart -lt 0 -or $headerEnd -lt 0) { throw 'Could not locate SciFiHomeHeader.' }
$homeHeaderText = $sourceText.Substring($headerStart, $headerEnd - $headerStart)
$titleFontPattern = 'titleFont\.setPixelSize\(\s*scaledInt\(\s*this,\s*66\s*\)\s*\)'
if (-not [regex]::IsMatch($sourceText, $titleFontPattern, [Text.RegularExpressions.RegexOptions]::Singleline)) {
    throw 'Home page main title font size must use the shared 1920x1080 scale.'
}


foreach ($required in @(
    'setMinimumHeight(scaledInt(this, 150))',
    'setMaximumHeight(scaledInt(this, 162))',
    'return scaledSize(this, 1200, 156)',
    'const QRectF titleRect(',
    'bottomLeft,',
    '18,'
    'bottomWidth,',
    '76'
)) {
    if (-not $sourceText.Contains($required)) {
        throw 'Home page header requirement missing.'
    }
}

foreach ($forbidden in @(
    'subtitleFont',
    'subtitleRect',
    'm_subtitle',
    'subtitleTextColor',
    '甘肃省基础地理信息中心地理信息智能质检平台'
)) {
    if ($homeHeaderText.Contains($forbidden)) {
        throw 'Home page subtitle code remains.'
    }
}

foreach ($required in @(
    'QStackedWidget',
    'buildSciFiHomePage',
    'buildSciFiWorkspacePage',
    'activateBusinessModule',
    'createModuleLaunchButton',
    'createWorkspaceToolButton',
    'm_workspaceStack',
    'm_homePage',
    'm_moduleActions',
    'm_mapCanvasPage',
    'm_activeModuleTitle',
    'm_homeContentStack',
    'm_homeLoginPage',
    'm_homeModulesPage'
)) {
    if (-not $sourceText.Contains($required) -and -not $headerText.Contains($required)) {
        throw "Sci-fi module homepage wiring missing: $required"
    }
}

$moduleLabels = @(
    -join ([char[]](0x5730, 0x56FE, 0x667A, 0x80FD, 0x7EFC, 0x5408)),
    -join ([char[]](0x5730, 0x56FE, 0x6210, 0x679C, 0x8D28, 0x68C0)),
    -join ([char[]](0x5730, 0x56FE, 0x6210, 0x679C, 0x7BA1, 0x7406)),
    -join ([char[]](0x5730, 0x56FE, 0x670D, 0x52A1, 0x7BA1, 0x7406))
)

foreach ($label in $moduleLabels) {
    if (-not $sourceText.Contains("QStringLiteral(`"$label`")")) {
        throw "Missing homepage module launch label: $label"
    }
}

foreach ($objectName in @(
    'sciFiWorkspacePage',
    'dashboardTopBar',
    'workspaceReturnHomeButton',
    'workspaceLayerManagerPanel',
    'workspaceMapPanel',
    'moduleButton_mapGeneralization',
    'moduleButton_mapQuality',
    'moduleButton_mapAchievementManagement',
    'moduleButton_mapServiceManagement',
    'qualityResultLayerHint'
)) {
    if (-not $sourceText.Contains($objectName)) {
        throw "Missing sci-fi homepage object/detail: $objectName"
    }
}

foreach ($required in @(
    'm_workspaceStack->setCurrentWidget(m_mapCanvasPage)',
    'm_workspaceStack->setCurrentWidget(m_homePage)',
    '&QToolButton::clicked',
    'connect(backHomeButton, &QToolButton::clicked',
    '&QAction::trigger',
    'QgsProject::instance()->mapLayers()',
    'QgsProject::instance()->addMapLayer(layer, false)',
    'm_qualityResultVisiblePaths',
    '.shp',
    ((-join ([char[]](0x68C0, 0x67E5, 0x7ED3, 0x679C))) + ' shp')
)) {
    if (-not $sourceText.Contains($required)) {
        throw "Homepage must preserve action triggering and result layer visibility hint: $required"
    }
}


$activateStart = $sourceText.IndexOf('void LTZK::activateBusinessModule')
$activateEnd = $sourceText.IndexOf('void LTZK::beginQualityResultMonitoring', $activateStart)
if ($activateStart -lt 0 -or $activateEnd -lt 0) {
    throw 'Could not locate activateBusinessModule.'
}
$activateBody = $sourceText.Substring($activateStart, $activateEnd - $activateStart)
foreach ($required in @(
    'loadLocalGansuShpBasemap();',
    'refreshMapCanvasLayerStack();',
    'setGansuInitialMapView();'
)) {
    if (-not $activateBody.Contains($required)) {
        throw "Business module entry must immediately show the default local Gansu shp basemap: $required"
    }
}
if ($sourceText.Contains('loadQualityResultShp(') -or $sourceText.Contains('addQualityResultLayer(')) {
    throw 'Homepage UI must not create or load quality result layers; result shp display should stay with existing map/layer-tree behavior.'
}

Write-Host 'Sci-fi module homepage wiring verified.'








