$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$sourceText = Get-Content -LiteralPath $sourcePath -Raw

$executeStart = $sourceText.IndexOf('void LTZK::executePluginFeatureAction')
$executeEnd = $sourceText.IndexOf('void LTZK::bindPluginFeatureActions', $executeStart)
if ($executeStart -lt 0 -or $executeEnd -lt 0) {
    throw 'Could not locate executePluginFeatureAction.'
}

$executeBody = $sourceText.Substring($executeStart, $executeEnd - $executeStart)
foreach ($forbidden in @(
    'QFileDialog::getOpenFileName',
    'QFileDialog::getOpenFileNames',
    'on_actionAddVectorData_triggered',
    'addLayerFiles(',
    'addMapLayer('
)) {
    if ($executeBody.Contains($forbidden)) {
        throw "Running a configured plugin feature must not require selecting/loading an input map shp: $forbidden"
    }
}

Write-Host 'Configured plugin actions run without requiring an input shp from the layer manager.'
