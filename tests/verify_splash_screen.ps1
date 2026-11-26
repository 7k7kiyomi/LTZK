$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$main = Join-Path $root "main.cpp"
$qrc = Join-Path $root "LTZK.qrc"
$splash = Join-Path $root "Resources\splash.png"

foreach ($path in @($main, $qrc, $splash)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$mainText = Get-Content $main -Raw -Encoding UTF8
$qrcText = Get-Content $qrc -Raw -Encoding UTF8

if ($mainText -notmatch "#include\s+<QSplashScreen>") {
    throw "main.cpp does not include QSplashScreen"
}

if ($mainText -notmatch "QSplashScreen\s+splash") {
    throw "main.cpp does not create a splash screen"
}

if ($mainText -notmatch "splash\.finish\(&w\)") {
    throw "main.cpp does not close the splash screen after showing the main window"
}

if ($qrcText -notmatch [regex]::Escape("<file>Resources/splash.png</file>")) {
    throw "LTZK.qrc does not include Resources/splash.png"
}

Add-Type -AssemblyName System.Drawing
$image = [System.Drawing.Image]::FromFile($splash)
try {
    if ($image.Width -ne 1977 -or $image.Height -ne 946) {
        throw "Splash image size is $($image.Width)x$($image.Height), expected 1977x946"
    }
}
finally {
    $image.Dispose()
}

Write-Host "Splash screen wiring verified."
