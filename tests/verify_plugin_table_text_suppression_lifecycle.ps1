$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'

if (-not (Test-Path $source)) { throw "Missing required file: $source" }
if (-not (Test-Path $header)) { throw "Missing required file: $header" }

$text = Get-Content -Raw -Encoding UTF8 $source
$headerText = Get-Content -Raw -Encoding UTF8 $header

foreach ($required in @(
    'void schedulePluginTableEmbeddedTextSuppression();',
    'void LTZK::schedulePluginTableEmbeddedTextSuppression()',
    'QTimer::singleShot(0, this, &LTZK::schedulePluginTableEmbeddedTextSuppression);',
    'installPluginTableEmbeddedWidgetDelegates(widget)',
    'view->setProperty("ltzkPluginEmbeddedTextSuppressTarget", true)',
    'viewport->setProperty("ltzkPluginEmbeddedTextSuppressTarget", true)',
    'view->installEventFilter(this)',
    'viewport->installEventFilter(this)',
    'QEvent::ChildAdded',
    'QEvent::LayoutRequest',
    'QEvent::Resize',
    'QTimer::singleShot(0, this, &LTZK::schedulePluginTableEmbeddedTextSuppression);'
)) {
    if (-not ($text.Contains($required) -or $headerText.Contains($required))) {
        throw "Missing plugin table embedded-text lifecycle behavior: $required"
    }
}

$eventFilterStart = $text.IndexOf('bool LTZK::eventFilter(QObject* watched, QEvent* event)')
$eventFilterEnd = $text.IndexOf('void LTZK::applyPlatformStyleToPluginWindow', $eventFilterStart)
if ($eventFilterStart -lt 0 -or $eventFilterEnd -lt 0) { throw 'Could not locate eventFilter body.' }
$eventFilterBody = $text.Substring($eventFilterStart, $eventFilterEnd - $eventFilterStart)
foreach ($required in @('QTimer::singleShot(0, this, &LTZK::schedulePluginTableEmbeddedTextSuppression);')) {

    if (-not $eventFilterBody.Contains($required)) {
        throw "Embedded-widget child/layout events must schedule delayed suppression after widgets finish being inserted: $required"
    }
}
$actionStart = $text.IndexOf('void LTZK::executePluginFeatureAction')
$actionEnd = $text.IndexOf('void LTZK::bindPluginFeatureActions', $actionStart)
if ($actionStart -lt 0 -or $actionEnd -lt 0) { throw 'Could not locate executePluginFeatureAction body.' }
$actionBody = $text.Substring($actionStart, $actionEnd - $actionStart)
foreach ($required in @('QTimer::singleShot(0, this, &LTZK::schedulePluginTableEmbeddedTextSuppression);')) {


    if (-not $actionBody.Contains($required)) {
        throw "Plugin action path must schedule lightweight table text suppression: $required"
    }
}
foreach ($forbidden in @(
    'normalizePluginTableEmbeddedControls',
    'polishSeriesContentTableWindows',
    'polishSeriesContentTableWindow',
    'scheduleSeriesContentTablePolish'
)) {
    if ($actionBody.Contains($forbidden)) {
        throw "Plugin action path must not use series-specific or geometry-heavy table fixes: $forbidden"
    }
}

$styleStart = $text.IndexOf('void LTZK::applyPlatformStyleToPluginWindow(QWidget* widget)')
$styleEnd = $text.IndexOf('void LTZK::polishPluginGroupBoxTitles', $styleStart)
if ($styleStart -lt 0 -or $styleEnd -lt 0) { throw 'Could not locate applyPlatformStyleToPluginWindow body.' }
$styleBody = $text.Substring($styleStart, $styleEnd - $styleStart)
foreach ($forbidden in @(
    'normalizePluginTableEmbeddedControls',
    'setGeometry(',
    'setRowHeight(',
    'setColumnWidth(',
    'style()->unpolish',
    'style()->polish'
)) {
    if ($styleBody.Contains($forbidden)) {
        throw "Plugin style path must stay lightweight to avoid plugin dialog crashes: $forbidden"
    }
}

Write-Host 'Plugin table embedded-text lifecycle suppression verified.'
