$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$ifaceHeader = Join-Path $root 'LTZKQgisInterface.h'
$ifaceSource = Join-Path $root 'LTZKQgisInterface.cpp'
$appHeader = Join-Path $root 'LTZK.h'
$appSource = Join-Path $root 'LTZK.cpp'

foreach ($file in @($ifaceHeader, $ifaceSource, $appHeader, $appSource)) {
    if (-not (Test-Path $file)) {
        throw "Missing required file: $file"
    }
}

$ifaceHeaderText = Get-Content -Raw -Encoding Default $ifaceHeader
$ifaceSourceText = Get-Content -Raw -Encoding Default $ifaceSource
$appHeaderText = Get-Content -Raw -Encoding Default $appHeader
$appSourceText = Get-Content -Raw -Encoding Default $appSource

foreach ($required in @(
    'QgsMessageBar* messageBarWidget() const;',
    'QgsMessageBar* m_messageBar = nullptr;'
)) {
    if (-not $appHeaderText.Contains($required)) {
        throw "LTZK must expose a real message bar for plugin startup: $required"
    }
}

foreach ($required in @(
    'm_messageBar = new QgsMessageBar(this)',
    'QgsMessageBar* LTZK::messageBarWidget() const'
)) {
    if (-not $appSourceText.Contains($required)) {
        throw "LTZK.cpp must create and return a real message bar: $required"
    }
}

foreach ($required in @(
    'QAction *safeAction(const QString &objectName, const QString &text) const;',
    'mutable QMap<QString, QAction *> mFallbackActions;'
)) {
    if (-not $ifaceHeaderText.Contains($required)) {
        throw "LTZKQgisInterface.h must provide safe fallback actions: $required"
    }
}

foreach ($required in @(
    'QAction *LTZKQgisInterface::safeAction',
    'return mApp ? mApp->messageBarWidget() : nullptr;',
    'return safeAction(QStringLiteral("actionOpenAttrTable"), QStringLiteral("Open Attribute Table"));',
    'return safeAction(QStringLiteral("actionAddVectorData"), QStringLiteral("Add Vector Layer"));',
    'return safeAction(QStringLiteral("actionPan"), QStringLiteral("Pan"));'
)) {
    if (-not $ifaceSourceText.Contains($required)) {
        throw "LTZKQgisInterface.cpp must avoid null plugin-facing UI objects: $required"
    }
}

Write-Host 'Plugin-facing QGIS interface returns safe UI objects for startup.'
