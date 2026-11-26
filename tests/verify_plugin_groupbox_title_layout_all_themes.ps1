$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$themeFiles = @(
    'Resources\styles\ltzk_sci_fi.qss',
    'Resources\styles\ltzk_black.qss',
    'Resources\styles\ltzk_white.qss'
)

foreach ($themeFile in $themeFiles) {
    $themePath = Join-Path $root $themeFile
    if (-not (Test-Path $themePath)) {
        throw "Theme file missing: $themeFile"
    }

    $qssText = Get-Content $themePath -Raw -Encoding UTF8
    $groupMatch = [regex]::Match($qssText, 'QGroupBox\s*\{(?<body>[^}]*)\}')
    if (-not $groupMatch.Success) {
        throw "Theme $themeFile is missing QGroupBox rule."
    }
    $groupBody = $groupMatch.Groups['body'].Value
    foreach ($required in @('margin-top: 52px', 'padding: 52px 14px 14px 14px')) {
        if (-not $groupBody.Contains($required)) {
            throw "Theme $themeFile QGroupBox title reserve is too small: $required"
        }
    }

    $titleMatch = [regex]::Match($qssText, 'QGroupBox::title\s*\{(?<body>[^}]*)\}')
    if (-not $titleMatch.Success) {
        throw "Theme $themeFile is missing QGroupBox::title rule."
    }
    $titleBody = $titleMatch.Groups['body'].Value
    foreach ($required in @('top: 5px', 'left: 18px', 'padding: 1px 12px', 'font-size: 32px', 'font-weight: 850')) {
        if (-not $titleBody.Contains($required)) {
            throw "Theme $themeFile QGroupBox title layout missing: $required"
        }
    }
}


$sourcePath = Join-Path $root 'LTZK.cpp'
$headerPath = Join-Path $root 'LTZK.h'
$sourceText = Get-Content $sourcePath -Raw -Encoding UTF8
$headerText = Get-Content $headerPath -Raw -Encoding UTF8
foreach ($required in @(
    'void polishPluginGroupBoxTitles(QWidget* widget);',
    'void LTZK::polishPluginGroupBoxTitles(QWidget* widget)',
    'findChildren<QGroupBox*>()',
    'titleFont.setPixelSize(32);',
    'target->setFont(titleFont);',
    'target->setStyleSheet(titleLabelStyle());',
    'groupBox->setTitle(QString());',
    '__ltzkPluginGroupBoxTitleOverlay',
    'findChildren<QLabel*>()',
    'isPluginSectionTitleLabel',
    'applyTitleFont(label);',
    'polishPluginGroupBoxTitles(widget);',
    'QTimer::singleShot(0, this, &LTZK::applyPlatformStyleToTopLevelWidgets);',
    'QTimer::singleShot(120, this, &LTZK::applyPlatformStyleToTopLevelWidgets);',
    'QTimer::singleShot(350, this, &LTZK::applyPlatformStyleToTopLevelWidgets);'
)) {
    if (-not ($sourceText.Contains($required) -or $headerText.Contains($required))) {
        throw "Plugin group box title font C++ polish missing: $required"
    }
}
Write-Host 'Plugin group box title layout verified for all platform themes.'





