$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding Default $source

$longitudeLabelEscape = '\u7ECF\u5EA6\uFF1A'
$latitudeLabelEscape = '\u7EAC\u5EA6\uFF1A'

if (-not $text.Contains($longitudeLabelEscape)) {
    throw 'Coordinate status label must display Chinese longitude label.'
}

if (-not $text.Contains($latitudeLabelEscape)) {
    throw 'Coordinate status label must display Chinese latitude label.'
}

$functionMatch = [regex]::Match(
    $text,
    'void\s+LTZK::mouseCoordinateChanged\s*\([^)]*\)\s*\{(?<body>[\s\S]*?)^\}',
    [System.Text.RegularExpressions.RegexOptions]::Multiline
)

if (-not $functionMatch.Success) {
    throw 'Could not find LTZK::mouseCoordinateChanged implementation.'
}

$body = $functionMatch.Groups['body'].Value

if ($body -match 'lon\s*:' -or $body -match 'lat\s*:') {
    throw 'Coordinate status label still uses lon:/lat: text instead of Chinese labels.'
}

Write-Host 'Coordinate status labels verified.'
