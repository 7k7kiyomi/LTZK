$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'
$main = Join-Path $root 'main.cpp'

foreach ($path in @($source, $header, $main)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$sourceText = Get-Content -Raw -Encoding Default $source
$headerText = Get-Content -Raw -Encoding Default $header
$mainText = Get-Content -Raw -Encoding UTF8 $main

foreach ($forbidden in @(
    'qgsmessagelogviewer.h',
    'QgsMessageLogViewer'
)) {
    if ($sourceText.Contains($forbidden) -or $headerText.Contains($forbidden)) {
        throw "Message log dock must not use dialog-style viewer: $forbidden"
    }
}

foreach ($required in @(
    'qgsmessagelog.h',
    'QgsApplication::messageLog()',
    'QTabWidget',
    'QPlainTextEdit',
    'appendMessageLogEntry',
    'm_messageLogDock',
    'm_messageLogTabs',
    'm_messageLogToggleAction',
    'addDockWidget(Qt::BottomDockWidgetArea, m_messageLogDock)',
    'setToggleVisibilityAction(m_messageLogToggleAction)',
    'm_messageLogDock->hide()',
    'm_messageLogDock->setFloating(false)',
    'resizeDocks(QList<QDockWidget*>() << m_messageLogDock',
    'm_messageLogToggleAction->setText(QString::fromUtf8("\xF0\x9F\x92\xAC"))'
)) {
    if (-not $sourceText.Contains($required) -and -not $headerText.Contains($required)) {
        throw "Message log dock wiring missing: $required"
    }
}

$messageLogInitStart = $sourceText.IndexOf('void LTZK::initMessageLogDock()')
$messageLogInitEnd = $sourceText.IndexOf('QPlainTextEdit* LTZK::messageLogTextEdit(', $messageLogInitStart)
if ($messageLogInitStart -lt 0 -or $messageLogInitEnd -le $messageLogInitStart) {
    throw 'Message log initialization boundary was not found.'
}
$messageLogInitText = $sourceText.Substring(
    $messageLogInitStart,
    $messageLogInitEnd - $messageLogInitStart)
if (-not $messageLogInitText.Contains('m_messageLogToggleAction->setChecked(false)') -or
    -not $messageLogInitText.Contains('m_messageLogDock->hide()')) {
    throw 'Message log dock must be hidden by default.'
}

if ($sourceText -notmatch 'statusBar\(\)->addWidget\(m_messageLogToggleButton\)') {
    throw 'Status bar must include the message log toggle button.'
}

if ($headerText -notmatch 'QgsDockWidget\*\s+m_messageLogDock') {
    throw 'LTZK.h must store the message log dock.'
}

if ($headerText -notmatch 'QAction\*\s+m_messageLogToggleAction') {
    throw 'LTZK.h must store the message log toggle action.'
}

if ($headerText -notmatch 'QToolButton\*\s+m_messageLogToggleButton') {
    throw 'LTZK.h must store the status bar log toggle button.'
}

foreach ($required in @(
    'QSplashScreen splash(QPixmap(":/LTZK/Resources/splash.png"))',
    'splash.show()',
    'a.processEvents()',
    'LTZK w;',
    'w.showMaximized()',
    'splash.finish(&w)'
)) {
    if (-not $mainText.Contains($required)) {
        throw "Splash startup wiring missing: $required"
    }
}

foreach ($forbidden in @(
    '#include <QElapsedTimer>',
    '#include <QEventLoop>',
    'QEventLoop splashDelayLoop;',
    'splashDelayLoop.exec()',
    'QThread::msleep'
)) {
    if ($mainText.Contains($forbidden)) {
        throw "Splash startup must not block or run a nested event loop before LTZK is constructed: $forbidden"
    }
}

$initIndex = $mainText.IndexOf('QgsApplication::initQgis();')
$windowIndex = $mainText.IndexOf('LTZK w;')
$finishIndex = $mainText.IndexOf('splash.finish(&w)')
if ($initIndex -lt 0 -or $windowIndex -lt 0 -or $finishIndex -lt 0) {
    throw 'Could not locate QGIS init, LTZK construction, and splash finish calls.'
}
if ($windowIndex -lt $initIndex) {
    throw 'Main window must be constructed after QGIS initialization.'
}
if ($finishIndex -lt $windowIndex) {
    throw 'Splash must finish after the main window exists.'
}
Write-Host 'Message log dock and stable splash startup wiring verified.'
