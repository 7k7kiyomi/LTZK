$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$generator = Join-Path $root 'tools\generate_sci_fi_splash.py'

if (-not (Test-Path $generator)) {
    throw "Missing splash generator: $generator"
}

$text = Get-Content -Raw -Encoding UTF8 $generator

if ($text.Contains('title_font = load_font(235, bold=True)')) {
    throw 'Splash title must not use bold title font.'
}

foreach ($required in @(
    'TITLE = "\u9647\u56fe\u667a\u63a7\u5e73\u53f0"',
    'SUBTITLE = "\u2014\u2014 \u7518\u8083\u7701\u57fa\u7840\u5730\u7406\u4fe1\u606f\u4e2d\u5fc3 \u2014\u2014"',
    'title_font = load_font(235, bold=False)',
    'subtitle_font = load_font(65, bold=False)',
    'subtitle_y = title_y + title_h + 82 - subtitle_bbox[1]',
    'draw.text((title_x, title_y), TITLE, font=title_font, fill=(255, 255, 255, 255))',
    'draw.text((subtitle_x, subtitle_y), SUBTITLE, font=subtitle_font, fill=(204, 237, 255, 235))'
)) {
    if (-not $text.Contains($required)) {
        throw "Splash title style must preserve the original title setting: $required"
    }
}

Write-Host 'Splash title preservation verified.'
