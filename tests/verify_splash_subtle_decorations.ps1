$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$generator = Join-Path $root 'tools\generate_sci_fi_splash.py'

if (-not (Test-Path $generator)) {
    throw "Missing splash generator: $generator"
}

$text = Get-Content -Raw -Encoding UTF8 $generator

foreach ($required in @(
    'def add_atmospheric_sci_fi_layer(',
    'add_atmospheric_sci_fi_layer(image)',
    'ImageFilter.GaussianBlur(72)',
    'fill=(27, 201, 255, 22)',
    'fill=(255, 178, 56, 14)'
)) {
    if (-not $text.Contains($required)) {
        throw "Splash background must use atmospheric sci-fi light decoration, not rings or particles: $required"
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

Write-Host 'Splash atmospheric sci-fi decoration verified.'
