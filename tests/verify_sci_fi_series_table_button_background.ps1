$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$qssPath = Join-Path $root 'Resources\styles\ltzk_sci_fi.qss'
$sourcePath = Join-Path $root 'LTZK.cpp'

if (-not (Test-Path -LiteralPath $qssPath)) { throw "Missing required file: $qssPath" }
if (-not (Test-Path -LiteralPath $sourcePath)) { throw "Missing required file: $sourcePath" }

$qss = Get-Content -Raw -Encoding UTF8 $qssPath
$source = Get-Content -Raw -Encoding UTF8 $sourcePath

$tableButtonRule = 'QTableView QPushButton, QTableWidget QPushButton, QTreeView QPushButton, QTreeWidget QPushButton'
$ruleStart = $qss.IndexOf($tableButtonRule)
$ruleEnd = $qss.IndexOf('}', $ruleStart)
if ($ruleStart -lt 0 -or $ruleEnd -lt 0) {
    throw 'Could not locate the sci-fi embedded table button rule.'
}
$rule = $qss.Substring($ruleStart, $ruleEnd - $ruleStart + 1)

foreach ($required in @(
    'color: #f8feff;',
    'background: #06111d;',
    'border-left: 1px solid rgba(91, 206, 255, 95);'
)) {
    if (-not $rule.Contains($required)) {
        throw "Sci-fi embedded table buttons must use an opaque themed background: $required"
    }
}
if ($rule.Contains('background: transparent;')) {
    throw 'Sci-fi embedded table buttons must not expose the native white parent palette.'
}

$polishStart = $source.IndexOf('void LTZK::polishSeriesContentTableWindow(QWidget* widget)')
$polishEnd = $source.IndexOf('void LTZK::applyPlatformStyleToTopLevelWidgets()', $polishStart)
if ($polishStart -lt 0 -or $polishEnd -lt 0) {
    throw 'Could not locate series content table polishing implementation.'
}
$polishBody = $source.Substring($polishStart, $polishEnd - $polishStart)

foreach ($required in @(
    'const QString buttonTextColor = m_currentTheme == PlatformTheme::White',
    'const QString buttonBackgroundColor = m_currentTheme == PlatformTheme::White',
    'QStringLiteral("#f8feff")',
    'QStringLiteral("#06111d")',
    'QStringLiteral("#082644")',
    'color: %1;',
    'background: %2;'
)) {
    if (-not $polishBody.Contains($required)) {
        throw "Runtime series-table polishing must preserve the opaque sci-fi button style: $required"
    }
}

Write-Host 'Sci-fi series table embedded buttons use an opaque themed background.'