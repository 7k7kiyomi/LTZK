$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$resourceFile = Join-Path $root 'LTZK.qrc'
if (-not (Test-Path -LiteralPath $resourceFile)) {
    throw "Missing Qt resource file: $resourceFile"
}

$resourceText = Get-Content -LiteralPath $resourceFile -Raw -Encoding UTF8
$themes = @(
    @{ Path = 'Resources\styles\ltzk_sci_fi.qss'; OpenArrow = 'tree_branch_open_dark_theme.svg'; ClosedArrow = 'tree_branch_closed_dark_theme.svg' },
    @{ Path = 'Resources\styles\ltzk_black.qss'; OpenArrow = 'tree_branch_open_dark_theme.svg'; ClosedArrow = 'tree_branch_closed_dark_theme.svg' },
    @{ Path = 'Resources\styles\ltzk_white.qss'; OpenArrow = 'tree_branch_open_light_theme.svg'; ClosedArrow = 'tree_branch_closed_light_theme.svg' },
    @{ Path = 'Resources\styles\ltzk_field_notes.qss'; OpenArrow = 'tree_branch_open_light_theme.svg'; ClosedArrow = 'tree_branch_closed_light_theme.svg' },
    @{ Path = 'Resources\styles\ltzk_steel_warm_white.qss'; OpenArrow = 'tree_branch_open_light_theme.svg'; ClosedArrow = 'tree_branch_closed_light_theme.svg' }
)

foreach ($theme in $themes) {
    $qssPath = Join-Path $root $theme.Path
    if (-not (Test-Path -LiteralPath $qssPath)) {
        throw "Missing required theme stylesheet: $qssPath"
    }

    $qss = Get-Content -LiteralPath $qssPath -Raw -Encoding UTF8
    $openRule = "QTreeView::branch:open:has-children:!has-siblings, QTreeView::branch:open:has-children:has-siblings { image: url(:/LTZK/Resources/$($theme.OpenArrow)); width: 16px; height: 16px; }"
    $closedRule = "QTreeView::branch:closed:has-children:!has-siblings, QTreeView::branch:closed:has-children:has-siblings { image: url(:/LTZK/Resources/$($theme.ClosedArrow)); width: 16px; height: 16px; }"

    if (-not $qss.Contains($openRule)) {
        throw "$($theme.Path) is missing its theme-specific open branch arrow rule."
    }
    if (-not $qss.Contains($closedRule)) {
        throw "$($theme.Path) is missing its theme-specific closed branch arrow rule."
    }
}

foreach ($asset in @(
    'Resources/tree_branch_open_dark_theme.svg',
    'Resources/tree_branch_closed_dark_theme.svg',
    'Resources/tree_branch_open_light_theme.svg',
    'Resources/tree_branch_closed_light_theme.svg'
)) {
    if ($resourceText -notmatch [regex]::Escape("<file>$asset</file>")) {
        throw "Qt resource manifest is missing $asset"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $root $asset))) {
        throw "Missing branch arrow asset: $(Join-Path $root $asset)"
    }
}

Write-Host 'Layer-tree branch arrow contrast is configured for all five themes.'
