$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$header = Join-Path $root 'LTZK.h'
$source = Join-Path $root 'LTZK.cpp'
$qrc = Join-Path $root 'LTZK.qrc'
$homeBackground = Join-Path $root 'Resources\home_login_background.png'
$accessHeader = Join-Path $root 'auth\LTZKAccessControlDialog.h'
$accessSource = Join-Path $root 'auth\LTZKAccessControlDialog.cpp'

foreach ($path in @($header, $source, $qrc, $homeBackground, $accessHeader, $accessSource)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$headerText = Get-Content -Raw -Encoding UTF8 $header
$sourceText = Get-Content -Raw -Encoding UTF8 $source
$qrcText = Get-Content -Raw -Encoding UTF8 $qrc
$accessHeaderText = Get-Content -Raw -Encoding UTF8 $accessHeader
$accessSourceText = Get-Content -Raw -Encoding UTF8 $accessSource

foreach ($needle in @(
    'QStackedWidget* m_homeContentStack',
    'QWidget* m_homeLoginPage',
    'QWidget* m_homeModulesPage',
    'QWidget* m_accessControlPage',
    'QLineEdit* m_homeLoginUserNameEdit',
    'QLineEdit* m_homeLoginPasswordEdit',
    'QLabel* m_homeLoginStatusLabel',
    'QWidget* buildHomeLoginPanel(QWidget* parent)',
    'QWidget* buildHomeModulesPanel(QWidget* parent)',
    'QWidget* buildAccessControlPage()',
    'void authenticateFromHomePage()',
    'void showHomeLoginPage()',
    'void showHomeModulesPage()',
    'void showAccessControlPage()',
    'qreal m_lastHomeScale = 0.0'
)) {
    if ($headerText -notmatch [regex]::Escape($needle)) {
        throw "Embedded login/access header marker missing: $needle"
    }
}

foreach ($needle in @(
    'm_homeContentStack = new QStackedWidget',
    'm_homeLoginPage = buildHomeLoginPanel',
    'm_homeModulesPage = buildHomeModulesPanel',
    'class SciFiHomePage final : public QWidget',
    'QPixmap(QStringLiteral(":/LTZK/Resources/home_login_background.png"))',
    'qreal backgroundOpacity = 0.68;',
    'backgroundOpacity = 0.52;',
    'backgroundOpacity = 0.58;',
    'QFont homeLoginTitleFont(QStringLiteral("Microsoft YaHei UI"));',
    'homeLoginTitleFont.setPixelSize(scaledInt(panel, 54));',
    'homeLoginTitleFont.setWeight(QFont::Black);',
    'titleLabel->setFont(homeLoginTitleFont);',
    'titleLabel->setMinimumHeight(scaledInt(panel, 64));',
    'QLabel#homeLoginTitle { ',
    'font-size: 54px; ',
    '#homeLoginPanel QLabel#homeLoginTitle { ',
    'font-size: 18px; ',
    'homeLoginAmbientBand',    'QVBoxLayout* rootLayout = new QVBoxLayout(page)',
    'rootLayout->addWidget(homeHeader)',    'homeLoginLowerTelemetry',
    'panel->setFixedSize(scaledSize(page, kHomeLoginPanelBaseWidth, kHomeLoginPanelBaseHeight))',
    'm_homeLoginUserNameEdit->setMinimumHeight(scaledInt(panel, 36))',
    'm_homeLoginPasswordEdit->setMinimumHeight(scaledInt(panel, 36))',
    'loginButton->setMinimumHeight(scaledInt(panel, 42))',
    'resetButton->setMinimumHeight(scaledInt(panel, 42))',
    'QPushButton* resetButton = new QPushButton(QStringLiteral("重置"), panel)',
    'resetButton->setObjectName(QStringLiteral("homeLoginResetButton"))',
    'QHBoxLayout* buttonLayout = new QHBoxLayout()',
    'buttonLayout->addWidget(loginButton)',
    'buttonLayout->addWidget(resetButton)',
    'panelLayout->addLayout(buttonLayout)',
    'connect(resetButton, &QPushButton::clicked, this, [this]()',
    'm_homeLoginUserNameEdit->clear();',
    'm_homeLoginPasswordEdit->clear();',
    'homeLoginScanLine',
    'm_homeContentStack->addWidget(m_homeLoginPage)',
    'm_homeContentStack->addWidget(m_homeModulesPage)',
    'LTZK::authenticateFromHomePage()',
    'LTZKAuthManager::instance().authenticateUser',
    'showHomeModulesPage();',
    'showHomeLoginPage();',
    'LTZK::showAccessControlPage()',
    'm_workspaceStack->setCurrentWidget(m_accessControlPage)',
    'm_accessControlWidget->setCurrentOperator(gCurrentUserSession.userName)'
)) {
    if ($sourceText -notmatch [regex]::Escape($needle)) {
        throw "Embedded login/access source marker missing: $needle"
    }
}

if ($qrcText -notmatch [regex]::Escape('Resources/home_login_background.png')) {
    throw 'Home login background image must be embedded in LTZK.qrc.'
}
$metricScript = @'
import sys
from PIL import Image, ImageFilter, ImageStat
im = Image.open(sys.argv[1]).convert("L").resize((512, 288))
contrast = ImageStat.Stat(im).stddev[0]
edge = ImageStat.Stat(im.filter(ImageFilter.FIND_EDGES)).mean[0]
print(f"Home login background pattern metrics: contrast={contrast:.2f}, edge={edge:.2f}")
if contrast < 3.0 or edge < 3.5:
    raise SystemExit(1)
'@
$metricScriptPath = Join-Path $env:TEMP 'verify_home_login_background_pattern.py'
Set-Content -Encoding UTF8 -LiteralPath $metricScriptPath -Value $metricScript
& py -3.10 $metricScriptPath $homeBackground
if ($LASTEXITCODE -ne 0) {
    throw 'Home login background must contain visible but restrained simple patterning.'
}

foreach ($forbidden in @(
    '登录后进入功能模块',
    'homeLoginSubtitle',
    'panelLayout->addWidget(loginButton, 0, Qt::AlignRight)',
    'homeLoginSideRailLeft',
    'homeLoginSideRailRight',
    'QFrame* sideRailLeft = new QFrame(page)',
    'QFrame* sideRailRight = new QFrame(page)',
    'QHBoxLayout* homeBodyLayout = new QHBoxLayout()',
    'homeBodyLayout->addWidget(sideRailLeft, 0)',
    'homeBodyLayout->addWidget(sideRailRight, 0)',
    'LTZKLoginDialog loginDialog(this)',
    'loginDialog.exec()',
    'LTZKAccessControlDialog* dialog = new LTZKAccessControlDialog(this)',
    'dialog->show()',
    'QHBoxLayout* homePageLayout = new QHBoxLayout(page)',
    'homePageLayout->addWidget(sideRailLeft, 0)',
    'homePageLayout->addWidget(sideRailRight, 0)',
    'QHBoxLayout* pageLayout = new QHBoxLayout(page)',
    '    pageLayout->addWidget(sideRailLeft, 0)',
    '    pageLayout->addWidget(sideRailRight, 0)'
)) {
    if ($sourceText -match [regex]::Escape($forbidden)) {
        throw "Popup login/access code must be removed from LTZK.cpp: $forbidden"
    }
}

if ($accessHeaderText -notmatch 'class LTZKAccessControlDialog final : public QWidget') {
    throw 'Access control component must be embeddable as QWidget.'
}

foreach ($needle in @(
    'void refreshScaledMetrics()',
    'QGridLayout* mEditorLayout',
    'QHBoxLayout* mMainLayout'
)) {
    if ($accessHeaderText -notmatch [regex]::Escape($needle)) {
        throw "Access control scale-refresh header marker missing: $needle"
    }
}

foreach ($needle in @(
    'void LTZKAccessControlDialog::refreshScaledMetrics()',
    'mPermissionsEdit->setMaximumHeight(LTZKUiScale::scaledInt(90, uiScale))',
    'mMainLayout->setContentsMargins(LTZKUiScale::scaledMargins(',
    'mEditorLayout->setHorizontalSpacing(LTZKUiScale::scaledInt(6, uiScale))'
)) {
    if ($accessSourceText -notmatch [regex]::Escape($needle)) {
        throw "Access control runtime scale-refresh marker missing: $needle"
    }
}

foreach ($forbidden in @(
    '登录后进入功能模块',
    'homeLoginSubtitle',
    'panelLayout->addWidget(loginButton, 0, Qt::AlignRight)',
    'homeLoginSideRailLeft',
    'homeLoginSideRailRight',
    'QFrame* sideRailLeft = new QFrame(page)',
    'QFrame* sideRailRight = new QFrame(page)',
    'QHBoxLayout* homeBodyLayout = new QHBoxLayout()',
    'homeBodyLayout->addWidget(sideRailLeft, 0)',
    'homeBodyLayout->addWidget(sideRailRight, 0)',
    'setWindowTitle',
    'setWindowFlags'
)) {
    if ($accessSourceText -match [regex]::Escape($forbidden)) {
        throw "Access control page must not configure itself as a popup window: $forbidden"
    }
}

Write-Host 'Embedded login and access-control pages verified.'






















