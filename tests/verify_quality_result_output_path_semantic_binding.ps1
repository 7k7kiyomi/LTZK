$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$source = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8

foreach ($required in @(
    'bool isQualityResultOutputLabel(const QString& labelText)',
    'bool isQualityResultOutputPathControl(QLineEdit* lineEdit, QWidget* pluginWindow)',
    '结果输出目录',
    '输出结果路径',
    'label->buddy() == lineEdit',
    'lineEdit->mapToGlobal',
    'isQualityResultOutputPathControl(lineEdit, monitoredWindow)'
)) {
    if (-not $source.Contains($required)) {
        throw "Semantic result output-path binding is missing: $required"
    }
}

$refreshStart = $source.IndexOf('void LTZK::refreshQualityResultOutputDirectories()')
$refreshEnd = $source.IndexOf('QStringList LTZK::collectQualityResultSearchRoots() const', $refreshStart)
if ($refreshStart -lt 0 -or $refreshEnd -lt 0) {
    throw 'Could not locate the quality result output-directory refresh function.'
}

$refreshBody = $source.Substring($refreshStart, $refreshEnd - $refreshStart)
if ($refreshBody -match 'if\s*\(\s*!controlName\.contains\(QStringLiteral\("output"\)\)\s*\)') {
    throw 'Generic lineEdit output paths must not be rejected by object name alone.'
}
if ($refreshBody -notmatch 'if\s*\(\s*!isQualityResultOutputPathControl\(lineEdit,\s*monitoredWindow\)\s*\)') {
    throw 'The live refresh loop must use semantic output-path control detection.'
}

Write-Host 'Quality result output path is bound by output semantics, including generic lineEdit controls.'
