$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}
if (-not (Test-Path $header)) {
    throw "Missing required file: $header"
}

$text = Get-Content -Raw -Encoding UTF8 $source
$headerText = Get-Content -Raw -Encoding UTF8 $header

foreach ($forbidden in @(
    'qApp->installEventFilter(this)',
    'QTimer::singleShot(0, this, &LTZK::applySciFiStyleToTopLevelWidgets)',
    'QTimer::singleShot(80, this, &LTZK::applySciFiStyleToTopLevelWidgets)',
    'QTimer::singleShot(250, this, &LTZK::applySciFiStyleToTopLevelWidgets)',
    'QTimer::singleShot(600, this, &LTZK::applySciFiStyleToTopLevelWidgets)'
)) {
    if ($text.Contains($forbidden)) {
        throw "Plugin crash guard forbids global style refresh trigger: $forbidden"
    }
}

$eventFilterStart = $text.IndexOf('bool LTZK::eventFilter(QObject* watched, QEvent* event)')
if ($eventFilterStart -lt 0) {
    throw 'Could not locate LTZK::eventFilter.'
}

$eventFilterEnd = $text.IndexOf('void LTZK::applyPlatformStyleToPluginWindow', $eventFilterStart)
if ($eventFilterEnd -lt 0) {
    throw 'Could not locate end of LTZK::eventFilter.'
}

$eventFilterBody = $text.Substring($eventFilterStart, $eventFilterEnd - $eventFilterStart)
foreach ($forbidden in @(
    'QEvent::Show',
    'QEvent::Polish',
    'QEvent::WinIdChange',
    'applySciFiStyleToPluginWindow(widget)'
)) {
    if ($eventFilterBody.Contains($forbidden)) {
        throw "Plugin crash guard forbids global event-filter plugin restyling: $forbidden"
    }
}
$pluginWindowStyleStart = $text.IndexOf('void LTZK::applyPlatformStyleToPluginWindow(QWidget* widget)')
if ($pluginWindowStyleStart -lt 0) {
    throw 'Could not locate applyPlatformStyleToPluginWindow.'
}

$pluginWindowStyleEnd = $text.IndexOf('void LTZK::normalizePluginTableEmbeddedControls', $pluginWindowStyleStart)
if ($pluginWindowStyleEnd -lt 0) {
    throw 'Could not locate end of applyPlatformStyleToPluginWindow.'
}

$pluginWindowStyleBody = $text.Substring($pluginWindowStyleStart, $pluginWindowStyleEnd - $pluginWindowStyleStart)

foreach ($forbidden in @(
    'widget->style()->unpolish(widget)',
    'widget->style()->polish(widget)',
    'normalizePluginTableEmbeddedControls'
)) {
    if ($pluginWindowStyleBody.Contains($forbidden)) {
        throw "Plugin crash guard forbids host-side plugin window restyling/reflow: $forbidden"
    }
}

foreach ($required in @(
    'void LTZK::applyPlatformStyleToPluginWindow(QWidget* widget)',
    'widget->setStyleSheet(style)',
    'widget->setAutoFillBackground(true)',
    'widget->update()'
)) {
    if (-not $pluginWindowStyleBody.Contains($required)) {
        throw "Plugin show-time stylesheet application missing: $required"
    }
}

foreach ($required in @(
    'bool eventFilter(QObject* watched, QEvent* event) override;',
    'void applySciFiStyleToPluginWindow(QWidget* widget);'
)) {
    if (-not $headerText.Contains($required)) {
        throw "Plugin style declarations unexpectedly missing: $required"
    }
}

Write-Host 'Plugin dialog crash guard style behavior verified.'




