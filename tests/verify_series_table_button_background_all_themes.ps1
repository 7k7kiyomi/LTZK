$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$themes = @(
    @{ Path = 'Resources\styles\ltzk_white.qss'; Text = '#1e293b'; Background = '#ffffff' },
    @{ Path = 'Resources\styles\ltzk_black.qss'; Text = '#fafafa'; Background = '#0f0f0f' },
    @{ Path = 'Resources\styles\ltzk_sci_fi.qss'; Text = '#f8feff'; Background = '#06111d' }
)

$selector = 'QTableView QPushButton, QTableWidget QPushButton, QTreeView QPushButton, QTreeWidget QPushButton'
foreach ($theme in $themes) {
    $path = Join-Path $root $theme.Path
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing required file: $path" }

    $qss = Get-Content -Raw -Encoding UTF8 $path
    $start = $qss.IndexOf($selector)
    $end = $qss.IndexOf('}', $start)
    if ($start -lt 0 -or $end -lt 0) { throw "Could not locate embedded button rule: $path" }
    $rule = $qss.Substring($start, $end - $start + 1)

    foreach ($required in @(
        "color: $($theme.Text);",
        "background: $($theme.Background);"
    )) {
        if (-not $rule.Contains($required)) {
            throw "Embedded buttons must use an explicit themed color in $path`: $required"
        }
    }
    if ($rule.Contains('background: transparent;')) {
        throw "Embedded buttons must not expose a native parent palette: $path"
    }
}

$sourcePath = Join-Path $root 'LTZK.cpp'
$source = Get-Content -Raw -Encoding UTF8 $sourcePath
$polishStart = $source.IndexOf('void LTZK::polishSeriesContentTableWindow(QWidget* widget)')
$polishEnd = $source.IndexOf('void LTZK::applyPlatformStyleToTopLevelWidgets()', $polishStart)
if ($polishStart -lt 0 -or $polishEnd -lt 0) {
    throw 'Could not locate series content table polishing implementation.'
}
$polishBody = $source.Substring($polishStart, $polishEnd - $polishStart)

foreach ($required in @(
    'const QString buttonTextColor = m_currentTheme == PlatformTheme::White',
    'const QString buttonBackgroundColor = m_currentTheme == PlatformTheme::White',
    'm_currentTheme == PlatformTheme::Black',
    'QStringLiteral("#ffffff")',
    'QStringLiteral("#0f0f0f")',
    'QStringLiteral("#06111d")',
    'background: %2;',
    '.arg(buttonTextColor, buttonBackgroundColor'
)) {
    if (-not $polishBody.Contains($required)) {
        throw "Runtime table button styling must be explicit for all themes: $required"
    }
}

$runtimeButtonStyleStart = $polishBody.IndexOf('const QString buttonStyle')
$runtimeButtonStyleEnd = $polishBody.IndexOf('const QList<QAbstractButton*> buttons', $runtimeButtonStyleStart)
if ($runtimeButtonStyleStart -lt 0 -or $runtimeButtonStyleEnd -lt 0) {
    throw 'Could not isolate runtime table button style.'
}
$runtimeButtonStyle = $polishBody.Substring($runtimeButtonStyleStart, $runtimeButtonStyleEnd - $runtimeButtonStyleStart)
if ($runtimeButtonStyle.Contains('background: transparent;')) {
    throw 'Runtime table button styling must not restore transparent backgrounds for white or black themes.'
}

Write-Host 'Series table embedded buttons use explicit backgrounds in all platform themes.'
