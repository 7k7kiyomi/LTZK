$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'auth\LTZKLoginDialog.cpp'
$header = Join-Path $root 'auth\LTZKLoginDialog.h'

if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}
if (-not (Test-Path $header)) {
    throw "Missing required file: $header"
}

$text = Get-Content -Raw -Encoding ASCII $source
$headerText = Get-Content -Raw -Encoding ASCII $header

if ($text -match 'setFixedSize\s*\(') {
    throw 'Login dialog must not use setFixedSize; it should be resizable/adaptive.'
}

foreach ($forbidden in @(
    'setMinimumWidth(420)',
    'font-size: 20px',
    'font-size: 14px',
    'setMinimumSize(560, 330)'
)) {
    if ($text -match [regex]::Escape($forbidden)) {
        throw "Login dialog still uses oversized layout marker: $forbidden"
    }
}

foreach ($needle in @(
    'setMinimumSize',
    'resize(',
    'loginTitleLabel',
    'loginFormLayout',
    'setColumnStretch(1, 1)',
    'setWordWrap(true)',
    'setMinimumWidth(260)',
    'setMinimumHeight(30)',
    'setSizeGripEnabled(true)'
)) {
    if ($text -notmatch [regex]::Escape($needle)) {
        throw "Login dialog responsive layout marker missing: $needle"
    }
}

foreach ($needle in @(
    'double uiScale() const',
    'void resizeEvent(QResizeEvent* event) override',
    'void updateAdaptiveLayout()',
    'QVBoxLayout* mMainLayout',
    'QGridLayout* mFormLayout',
    'QHBoxLayout* mButtonLayout'
)) {
    if ($headerText -notmatch [regex]::Escape($needle)) {
        throw "Login dialog adaptive header marker missing: $needle"
    }
}

foreach ($needle in @(
    '#include "../LTZKUiScale.h"',
    'LTZKLoginDialog::resizeEvent(QResizeEvent* event)',
    'QDialog::resizeEvent(event)',
    'LTZKLoginDialog::uiScale() const',
    'LTZKUiScale::factorForWidget(this)',
    'LTZKUiScale::scaledInt',
    'LTZKLoginDialog::updateAdaptiveLayout()',
    'font-size: %1px',
    'mMainLayout->setContentsMargins',
    'mFormLayout->setHorizontalSpacing',
    'mButtonLayout->setContentsMargins'
)) {
    if ($text -notmatch [regex]::Escape($needle)) {
        throw "Login dialog adaptive resize marker missing: $needle"
    }
}

foreach ($forbidden in @(
    'width() / kBaseDialogWidth',
    'height() / kBaseDialogHeight',
    'qBound(0.85'
)) {
    if ($text -match [regex]::Escape($forbidden)) {
        throw "Login dialog still uses local resize-derived scaling: $forbidden"
    }
}

Write-Host 'Login dialog responsive layout verified.'
