$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root "LTZK.cpp"
$header = Join-Path $root "LTZK.h"
$ui = Join-Path $root "LTZK.ui"

foreach ($path in @($source, $header, $ui)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$sourceText = Get-Content $source -Raw -Encoding Default
$headerText = Get-Content $header -Raw -Encoding Default
$uiText = Get-Content $ui -Raw -Encoding UTF8

foreach ($text in @($sourceText, $headerText, $uiText)) {
    if ($text -match "QgsMapOverviewCanvas|initMapOverviewCanvas|on_actionOverviewMap_triggered|OverviewMap|actionOverviewMap|鹰眼视图") {
        throw "Overview map wiring is still present"
    }
}

if ($sourceText -notmatch "\*\.qgs\s+\*\.qgz") {
    throw "Open project filter does not include both .qgs and .qgz"
}

if ($sourceText -notmatch "setAcceptDrops\(true\)") {
    throw "Main window does not accept drag-and-drop"
}

if ($headerText -notmatch "dragEnterEvent\(QDragEnterEvent\* event\) override") {
    throw "LTZK.h does not declare dragEnterEvent override"
}

if ($headerText -notmatch "dropEvent\(QDropEvent\* event\) override") {
    throw "LTZK.h does not declare dropEvent override"
}

if ($sourceText -notmatch "void LTZK::dragEnterEvent\(QDragEnterEvent\* event\)") {
    throw "LTZK.cpp does not implement dragEnterEvent"
}

if ($sourceText -notmatch "void LTZK::dropEvent\(QDropEvent\* event\)") {
    throw "LTZK.cpp does not implement dropEvent"
}

foreach ($extension in @(".shp", ".geojson", ".gpkg", ".tif", ".tiff", ".img", ".vrt")) {
    if ($sourceText -notmatch [regex]::Escape($extension)) {
        throw "Drag-and-drop loader does not mention supported extension $extension"
    }
}

Write-Host "Project open, drag-and-drop loading, and overview removal verified."
