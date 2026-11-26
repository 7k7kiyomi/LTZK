$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$splash = Join-Path $root 'Resources\splash.png'

if (-not (Test-Path $splash)) {
    throw "Missing splash image: $splash"
}

Add-Type -AssemblyName System.Drawing
$image = [System.Drawing.Bitmap]::FromFile($splash)
try {
    if ($image.Width -ne 1977 -or $image.Height -ne 946) {
        throw "Splash image size changed: $($image.Width)x$($image.Height)"
    }

    $samplePoints = @()
    foreach ($x in 20, 160, 360, 760, 1200, 1620, 1950) {
        foreach ($y in 20, 120, 820, 925) {
            $samplePoints += ,@($x, $y)
        }
    }

    $r = 0
    $g = 0
    $b = 0
    foreach ($point in $samplePoints) {
        $pixel = $image.GetPixel($point[0], $point[1])
        $r += $pixel.R
        $g += $pixel.G
        $b += $pixel.B
    }

    $count = [double]$samplePoints.Count
    $avgR = $r / $count
    $avgG = $g / $count
    $avgB = $b / $count
    $brightness = ($avgR + $avgG + $avgB) / 3.0

    if ($brightness -gt 60) {
        throw "Splash edge background is too bright for the sci-fi platform theme: brightness=$([math]::Round($brightness, 2))"
    }

    if ($brightness -lt 32) {
        throw "Splash edge background is too dark after the requested lightening: brightness=$([math]::Round($brightness, 2))"
    }

    if ($avgB -lt $avgG -or $avgG -lt $avgR) {
        throw "Splash edge background should stay in the platform blue-cyan family: avg=($([math]::Round($avgR, 2)), $([math]::Round($avgG, 2)), $([math]::Round($avgB, 2)))"
    }

}
finally {
    $image.Dispose()
}

Write-Host 'Sci-fi splash palette verified.'
