$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding Default $source

foreach ($required in @(
    'void logLtzKMessage',
    'QgsMessageLog::logMessage(message, QStringLiteral("LTZK")',
    'logLtzKMessage(QStringLiteral("LTZK application initialized."))',
    'logLtzKMessage(QStringLiteral("Plugin loaded: %1").arg(pluginPath))',
    'logLtzKMessage(QStringLiteral("Plugin load failed: %1").arg(pluginError), Qgis::Warning)',
    'logLtzKMessage(QStringLiteral("Project opened: %1").arg(filename))',
    'logLtzKMessage(QStringLiteral("Project open failed: %1").arg(filename), Qgis::Warning)',
    'logLtzKMessage(QStringLiteral("Project saved: %1").arg(filename))',
    'logLtzKMessage(QStringLiteral("Project save failed: %1").arg(filename), Qgis::Warning)',
    'logLtzKMessage(QStringLiteral("Layer loaded: %1").arg(mapLayer->name()))',
    'logLtzKMessage(QStringLiteral("Layer load failed: %1").arg(fi.fileName()), Qgis::Warning)',
    'logLtzKMessage(QStringLiteral("Layers added: %1").arg(layerList.size()))',
    'logLtzKMessage(QStringLiteral("Opening %1 source selector.").arg(service))',
    'logLtzKMessage(QStringLiteral("Attribute table opened: %1").arg(layer->name()))',
    'logLtzKMessage(QStringLiteral("Field editor opened: %1").arg(layer->name()))',
    'logLtzKMessage(QStringLiteral("Coordinate transform output added: %1").arg(outputFile))'
)) {
    if (-not $text.Contains($required)) {
        throw "Missing LTZK business logging marker: $required"
    }
}

Write-Host 'LTZK business logging verified.'
