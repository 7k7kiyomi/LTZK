$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$main = Join-Path $root 'main.cpp'

if (-not (Test-Path $main)) {
    throw "Missing required file: $main"
}

$text = Get-Content -Raw -Encoding Default $main

foreach ($required in @(
    'loadApplicationIcon',
    'QCoreApplication::applicationDirPath()',
    'resources/LTZK.svg',
    'Resources/LTZK.svg',
    ':/LTZK/Resources/LTZK.svg'
)) {
    if (-not $text.Contains($required)) {
        throw "Missing runtime icon loading detail in main.cpp: $required"
    }
}

if ($text -match 'QIcon\s+icon\s*\(\s*":/LTZK/Resources/LTZK\.svg"\s*\)') {
    throw 'Application icon still directly uses only the compiled Qt resource.'
}

if ($text -notmatch 'a\.setWindowIcon\s*\(\s*loadApplicationIcon\s*\(\s*\)\s*\)') {
    throw 'Application window icon should be loaded through loadApplicationIcon().'
}

Write-Host 'Runtime icon precedence verified.'
