$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding UTF8 $source

$metricsStart = $text.IndexOf('void setTopBarToolButtonResponsiveMetrics(QToolButton* button, const QWidget* scaleWidget)')
$metricsEnd = $text.IndexOf('class ResponsiveHomeModuleButton', $metricsStart)
if ($metricsStart -lt 0 -or $metricsEnd -lt 0) {
    throw 'Could not locate the common top-bar button metrics helper.'
}

$metricsBody = $text.Substring($metricsStart, $metricsEnd - $metricsStart)
if (-not $metricsBody.Contains('button->setMinimumSize(QSize(topBarScaledInt(scaleWidget, 105), topBarScaledInt(scaleWidget, 84)))')) {
    throw 'Common top-bar buttons must retain the 105x84 scaled minimum size.'
}

$compactStart = $text.IndexOf('void setWorkspacePluginToolButtonCompactMetrics(QToolButton* button, const QWidget* scaleWidget)')
if ($compactStart -lt 0) {
    throw 'Missing the dynamic plugin button compact metrics helper.'
}
$compactEnd = $text.IndexOf('class ResponsiveHomeModuleButton', $compactStart)
if ($compactEnd -lt 0) {
    throw 'Could not locate the end of the dynamic plugin button compact metrics helper.'
}

$compactBody = $text.Substring($compactStart, $compactEnd - $compactStart)
foreach ($required in @(
    'setTopBarToolButtonResponsiveMetrics(button, scaleWidget)',
    'button->setMinimumWidth(topBarScaledInt(scaleWidget, 100))'
)) {
    if (-not $compactBody.Contains($required)) {
        throw "Dynamic plugin compact metrics requirement missing: $required"
    }
}

$setterStart = $text.IndexOf('void LTZK::setWorkspacePluginActions(const QList<QAction*>& actions)')
$setterEnd = $text.IndexOf('void LTZK::activateBusinessModule', $setterStart)
if ($setterStart -lt 0 -or $setterEnd -lt 0) {
    throw 'Could not locate setWorkspacePluginActions body.'
}

$setterBody = $text.Substring($setterStart, $setterEnd - $setterStart)
if (-not $setterBody.Contains('setWorkspacePluginToolButtonCompactMetrics(featureButton, pluginStrip)')) {
    throw 'Dynamic plugin buttons must use the compact metrics helper.'
}
if ($setterBody.Contains('setTopBarToolButtonResponsiveMetrics(featureButton, pluginStrip)')) {
    throw 'Dynamic plugin buttons must not use the common fixed metrics directly.'
}

foreach ($required in @(
    'setTopBarToolButtonResponsiveMetrics(m_workspaceLogoutButton, topActionStrip)',
    'setTopBarToolButtonResponsiveMetrics(backHomeButton, topActionStrip)'
)) {
    if (-not $text.Contains($required)) {
        throw "Fixed top-bar button must retain common metrics: $required"
    }
}

Write-Host 'Workspace dynamic plugin buttons use a 100-scaled compact minimum width while fixed top-bar buttons retain 105x84.'
