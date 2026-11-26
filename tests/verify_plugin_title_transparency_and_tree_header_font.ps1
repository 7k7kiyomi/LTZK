$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$themeFiles = @(
    'Resources\styles\ltzk_sci_fi.qss',
    'Resources\styles\ltzk_black.qss',
    'Resources\styles\ltzk_white.qss'
)

$source = Get-Content $sourcePath -Raw -Encoding UTF8

foreach ($required in @(
    'int titleLeft = groupBox->rect().left();'
    'titleOverlay->setGeometry(titleLeft, 0, overlayWidth, 44);'
)) {
    if (-not $source.Contains($required)) {
        throw "Plugin title implementation is missing: $required"
    }
}

if ($source.Contains('titleOverlay->setGeometry(18, 0, overlayWidth, 44)')) {
    throw 'Plugin title overlay must not use the fixed x position 18.'
}

if ($source.Contains('widestItemWidth') -or $source.Contains('itemRect.left()') -or $source.Contains('groupBox->contentsRect().left()')) {
    throw 'Plugin section title alignment must use the group box outer edge, not a content or child widget edge.'
}
foreach ($themeFile in $themeFiles) {
    $themePath = Join-Path $root $themeFile
    $qss = Get-Content $themePath -Raw -Encoding UTF8
    $titleMatch = [regex]::Match($qss, 'QGroupBox::title\s*\{(?<body>[^}]*)\}')
    if (-not $titleMatch.Success -or $titleMatch.Groups['body'].Value -notmatch 'background:\s*transparent') {
        throw "Theme $themeFile must make QGroupBox titles transparent."
    }

    if ($qss -notmatch 'QTableView\s+QHeaderView::section,\s*QTableWidget\s+QHeaderView::section,\s*QTreeWidget\s+QHeaderView::section\s*\{[^}]*font-size:\s*16px') {
        throw "Theme $themeFile is missing the unified 16px table/tree header font size."
    }
}

Write-Host 'Plugin section titles and tree headers are scoped for transparent styling and readable alignment.'
