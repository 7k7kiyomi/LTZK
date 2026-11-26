$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$sourceText = Get-Content -LiteralPath $sourcePath -Raw

foreach ($forbidden in @(
    'SLOT(writeProject(QDomDocument&))',
    'SLOT(readProject(QDomDocument))'
)) {
    if ($sourceText.Contains($forbidden)) {
        throw "QgsLayerTreeMapCanvasBridge does not expose this old slot signature: $forbidden"
    }
}

if ($sourceText -notmatch 'new\s+QgsLayerTreeMapCanvasBridge\s*\(') {
    throw 'Layer tree bridge creation was not found.'
}

Write-Host 'Layer tree bridge invalid slot guard passed.'