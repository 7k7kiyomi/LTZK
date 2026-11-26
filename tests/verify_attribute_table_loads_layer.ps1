$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root "LTZK.cpp"

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$sourceText = Get-Content $source -Raw -Encoding Default

if ($sourceText -match "//if \(!tm->loadLayer\(\)\)") {
    throw "Attribute table layer loading is still commented out"
}

if ($sourceText -notmatch "tm->loadLayer\(\);") {
    throw "Attribute table model does not load layer features"
}

if ($sourceText -notmatch "QgsVectorLayerCache\* lc = new QgsVectorLayerCache\(layer, qMax\(layer->featureCount\(\), 1LL\), tv\);") {
    throw "Attribute table cache is not parented to the table view with enough room for layer features"
}

if ($sourceText -notmatch "QgsAttributeTableModel\* tm = new QgsAttributeTableModel\(lc, tv\);") {
    throw "Attribute table model is not parented to the table view"
}

if ($sourceText -notmatch "QgsAttributeTableFilterModel\* tfm = new QgsAttributeTableFilterModel\(m_mapCanvas, tm, tv\);") {
    throw "Attribute table filter model is not parented to the table view"
}

Write-Host "Attribute table model loads layer features."
