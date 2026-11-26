$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'LTZK.vcxproj'
if (-not (Test-Path -LiteralPath $project)) {
    throw "Missing project file: $project"
}

$projectText = Get-Content -Raw -Encoding UTF8 $project

foreach ($configuration in @('Debug', 'Release')) {
    $groupPattern = '(?s)<ItemDefinitionGroup Condition="[^"]*' + $configuration + '\|x64[^"]*" Label="Configuration".*?</ItemDefinitionGroup>'
    $matches = [regex]::Matches($projectText, $groupPattern)
    if ($matches.Count -eq 0) {
        throw "Could not find $configuration|x64 compiler configuration."
    }

    $groupText = ($matches | ForEach-Object { $_.Value }) -join "`n"
    if (-not $groupText.Contains('<MultiProcessorCompilation>false</MultiProcessorCompilation>')) {
        throw "$configuration|x64 must disable /MP so multiple CL processes cannot write the same compiler PDB."
    }
    if (-not $groupText.Contains('<ForceSynchronousPDBWrites>true</ForceSynchronousPDBWrites>')) {
        throw "$configuration|x64 must enable ForceSynchronousPDBWrites so Qt/MSBuild emits /FS for every CL.EXE invocation."
    }
    if (-not $groupText.Contains('<AdditionalOptions>/FS')) {
        throw "$configuration|x64 final compiler configuration must pass /FS explicitly to every CL.EXE invocation."
    }
    if ($configuration -eq 'Release' -and -not $groupText.Contains('<DebugInformationFormat>None</DebugInformationFormat>')) {
        throw 'Release|x64 must disable compiler debug information so release CL.EXE invocations do not contend for vc142.pdb.'
    }
}

foreach ($configuration in @('Debug', 'Release')) {
    $propertyPattern = '<PropertyGroup Condition="[^\"]*' + $configuration + '\|x64[^\"]*"[^>]*>.*?</PropertyGroup>'
    $propertyMatches = [regex]::Matches($projectText, $propertyPattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)
    $propertyText = ($propertyMatches | ForEach-Object { $_.Value }) -join "`n"
    if (-not $propertyText.Contains('<UseMultiToolTask>false</UseMultiToolTask>')) {
        throw "$configuration|x64 must disable MSBuild's multi-tool compiler task so CL.EXE cannot race on vc142.pdb."
    }
}

Write-Host 'PDB compilation serialization settings verified.'
