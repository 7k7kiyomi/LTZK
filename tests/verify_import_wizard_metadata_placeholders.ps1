$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$platform = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'LTZK.cpp')
$plugin = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'map_product_manager_tools\ui_class\data_import_wizard.cpp')

$start = $platform.IndexOf('void LTZK::applyPlatformStyleToPluginWindow(QWidget* widget)')
$end = $platform.IndexOf('void LTZK::scheduleQualityWindowTableColumnWidthNormalization', $start)
if ($start -lt 0 -or $end -lt 0) { throw 'Platform wizard styling function not found' }
$styling = $platform.Substring($start, $end - $start)

foreach ($required in @(
    'QStringLiteral("基础元数据")',
    'QStringLiteral("专有元数据")',
    'QFormLayout::FieldRole',
    'QLineEdit::textChanged',
    'ltzkMetadataPlaceholder',
    'style()->unpolish(',
    'style()->polish('
)) {
    if (-not $styling.Contains($required)) { throw "Metadata placeholder styling missing: $required" }
}
if ($styling.Contains('QPalette::PlaceholderText')) {
    throw 'Metadata placeholder color still depends on the ineffective palette role'
}
if ($plugin.Contains('autoMetadataField') -or $plugin.Contains('autoBasicKeys')) {
    throw 'Plugin source must not carry platform placeholder styling changes'
}
foreach ($theme in @('white', 'black', 'sci_fi', 'field_notes', 'steel_warm_white')) {
    $qss = Get-Content -Raw -Encoding UTF8 (Join-Path $root "Resources\styles\ltzk_$theme.qss")
    if (-not $qss.Contains('QLineEdit[ltzkMetadataPlaceholder="true"]')) {
        throw "Theme has no metadata placeholder selector: $theme"
    }
}
Write-Host 'Import wizard metadata placeholder platform contract verified.'
