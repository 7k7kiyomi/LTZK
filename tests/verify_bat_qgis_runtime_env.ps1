$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$main = Join-Path $root 'main.cpp'
$project = Join-Path $root 'LTZK.vcxproj'
$deploymentDir = Join-Path $root 'deployment'
$batFiles = @(
    (Join-Path $root 'x64\Debug\start_LTZK.bat'),
    (Join-Path $root 'x64\Release\start_LTZK.bat')
)

$outputDirs = @(
    (Join-Path $root 'x64\Debug'),
    (Join-Path $root 'x64\Release')
)

if (-not (Test-Path $main)) {
    throw "Missing required file: $main"
}

if (-not (Test-Path $project)) {
    throw "Missing required file: $project"
}

$mainText = Get-Content -Raw -Encoding Default $main
$projectText = Get-Content -Raw -Encoding UTF8 $project

if ($mainText.Contains('QgsApplication::setPrefixPath("E:/extern/QGIS_32815", true)')) {
    throw 'main.cpp still hardcodes the QGIS prefix path.'
}

if ($mainText -notmatch 'qEnvironmentVariable\s*\(\s*"QGIS_PREFIX_PATH"\s*\)') {
    throw 'main.cpp does not read QGIS_PREFIX_PATH from the launch environment.'
}

if ($mainText -notmatch 'QgsApplication::setPrefixPath\s*\(\s*qgisPrefixPath\s*,\s*true\s*\)') {
    throw 'main.cpp does not use the resolved QGIS prefix path.'
}

if (-not $mainText.Contains('QGIS runtime path was not found.')) {
    throw 'main.cpp should report a missing QGIS runtime path before initializing QGIS.'
}

foreach ($required in @(
    'CopyLtzKStartupScripts',
    '$(MSBuildProjectDirectory)\deployment\start_LTZK.bat',
    '$(MSBuildProjectDirectory)\deployment\.env',
    'DestinationFolder="$(OutDir)"'
)) {
    if (-not $projectText.Contains($required)) {
        throw "Project does not copy distributable startup scripts: $required"
    }
}

if ($projectText.Contains('$(MSBuildProjectDirectory)\deployment\.env.example')) {
    throw 'Project should not copy .env.example. Distributable package uses .env directly.'
}

foreach ($file in @('start_LTZK.bat', '.env')) {
    $path = Join-Path $deploymentDir $file
    if (-not (Test-Path $path)) {
        throw "Missing deployment startup template: $path"
    }
}

$deploymentExample = Join-Path $deploymentDir '.env.example'
if (Test-Path $deploymentExample) {
    throw "Deployment should not include .env.example: $deploymentExample"
}

foreach ($bat in $batFiles) {
    if (-not (Test-Path $bat)) {
        throw "Missing startup script: $bat"
    }

    $batText = Get-Content -Raw -Encoding Default $bat

    foreach ($required in @(
        'if not exist "%APP_DIR%.env"',
        'for /f "usebackq eol=# tokens=1,* delims=="',
        'if not "%QGIS_PREFIX_PATH:~1,1%"==":"',
        'set "QGIS_PREFIX_PATH=%APP_DIR%%QGIS_PREFIX_PATH%"',
        'if not "%OSGEO4W_ROOT:~1,1%"==":"',
        'set "OSGEO4W_ROOT=%APP_DIR%%OSGEO4W_ROOT%"',
        'LTZK.exe'
    )) {
        if (-not $batText.Contains($required)) {
            throw "Startup script is missing from $bat`: $required"
        }
    }

    foreach ($forbidden in @(
        'set "QGIS_PREFIX_PATH=E:\extern\QGIS_32815"',
        'set "OSGEO4W_ROOT=E:\extern\OSGeo4W_32815"',
        'call "%APP_DIR%env.bat"',
        'env.bat',
        '.env.example'
    )) {
        if ($batText.Contains($forbidden)) {
            throw "Startup script should read .env directly and should not hardcode or call env.bat: $bat"
        }
    }
}

foreach ($dir in $outputDirs) {
    $envFile = Join-Path $dir '.env'

    if (-not (Test-Path $envFile)) {
        throw "Missing runtime environment file: $envFile"
    }

    $envExample = Join-Path $dir '.env.example'
    if (Test-Path $envExample) {
        throw "Output directory should not include .env.example: $envExample"
    }

    $envText = Get-Content -Raw -Encoding Default $envFile
    foreach ($required in @(
        'QGIS_PREFIX_PATH=',
        'OSGEO4W_ROOT='
    )) {
        if (-not $envText.Contains($required)) {
            throw "Runtime environment file is missing from $envFile`: $required"
        }
    }

}

Write-Host 'BAT startup QGIS runtime environment verified.'
