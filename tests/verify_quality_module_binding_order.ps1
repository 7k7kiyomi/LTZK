$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot

function TextFromCodepoints([int[]]$codepoints) {
    return -join ($codepoints | ForEach-Object { [char]$_ })
}

$expectedFeatures = @(
    (TextFromCodepoints @(0x5730, 0x56FE, 0x6570, 0x636E, 0x9884, 0x5904, 0x7406)),
    (TextFromCodepoints @(0x8D28, 0x68C0, 0x65B9, 0x6848, 0x914D, 0x7F6E)),
    (TextFromCodepoints @(0x8D28, 0x68C0, 0x4EFB, 0x52A1, 0x7BA1, 0x7406)),
    (TextFromCodepoints @(0x8D28, 0x68C0, 0x62A5, 0x544A, 0x751F, 0x6210, 0x4E0E, 0x5BFC, 0x51FA)),
    (TextFromCodepoints @(0x56FE, 0x7247, 0x5730, 0x56FE, 0x8D28, 0x68C0)),
    (TextFromCodepoints @(0x56FE, 0x5E45, 0x63A5, 0x8FB9, 0x4E0E, 0x7CFB, 0x5217, 0x5316, 0x8868, 0x8FBE)),
    (TextFromCodepoints @(0x540C, 0x7CFB, 0x5217, 0x8DE8, 0x6BD4, 0x4F8B, 0x5C3A, 0x8868, 0x8FBE, 0x68C0, 0x67E5))
)

$excludedFeatures = @(
    (TextFromCodepoints @(0x6BD4, 0x4F8B, 0x5C3A, 0x7CBE, 0x5EA6, 0x68C0, 0x67E5)),
    (TextFromCodepoints @(0x5730, 0x56FE, 0x7248, 0x5F0F, 0x914D, 0x7F6E))
)

$qualityModuleTitle = TextFromCodepoints @(0x5730, 0x56FE, 0x6210, 0x679C, 0x8D28, 0x68C0)

$expectedIconNames = @(
    'mActionMapDataPreprocess',
    'mActionQualitySchemeConfig',
    'mActionQualityTaskManager',
    'mActionQualityReportExport',
    'mActionMapLayoutConfig',
    'mActionSheetEdgeSeries',
    'mActionScaleAccuracy'
)

$configPaths = @(
    (Join-Path $root 'config\plugin_bindings.json'),
    (Join-Path $root 'x64\Debug\config\plugin_bindings.json'),
    (Join-Path $root 'x64\Release\config\plugin_bindings.json'),
    (Join-Path $root 'dist\LTZK\config\plugin_bindings.json')
) | Where-Object { Test-Path $_ }

if ($configPaths.Count -eq 0) {
    throw 'No plugin binding configs were found.'
}

foreach ($configPath in $configPaths) {
    $json = Get-Content -Raw -Encoding UTF8 $configPath | ConvertFrom-Json
    $plugin = $json.plugins | Where-Object { $_.dll -eq 'plugin_map_quality_check_tools.dll' } | Select-Object -First 1
    if (-not $plugin) {
        throw "Quality plugin is missing from config: $configPath"
    }

    $qualityBindings = @($plugin.bindings | Where-Object {
        $_.menuPath.Count -gt 0 -and $_.menuPath[0] -eq $qualityModuleTitle
    })

    $actualFeatures = @($qualityBindings | ForEach-Object { $_.featureName })
    if ($actualFeatures.Count -ne $expectedFeatures.Count) {
        throw "Quality feature count mismatch in ${configPath}. Expected $($expectedFeatures.Count), got $($actualFeatures.Count): $($actualFeatures -join ', ')"
    }

    for ($i = 0; $i -lt $expectedFeatures.Count; $i++) {
        if ($actualFeatures[$i] -ne $expectedFeatures[$i]) {
            throw "Quality feature order mismatch in ${configPath} at position $($i + 1). Expected '$($expectedFeatures[$i])', got '$($actualFeatures[$i])'."
        }

        if ($qualityBindings[$i].fallbackActionIndex -ne $i) {
            throw "Quality feature fallback index mismatch in ${configPath} at position $($i + 1). Expected $i, got $($qualityBindings[$i].fallbackActionIndex)."
        }

        $expectedExtension = if ($configPath -like '*\dist\LTZK\*') { 'png' } else { 'svg' }
        $expectedIcon = "Resources/$($expectedIconNames[$i]).$expectedExtension"
        if ($qualityBindings[$i].icon -ne $expectedIcon) {
            throw "Quality feature icon mismatch in ${configPath} at position $($i + 1). Expected '$expectedIcon', got '$($qualityBindings[$i].icon)'."
        }
    }

    foreach ($excludedFeature in $excludedFeatures) {
        if ($actualFeatures -contains $excludedFeature) {
            throw "Excluded quality feature is still configured in ${configPath}: $excludedFeature"
        }
    }
}

Write-Host 'Quality module binding order verified.'

