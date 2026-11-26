$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'

foreach ($path in @($source, $header)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$sourceText = Get-Content -Raw -Encoding Default $source
$headerText = Get-Content -Raw -Encoding Default $header

foreach ($required in @(
    '#include "qgspallabeling.h"',
    '#include "qgsvectorlayerlabeling.h"',
    '#include "qgstextformat.h"',
    '#include "qgstextbuffersettings.h"',
    'QString localGansuLabelField(const QgsVectorLayer* layer)',
    'void applyLocalGansuBasemapLabels(QgsVectorLayer* layer, bool isWhiteTheme)',
    'localGansuLabelField(layer)',
    'QStringLiteral("\u4e61")',
    'QStringLiteral("\u53bf")',
    'QStringLiteral("\u5e02")',
    'QStringLiteral("\u7701")',
    'QStringLiteral("name")',
    'QStringLiteral("NAME")',
    'QgsPalLayerSettings labelSettings;',
    'labelSettings.fieldName = labelField;',
    'labelSettings.placement = Qgis::LabelPlacement::OverPoint;',
    'textFormat.setColor(isWhiteTheme ? QColor(15, 23, 42, 235) : QColor(255, 255, 255, 235));',
    'bufferSettings.setEnabled(true);',
    'bufferSettings.setColor(isWhiteTheme ? QColor(255, 255, 255, 225) : QColor(2, 10, 20, 220));',
    'layer->setLabeling(new QgsVectorLayerSimpleLabeling(labelSettings));',
    'layer->setLabelsEnabled(true);',
    'applyLocalGansuBasemapLabels(layer, isWhiteTheme);'
)) {
    if (-not $sourceText.Contains($required) -and -not $headerText.Contains($required)) {
        throw "Local Gansu SHP label support missing: $required"
    }
}

Write-Host 'Local Gansu SHP label support verified.'
