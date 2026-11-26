$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$sourceText = Get-Content -Raw -Encoding Default $source
$matches = [regex]::Matches($sourceText, 'QStringLiteral\("([^"]*)"\)')
$bad = @()

foreach ($match in $matches) {
    $literal = $match.Groups[1].Value
    if ($literal -match '\?{3,}') {
        $line = ($sourceText.Substring(0, $match.Index) -split "`n").Count
        $bad += "line ${line}: $literal"
    }
}

if ($bad.Count -gt 0) {
    throw "Visible QStringLiteral text contains replacement question marks:`n$($bad -join "`n")"
}

Write-Host 'Visible QStringLiteral text has no replacement question mark runs.'
