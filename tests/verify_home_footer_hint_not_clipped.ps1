$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding UTF8 $source

foreach ($required in @(
    'resultHint->setMinimumHeight(qMax(34, scaledInt(stage, 38)))',
    'resultHint->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum)',
    'resultHint->setWordWrap(false)',
    'resultHint->setTextFormat(Qt::PlainText)'
)) {
    if (-not $text.Contains($required)) {
        throw "Home footer hint clipping guard missing: $required"
    }
}

foreach ($forbidden in @(
    'resultHint->setFixedHeight(scaledInt(stage, 38))',
    'resultHint->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)'
)) {
    if ($text.Contains($forbidden)) {
        throw "Home footer hint still uses shrinkable fixed-height layout: $forbidden"
    }
}

Write-Host 'Home footer hint clipping guard verified.'
