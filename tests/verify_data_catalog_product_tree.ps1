$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$widgetHeader = Join-Path $root 'DataCatalogWidget.h'
$widgetSource = Join-Path $root 'DataCatalogWidget.cpp'
$serviceHeader = Join-Path $root 'DataCatalogProductCatalog.h'
$serviceSource = Join-Path $root 'DataCatalogProductCatalog.cpp'
$ltzkSource = Join-Path $root 'LTZK.cpp'
$pluginHeader = Join-Path $root 'map_product_manager_tools\map_product_manager_tools.h'
$pluginSource = Join-Path $root 'map_product_manager_tools\map_product_manager_tools.cpp'
$pluginDialogHeader = Join-Path $root 'map_product_manager_tools\ui_class\product_storage_dialog.h'
$pluginDialogSource = Join-Path $root 'map_product_manager_tools\ui_class\product_storage_dialog.cpp'
$bindingConfig = Join-Path $root 'config\plugin_bindings.json'
$projectFile = Join-Path $root 'LTZK.vcxproj'
$filterFile = Join-Path $root 'LTZK.vcxproj.filters'

foreach ($path in @(
    $widgetHeader,
    $widgetSource,
    $serviceHeader,
    $serviceSource,
    $ltzkSource,
    $pluginHeader,
    $pluginSource,
    $pluginDialogHeader,
    $pluginDialogSource,
    $bindingConfig,
    $projectFile,
    $filterFile
)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing database product catalog component: $path"
    }
}

$headerText = Get-Content -Raw -Encoding UTF8 $widgetHeader
$sourceText = Get-Content -Raw -Encoding UTF8 $widgetSource
$serviceHeaderText = Get-Content -Raw -Encoding UTF8 $serviceHeader
$serviceText = Get-Content -Raw -Encoding UTF8 $serviceSource
$ltzkText = Get-Content -Raw -Encoding Default $ltzkSource
$pluginHeaderText = Get-Content -Raw -Encoding UTF8 $pluginHeader
$pluginText = Get-Content -Raw -Encoding Default $pluginSource
$pluginDialogHeaderText = Get-Content -Raw -Encoding UTF8 $pluginDialogHeader
$pluginDialogText = Get-Content -Raw -Encoding Default $pluginDialogSource
$menuSourcesText = "$sourceText`n$pluginDialogText"
$newMenuLabelCount = [regex]::Matches(
    $menuSourcesText,
    'addAction\(QStringLiteral\("按制图导出"\)\)'
).Count
if ($newMenuLabelCount -ne 4) {
    throw "Expected four right-click menu labels named 按制图导出, found: $newMenuLabelCount"
}

$oldMenuLabelCount = [regex]::Matches(
    $menuSourcesText,
    'addAction\(QStringLiteral\("按主区裁切导出"\)\)'
).Count
if ($oldMenuLabelCount -ne 0) {
    throw "Legacy right-click menu label remains in $oldMenuLabelCount definition(s)."
}

if (-not $menuSourcesText.Contains('QStringLiteral("main-area")')) {
    throw 'Main-area export operation identifier must remain unchanged.'
}
$bindingText = Get-Content -Raw -Encoding UTF8 $bindingConfig
$projectText = Get-Content -Raw -Encoding UTF8 $projectFile
$filterText = Get-Content -Raw -Encoding UTF8 $filterFile

foreach ($required in @(
    'ProductDirectoryIdRole',
    'ProductIdRole',
    'ProductNodeKindRole',
    'ProductTypeRole',
    'ProductLayerTableRole',
    'ProductFileOidRole',
    'ProductCatalogNodeKind',
    'using DataImportCallback = std::function<void(int)>;'
)) {
    if (-not $headerText.Contains($required)) {
        throw "Data catalog product-node contract is missing: $required"
    }
}

foreach ($required in @(
    'product_directory',
    'product_metadata',
    'hasProductCatalogSchema',
    'loadProductDirectoryTree',
    'sort_order, name',
    '制图成果',
    '制图要素',
    '制图资料',
    '导出所选数据',
    '按范围导出',
    '按制图导出',
    '添加到主题',
    'createProductDirectory',
    'renameProductDirectory',
    'deleteProductDirectory'
)) {
    if (-not $sourceText.Contains($required) -and -not $serviceText.Contains($required)) {
        throw "Product directory behavior is missing: $required"
    }
}

$productTreeLoadStart = $sourceText.IndexOf('if (productCatalog.hasProductCatalogSchema(&productCatalogError))')
$legacyTreeStart = $sourceText.IndexOf('// A database without the成果目录 schema', $productTreeLoadStart)
if ($productTreeLoadStart -lt 0 -or $legacyTreeStart -le $productTreeLoadStart) {
    throw 'Database product tree load boundary was not found.'
}
$productTreeLoadText = $sourceText.Substring(
    $productTreeLoadStart,
    $legacyTreeStart - $productTreeLoadStart)
if (-not $productTreeLoadText.Contains('m_databaseTree->collapseAll();') -or
    -not $productTreeLoadText.Contains('m_databaseTree->expand(databaseIndex);') -or
    $productTreeLoadText.Contains('m_databaseTree->expandAll();')) {
    throw 'Database product tree must expand only the database root by default.'
}

foreach ($required in @(
    'directory.nodeType == 1',
    'directory.nodeType != 1'
)) {
    if (-not $sourceText.Contains($required) -and -not $serviceHeaderText.Contains($required)) {
        throw "Database product tree must reuse the original layer-directory rule: $required"
    }
}

foreach ($required in @(
    'class DataCatalogProductCatalog',
    'bool hasProductCatalogSchema',
    'QList<ProductDirectoryRecord> childDirectories',
    'QList<ProductRecord> productsForDirectory',
    'bool createDirectory',
    'bool renameDirectory',
    'bool deleteDirectory'
)) {
    if (-not $serviceHeaderText.Contains($required)) {
        throw "Product catalog adapter contract is missing: $required"
    }
}

foreach ($required in @(
    'QSqlDatabase',
    'QSqlQuery',
    'information_schema.tables',
    'product_directory',
    'product_metadata',
    'lo_get',
    'lo_unlink',
    'ORDER BY sort_order, name'
)) {
    if (-not $serviceText.Contains($required)) {
        throw "Product catalog adapter implementation is missing: $required"
    }
}

foreach ($required in @(
    'm_dataImportCallback(targetDirectoryId)',
    'DataImport',
    'ltzkProductImportTargetDirId',
    'setTargetParentDirId'
)) {
    if (-not $ltzkText.Contains($required) -and
        -not $pluginText.Contains($required) -and
        -not $sourceText.Contains($required)) {
        throw "Product import bridge is missing: $required"
    }
}

foreach ($required in @(
    'void DataCatalogOperation();',
    'syncExternalCatalogConnection',
    'ltzkProductCatalogRequest',
    'ltzkProductImportTargetDirId',
    'executeExternalDirectoryOperation',
    'create-directory',
    'delete-directory',
    'rename-directory',
    'DataCatalogOperation'
)) {
    if (-not $pluginText.Contains($required) -and
        -not $pluginHeaderText.Contains($required) -and
        -not $pluginDialogText.Contains($required)) {
        throw "Product right-click bridge is missing: $required"
    }
}

foreach ($required in @(
    '成果目录操作',
    '"showInMenu": false'
)) {
    if (-not $bindingText.Contains($required)) {
        throw "Hidden product catalog action binding is missing: $required"
    }
}

foreach ($required in @(
    'DataCatalogProductCatalog.cpp',
    'DataCatalogProductCatalog.h'
)) {
    if (-not $projectText.Contains($required) -or -not $filterText.Contains($required)) {
        throw "Visual Studio project entry missing: $required"
    }
}

$createStart = $sourceText.IndexOf('void DataCatalogWidget::createProductDirectory(')
$deleteStart = $sourceText.IndexOf('void DataCatalogWidget::deleteProductDirectory(', $createStart)
$renameStart = $sourceText.IndexOf('void DataCatalogWidget::renameProductDirectory(', $deleteStart)
if ($createStart -lt 0 -or $deleteStart -le $createStart -or $renameStart -le $deleteStart) {
    throw 'Product directory dialog boundaries were not found.'
}
$createDialogText = $sourceText.Substring($createStart, $deleteStart - $createStart)
$deleteDialogText = $sourceText.Substring($deleteStart, $renameStart - $deleteStart)
$deleteProductStart = $serviceText.IndexOf('bool DataCatalogProductCatalog::deleteProductInTransaction(')
$deleteProductEnd = $serviceText.IndexOf('bool DataCatalogProductCatalog::deleteProduct(', $deleteProductStart)
if ($deleteProductStart -lt 0 -or $deleteProductEnd -le $deleteProductStart) {
    throw 'Product deletion SQL boundary was not found.'
}
$deleteProductText = $serviceText.Substring($deleteProductStart, $deleteProductEnd - $deleteProductStart)
if ($deleteProductText.Contains('dropQuery.prepare(') -or
    -not $deleteProductText.Contains('dropQuery.exec(QStringLiteral("DROP TABLE IF EXISTS %1 CASCADE").arg(quotedTable))')) {
    throw 'Product layer DROP TABLE must execute directly; PostgreSQL cannot prepare DDL.'
}
$menuStart = $sourceText.IndexOf('void DataCatalogWidget::populateProductContextMenu(')
$menuEnd = $sourceText.IndexOf('void DataCatalogWidget::populateContextMenu(', $menuStart)
if ($menuStart -lt 0 -or $menuEnd -le $menuStart) {
    throw 'Product context-menu boundary was not found.'
}
$menuText = $sourceText.Substring($menuStart, $menuEnd - $menuStart)
if ($menuText.Contains('勾选本节点下全部成果') -or
    $menuText.Contains('取消勾选本节点下全部成果')) {
    throw 'Database and local product context menus must hide bulk-check actions.'
}
foreach ($required in @(
    '[this, context]() { createProductDirectory(context); }',
    '[this, context]() { deleteProductDirectory(context); }'
)) {
    if (-not $menuText.Contains($required)) {
        throw "Product context menu must invoke the platform directory dialog: $required"
    }
}
foreach ($required in @(
    'setOkButtonText(QStringLiteral("确定"))',
    'setCancelButtonText(QStringLiteral("取消"))'
)) {
    if (-not $createDialogText.Contains($required)) {
        throw "New-directory dialog must use Chinese button text: $required"
    }
}
foreach ($required in @(
    'button(QMessageBox::Yes)->setText(QStringLiteral("是"))',
    'button(QMessageBox::No)->setText(QStringLiteral("否"))',
    'QMessageBox::Yes | QMessageBox::No',
    'setDefaultButton(QMessageBox::No)'
)) {
    if (-not $deleteDialogText.Contains($required)) {
        throw "Delete-directory dialog must keep its choices and use Chinese text: $required"
    }
}

Write-Output 'Data catalog product directory verification passed.'
