$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$cpp = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'

if (-not (Test-Path $cpp)) {
    throw "Missing required file: $cpp"
}

if (-not (Test-Path $header)) {
    throw "Missing required file: $header"
}

$cppText = Get-Content -Raw -Encoding Default $cpp
$headerText = Get-Content -Raw -Encoding Default $header

foreach ($slot in @(
    'on_actionAddWmsLayer_triggered',
    'on_actionAddWfsLayer_triggered',
    'on_actionAddWcsLayer_triggered',
    'on_actionZoomActual_triggered'
)) {
    if ($cppText -match "void LTZK::$slot\(\)\s*\{\s*\}") {
        throw "$slot is still empty."
    }
}

# Provider widgets and loading now live in the shared native source manager.
# Runtime loading, service page selection and refresh are exercised by
# run_workspace_data_source_host_test.cmd; keep this check on entry points
# and the unchanged initial-view contract rather than the removed widget internals.
foreach ($required in @(
    'openOwsSourceSelect(QStringLiteral("WMS"))',
    'openOwsSourceSelect(QStringLiteral("WFS"))',
    'openOwsSourceSelect(QStringLiteral("WCS"))'
)) {
    if (-not $cppText.Contains($required)) {
        throw "Missing OWS implementation detail in LTZK.cpp: $required"
    }
}

foreach ($required in @(
    'openOwsSourceSelect',
    'rememberInitialMapView',
    'restoreInitialMapView',
    'm_initialMapScale',
    'm_initialMapExtent'
)) {
    if (-not $headerText.Contains($required) -and -not $cppText.Contains($required)) {
        throw "Missing initial zoom implementation detail: $required"
    }
}

if ($cppText -notmatch 'zoomScale\s*\(\s*m_initialMapScale') {
    throw 'Initial zoom does not restore the stored map scale.'
}

if ($cppText -notmatch 'setExtent\s*\(\s*m_initialMapExtent') {
    throw 'Initial zoom does not restore the stored map extent.'
}

Write-Host 'OWS toolbar buttons and initial zoom implementation verified.'
