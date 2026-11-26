$ErrorActionPreference = 'Stop'

# Backward-compatible test entry point. The implementation is now deliberately
# platform-side, so the old plugin-bridge assertions are no longer applicable.
$platformTest = Join-Path $PSScriptRoot 'verify_data_catalog_platform_settings.ps1'
& $platformTest
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
return
