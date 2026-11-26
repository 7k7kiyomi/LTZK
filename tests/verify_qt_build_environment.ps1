$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$projectPath = Join-Path $root 'map_product_manager_tools\map_product_manager_tools.vcxproj'
$wrapperPath = Join-Path $root 'map_product_manager_tools\qmake.bat'

foreach ($path in @($projectPath, $wrapperPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing Qt build environment component: $path"
    }
}

$project = Get-Content -Raw -Encoding UTF8 $projectPath
$wrapper = Get-Content -Raw -Encoding Default $wrapperPath

$releaseMarker = @'
'<PropertyGroup Label="QtSettings" Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
'@.Trim()
$releaseStart = $project.IndexOf($releaseMarker)
if ($releaseStart -lt 0) {
    throw 'Could not locate the Release x64 Qt settings.'
}
$releaseEnd = $project.IndexOf('</PropertyGroup>', $releaseStart)
if ($releaseEnd -lt 0) {
    throw 'Could not locate the end of the Release x64 Qt settings.'
}
$releaseSettings = $project.Substring($releaseStart, $releaseEnd - $releaseStart)
if (-not $releaseSettings.Contains('<QtToolsPath>$(MSBuildProjectDirectory)</QtToolsPath>')) {
    throw 'Release x64 must use the project qmake wrapper.'
}

foreach ($required in @(
    'o4w_env.bat',
    'apps\Qt5\bin\qmake.exe',
    'LTZK_SAVED_PATH',
    'PATH=%PATH%;%LTZK_SAVED_PATH%',
    '%*'
)) {
    if (-not $wrapper.Contains($required)) {
        throw "qmake wrapper is missing: $required"
    }
}

Write-Output 'Qt build environment verification passed.'
