$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$sourceText = [System.IO.File]::ReadAllText($sourcePath, [System.Text.Encoding]::UTF8)

$qualityTaskTitle = [string]::Concat(
    [char]0x8D28, [char]0x68C0, [char]0x4EFB, [char]0x52A1, [char]0x7BA1, [char]0x7406)
$qualitySchemeTitle = [string]::Concat(
    [char]0x8D28, [char]0x68C0, [char]0x65B9, [char]0x6848, [char]0x914D, [char]0x7F6E)

$styleStart = $sourceText.IndexOf('void LTZK::applyPlatformStyleToPluginWindow(QWidget* widget)')
$styleEnd = $sourceText.IndexOf('void LTZK::polishPluginGroupBoxTitles(QWidget* widget)', $styleStart)
if ($styleStart -lt 0 -or $styleEnd -lt 0) {
    throw 'Could not locate plugin-window style application body.'
}

$styleBody = $sourceText.Substring($styleStart, $styleEnd - $styleStart)

foreach ($required in @(
    'widget->windowTitle().trimmed()',
    $qualityTaskTitle,
    $qualitySchemeTitle,
    'QGroupBox[ltzkCompactTopGroupBox=\"true\"] { margin-top: 0px; }',
    'ltzkCompactTopGroupBox',
    'findChildren<QGroupBox*>()',
    'setProperty("ltzkCompactTopGroupBox", false)',
    'setProperty("ltzkCompactTopGroupBox", true)',
    'style += compactQualityWindowStyle',
    'widget->setStyleSheet(style);'
)) {
    if (-not $styleBody.Contains($required)) {
        throw "Plugin-window margin override is missing: $required"
    }
}

if ($styleBody.Contains('QGroupBox { margin-top: 0px; }')) {
    throw 'Quality-window override must not remove the margin from every QGroupBox.'
}

if ($styleBody.Contains('setProperty(QStringLiteral("ltzkCompactTopGroupBox")')) {
    throw 'QObject::setProperty must use a const char* property name.'
}

foreach ($themeFile in @(
    'Resources\styles\ltzk_sci_fi.qss',
    'Resources\styles\ltzk_black.qss',
    'Resources\styles\ltzk_white.qss'
)) {
    $themePath = Join-Path $root $themeFile
    $qssText = Get-Content $themePath -Raw -Encoding UTF8
    if (-not $qssText.Contains('margin-top: 52px')) {
        throw "Global QGroupBox margin must remain unchanged in $themeFile"
    }
}

Write-Host 'Quality task and scheme windows use a local QGroupBox margin override.'
