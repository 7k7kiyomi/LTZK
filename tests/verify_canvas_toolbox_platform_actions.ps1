$ErrorActionPreference = "Stop"

$verificationScript = Join-Path $PSScriptRoot "verify_canvas_toolbox_icon_and_menu_scope.ps1"
& powershell -NoProfile -ExecutionPolicy Bypass -File $verificationScript
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Canvas toolbox platform actions verified."
