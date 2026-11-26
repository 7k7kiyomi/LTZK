$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing source file: $sourcePath"
}

$source = Get-Content -Raw -Encoding UTF8 $sourcePath
$greenPalettePattern = '(?s)if\s*\(themeKey\s*==\s*QStringLiteral\("field-notes"\)\)\s*\{\s*return\s*\{\s*QStringLiteral\("(?<primary>#[0-9A-Fa-f]{6})"\)\s*,\s*QStringLiteral\("(?<secondary>#[0-9A-Fa-f]{6})"\)\s*,\s*QStringLiteral\("(?<accent>#[0-9A-Fa-f]{6})"\)\s*\}\s*;'
$greenPalette = [regex]::Match($source, $greenPalettePattern)
if (-not $greenPalette.Success) {
    throw 'Could not locate the Field Notes plugin icon palette.'
}

$expected = @('#C5E987', '#6FD8D0', '#FFC45E')
$actual = @(
    $greenPalette.Groups['primary'].Value.ToUpperInvariant()
    $greenPalette.Groups['secondary'].Value.ToUpperInvariant()
    $greenPalette.Groups['accent'].Value.ToUpperInvariant()
)
for ($index = 0; $index -lt $expected.Count; $index++) {
    if ($actual[$index] -ne $expected[$index]) {
        throw "Field Notes palette B mismatch at color $index. Expected $($expected[$index]), got $($actual[$index])."
    }
}

Write-Host 'Field Notes plugin icon palette B verified.'
