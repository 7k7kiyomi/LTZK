$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$userProject = Join-Path $root 'LTZK.vcxproj.user'

if (-not (Test-Path $userProject)) {
    throw "Missing required file: $userProject"
}

[xml]$xml = Get-Content -Raw -Encoding UTF8 $userProject
$ns = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
$ns.AddNamespace('msb', 'http://schemas.microsoft.com/developer/msbuild/2003')

foreach ($configuration in @('Debug|x64', 'Release|x64')) {
    $condition = "'`$(Configuration)|`$(Platform)'=='$configuration'"
    $group = $xml.SelectSingleNode("//msb:PropertyGroup[@Condition=""$condition""]", $ns)
    if ($null -eq $group) {
        throw "Missing VS debugger property group: $configuration"
    }

    $checks = @{
        LocalDebuggerCommand = '$(TargetPath)'
        LocalDebuggerWorkingDirectory = '$(OutDir)'
        LocalDebuggerMergeEnvironment = 'true'
        DebuggerFlavor = 'WindowsLocalDebugger'
    }

    foreach ($name in $checks.Keys) {
        $node = $group.SelectSingleNode("msb:$name", $ns)
        if ($null -eq $node -or $node.InnerText.Trim() -ne $checks[$name]) {
            throw "$configuration VS debugger setting mismatch: $name"
        }
    }

    $environment = $group.SelectSingleNode('msb:LocalDebuggerEnvironment', $ns)
    if ($null -eq $environment) {
        throw "$configuration is missing LocalDebuggerEnvironment"
    }

    foreach ($required in @(
        'PATH=E:\extern\QGIS_32815\bin;E:\extern\OSGeo4W_32815\bin;E:\extern\OSGeo4W_32815\apps\Qt5\bin;%PATH%',
        'QGIS_PREFIX_PATH=E:\extern\QGIS_32815',
        'QT_PLUGIN_PATH=E:\extern\OSGeo4W_32815\apps\Qt5\plugins',
        'QT_QPA_PLATFORM_PLUGIN_PATH=E:\extern\OSGeo4W_32815\apps\Qt5\plugins\platforms',
        'GDAL_DATA=E:\extern\OSGeo4W_32815\share\gdal',
        'PROJ_LIB=E:\extern\OSGeo4W_32815\share\proj',
        'PROJ_DATA=E:\extern\OSGeo4W_32815\share\proj'
    )) {
        if (-not $environment.InnerText.Contains($required)) {
            throw "$configuration VS debugger environment is missing: $required"
        }
    }

    if ($environment.InnerText.Contains('$(LocalDebuggerEnvironment)')) {
        throw "$configuration VS debugger environment should not recursively reference LocalDebuggerEnvironment."
    }
}

Write-Host 'VS Debug and Release debugger configurations are aligned.'
