$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$header = Join-Path $root 'LTZK.h'
$source = Join-Path $root 'LTZK.cpp'

foreach ($path in @($header, $source)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

$headerText = Get-Content -Raw -Encoding ASCII $header
$sourceText = Get-Content -Raw -Encoding Default $source

foreach ($needle in @(
    'QString userLogPath() const',
    'void saveUserLog(const QString& userName, const QString& password)',
    'void clearUserLog()'
)) {
    if ($headerText -notmatch [regex]::Escape($needle)) {
        throw "Local user log header marker missing: $needle"
    }
}

foreach ($needle in @(
    'LTZK::userLogPath() const',
    'LTZK::saveUserLog(const QString& userName, const QString& password)',
    'LTZK::clearUserLog()',
    'config/user_login.ini',
    'database.ini',
    'QSettings settings(userLogPath(), QSettings::IniFormat)',
    'settings.setValue(QStringLiteral("user/user_name"), userName)',
    'settings.setValue(QStringLiteral("user/password"), password)',
    'settings.sync()',
    'saveUserLog(userName, password)',
    'clearUserLog()',
    'QFile file(userLogPath())',
    'file.open(QIODevice::WriteOnly | QIODevice::Truncate)'
)) {
    if ($sourceText -notmatch [regex]::Escape($needle)) {
        throw "Local user log source marker missing: $needle"
    }
}

$loginPattern = 'gCurrentUserSession\s*=\s*session;\s*[\r\n\s]*saveUserLog\(userName,\s*password\);'
if ($sourceText -notmatch $loginPattern) {
    throw 'Home-page login must save config/user_login.ini immediately after the session is accepted.'
}

$logoutPattern = 'void LTZK::logoutCurrentUser\(\)\s*\{\s*clearUserLog\(\);\s*gCurrentUserSession\.clear\(\);'
if ($sourceText -notmatch $logoutPattern) {
    throw 'Logout must clear config/user_login.ini before clearing the in-memory session.'
}

$destructorPattern = 'LTZK::~LTZK\(\)\s*\{\s*clearUserLog\(\);'
if ($sourceText -notmatch $destructorPattern) {
    throw 'Program shutdown must clear config/user_login.ini from the LTZK destructor.'
}

Write-Host 'Local user_login.ini login state verified.'
