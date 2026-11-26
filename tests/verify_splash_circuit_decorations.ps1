$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$generator = Join-Path $root 'tools\generate_sci_fi_splash.py'

if (-not (Test-Path $generator)) {
    throw "Missing splash generator: $generator"
}

$text = Get-Content -Raw -Encoding UTF8 $generator

foreach ($required in @(
    'def add_circuit_sci_fi_layer(',
    'add_circuit_sci_fi_layer(image)',
    'circuit_segments =',
    'draw.line((x1, y1, x2, y2), fill=(27, 201, 255, 30), width=2)',
    'draw.rectangle((x - 18, y - 5, x + 18, y + 5), fill=(27, 201, 255, 34))',
    'draw.rectangle((width - x - 18, y - 5, width - x + 18, y + 5), fill=(255, 178, 56, 20))'
)) {
    if (-not $text.Contains($required)) {
        throw "Splash background must include subtle circuit-style sci-fi decoration: $required"
    }
}

foreach ($forbidden in @(
    'def draw_sci_fi_decorations(',
    'draw_sci_fi_decorations(image)',
    'outline=(27, 201, 255',
    'outline=(255, 178, 56',
    'for x, y, size, color in',
    'draw_frame(image)',
    'def draw_frame(',
    'draw_grid(image)',
    'def draw_grid(',
    'draw_panel(image)',
    'def draw_panel('
)) {
    if ($text.Contains($forbidden)) {
        throw "Splash decoration must not restore rings, particles, frames, boxes, or dashed/grid lines: $forbidden"
    }
}

Write-Host 'Splash circuit sci-fi decoration verified.'
