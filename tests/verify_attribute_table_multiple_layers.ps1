$ErrorActionPreference = 'Stop'

$source = Get-Content -Raw -Encoding Default 'LTZK.cpp'

$start = $source.IndexOf('void LTZK::on_actionOpenAttrTable_triggered()')
$end = $source.IndexOf('void LTZK::on_actionOpenFields_triggered()', $start + 1)

if ($start -lt 0 -or $end -lt 0 -or $end -le $start) {
    throw 'Cannot find on_actionOpenAttrTable_triggered method.'
}

$body = $source.Substring($start, $end - $start)

if ($body -match 'static\s+QgsAttributeTableView\s*\*\s*tv') {
    throw 'Attribute table still uses one static view, which blocks opening tables for additional layers.'
}

if ($body -match 'activateWindow\(\);\s*return;') {
    throw 'Attribute table still returns early when any previous table view exists.'
}

if ($body -notmatch 'new\s+QgsAttributeTableView\s*\(\s*\)') {
    throw 'Attribute table should create a view for the current selected vector layer.'
}

if ($body -notmatch 'setAttribute\s*\(\s*Qt::WA_DeleteOnClose\s*\)') {
    throw 'Attribute table view should delete itself on close to avoid leaking multiple layer windows.'
}

if ($body -notmatch 'QgsVectorLayerCache\s*\*\s*lc\s*=\s*new\s+QgsVectorLayerCache\s*\(\s*layer\s*,') {
    throw 'Attribute table cache must be created from the current selected vector layer.'
}

if ($body -notmatch 'tm->loadLayer\s*\(\s*\)') {
    throw 'Attribute table model must load the current layer features.'
}

Write-Host 'Attribute tables can be opened independently for multiple layers.'
