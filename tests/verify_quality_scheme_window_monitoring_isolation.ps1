$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$headerPath = Join-Path $root 'PluginBindingConfig.h'
$parserPath = Join-Path $root 'PluginBindingConfig.cpp'

$source = [System.IO.File]::ReadAllText($sourcePath, [System.Text.Encoding]::UTF8)
$header = [System.IO.File]::ReadAllText($headerPath, [System.Text.Encoding]::UTF8)
$parser = [System.IO.File]::ReadAllText($parserPath, [System.Text.Encoding]::UTF8)

$featureStart = $header.IndexOf('struct FeatureBinding')
$featureEnd = $header.IndexOf('};', $featureStart)
if ($featureStart -lt 0 -or $featureEnd -lt 0) {
    throw 'Could not locate FeatureBinding.'
}
$featureBody = $header.Substring($featureStart, $featureEnd - $featureStart)
if (-not $featureBody.Contains('bool watchResultOutputs = true;')) {
    throw 'FeatureBinding must default result monitoring to enabled for backward compatibility.'
}

if (-not $parser.Contains('binding.watchResultOutputs = object.value(QStringLiteral("watchResultOutputs")).toBool(true);')) {
    throw 'Feature-level result-monitoring configuration is not parsed.'
}

$executeStart = $source.IndexOf('void LTZK::executePluginFeatureAction')
$executeEnd = $source.IndexOf('void LTZK::bindPluginFeatureActions', $executeStart)
if ($executeStart -lt 0 -or $executeEnd -lt 0) {
    throw 'Could not locate executePluginFeatureAction.'
}
$executeBody = $source.Substring($executeStart, $executeEnd - $executeStart)
if (-not $executeBody.Contains('const bool watchesResultOutputs = plugin.watchResultOutputs && binding.watchResultOutputs;')) {
    throw 'Result monitoring must require both the plugin capability and feature intent.'
}

$watcherStart = $source.IndexOf('void LTZK::updateQualityResultOutputWatcher()')
$watcherEnd = $source.IndexOf('void LTZK::handleQualityResultDirectoryChanged', $watcherStart)
if ($watcherStart -lt 0 -or $watcherEnd -lt 0) {
    throw 'Could not locate updateQualityResultOutputWatcher.'
}
$watcherBody = $source.Substring($watcherStart, $watcherEnd - $watcherStart)
if ($watcherBody.Contains('QDirIterator::Subdirectories')) {
    throw 'Opening a feature window must not recursively register every output subdirectory on the GUI thread.'
}
foreach ($required in @(
    'appendUniqueDir(directoriesToWatch, selectedOutputDir);',
    'm_qualityResultOutputWatcher->addPath(selectedOutputDir)'
)) {
    if (-not $watcherBody.Contains($required)) {
        throw "Selected result root monitoring is missing: $required"
    }
}

foreach ($required in @(
    'windowTitle.contains(QStringLiteral("质检方案配置"))',
    'int titleLeft = groupBox->rect().left();',
    'titleOverlay->setGeometry(titleLeft, 0, overlayWidth, 44);'
)) {
    if (-not $source.Contains($required)) {
        throw "Quality-scheme title alignment lifecycle is missing: $required"
    }
}

$configPaths = @(
    (Join-Path $root 'config\plugin_bindings.json'),
    (Join-Path $root 'LTZK_Developer_Package\01_Source\config\plugin_bindings.json'),
    (Join-Path $root 'package_build\out\config\plugin_bindings.json'),
    (Join-Path $root 'x64\Release\config\plugin_bindings.json'),
    (Join-Path $root 'x64\Debug\config\plugin_bindings.json'),
    (Join-Path $root 'x64\Release\verify-build\config\plugin_bindings.json'),
    'E:\LTZK\config\plugin_bindings.json'
)

foreach ($configPath in $configPaths) {
    if (-not (Test-Path -LiteralPath $configPath)) {
        throw "Missing plugin binding configuration: $configPath"
    }

    $config = [System.IO.File]::ReadAllText($configPath, [System.Text.Encoding]::UTF8)
    $featureMatch = [regex]::Match(
        $config,
        '"featureName"\s*:\s*"质检方案配置"(?<body>[\s\S]*?)(?="featureName"\s*:|\]\s*\}\s*\]\s*\})')
    if (-not $featureMatch.Success) {
        throw "Could not locate quality scheme feature in $configPath"
    }
    if ($featureMatch.Groups['body'].Value -notmatch '"watchResultOutputs"\s*:\s*false') {
        throw "Quality scheme feature must disable result monitoring in $configPath"
    }
}

Write-Host 'Quality scheme monitoring isolation, root-only watcher, and title alignment lifecycle verified.'
