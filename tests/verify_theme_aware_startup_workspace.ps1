$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $repoRoot 'LTZK.cpp'
$header = Join-Path $repoRoot 'LTZK.h'

$sourceText = Get-Content -Raw -Encoding UTF8 $source
$headerText = Get-Content -Raw -Encoding UTF8 $header

function Get-BlockText {
    param(
        [string]$Text,
        [string]$StartMarker,
        [string]$EndMarker
    )

    $start = $Text.IndexOf($StartMarker)
    if ($start -lt 0) {
        throw "Could not locate block start: $StartMarker"
    }

    $end = $Text.IndexOf($EndMarker, $start + $StartMarker.Length)
    if ($end -lt 0) {
        throw "Could not locate block end after: $StartMarker"
    }

    return $Text.Substring($start, $end - $start)
}

function Assert-Contains {
    param(
        [string]$Text,
        [string]$Expected,
        [string]$Message
    )

    if (-not $Text.Contains($Expected)) {
        throw $Message
    }
}

function Assert-NotContains {
    param(
        [string]$Text,
        [string]$Forbidden,
        [string]$Message
    )

    if ($Text.Contains($Forbidden)) {
        throw $Message
    }
}

$ctorBody = Get-BlockText $sourceText 'LTZK::LTZK(QWidget* parent)' 'LTZK::~LTZK()'
$loadThemeIndex = $ctorBody.IndexOf('loadSavedPlatformTheme();')
$canvasCreateIndex = $ctorBody.IndexOf('m_mapCanvas = new QgsMapCanvas(this);')
if ($loadThemeIndex -lt 0 -or $canvasCreateIndex -lt 0 -or $loadThemeIndex -gt $canvasCreateIndex) {
    throw 'Startup must load the saved platform theme before creating and coloring the map canvas.'
}
Assert-NotContains $ctorBody 'm_mapCanvas->setCanvasColor(QColor(3, 16, 29));' 'Startup map canvas color must not be hard-coded to sci-fi dark.'
Assert-Contains $ctorBody 'applyWorkspaceShpCanvasBackground(false);' 'Startup map canvas must use the theme-aware background helper.'

$workspaceBody = Get-BlockText $sourceText 'QWidget* LTZK::buildSciFiWorkspacePage()' 'QFrame* LTZK::createDashboardPanel'
Assert-Contains $workspaceBody 'page->setStyleSheet(platformStyleSheet());' 'Workspace page must initialize with the current platform theme stylesheet.'
Assert-NotContains $workspaceBody 'page->setStyleSheet(workspaceDarkStyleSheet());' 'Workspace page must not initialize with sci-fi dark stylesheet.'

$loadBody = Get-BlockText $sourceText 'void LTZK::loadLocalGansuShpBasemap()' 'void LTZK::styleLocalGansuBasemapLayer'
Assert-NotContains $loadBody 'm_mapCanvas->setCanvasColor(QColor(3, 16, 29));' 'Local Gansu basemap loading must not force the canvas back to sci-fi dark.'
Assert-NotContains $loadBody 'dark basemap loaded' 'Local Gansu basemap logging must not describe every theme as dark.'
Assert-Contains $loadBody 'applyWorkspaceShpCanvasBackground(m_workspaceUsesLightShpCanvas);' 'Local Gansu basemap loading must reapply the current theme-aware canvas background.'

$styleBody = Get-BlockText $sourceText 'void LTZK::styleLocalGansuBasemapLayer(QgsVectorLayer* layer)' 'void LTZK::setGansuInitialMapView()'
Assert-Contains $styleBody 'm_currentTheme == PlatformTheme::White' 'Local Gansu basemap symbol must branch for the white platform theme.'
Assert-Contains $styleBody 'applyLocalGansuBasemapLabels(layer, isWhiteTheme);' 'Local Gansu basemap labels must receive the current theme.'

$refreshBody = Get-BlockText $sourceText 'void LTZK::refreshPlatformThemeStyles()' 'void LTZK::initSciFiWorkspace()'
Assert-Contains $refreshBody 'styleLocalGansuBasemapLayer(vectorLayer);' 'Theme refresh must restyle the already-loaded system basemap.'
Assert-Contains $refreshBody 'applyWorkspaceShpCanvasBackground(m_workspaceUsesLightShpCanvas);' 'Theme refresh must repaint the map canvas background for the current theme.'

Assert-Contains $headerText 'void applyWorkspaceShpCanvasBackground(bool useLightCanvas);' 'Theme-aware canvas background helper declaration missing.'

Write-Host 'Theme-aware startup workspace verified.'
