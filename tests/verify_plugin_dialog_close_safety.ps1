$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$source = Get-Content -Raw -Encoding UTF8 $sourcePath

$styleStart = $source.IndexOf('void LTZK::applyPlatformStyleToPluginWindow(QWidget* widget)')
if ($styleStart -lt 0) {
    throw 'Could not locate applyPlatformStyleToPluginWindow.'
}

$styleEnd = $source.IndexOf('void LTZK::normalizePluginTableEmbeddedControls', $styleStart)
if ($styleEnd -lt 0) {
    throw 'Could not locate end of applyPlatformStyleToPluginWindow.'
}

$styleBody = $source.Substring($styleStart, $styleEnd - $styleStart)
foreach ($forbidden in @(
    'QPointer<QWidget> guardedWidget(widget);',
    'QTimer::singleShot(0, widget',
    'QTimer::singleShot(80, widget',
    'QTimer::singleShot(250, widget',
    'normalizePluginTableEmbeddedControls'
)) {
    if ($styleBody.Contains($forbidden)) {
        throw "Plugin dialog crash guard forbids delayed host-side normalization: $forbidden"
    }
}

$normalizeStart = $source.IndexOf('void LTZK::normalizePluginTableEmbeddedControls(QWidget* widget)')
if ($normalizeStart -lt 0) {
    throw 'Could not locate normalizePluginTableEmbeddedControls.'
}

$required = @(
    'if (!widget)',
    'if (widget->isHidden() || widget->testAttribute(Qt::WA_WState_Hidden))',
    'QSignalBlocker blocker(lineEdit);'
)

foreach ($token in $required) {
    if (-not $source.Contains($token)) {
        throw "Plugin table normalization internal guard missing: $token"
    }
}

Write-Host 'Plugin dialog delayed normalization removal verified.'



