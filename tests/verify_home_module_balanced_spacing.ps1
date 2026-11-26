$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -LiteralPath (Join-Path $root 'LTZK.cpp') -Raw

foreach ($required in @(
    'constexpr int kHomeGridHorizontalSpacing = 80',
    'constexpr int kHomeGridVerticalSpacing = 74',
    'grid->setHorizontalSpacing(scaledInt(panel, kHomeGridHorizontalSpacing))',
    'grid->setVerticalSpacing(scaledInt(panel, kHomeGridVerticalSpacing))',
    'stageLayout->addLayout(grid, 0)',
    'stageLayout->setAlignment(grid, Qt::AlignHCenter)',
    'stageLayout->addStretch(1)'
)) {
    if (-not $source.Contains($required)) {
        throw "主页模块均衡间距要求缺失: $required"
    }
}

foreach ($forbidden in @(
    'moduleGridContainer',
    'grid->setHorizontalSpacing(scaledInt(panel, 200))',
    'grid->setVerticalSpacing(scaledInt(panel, 18))',
    'grid->setHorizontalSpacing(scaledInt(panel, 146))',
    'grid->setVerticalSpacing(scaledInt(panel, 142))',
    'stageLayout->addStretch(2)',
    'grid->setColumnStretch(0, 1)',
    'grid->setColumnStretch(1, 1)'
)) {
    if ($source.Contains($forbidden)) {
        throw "主页模块仍保留旧间距或裁剪风险布局: $forbidden"
    }
}

Write-Host 'Home module balanced spacing verified.'





