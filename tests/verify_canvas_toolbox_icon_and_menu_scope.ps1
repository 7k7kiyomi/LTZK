$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$cpp = Get-Content -LiteralPath (Join-Path $repoRoot "LTZK.cpp") -Raw -Encoding UTF8
$header = Get-Content -LiteralPath (Join-Path $repoRoot "LTZK.h") -Raw -Encoding UTF8
$qrc = Get-Content -LiteralPath (Join-Path $repoRoot "LTZK.qrc") -Raw -Encoding UTF8
$icon = Get-Content -LiteralPath (Join-Path $repoRoot "Resources/toolbox_map_pin.svg") -Raw -Encoding UTF8
$themeFiles = @(
    "Resources/styles/ltzk_white.qss",
    "Resources/styles/ltzk_black.qss",
    "Resources/styles/ltzk_sci_fi.qss"
)

function Assert-Contains {
    param(
        [string]$Content,
        [string]$Pattern,
        [string]$Message
    )

    if ($Content -notmatch $Pattern) {
        throw $Message
    }
}

function Assert-NotContains {
    param(
        [string]$Content,
        [string]$Pattern,
        [string]$Message
    )

    if ($Content -match $Pattern) {
        throw $Message
    }
}

$toolboxStart = $cpp.IndexOf('QMenu* mapToolboxMenu = new QMenu(m_mapCanvasToolboxButton);')
$toolboxEnd = $cpp.IndexOf('m_mapCanvasToolboxButton->setMenu(mapToolboxMenu);', $toolboxStart)
if ($toolboxStart -lt 0 -or $toolboxEnd -lt 0) {
    throw 'Toolbox menu construction block was not found.'
}
$toolboxBlock = $cpp.Substring($toolboxStart, $toolboxEnd - $toolboxStart)

Assert-NotContains $toolboxBlock 'addAction\(ui\.actionOpenAttrTable\)' 'Toolbox menu must not expose Open Attribute Table.'
Assert-NotContains $toolboxBlock 'mapToolboxMenu->addAction\(QStringLiteral\("结果面板"\)\)' 'Toolbox menu must not expose Result Panel.'
Assert-Contains $toolboxBlock 'addAction\(ui\.actionOpenProject\)' 'Toolbox menu must keep Open Project.'
Assert-Contains $toolboxBlock 'addAction\(ui\.actionAddVectorData\)' 'Toolbox menu must keep Add Vector Data.'
Assert-Contains $toolboxBlock 'addAction\(ui\.actionAddRasterData\)' 'Toolbox menu must keep Add Raster Data.'
Assert-Contains $toolboxBlock 'addAction\(ui\.actionZoomIn\)' 'Toolbox menu must keep Zoom In.'
Assert-Contains $toolboxBlock 'addAction\(ui\.actionZoomOut\)' 'Toolbox menu must keep Zoom Out.'
Assert-Contains $toolboxBlock 'addAction\(ui\.actionPan\)' 'Toolbox menu must keep Pan.'
Assert-Contains $toolboxBlock 'addAction\(ui\.actionFullExtent\)' 'Toolbox menu must keep Full Extent.'
Assert-Contains $toolboxBlock 'addAction\(ui\.actionZoomActual\)' 'Toolbox menu must keep Zoom Actual.'

Assert-Contains $header 'QToolButton\* m_mapCanvasToolboxButton = nullptr;' 'LTZK.h should store the map canvas toolbox button.'
Assert-Contains $cpp 'actionOpenAttributeTable' 'Layer context menu must keep Open Attribute Table.'
Assert-NotContains $cpp 'workspaceToolboxResultPanelAction' 'Removed result panel must not leave a toolbox action behind.'
Assert-Contains $cpp 'm_mapCanvasToolboxButton->setIcon\(QIcon\(QStringLiteral\("\:/LTZK/Resources/toolbox_map_pin\.svg"\)\)\)' 'Toolbox button must use the new GIS icon.'
Assert-Contains $cpp 'm_mapCanvasToolboxButton->setToolButtonStyle\(Qt::ToolButtonIconOnly\)' 'Toolbox button must be icon-only.'
Assert-Contains $cpp 'const QSize floatingToolButtonSize = scaledSize\(page, 64, 64\)' 'Toolbox button must use a compact square size.'
Assert-Contains $cpp 'const int toolboxInset = scaledInt\(page, 12\)' 'Toolbox button must reserve an inset from the canvas corner.'
Assert-Contains $cpp 'toolboxColumn->setFixedSize\(\s*floatingToolButtonSize\.width\(\) \+ toolboxInset,\s*floatingToolButtonSize\.height\(\) \+ toolboxInset\s*\)' 'Toolbox column must provide the corner inset.'
Assert-Contains $cpp 'toolboxColumnLayout->addWidget\(m_mapCanvasToolboxButton, 0, Qt::AlignLeft \| Qt::AlignTop\)' 'Toolbox button must sit inside the inset corner frame.'
Assert-Contains $cpp 'toolboxColumn->setAttribute\(Qt::WA_TranslucentBackground\)' 'Toolbox container must use a transparent widget surface.'
Assert-Contains $cpp 'toolboxColumn->setStyleSheet\(QStringLiteral\(' 'Toolbox container must override the global QWidget background.'
Assert-Contains $cpp 'm_mapCanvasToolboxButton->setAttribute\(Qt::WA_TranslucentBackground\)' 'Toolbox button must use a transparent widget surface.'
Assert-Contains $cpp 'm_mapCanvasToolboxButton->setStyleSheet\(QStringLiteral\(' 'Toolbox button must override inherited rectangular button styling.'
Assert-Contains $qrc '<file>Resources/toolbox_map_pin\.svg</file>' 'New toolbox icon must be registered in Qt resources.'
Assert-Contains $icon '<title>' 'SVG should describe its GIS toolbox purpose.'
Assert-Contains $icon '<circle' 'SVG should contain a circular icon base.'
Assert-Contains $icon '<path' 'SVG should contain vector GIS geometry.'

foreach ($themeFile in $themeFiles) {
    $theme = Get-Content -LiteralPath (Join-Path $repoRoot $themeFile) -Raw -Encoding UTF8
    Assert-Contains $theme '#workspaceMapToolboxButton\s*\{[^}]*background:\s*transparent;[^}]*border:\s*0;' "$themeFile should keep the circular icon free of a rectangular button background."
    Assert-Contains $theme '#workspaceMapToolboxButton:hover\s*\{[^}]*background:\s*transparent;[^}]*border:\s*0;' "$themeFile should keep the circular icon free of a rectangular hover border."
}

Write-Host 'Canvas toolbox icon and menu scope verified.'
