$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
$sourceText = Get-Content -Raw -Encoding UTF8 $sourcePath
$displayLabel = -join ([char]0x663E, [char]0x793A, [char]0x8981, [char]0x7D20)
$oldErrorLabel = -join ([char]0x9519, [char]0x8BEF, [char]0x8981, [char]0x7D20)

foreach ($required in @(
    'sideTabBar->setExpanding(!visible);',
    'm_layerTreePanelWidget->setAttribute(Qt::WA_StyledBackground, true);',
    'm_layerTreePanelWidget->setAutoFillBackground(true);',
    'void LTZK::polishPluginFrameHierarchy(QWidget* widget)',
    'ltzkFrameDepth',
    'void LTZK::applyPluginWindowGlowEffects(QWidget* widget)'
)) {
    if (-not $sourceText.Contains($required)) {
        throw "Workspace visual polish implementation missing: $required"
    }
}

if (-not $sourceText.Contains($displayLabel)) {
    throw 'The result summary is missing the display-element label.'
}
if ($sourceText.Contains($oldErrorLabel)) {
    throw 'The result summary still contains the old error-element label.'
}

$glowStart = $sourceText.IndexOf('void LTZK::applyPluginWindowGlowEffects(QWidget* widget)')
$glowEnd = $sourceText.IndexOf('void LTZK::installPluginTableEmbeddedWidgetDelegates', $glowStart)
if ($glowStart -lt 0 -or $glowEnd -lt 0) {
    throw 'Could not locate plugin window glow cleanup implementation.'
}
$glowBody = $sourceText.Substring($glowStart, $glowEnd - $glowStart)
foreach ($required in @(
    'target->setGraphicsEffect(nullptr);',
    'ltzkGlowEffectApplied',
    'QVariant()'
)) {
    if (-not $glowBody.Contains($required)) {
        throw "Plugin window glow cleanup is missing: $required"
    }
}
foreach ($forbidden in @(
    'new QGraphicsDropShadowEffect',
    'setBlurRadius(',
    'setColor(glowColor)'
)) {
    if ($glowBody.Contains($forbidden)) {
        throw "Plugin window glow cleanup still creates a luminous effect: $forbidden"
    }
}

$themeExpectations = @{
    'Resources\styles\ltzk_sci_fi.qss' = @(
        'QTabWidget#workspaceSideTabs QTabBar { background: rgba(8, 26, 43, 235); border: 1px solid rgba(83, 137, 157, 135); }',
        '#workspaceLayerTreePanelWidget { background: rgba(8, 26, 43, 235); border: 0; }',
        'QGroupBox[ltzkFrameDepth="0"] { background: rgba(10, 35, 55, 238); border: 1px solid rgba(93, 173, 197, 165); border-left: 2px solid rgba(112, 184, 202, 175); }',
        'QGroupBox[ltzkFrameDepth="1"] { background: rgba(8, 29, 47, 238); border: 1px solid rgba(87, 126, 144, 150); }',
        'QGroupBox[ltzkFrameDepth="2"] { background: rgba(6, 23, 38, 240); border: 1px solid rgba(72, 101, 117, 140); }',
        'QTableView, QTableWidget, QTreeView, QTreeWidget, QListView { background: rgba(5, 21, 35, 245); alternate-background-color: rgba(12, 37, 56, 180); border: 1px solid rgba(72, 101, 117, 150); gridline-color: rgba(92, 125, 137, 95); }',
        'QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QDateEdit, QDateTimeEdit, QComboBox { background: rgba(7, 25, 41, 245); border: 1px solid rgba(87, 126, 144, 160); }',
        'QFrame[dashboardPanel="true"] { background: rgba(8, 29, 47, 235); border: 1px solid rgba(83, 137, 157, 135); border-radius: 4px; border-left: 2px solid rgba(112, 184, 202, 175); }'
    )
    'Resources\styles\ltzk_black.qss' = @(
        'QTabWidget#workspaceSideTabs QTabBar { background: #0f0f0f; border: 1px solid #353535; }',
        '#workspaceLayerTreePanelWidget { background: #0f0f0f; border: 0; }',
        'QGroupBox[ltzkFrameDepth="0"] { background: #161616; border: 1px solid #5f451d; border-left: 2px solid #9a6a1b; }',
        'QGroupBox[ltzkFrameDepth="1"] { background: #121212; border: 1px solid #555555; }',
        'QGroupBox[ltzkFrameDepth="2"] { background: #0e0e0e; border: 1px solid #3f3f3f; }',
        'QTableView, QTableWidget, QTreeView, QTreeWidget, QListView { background: #0d0d0d; alternate-background-color: #161616; border: 1px solid #3f3f3f; gridline-color: #242424; }',
        'QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QDateEdit, QDateTimeEdit, QComboBox { background: #0f0f0f; border: 1px solid #4b4b4b; }',
        'QFrame[dashboardPanel="true"] { background: #121212; border: 1px solid #3b3b3b; border-radius: 4px; border-left: 2px solid #9a6a1b; }'
    )
    'Resources\styles\ltzk_white.qss' = @(
        'QTabWidget#workspaceSideTabs QTabBar { background: #fbfcfe; border: 1px solid #d7dee7; }',
        '#workspaceLayerTreePanelWidget { background: #fbfcfe; border: 0; }',
        'QGroupBox[ltzkFrameDepth="0"] { background: #f6f8fb; border: 1px solid #6686b3; border-left: 2px solid #4f78af; }',
        'QGroupBox[ltzkFrameDepth="1"] { background: #ffffff; border: 1px solid #bac5d1; }',
        'QGroupBox[ltzkFrameDepth="2"] { background: #f9fafb; border: 1px solid #d8dee6; }',
        'QTableView, QTableWidget, QTreeView, QTreeWidget, QListView { background: #ffffff; alternate-background-color: #f7f9fb; border: 1px solid #c4ced9; gridline-color: #e5e9ee; }',
        'QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QDateEdit, QDateTimeEdit, QComboBox { background: #ffffff; border: 1px solid #c4ced9; }',
        'QFrame[dashboardPanel="true"] { background: #ffffff; border: 1px solid #d2dae4; border-radius: 4px; border-left: 2px solid #6686b3; }'
    )
    'Resources\styles\ltzk_field_notes.qss' = @(
        'QTabWidget#workspaceSideTabs QTabBar { background: #F0EBDD; border: 1px solid #C1B99F; }',
        '#workspaceLayerTreePanelWidget { background: #F0EBDD; border: 0; }',
        'QGroupBox[ltzkFrameDepth="0"] { background: #EBE3CF; border: 1px solid #9A9B77; border-left: 2px solid #7F8F66; }',
        'QGroupBox[ltzkFrameDepth="1"] { background: #F3EDDE; border: 1px solid #B7B79B; }',
        'QGroupBox[ltzkFrameDepth="2"] { background: #F8F1E3; border: 1px solid #D0B092; }',
        'QTableView, QTableWidget, QTreeView, QTreeWidget, QListView { background: #F8F2E6; alternate-background-color: #EFE7D7; border: 1px solid #BEB9A4; gridline-color: #DED6C4; }',
        'QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QDateEdit, QDateTimeEdit, QComboBox { background: #FBF7ED; border: 1px solid #C3BDA6; }',
        'QFrame[dashboardPanel="true"] { background: #F0EBDD; border: 1px solid #C1B99F; border-radius: 4px; border-left: 2px solid #87936B; }'
    )
    'Resources\styles\ltzk_steel_warm_white.qss' = @(
        'QTabWidget#workspaceSideTabs QTabBar { background: #F1F3EF; border: 1px solid #B6C1BD; }',
        '#workspaceLayerTreePanelWidget { background: #F1F3EF; border: 0; }',
        'QGroupBox[ltzkFrameDepth="0"] { background: #E9EFEC; border: 1px solid #789692; border-left: 2px solid #5F8581; }',
        'QGroupBox[ltzkFrameDepth="1"] { background: #F3F5F1; border: 1px solid #B2BFBB; }',
        'QGroupBox[ltzkFrameDepth="2"] { background: #F8F9F6; border: 1px solid #D0D7D3; }',
        'QTableView, QTableWidget, QTreeView, QTreeWidget, QListView { background: #FAFAF7; alternate-background-color: #F0F3F0; border: 1px solid #B9C5C0; gridline-color: #DCE3DF; }',
        'QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QDateEdit, QDateTimeEdit, QComboBox { background: #FFFFFF; border: 1px solid #BFCAC6; }',
        'QFrame[dashboardPanel="true"] { background: #F1F3EF; border: 1px solid #B6C1BD; border-radius: 4px; border-left: 2px solid #789692; }'
    )
}

foreach ($themeFile in $themeExpectations.Keys) {
    $themePath = Join-Path $root $themeFile
    $qssText = Get-Content -Raw -Encoding UTF8 $themePath
    foreach ($required in $themeExpectations[$themeFile]) {
        if (-not $qssText.Contains($required)) {
            throw "Theme $themeFile is missing workspace visual polish rule: $required"
        }
    }
}

Write-Host 'Workspace layer manager and framed-window visual hierarchy verified.'
