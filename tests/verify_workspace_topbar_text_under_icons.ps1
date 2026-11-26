$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$sourceText = Get-Content -Raw -Encoding UTF8 $source

foreach ($required in @(
    'QString topBarButtonText(QString text)',
    'Qt::ToolButtonTextUnderIcon',
    'button->setText(',
    'topBarButtonText(',
    'int topBarScaledInt(const QWidget* widget, int value)',
    'return scaledInt(widget, value)',
    'button->setIconSize(QSize(topBarScaledInt(scaleWidget, 30), topBarScaledInt(scaleWidget, 30)))',
    'button->setMinimumSize(QSize(topBarScaledInt(scaleWidget, 105), topBarScaledInt(scaleWidget, 84)))',
    'void setWorkspacePluginToolButtonCompactMetrics(QToolButton* button, const QWidget* scaleWidget)',
    'button->setMinimumWidth(topBarScaledInt(scaleWidget, 100))',

    'button->setSizePolicy(',
    'featureButton->setObjectName(QStringLiteral("workspacePluginAction_%1").arg(featureAction->objectName()))',
    'backHomeButton->setProperty("topBarTool", true)',
    'backHomeButton->setText(QStringLiteral(',
    'backHomeButton->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/template_icon_return_home.svg")))',
    'backHomeButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon)',
    'setTopBarToolButtonResponsiveMetrics(backHomeButton, topActionStrip)'
)) {
    if (-not $sourceText.Contains($required)) {
        throw "Workspace top bar text-under-icon behavior missing: $required"
    }
}

$createStart = $sourceText.IndexOf('QToolButton* LTZK::createTopBarToolButton')
$createEnd = $sourceText.IndexOf('QToolButton* LTZK::createModuleLaunchButton', $createStart)
if ($createStart -lt 0 -or $createEnd -lt 0) {
    throw 'Could not locate createTopBarToolButton body before createModuleLaunchButton.'
}

$createBody = $sourceText.Substring($createStart, $createEnd - $createStart)
if ($createBody.Contains('Qt::ToolButtonIconOnly')) {
    throw 'Workspace top bar buttons must no longer be icon-only.'
}

foreach ($forbidden in @(
    'button->setIconSize(QSize(30, 30))',
    'button->setMinimumSize(QSize(105, 84))',

    'Q_UNUSED(scaleWidget)'
)) {
    if ($sourceText.Contains($forbidden)) {
        throw "Workspace top bar buttons must not use fixed non-adaptive metrics: $forbidden"
    }
}

Write-Host 'Workspace top bar buttons show text under icons with adaptive metrics.'








