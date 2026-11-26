$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'
$qrc = Join-Path $root 'LTZK.qrc'
$qss = Join-Path $root 'Resources\styles\ltzk_sci_fi.qss'

if (-not (Test-Path $qss)) {
    throw 'Platform QSS file is missing: Resources\styles\ltzk_sci_fi.qss'
}

$sourceText = Get-Content -Raw -Encoding Default $source
$headerText = Get-Content -Raw -Encoding Default $header
$qrcText = Get-Content -Raw -Encoding Default $qrc
$qssText = Get-Content -Raw -Encoding UTF8 $qss

foreach ($required in @(
    'QDialog, QMessageBox',
    'QPushButton, QToolButton',
    'QLineEdit, QTextEdit, QPlainTextEdit',
    '#sciFiWorkspacePage',
    '#dashboardTitle',
    'QToolButton[topBarTool="true"]',
    'QLabel[primaryPanelTitle="true"]',
    '#workspacePluginActionScrollArea'
)) {
    if (-not $qssText.Contains($required)) {
        throw "Platform QSS missing selector: $required"
    }
}

foreach ($required in @(
    'QString platformSciFiQssPath()',
    'QString fallbackPlatformSciFiQss()',
    'QString loadPlatformSciFiQss()',
    'QString LTZK::platformStyleSheet() const',
    'const QString style = platformStyleSheet();',
    'qApp->setStyleSheet(platformStyleSheet())',
    'widget->setStyleSheet(style)'
)) {
    if (-not $sourceText.Contains($required)) {
        throw "Platform QSS loading requirement missing: $required"
    }
}

if (-not $headerText.Contains('QString platformStyleSheet() const;')) {
    throw 'LTZK.h must expose platformStyleSheet() for platform/plugin reuse.'
}

if (-not $qrcText.Contains('<file>Resources/styles/ltzk_sci_fi.qss</file>')) {
    throw 'LTZK.qrc must include Resources/styles/ltzk_sci_fi.qss'
}

if ($sourceText.Contains('qApp->setStyleSheet(sciFiApplicationStyleSheet())')) {
    throw 'Application must load the external platform QSS instead of applying the old inline application string directly.'
}

Write-Host 'Platform QSS extraction verified.'
