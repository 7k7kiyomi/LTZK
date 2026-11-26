$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'
$qrc = Join-Path $root 'LTZK.qrc'
$whiteQss = Join-Path $root 'Resources\styles\ltzk_white.qss'
$blackQss = Join-Path $root 'Resources\styles\ltzk_black.qss'
$sciFiQss = Join-Path $root 'Resources\styles\ltzk_sci_fi.qss'

foreach ($path in @($source, $header, $qrc, $sciFiQss)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

foreach ($path in @($whiteQss, $blackQss)) {
    if (-not (Test-Path $path)) {
        throw "Missing theme QSS file: $path"
    }
}

$sourceText = Get-Content -Raw -Encoding Default $source
$headerText = Get-Content -Raw -Encoding Default $header
$qrcText = Get-Content -Raw -Encoding Default $qrc
$whiteText = Get-Content -Raw -Encoding UTF8 $whiteQss
$blackText = Get-Content -Raw -Encoding UTF8 $blackQss
$sciFiText = Get-Content -Raw -Encoding UTF8 $sciFiQss

foreach ($required in @(
    'enum class PlatformTheme',
    'm_currentTheme',
    'loadSavedPlatformTheme',
    'savePlatformTheme',
    'setPlatformTheme',
    'showPlatformThemeDialog',
    'refreshPlatformThemeStyles',
    'homeThemeButton',
    '平台风格',
    '白色系',
    '黑色系',
    '科幻系',
    'Resources/styles/ltzk_white.qss',
    'Resources/styles/ltzk_black.qss',
    'platformThemeQssResourcePath',
    'platformThemeQssFileName',
    'QSettings settings(',
    'settings.setValue(QStringLiteral("platform/theme")',
    'qApp->setStyleSheet(platformStyleSheet())',
    'applyPlatformStyleToTopLevelWidgets'
)) {
    if (-not ($sourceText.Contains($required) -or $headerText.Contains($required))) {
        throw "Platform theme switcher requirement missing: $required"
    }
}

foreach ($required in @(
    '<file>Resources/styles/ltzk_white.qss</file>',
    '<file>Resources/styles/ltzk_black.qss</file>'
)) {
    if (-not $qrcText.Contains($required)) {
        throw "QRC theme resource missing: $required"
    }
}

foreach ($pair in @(
    @{ Name = 'white'; Text = $whiteText; Tokens = @('QDialog, QMessageBox', '#sciFiHomePage', '#dashboardTopBar', 'QToolButton[themeOptionButton="true"]', '#f8fafc', '#2563eb') },
    @{ Name = 'black'; Text = $blackText; Tokens = @('QDialog, QMessageBox', '#sciFiHomePage', '#dashboardTopBar', 'QToolButton[themeOptionButton="true"]', '#050505', '#f59e0b') }
)) {
    foreach ($token in $pair.Tokens) {
        if (-not $pair.Text.Contains($token)) {
            throw "$($pair.Name) theme QSS missing token: $token"
        }
    }
}

foreach ($pair in @(
    @{ Name = 'white'; Text = $whiteText },
    @{ Name = 'black'; Text = $blackText }
)) {
    if ($pair.Text.Contains('#workspacePluginActionScrollArea QWidget')) {
        throw "$($pair.Name) theme must not apply transparent workspace scroll-area styling to every descendant QWidget, because that also matches platform QToolButtons."
    }
    foreach ($required in @(
        'QWidget#workspaceTopActionScrollContent',
        'QWidget#workspacePluginActionStrip'
    )) {
        if (-not $pair.Text.Contains($required)) {
            throw "$($pair.Name) theme must scope workspace scroll-area transparency to container widgets only: $required"
        }
    }
}

if ($sourceText.Contains('QScrollArea#workspacePluginActionScrollArea QWidget')) {
    throw 'Runtime workspace scroll-area style must not target every descendant QWidget; it hides platform button backgrounds.'
}
foreach ($required in @(
    'QWidget#workspaceTopActionScrollContent',
    'QWidget#workspacePluginActionStrip'
)) {
    if (-not $sourceText.Contains($required)) {
        throw "Runtime workspace scroll-area style must scope transparency to container widgets only: $required"
    }
}

foreach ($pair in @(
    @{ Name = 'white'; Text = $whiteText },
    @{ Name = 'black'; Text = $blackText }
)) {
    foreach ($forbidden in @(
        'QToolButton#workspaceReturnHomeButton',
        '#workspaceReturnHomeButton'
    )) {
        if ($pair.Text.Contains($forbidden)) {
            throw "$($pair.Name) theme must not give the platform return-home button a special toolbar style: $forbidden"
        }
    }
}

foreach ($required in @(
    'backHomeButton->setProperty("topBarTool", true)',
    'backHomeButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon)',
    'setTopBarToolButtonResponsiveMetrics(backHomeButton, topActionStrip)'
)) {
    if (-not $sourceText.Contains($required)) {
        throw "Return-home platform button must share the normal top-bar tool button setup: $required"
    }
}

foreach ($required in @(
    'm_currentTheme = PlatformTheme::SciFi',
    'settings.value(QStringLiteral("platform/theme"), QStringLiteral("sci-fi"))'
)) {
    if (-not $sourceText.Contains($required) -and -not $headerText.Contains($required)) {
        throw "First-run platform theme must default to sci-fi: $required"
    }
}

$themeButtonIndex = $sourceText.IndexOf('stageLayout->addLayout(homeThemeLayout, 0);')
$resultHintIndex = $sourceText.IndexOf('stageLayout->addWidget(resultHint);')
if ($themeButtonIndex -lt 0 -or $resultHintIndex -lt 0 -or $themeButtonIndex -gt $resultHintIndex) {
    throw 'Home platform theme button must be added above the bottom result/copyright bar.'
}

foreach ($forbidden in @(
    'QLabel* title = new QLabel(QStringLiteral("选择平台风格")',
    'QLabel* description = new QLabel(QStringLiteral("风格会同步应用到首页、各子平台页面和 DLL 弹窗。")'
)) {
    if ($sourceText.Contains($forbidden)) {
        throw "Platform theme dialog must only show three centered style buttons, without extra text: $forbidden"
    }
}

foreach ($required in @(
    'dialog.setWindowTitle(QStringLiteral("风格选择"))',
    'dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint)',
    'dialog.setMinimumSize(scaledSize(this, 460, 340))',
    'dialog.resize(scaledSize(this, 460, 340))',
    'layout->setAlignment(Qt::AlignCenter)',
    'optionButton->setText(platformThemeLabel(theme) + (theme == m_currentTheme ? QStringLiteral("（当前风格）") : QString()))',
    'optionButton->setMinimumWidth(scaledInt(&dialog, 300))',
    'optionButton->setMinimumHeight(scaledInt(&dialog, 68))',
    'layout->addWidget(optionButton, 0, Qt::AlignHCenter)'
)) {
    if (-not $sourceText.Contains($required)) {
        throw "Platform theme dialog centered-button requirement missing: $required"
    }
}

foreach ($themeText in @($whiteText, $blackText, $sciFiText)) {
    foreach ($requiredThemeButtonStyle in @('text-align: center', 'padding: 14px 22px', 'font-size: 18px')) {
        if (-not $themeText.Contains($requiredThemeButtonStyle)) {
            throw "Platform theme option buttons must be centered and enlarged: $requiredThemeButtonStyle"
        }
    }
}

if ($sourceText.Contains('panelPath.addRoundedRect(panelRect, 8.0, 8.0);')) {
    throw 'White/black home header must keep the same trapezoid frame as sci-fi, not draw a rectangular panel.'
}

foreach ($required in @(
    'const bool isWhiteTheme = themeKey == QStringLiteral("white")',
    'const bool isBlackTheme = themeKey == QStringLiteral("black")',
    'headerBarLeftColor',
    'centerPanel.moveTo(',
    'centerPanel.quadTo('
)) {
    if (-not $sourceText.Contains($required)) {
        throw "Home title trapezoid theme requirement missing: $required"
    }
}
Write-Host 'Platform theme switcher verified.'








