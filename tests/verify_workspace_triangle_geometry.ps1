$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'LTZK.cpp'
if (-not (Test-Path $source)) {
    throw "Missing required file: $source"
}

$text = Get-Content -Raw -Encoding UTF8 $source

$equilateralAltitudeRatio = [Math]::Sqrt(3.0) / 2.0
$triangleHeightFor6Px = 6.0 / $equilateralAltitudeRatio
if ([Math]::Abs($triangleHeightFor6Px - 6.92820323027551) -gt 0.0001) {
    throw 'The 6px horizontal span must produce a 6.93px equilateral-triangle height.'
}

foreach ($required in @(
    'const qreal equilateralAltitudeRatio = 0.8660254037844386;',
    'const QRectF bounds(0.0, 0.0, rect.width(), rect.height());',
    'const qreal horizontalSide = qMin(bounds.height(), bounds.width() / equilateralAltitudeRatio);',
    'const qreal horizontalAltitude = horizontalSide * equilateralAltitudeRatio;',
    'const qreal horizontalHalfSide = horizontalSide * 0.5;',
    'const qreal verticalSide = qMin(bounds.width(), bounds.height() / equilateralAltitudeRatio);',
    'const qreal verticalAltitude = verticalSide * equilateralAltitudeRatio;',
    'const qreal verticalHalfSide = verticalSide * 0.5',
    'bounds.left() + horizontalAltitude',
    'bounds.right() - horizontalAltitude',
    'center.y() - horizontalHalfSide',
    'center.y() + horizontalHalfSide',
    'center.x() - verticalHalfSide',
    'center.x() + verticalHalfSide',
    'bounds.top() + verticalAltitude',
    'bounds.bottom() - verticalAltitude'
)) {
    if (-not $text.Contains($required)) {
        throw "Equilateral workspace triangle geometry requirement missing: $required"
    }
}

foreach ($forbidden in @(
    'const qreal horizontalHalfHeight = qMax(2.0, bounds.width() * 0.30);',
    'const qreal verticalHalfWidth = qMax(2.0, bounds.height() * 0.30);',
    'return QPolygonF() << QPointF(bounds.left(), center.y()) << QPointF(bounds.right(), bounds.top()) << QPointF(bounds.right(), bounds.bottom());',
    'return QPolygonF() << QPointF(bounds.right(), center.y()) << QPointF(bounds.left(), bounds.top()) << QPointF(bounds.left(), bounds.bottom());'
)) {
    if ($text.Contains($forbidden)) {
        throw "Non-equilateral workspace triangle geometry remains: $forbidden"
    }
}

Write-Host 'Workspace triangle equilateral geometry verified.'
