$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$generator = Join-Path $root 'tools\generate_sci_fi_splash.py'

if (-not (Test-Path $generator)) {
    throw "Missing splash generator: $generator"
}

$text = Get-Content -Raw -Encoding UTF8 $generator

foreach ($required in @(
    'def add_gis_spatial_depth_layer(',
    'add_gis_spatial_depth_layer(image)',
    'scan_arcs = (',
    'map_paths = (',
    'coordinate_points = (',
    'telemetry_surfaces = (',
    'draw.arc(',
    'draw.line(path, fill=(27, 201, 255, 36), width=2, joint="curve")',
    'draw.ellipse((x - 4, y - 4, x + 4, y + 4), fill=(204, 237, 255, 96))',
    'draw.ellipse((x - 12, y - 12, x + 12, y + 12), fill=(27, 201, 255, 18))',
    'draw.rounded_rectangle(surface, radius=6, fill=(6, 24, 40, 42))'
)) {
    if (-not $text.Contains($required)) {
        throw "Splash designer GIS depth layer is missing: $required"
    }
}

$gisIndex = $text.IndexOf('add_gis_spatial_depth_layer(image)')
$circuitIndex = $text.IndexOf('add_circuit_sci_fi_layer(image)')
$textIndex = $text.IndexOf('draw_text(image)')
if ($gisIndex -lt 0 -or $circuitIndex -lt 0 -or $textIndex -lt 0 -or $gisIndex -gt $circuitIndex -or $circuitIndex -gt $textIndex) {
    throw 'Splash GIS depth layer must render before circuit details and text.'
}

Write-Host 'Splash designer GIS depth layer verified.'
