$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
$header = Join-Path $root 'LTZK.h'

foreach ($path in @($source, $header)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$sourceText = Get-Content -Raw -Encoding Default $source
$headerText = Get-Content -Raw -Encoding Default $header

foreach ($required in @(
    'qualityResultMessageField',
    'QStringLiteral("message")',
    'QStringLiteral("MESSAGE")',
    'm_qualityResultVisiblePaths'
)) {
    if (-not $sourceText.Contains($required) -and -not $headerText.Contains($required)) {
        throw "Grouped message result-panel behavior missing: $required"
    }
}

$messageHelper = [regex]::Match(
    $sourceText,
    'QString LTZK::qualityResultMessageField\(const QgsVectorLayer\* layer\) const(?<body>.*?)QString LTZK::qualityResultErrorTypeField',
    [System.Text.RegularExpressions.RegexOptions]::Singleline)
if (-not $messageHelper.Success) {
    throw 'Could not locate qualityResultMessageField before fallback error-type helper.'
}

if ($messageHelper.Groups['body'].Value.IndexOf('QStringLiteral("message")') -lt 0) {
    throw 'message field must be the first preferred message candidate.'
}

if ($sourceText.Contains('m_qualityResultList') -or $headerText.Contains('m_qualityResultList')) {
    throw 'Result panel list controls must not remain after removing the result panel display.'
}

Write-Host 'Quality result message-field fallback remains available without a result panel.'
