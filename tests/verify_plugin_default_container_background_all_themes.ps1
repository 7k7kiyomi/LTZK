$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$expectedBackgrounds = @{
    'Resources\styles\ltzk_sci_fi.qss' = '#071729'
    'Resources\styles\ltzk_black.qss' = '#050505'
    'Resources\styles\ltzk_white.qss' = '#f8fafc'
}

foreach ($themeFile in $expectedBackgrounds.Keys) {
    $themePath = Join-Path $root $themeFile
    if (-not (Test-Path $themePath)) {
        throw "Theme file missing: $themeFile"
    }

    $qssText = Get-Content $themePath -Raw -Encoding UTF8
    $widgetRule = [regex]::Match($qssText, '(?m)^QWidget\s*\{[^\r\n]*\}')
    if (-not $widgetRule.Success) {
        throw "Theme $themeFile is missing a generic QWidget rule."
    }

    $expectedBackground = "background: $($expectedBackgrounds[$themeFile]);"
    if (-not $widgetRule.Value.Contains($expectedBackground)) {
        throw "Theme $themeFile must define the default container background as $expectedBackground"
    }
}

Write-Host 'Default plugin container backgrounds verified for all platform themes.'
