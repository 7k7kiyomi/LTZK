$ErrorActionPreference = 'Stop'

$source = Get-Content -Raw (Join-Path $PSScriptRoot '..\LTZK.cpp')

$resourceFile = Get-Content -Raw (Join-Path $PSScriptRoot '..\LTZK.qrc')

foreach ($resource in @(
    'Resources/combo_arrow_dark.svg',
    'Resources/combo_arrow_light.svg'
)) {
    if ($resourceFile -notmatch [regex]::Escape('<file>' + $resource + '</file>')) {
        throw "Theme-aware combo arrow resource missing: $resource"
    }
}


if ($source -notmatch '#include <QSvgRenderer>') {
    throw 'Runtime SVG rendering must use QSvgRenderer.'
}

if ($source -notmatch 'QFile resourceFile\(comboArrowResource\)') {
    throw 'Arrow diagnostics must verify the resource exists at runtime.'
}


foreach ($required in @('combo_arrow_runtime.log', 'QTextStream diagnosticStream', 'diagnosticLogDir.mkpath(QStringLiteral("."))', 'QIODevice::Append')) {
    if ($source -notmatch [regex]::Escape($required)) {
        throw "Missing file-based combo arrow runtime diagnostics: $required"
    }
}
foreach ($required in @('resourceFile.exists()', 'pixmap.isNull()', 'renderer.isValid()', 'QCoreApplication::libraryPaths()', 'renderer.render(&painter','setTextMargins(0, 0, 24, 0)','isDarkTheme')) {
    if ($source -notmatch [regex]::Escape($required)) {
        throw "Missing runtime SVG/arrow diagnostic or visibility guard: $required"
    }
}


if ($source -notmatch 'comboArrowResource') {
    throw 'Embedded combo style must choose an arrow resource from the active theme.'
}

if ($source -notmatch 'ltzkDrawingLayoutComboArrow') {
    throw 'Embedded combo must have a dedicated visible arrow child control.'
}


if ($source -notmatch 'polishDrawingLayoutTreeEmbeddedCombos\(LTZK\* owner, QWidget\* widget, bool isDarkTheme, bool isBlackTheme, QComboBox\* onlyCombo = nullptr\)') {
    throw 'Combo polishing must receive the LTZK owner explicitly for event-filter installation.'
}

if ($source -notmatch 'const QList<QComboBox\*> combos = onlyCombo \?') {
    throw 'Combo polishing must find QComboBox controls directly from the plugin window.'
}
foreach ($required in @(
    'qApp->installEventFilter(this);',
    'void LTZK::schedulePluginWindowPolish(QWidget* window)',
    'QEvent::Show',
    'QEvent::WindowActivate',
    'QEvent::ChildAdded',
    'QEvent::LayoutRequest',
    'QEvent::StyleChange',
    'QEvent::PaletteChange',
    'QChildEvent* childEvent',
    'window->property("ltzkWindowInitialized").toBool()',
    'widget->setProperty("ltzkWindowInitialized", true);',
    'combo->property("ltzkComboPolishedTheme").toString()',
    'static QPixmap darkArrowPixmap',
    'static QPixmap lightArrowPixmap',
    'const QString comboBaseColor',
    'const QString comboDropDownColor',
    'const QString comboTextColor',
    'isBlackTheme ? QStringLiteral("#050505")',
    'QStringLiteral("#06111d")',
    'QStringLiteral("#f8fafc")',
    'QStringLiteral("#e2e8f0")',
    'comboTextPaletteColor'
)) {
    if ($source -notmatch [regex]::Escape($required)) {
        throw "Missing bounded plugin-control lifecycle behavior: $required"
    }
}

if ($source -notmatch 'polishDrawingLayoutTreeEmbeddedCombos\(this,\s*window,\s*(m_currentTheme\s*!=\s*PlatformTheme::White|!isLightPlatformTheme\(\)),\s*m_currentTheme\s*==\s*PlatformTheme::Black,\s*combo\);') {
    throw 'Missing bounded plugin-control lifecycle behavior for the active theme detector.'
}

if (($source -match 'QTimer::singleShot\(120, this, &LTZK::applyPlatformStyleToTopLevelWidgets\)') -or ($source -match 'QTimer::singleShot\(350, this, &LTZK::applyPlatformStyleToTopLevelWidgets\)')) {

    throw 'Plugin actions must not schedule repeated full top-level style scans.'
}
if ($source -notmatch 'ltzkDrawingLayoutComboArrowTarget') {
    throw 'Embedded combo arrow must be tracked for geometry updates.'
}

if ($source -notmatch 'QEvent::Resize[\s\S]*ltzkDrawingLayoutComboArrow') {
    throw 'Embedded combo arrow must be repositioned after combo resize.'
}

$comboStyleStart = $source.IndexOf('const QString comboStyleTemplate = QStringLiteral(')
if ($comboStyleStart -lt 0) {
    throw 'Could not locate the drawing-layout embedded combo style.'
}
$comboStyleEnd = $source.IndexOf('const QString comboLineEditStyle', $comboStyleStart)
if ($comboStyleEnd -lt 0) {
    throw 'Could not locate the end of the drawing-layout embedded combo style.'
}
$comboStyle = $source.Substring($comboStyleStart, $comboStyleEnd - $comboStyleStart)


if ($comboStyle -notmatch 'QComboBox::drop-down') {
    throw 'Embedded combo style must preserve a visible drop-down subcontrol.'
}

$comboBlockEnd = $source.IndexOf('if (combo->lineEdit())', $comboStyleEnd)
if ($comboBlockEnd -lt 0) {
    throw 'Could not locate the embedded combo palette block.'
}
$comboBlock = $source.Substring($comboStyleEnd, $comboBlockEnd - $comboStyleEnd)
if ($comboBlock -match 'QPalette::ButtonText, QColor\(255, 255, 255, 0\)') {
    throw 'Embedded combo palette must not make the native arrow text color transparent.'
}

if ($source -notmatch 'const bool isEllipsisButton = button->text\(\)\.trimmed\(\) == QStringLiteral\("\.\.\."\)') {
    throw 'Embedded table button normalization must identify ellipsis selector buttons.'
}

if ($source -notmatch 'isEllipsisButton[^\r\n]*buttonWidth') {
    throw 'Ellipsis selector buttons must receive a width calculation that prevents clipping.'
}

foreach ($required in @(
    'QComboBox { color: %4;',
    'palette.setColor(QPalette::WindowText, comboTextPaletteColor);',
    'palette.setColor(QPalette::Text, comboTextPaletteColor);'
)) {
    if ($source -notmatch [regex]::Escape($required)) {
        throw "Combo current text must remain visible in all themes: $required"
    }
}

Write-Host 'Plugin embedded combo arrows and ellipsis selector sizing are protected.'

if ($source -match 'QComboBox::down-arrow') {
    throw 'Embedded combo arrow must use one custom SVG rendering path, not QSS down-arrow.'
}
