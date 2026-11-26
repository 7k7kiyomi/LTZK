$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content (Join-Path $root 'LTZK.cpp') -Raw -Encoding UTF8
$themeFiles = @(
    'Resources\styles\ltzk_sci_fi.qss',
    'Resources\styles\ltzk_black.qss',
    'Resources\styles\ltzk_white.qss',
    'Resources\styles\ltzk_field_notes.qss',
    'Resources\styles\ltzk_steel_warm_white.qss'
)

if ($source -notmatch 'titleContainer->setObjectName\(QStringLiteral\("workspaceTitleContainer"\)\)') {
    throw 'Workspace subplatform title container must have an explicit transparent-style object name.'
}

$accessBlock = [regex]::Match(
    $source,
    'QWidget\* LTZK::buildAccessControlPage\(\)[\s\S]*?QWidget\* LTZK::buildSciFiWorkspacePage\(\)'
).Value
if ([string]::IsNullOrWhiteSpace($accessBlock) -or
    $accessBlock -notmatch 'titleLabel->setObjectName\(QStringLiteral\("dashboardTitle"\)\)') {
    throw 'Permission-management subplatform title must continue using dashboardTitle.'
}
if ([string]::IsNullOrWhiteSpace($accessBlock) -or
    $accessBlock -notmatch 'titleLabel->setAttribute\(Qt::WA_TranslucentBackground\)' -or
    $accessBlock -notmatch 'titleLabel->setAutoFillBackground\(false\)') {
    throw 'Permission-management subplatform title must explicitly disable its own background fill.'
}

foreach ($themeFile in $themeFiles) {
    $qss = Get-Content (Join-Path $root $themeFile) -Raw -Encoding UTF8

    $titleRule = [regex]::Match($qss, '#dashboardTitle\s*\{(?<body>[^}]*)\}')
    if (-not $titleRule.Success -or $titleRule.Groups['body'].Value -notmatch 'background:\s*transparent') {
        throw "$themeFile must make dashboardTitle background transparent."
    }

    $containerRule = [regex]::Match($qss, '#workspaceTitleContainer\s*\{(?<body>[^}]*)\}')
    if (-not $containerRule.Success -or
        $containerRule.Groups['body'].Value -notmatch 'background:\s*transparent' -or
        $containerRule.Groups['body'].Value -notmatch 'border:\s*0') {
        throw "$themeFile must make workspaceTitleContainer transparent and borderless."
    }
}

Write-Host 'Subplatform title transparency contract verified.'
