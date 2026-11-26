$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$headerPath = Join-Path $root 'LTZK.h'
$headerText = Get-Content -LiteralPath $headerPath -Raw

$slotsStart = $headerText.IndexOf('public slots:')
$slotsEnd = $headerText.IndexOf('private:', $slotsStart)
if ($slotsStart -lt 0 -or $slotsEnd -lt 0) {
    throw 'Could not locate LTZK public slots block.'
}

$slotsBlock = $headerText.Substring($slotsStart, $slotsEnd - $slotsStart)
foreach ($forbidden in @(
    'on_actionCoordTrans_triggered',
    'on_actionLoadQgisPlugin_triggered'
)) {
    if ($slotsBlock.Contains($forbidden)) {
        throw "Slot is auto-connected by name but the UI has no matching QAction: $forbidden"
    }
}

Write-Host 'No missing QAction auto-connected slots remain.'
