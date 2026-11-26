$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$headerPath = Join-Path $root 'LTZK.h'
$sourceText = Get-Content $sourcePath -Raw -Encoding UTF8
$headerText = Get-Content $headerPath -Raw -Encoding UTF8

$themeExpectations = @{
    'Resources\styles\ltzk_sci_fi.qss' = @(
        'border: 1px solid rgba(116, 238, 255, 150);',
        'border: 1px solid rgba(132, 236, 255, 140);',
        'border: 1px solid rgba(132, 236, 255, 105);'
    )
    'Resources\styles\ltzk_black.qss' = @(
        'border: 1px solid rgba(245, 158, 11, 135);',
        'border: 1px solid rgba(245, 158, 11, 120);',
        'border: 1px solid rgba(245, 158, 11, 95);'
    )
    'Resources\styles\ltzk_white.qss' = @(
        'border: 1px solid rgba(37, 99, 235, 130);',
        'border: 1px solid rgba(37, 99, 235, 115);',
        'border: 1px solid rgba(37, 99, 235, 95);'
    )
}

foreach ($themeFile in $themeExpectations.Keys) {
    $themePath = Join-Path $root $themeFile
    if (-not (Test-Path $themePath)) {
        throw "Theme file missing: $themeFile"
    }

    $qssText = Get-Content $themePath -Raw -Encoding UTF8
    foreach ($required in @(
        'QGroupBox',
        'qlineargradient',
        'QTableView, QTableWidget, QTreeView, QTreeWidget, QListView',
        'QTabWidget::pane',
        'QScrollArea',
        'rgba('
    )) {
        if (-not $qssText.Contains($required)) {
            throw "Theme $themeFile is missing softened glow styling token: $required"
        }
    }

    if (-not ($qssText -match 'QGroupBox\s*\{[^}]*qlineargradient')) {
        throw "Theme $themeFile must give QGroupBox a layered gradient background."
    }

    foreach ($requiredBorder in $themeExpectations[$themeFile]) {
        if (-not $qssText.Contains($requiredBorder)) {
            throw "Theme $themeFile is missing softened border token: $requiredBorder"
        }
    }
}

foreach ($required in @(
    'void applyPluginWindowGlowEffects(QWidget* widget);',
    'void LTZK::applyPluginWindowGlowEffects(QWidget* widget)',
    'ltzkGlowEffectApplied',
    'target->setGraphicsEffect(nullptr);',
    'target->setProperty("ltzkGlowEffectApplied", QVariant());',
    'target->setProperty("ltzkGlowEffectTheme", QVariant());',
    'applyPluginWindowGlowEffects(widget);'
)) {
    if (-not ($sourceText.Contains($required) -or $headerText.Contains($required))) {
        throw "Plugin window glow implementation missing: $required"
    }
}

foreach ($requiredSoftGlow in @(
    '// Frame hierarchy is now carried by restrained QSS borders and surfaces.',
    'const QList<QWidget*> descendants = widget->findChildren<QWidget*>();',
    'clearGlow(descendant);'
)) {
    if (-not $sourceText.Contains($requiredSoftGlow)) {
        throw "Plugin window glow is not softened as expected: $requiredSoftGlow"
    }
}

foreach ($forbiddenStrongGlow in @(
    'new QGraphicsDropShadowEffect',
    'setBlurRadius(',
    'setColor(glowColor)',
    'setOffset(0, 0)',
    'applyGlow('
)) {
    $glowStart = $sourceText.IndexOf('void LTZK::applyPluginWindowGlowEffects(QWidget* widget)')
    $glowEnd = $sourceText.IndexOf('void LTZK::installPluginTableEmbeddedWidgetDelegates', $glowStart)
    $glowBody = $sourceText.Substring($glowStart, $glowEnd - $glowStart)
    if ($glowBody.Contains($forbiddenStrongGlow)) {
        throw "Plugin window glow still contains old strong value: $forbiddenStrongGlow"
    }
}

$styleStart = $sourceText.IndexOf('void LTZK::applyPlatformStyleToPluginWindow(QWidget* widget)')
$styleEnd = $sourceText.IndexOf('void LTZK::installPluginTableEmbeddedWidgetDelegates', $styleStart)
if ($styleStart -lt 0 -or $styleEnd -lt 0) {
    throw 'Could not locate applyPlatformStyleToPluginWindow body.'
}

$styleBody = $sourceText.Substring($styleStart, $styleEnd - $styleStart)
if (-not $styleBody.Contains('applyPluginWindowGlowEffects(widget);')) {
    throw 'Plugin windows must clear legacy glow effects when platform style is applied.'
}

Write-Host 'Plugin window glow edges verified for all platform themes.'
