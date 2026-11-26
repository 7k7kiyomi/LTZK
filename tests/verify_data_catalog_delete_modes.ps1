$root = Split-Path -Parent $PSScriptRoot
$widgetHeader = Join-Path $root 'DataCatalogWidget.h'
$widgetSource = Join-Path $root 'DataCatalogWidget.cpp'
$catalogHeader = Join-Path $root 'DataCatalogProductCatalog.h'
$catalogSource = Join-Path $root 'DataCatalogProductCatalog.cpp'

foreach ($path in @($widgetHeader, $widgetSource, $catalogHeader, $catalogSource)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing source: $path"
    }
}

$header = Get-Content -Raw -Encoding UTF8 $widgetHeader
$source = Get-Content -Raw -Encoding UTF8 $widgetSource
$serviceHeader = Get-Content -Raw -Encoding UTF8 $catalogHeader
$serviceSource = Get-Content -Raw -Encoding UTF8 $catalogSource
$all = "$header`n$source`n$serviceHeader`n$serviceSource"

foreach ($required in @(
    '删除数据',
    '仅从平台列表移除（保留源数据）',
    '永久删除源数据',
    'QSettings',
    'filterAcceptsRow',
    'deleteProducts',
    'lo_unlink',
    'QFile::remove',
    'localImportCompanionFiles',
    'isSafeLocalDeleteTarget',
    'settings.status()',
    'QAction* deleteData = nullptr',
    'if (isDeletableDataContext(context))',
    'm_database.transaction()'
)) {
    if (-not $all.Contains($required)) {
        throw "Missing delete-mode contract: $required"
    }
}

if ($source -match 'addAction\(QStringLiteral\("隐藏"\)\)') {
    throw 'Standalone 隐藏 context-menu action must not exist.'
}
if (-not $source.Contains('m_removedDatabaseKeys') -or
    -not $source.Contains('m_removedLocalPaths')) {
    throw 'Persistent platform-removal state is not wired in the widget.'
}

Write-Output 'Data catalog delete-mode static contract passed.'
