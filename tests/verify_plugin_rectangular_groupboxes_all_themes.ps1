$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$themeFiles = @(
    'Resources\styles\ltzk_sci_fi.qss',
    'Resources\styles\ltzk_black.qss',
    'Resources\styles\ltzk_white.qss',
    'Resources\styles\ltzk_field_notes.qss',
    'Resources\styles\ltzk_steel_warm_white.qss'
)

foreach ($themeFile in $themeFiles) {
    $themePath = Join-Path $root $themeFile
    if (-not (Test-Path -LiteralPath $themePath)) {
        throw "Theme file missing: $themeFile"
    }

    $qssText = Get-Content -Raw -Encoding UTF8 $themePath
    $groupRules = [regex]::Matches($qssText, '(?m)^QGroupBox\s*\{(?<body>[^}]*)\}')
    if ($groupRules.Count -eq 0) {
        throw "Theme $themeFile is missing a QGroupBox rule."
    }

    $effectiveRuleBody = $groupRules[$groupRules.Count - 1].Groups['body'].Value
    $radiusMatch = [regex]::Match($effectiveRuleBody, 'border-radius\s*:\s*(?<value>[^;]+);')
    if ($radiusMatch.Success -and $radiusMatch.Groups['value'].Value.Trim() -ne '0px') {
        throw "Theme $themeFile must keep plugin QGroupBox containers rectangular."
    }
}

Write-Host 'Plugin QGroupBox containers are rectangular in all platform themes.'
