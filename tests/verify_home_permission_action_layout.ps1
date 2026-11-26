$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -LiteralPath (Join-Path $root 'LTZK.cpp') -Raw
$header = Get-Content -LiteralPath (Join-Path $root 'LTZK.h') -Raw
$styleDir = Join-Path $root 'Resources\styles'

if ($source -match 'QStringLiteral\("权限管理"\),\s*\r?\n\s*QStringLiteral\("用户角色 · 权限配置"\)') {
    throw '权限管理仍保留在主页模块卡片配置中。'
}

foreach ($required in @(
    'm_homeAccessControlButton->setText(QStringLiteral("权限管理"))',
    'm_homeLogoutButton->setText(QStringLiteral("退出账号"))',
    'm_homeAccessControlButton->setProperty("homeUtilityButton", true)',
    'm_homeLogoutButton->setProperty("homeUtilityButton", true)',
    'homeUtilityButtonFont.setPixelSize(scaledInt(panel,',
    'homeUtilityButtonFont.setWeight(QFont::DemiBold)',
    'm_homeAccessControlButton->setFont(homeUtilityButtonFont)',
    'm_homeLogoutButton->setFont(homeUtilityButtonFont)',
    'm_homeAccessControlButton->setToolButtonStyle(Qt::ToolButtonTextOnly)',
    'm_homeLogoutButton->setToolButtonStyle(Qt::ToolButtonTextOnly)',
    'homeUtilityActionsLayout->addWidget(m_homeAccessControlButton)',
    'homeUtilityActionsLayout->addWidget(m_homeLogoutButton)',
    'connect(m_homeAccessControlButton, &QToolButton::clicked, this, &LTZK::showAccessControlPage)'
)) {
    if (-not $source.Contains($required)) {
        throw "主页权限管理/退出账号按钮布局缺少: $required"
    }
}

foreach ($requiredUtilitySizeLine in @(
    'm_homeAccessControlButton->setFixedSize(scaledSize(panel, kHomeUtilityButtonWidth, kHomeUtilityButtonHeight))',
    'm_homeLogoutButton->setFixedSize(scaledSize(panel, kHomeUtilityButtonWidth, kHomeUtilityButtonHeight))'
)) {
    if (-not $source.Contains($requiredUtilitySizeLine)) {
        throw "主页权限管理/退出账号按钮必须使用同一组首页底部按钮尺寸常量: $requiredUtilitySizeLine"
    }
}

foreach ($requiredResponsiveConstraint in @(
    'constexpr int kHomeUtilityButtonWidth = 172;',
    'constexpr int kHomeUtilityButtonHeight = 54;',
    'constexpr int kHomeUtilityButtonFontSize = 16;',
    'constexpr int kHomeGridToUtilityGap = 45;',
    'constexpr int kHomeUtilityBottomGap = 20;',
    'scaledSize(panel, kHomeUtilityButtonWidth, kHomeUtilityButtonHeight)',
    'scaledSize(scaleWidget, kHomeUtilityButtonWidth, kHomeUtilityButtonHeight)',
    'scaledInt(panel, kHomeGridToUtilityGap)',
    'scaledInt(panel, kHomeUtilityBottomGap)'
)) {
    if (-not $source.Contains($requiredResponsiveConstraint)) {
        throw "主页底部按钮缺少路线 2 统一自适应约束: $requiredResponsiveConstraint"
    }
}

$gridToUtilityPattern = 'stageLayout->addLayout\(grid, 0\);\s*stageLayout->setAlignment\(grid, Qt::AlignHCenter\);\s*QSpacerItem\* utilityGap = new QSpacerItem\('
if ($source -notmatch $gridToUtilityPattern) {
    throw '四个模块和主页底部按钮之间必须使用统一自适应间距，不能继续依赖中间弹性留白。'
}

if (-not $header.Contains('void updateHomeUtilityButtonMetrics();')) {
    throw '主页底部按钮缺少 QSS 后尺寸重锁声明: void updateHomeUtilityButtonMetrics();'
}
foreach ($requiredMetricGuard in @(
    'void LTZK::updateHomeUtilityButtonMetrics()',
    'button->setMinimumSize(buttonSize)',
    'button->setMaximumSize(buttonSize)',
    'updateHomeUtilityButtonMetrics();'
)) {
    if (-not $source.Contains($requiredMetricGuard)) {
        throw "主页底部按钮缺少 QSS 后尺寸重锁逻辑: $requiredMetricGuard"
    }
}

$homeBlockPattern = 'm_homeAccessControlButton = new QToolButton\(panel\);(?s).*?connect\(m_homeLogoutButton, &QToolButton::clicked, this, &LTZK::logoutCurrentUser\);'
$homeBlock = [regex]::Match($source, $homeBlockPattern)
if (-not $homeBlock.Success) {
    throw '未找到主页权限管理/退出账号按钮代码块。'
}
foreach ($forbiddenInBlock in @(
    'setProperty("homeModuleButton", true)',
    'm_homeLogoutButton->setIcon(',
    'm_homeLogoutButton->setIconSize(',
    'm_homeLogoutButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon)',
    'setMinimumSize(scaledSize(panel,'
)) {
    if ($homeBlock.Value.Contains($forbiddenInBlock)) {
        throw "主页底部按钮仍受到模块按钮体系或非最终尺寸控制影响: $forbiddenInBlock"
    }
}

$adaptiveFooterPattern = 'stageLayout->addLayout\(homeUtilityActionsLayout, 0\);\s*stageLayout->setAlignment\(homeUtilityActionsLayout, Qt::AlignHCenter\);\s*QSpacerItem\* bottomGap = new QSpacerItem\('
if ($source -notmatch $adaptiveFooterPattern) {
    throw '主页底部操作按钮下方应使用独立的自适应底部间距，不能继续依赖末尾弹性留白。'
}

foreach ($themeFile in @('ltzk_sci_fi.qss', 'ltzk_white.qss', 'ltzk_black.qss')) {
    $themePath = Join-Path $styleDir $themeFile
    $theme = Get-Content -LiteralPath $themePath -Raw
    if ($theme -notmatch 'QToolButton\[homeUtilityButton="true"\]\s*\{[^}]*padding:\s*0[^}]*font-weight:\s*600') {
        throw "$themeFile 缺少主页底部按钮的基准字号/字重样式。"
    }
}

if ($source.Contains('stageLayout->addSpacing(scaledInt(panel, 188))')) {
    throw '主页布局仍保留固定底部留白。'
}

Write-Host 'Home permission action layout verified.'
