$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$generator = Join-Path $root 'tools\generate_sci_fi_splash.py'

if (-not (Test-Path $generator)) {
    throw "Missing splash generator: $generator"
}

$text = Get-Content -Raw -Encoding UTF8 $generator

foreach ($forbidden in @(
    'def draw_grid(',
    'draw_grid(image)',
    'def draw_panel(',
    'draw_panel(image)',
    'def draw_frame(',
    'draw_frame(image)',
    'center_y = int(height * 0.73)',
    'width // 2 - 360',
    'width // 2 - 210',
    'for i in range(28):'
)) {
    if ($text.Contains($forbidden)) {
        throw "Splash must not draw title frame, lower lines, or page dashed/grid lines: $forbidden"
    }
}

foreach ($required in @(
    'draw_background(ImageDraw.Draw(image), *SIZE)',
    'add_glow_layer(image)',
    'draw_text(image)'
)) {
    if (-not $text.Contains($required)) {
        throw "Splash must keep only the sci-fi background and original title: $required"
    }
}

Write-Host 'Splash extra line removal verified.'
