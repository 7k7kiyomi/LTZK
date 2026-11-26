$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$headerPath = Join-Path $root 'LTZK.h'

foreach ($path in @($sourcePath, $headerPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing required file: $path"
    }
}

$source = Get-Content -Raw -Encoding UTF8 $sourcePath
$header = Get-Content -Raw -Encoding UTF8 $headerPath
$combined = $source + [Environment]::NewLine + $header

$predicateStart = $source.IndexOf('bool hasWorkspaceTopBarThemeBackground(const QString& moduleTitle)')
$paletteStart = $source.IndexOf('struct WorkspaceTopBarBackgroundPalette', $predicateStart)
$gradientClassStart = $source.IndexOf('class WorkspaceTopBarGradientScrollArea')
if ($predicateStart -lt 0 -or $paletteStart -le $predicateStart -or $gradientClassStart -le $paletteStart) {
    throw 'Theme-specific top-bar background palette or gradient renderer is missing.'
}
$predicate = $source.Substring($predicateStart, $paletteStart - $predicateStart)
foreach ($required in @(
    'QStringLiteral("\u5730\u56FE\u6210\u679C\u7BA1\u7406")',
    'QStringLiteral("\u5730\u56FE\u667A\u80FD\u5236\u56FE")',
    'QStringLiteral("\u5730\u56FE\u6210\u679C\u8D28\u68C0")',
    'QStringLiteral("\u5730\u56FE\u670D\u52A1\u7BA1\u7406")'
)) {
    if (-not $predicate.Contains($required)) {
        throw "The previous top-bar background module scope changed unexpectedly: $required"
    }
}
$palette = $source.Substring($paletteStart, $gradientClassStart - $paletteStart)

foreach ($required in @(
    'QStringLiteral("white")',
    'QStringLiteral("black")',
    'QStringLiteral("sci-fi")',
    'QStringLiteral("field-notes")',
    'QStringLiteral("steel-warm-white")',
    'QColor(QStringLiteral("#FFFFFF"))',
    'QColor(QStringLiteral("#101010"))',
    'QColor(QStringLiteral("#07598C"))',
    'QColor(QStringLiteral("#3D5540"))',
    'QColor(QStringLiteral("#34464B"))'
)) {
    if (-not $palette.Contains($required)) {
        throw "Theme-specific background palette is missing: $required"
    }
}

$gradientClassEnd = $source.IndexOf('class LTZKLayerTreeModel', $gradientClassStart)
if ($gradientClassEnd -le $gradientClassStart) {
    throw 'The themed top-bar scroll area implementation is incomplete.'
}
$gradientClass = $source.Substring(
    $gradientClassStart,
    $gradientClassEnd - $gradientClassStart)

foreach ($required in @(
    'void paintEvent(QPaintEvent* event) override',
    'QScrollArea::paintEvent(event);',
    'viewport()->property("topBarThemeBackgroundActive").toBool()',
    'QPainter painter(viewport())',
    'viewport()->property("topBarThemeKey").toString()',
    'const WorkspaceTopBarBackgroundPalette palette = workspaceTopBarBackgroundPalette(themeKey);',
    'QLinearGradient backgroundGradient(',
    'painter.fillRect(viewport()->rect(), backgroundGradient);'
)) {
    if (-not $gradientClass.Contains($required)) {
        throw "The themed top-bar background gradient requirement is missing: $required"
    }
}

foreach ($forbidden in @(
    'contour',
    'Contour',
    'QPainterPath',
    'drawPath',
    'TerrainContours',
    'contourRegionAfterActionButtons'
)) {
    if ($gradientClass.Contains($forbidden)) {
        throw "Top-bar contour drawing remains in the scroll area: $forbidden"
    }
}

foreach ($forbidden in @(
    'WorkspaceTopBarContour',
    'contourWatermark',
    'contourThemeKey',
    'updateWorkspaceTopBarContourWatermark',
    'buildWorkspaceTopBarTerrainContours'
)) {
    if ($combined.Contains($forbidden)) {
        throw "Retired contour-rendering code remains active in source/header: $forbidden"
    }
}

foreach ($required in @(
    'WorkspaceTopBarGradientScrollArea* pluginScrollArea =',
    'new WorkspaceTopBarGradientScrollArea(topActionStrip)',
    'pluginScrollArea->viewport()->setAutoFillBackground(false)',
    'void LTZK::updateWorkspaceTopBarThemeBackground(const QString& moduleTitle)',
    'const bool active = hasWorkspaceTopBarThemeBackground(moduleTitle)',
    'scrollArea->viewport()->setProperty("topBarThemeBackgroundActive", active)',
    'scrollArea->viewport()->setProperty("topBarThemeKey", platformThemeKey(m_currentTheme))',
    'scrollArea->viewport()->update()',
    'updateWorkspaceTopBarThemeBackground(moduleTitle)',
    'updateWorkspaceTopBarThemeBackground(m_currentBusinessModuleTitle)'
)) {
    if (-not $source.Contains($required)) {
        throw "Top-bar gradient scroll-area setup is missing: $required"
    }
}
if (-not $header.Contains('void updateWorkspaceTopBarThemeBackground(const QString& moduleTitle);')) {
    throw 'The active theme background refresh must be declared in LTZK.h.'
}

Write-Host 'All five top-bar contours are removed while theme-specific gradients remain.'
