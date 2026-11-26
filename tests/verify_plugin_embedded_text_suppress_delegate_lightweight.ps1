$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'

if (-not (Test-Path $source)) { throw "Missing required file: $source" }
if (-not (Test-Path $header)) { throw "Missing required file: $header" }

$text = Get-Content -Raw -Encoding UTF8 $source
$headerText = Get-Content -Raw -Encoding UTF8 $header

foreach ($required in @(
    'PluginEmbeddedControlDelegate',
    'suppressPluginTableEmbeddedWidgetTexts',
    'indexForEmbeddedWidget',
    'view->indexWidget(index)',
    'viewport->findChildren<QWidget*>',
    'option.text.clear()',
    'installPluginTableEmbeddedWidgetDelegates',
    'installPluginTableEmbeddedWidgetDelegates(widget)'
)) {
    if (-not $text.Contains($required) -and -not $headerText.Contains($required)) {
        throw "Missing lightweight embedded-widget text suppression behavior: $required"
    }
}

$styleStart = $text.IndexOf('void LTZK::applyPlatformStyleToPluginWindow(QWidget* widget)')
$styleEnd = $text.IndexOf('void LTZK::normalizePluginTableEmbeddedControls', $styleStart)
if ($styleStart -lt 0 -or $styleEnd -lt 0) { throw 'Could not locate applyPlatformStyleToPluginWindow.' }
$styleBody = $text.Substring($styleStart, $styleEnd - $styleStart)
foreach ($required in @('widget->setStyleSheet(style)', 'widget->setAutoFillBackground(true)', 'widget->update()', 'installPluginTableEmbeddedWidgetDelegates(widget)')) {
    if (-not $styleBody.Contains($required)) { throw "Plugin style path missing behavior: $required" }
}

foreach ($forbidden in @('applyPluginTableEmbeddedWidgetGeometry', 'setGeometry(', 'setColumnWidth(', 'setRowHeight(', 'normalizePluginTableEmbeddedControls(widget)', 'hidePluginCellLabels(widget')) {
    if ($styleBody.Contains($forbidden)) { throw "Plugin crash guard forbids heavy table geometry/reflow in style path: $forbidden" }
}

$actionStart = $text.IndexOf('void LTZK::executePluginFeatureAction')
$actionEnd = $text.IndexOf('void LTZK::bindPluginFeatureActions', $actionStart)
if ($actionStart -lt 0 -or $actionEnd -lt 0) { throw 'Could not locate executePluginFeatureAction body.' }
$actionBody = $text.Substring($actionStart, $actionEnd - $actionStart)
foreach ($forbidden in @('windowsBeforeAction', 'applyPlatformStyleToPluginWindowsCreatedAfter', 'scheduleSeriesContentTablePolish')) {
    if ($actionBody.Contains($forbidden)) { throw "Plugin action path must not use series-specific or construction-time window scanning: $forbidden" }
}

Write-Host 'Plugin tables use lightweight embedded-widget text suppression without geometry reflow.'



