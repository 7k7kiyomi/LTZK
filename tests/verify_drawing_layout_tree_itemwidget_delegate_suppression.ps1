$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path $source)) { throw "Missing required file: $source" }

$text = Get-Content -Raw -Encoding UTF8 $source

$helperStart = $text.IndexOf('void suppressDrawingLayoutTreeEmbeddedItemText(QTreeWidget* tree)')
$helperEnd = $text.IndexOf('void polishDrawingLayoutTreeEmbeddedCombos(LTZK* owner, QWidget* widget, bool isDarkTheme, bool isBlackTheme, QComboBox* onlyCombo = nullptr)', $helperStart)
if ($helperStart -lt 0 -or $helperEnd -lt 0) {
    throw 'Could not locate suppressDrawingLayoutTreeEmbeddedItemText helper body.'
}

$helperBody = $text.Substring($helperStart, $helperEnd - $helperStart)
foreach ($required in @(
    'treeModelIndexForItem(tree, item, column)',
    'suppressPluginEmbeddedControlItemText(tree, index)',
    'tree->viewport()->update();'
)) {
    if (-not $helperBody.Contains($required)) {
        throw "treeWidget_csv itemWidget cells must suppress backing item text through the delegate, not only through foreground color: $required"
    }
}

Write-Host 'Drawing layout tree itemWidget delegate suppression verified.'
