$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding UTF8 $source
$classStart = $text.IndexOf('class ResponsiveHomeModuleButton final : public QToolButton')
$classEnd = $text.IndexOf('class SciFiHomePage final', $classStart)
if ($classStart -lt 0 -or $classEnd -lt 0) {
    throw 'Could not locate ResponsiveHomeModuleButton class body.'
}
$classBody = $text.Substring($classStart, $classEnd - $classStart)

foreach ($required in @(
    'setText(QString())',
    'setToolTip(moduleTitle + QStringLiteral(" - ") + subtitle)',
    'setAccessibleName(moduleCode + QStringLiteral(" ") + moduleTitle + QStringLiteral(" ") + subtitle)',
    'm_codeLabel = new QLabel(moduleCode, this)',
    'm_titleLabel = new QLabel(moduleTitle, this)',
    'm_subtitleLabel = new QLabel(subtitle, this)'
)) {
    if (-not $classBody.Contains($required)) {
        throw "Single text layer requirement missing: $required"
    }
}

foreach ($forbidden in @(
    'setText(moduleCode + QStringLiteral("\n") + moduleTitle + QStringLiteral("\n") + subtitle)',
    'setText(moduleCode +',
    'QToolButton::paintEvent'
)) {
    if ($classBody.Contains($forbidden)) {
        throw "Home module button would draw duplicate text: $forbidden"
    }
}

Write-Host 'Home module card text is drawn by labels only, not by QToolButton text.'