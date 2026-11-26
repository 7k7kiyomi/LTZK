$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding UTF8 $source

foreach ($required in @(
    'QString topBarButtonText(QString text)',
    'constexpr int kMaxCharsPerTopBarLine = 7',
    'lines << text.mid(index, kMaxCharsPerTopBarLine)',
    'button->setMinimumSize(QSize(topBarScaledInt(scaleWidget, 105), topBarScaledInt(scaleWidget, 84)))'
)) {
    if (-not $text.Contains($required)) {
        throw "Top-bar button text wrapping must avoid Qt eliding long Chinese labels: $required"
    }
}

foreach ($tooNarrow in @(96, 112)) {
    if ($text.Contains("button->setMinimumSize(QSize(topBarScaledInt(scaleWidget, $tooNarrow), topBarScaledInt(scaleWidget, 84)))")) {
        throw "Top-bar button width $tooNarrow is too narrow for seven Chinese characters per line and lets QToolButton elide text."
    }
}

foreach ($relativePath in @(
    'Resources\styles\ltzk_sci_fi.qss',
    'Resources\styles\ltzk_black.qss',
    'Resources\styles\ltzk_white.qss'
)) {
    $path = Join-Path $root $relativePath
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }

    $qssText = Get-Content -Raw -Encoding UTF8 $path
    if ($qssText -notmatch 'QToolButton\[topBarTool="true"\]\s*\{[^}]*min-width:\s*105px') {
        throw "Top-bar QSS min-width must match the widened C++ metric in ${relativePath}."
    }
    if ($qssText -match 'QToolButton\[topBarTool="true"\]\s*\{[^}]*min-width:\s*(96)px') {
        throw "Top-bar QSS min-width is still too narrow in ${relativePath}."
    }
}

Write-Host 'Top-bar button text wraps before Qt elides long Chinese labels.'
