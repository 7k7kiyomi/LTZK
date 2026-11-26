$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing required file: $sourcePath"
}

$source = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8
$start = $source.IndexOf('void polishDrawingLayoutTreeEmbeddedCombos(')
$end = $source.IndexOf('void suppressPluginTableEmbeddedWidgetTexts(', $start)
if ($start -lt 0 -or $end -lt 0) {
    throw 'Could not locate the plugin combo polish helper.'
}

$body = $source.Substring($start, $end - $start)
if ($source -notmatch 'bool isDrawingLayoutTreeCombo\(QComboBox\* combo\)') {
    throw 'The host must define a helper that identifies drawing-layout tree combos.'
}
if ($source -notmatch 'return tree->objectName\(\)\s*==\s*QStringLiteral\("treeWidget_csv"\)') {
    throw 'The drawing-layout combo scope must be limited to treeWidget_csv.'
}
if ($body -notmatch 'if\s*\(\s*!isDrawingLayoutTreeCombo\(combo\)\s*\)\s*\{\s*return;\s*\}') {
    throw 'General plugin combos must be skipped before drawing-layout combo polishing.'
}

$themes = @(
    @{ Path = 'Resources\styles\ltzk_sci_fi.qss'; Arrow = 'combo_arrow_dark.svg' },
    @{ Path = 'Resources\styles\ltzk_black.qss'; Arrow = 'combo_arrow_dark.svg' },
    @{ Path = 'Resources\styles\ltzk_white.qss'; Arrow = 'combo_arrow_light.svg' },
    @{ Path = 'Resources\styles\ltzk_field_notes.qss'; Arrow = 'combo_arrow_light.svg' },
    @{ Path = 'Resources\styles\ltzk_steel_warm_white.qss'; Arrow = 'combo_arrow_light.svg' }
)

foreach ($theme in $themes) {
    $qssPath = Join-Path $root $theme.Path
    if (-not (Test-Path -LiteralPath $qssPath)) {
        throw "Missing required theme stylesheet: $qssPath"
    }

    $qss = Get-Content -LiteralPath $qssPath -Raw -Encoding UTF8
    if ($qss -notmatch 'QDateEdit::down-arrow\s*,\s*QDateTimeEdit::down-arrow') {
        throw "Missing date-edit arrow selectors in $($theme.Path)."
    }
    if ($qss -notmatch "image:\s*url\(:/LTZK/Resources/$([regex]::Escape($theme.Arrow))\)") {
        throw "Missing matching arrow resource in $($theme.Path)."
    }
}

Write-Host 'Plugin combo scope and date-time arrow styling verified.'
