$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$ltzkCpp = Join-Path $root 'LTZK.cpp'
$ltzkH = Join-Path $root 'LTZK.h'
$project = Join-Path $root 'LTZK.vcxproj'
$authHeader = Join-Path $root 'auth\LTZKAuthManager.h'
$authCpp = Join-Path $root 'auth\LTZKAuthManager.cpp'
$loginHeader = Join-Path $root 'auth\LTZKLoginDialog.h'
$loginCpp = Join-Path $root 'auth\LTZKLoginDialog.cpp'
$accessHeader = Join-Path $root 'auth\LTZKAccessControlDialog.h'
$accessCpp = Join-Path $root 'auth\LTZKAccessControlDialog.cpp'
$setupSql = Join-Path $root 'database\postgis_user_permissions_setup.sql'
$setupDoc = Join-Path $root 'docs\postgis_login_permissions_setup.md'
$dbIni = Join-Path $root 'config\database.ini'

foreach ($path in @($ltzkCpp, $ltzkH, $project)) {
    if (-not (Test-Path $path)) {
        throw "Missing required file: $path"
    }
}

foreach ($path in @($authHeader, $authCpp, $loginHeader, $loginCpp, $accessHeader, $accessCpp, $setupSql, $setupDoc, $dbIni)) {
    if (-not (Test-Path $path)) {
        throw "Missing new login/permission file: $path"
    }
}

$authText = Get-Content -Raw -Encoding ASCII $authHeader
$authImpl = Get-Content -Raw -Encoding ASCII $authCpp
$loginText = Get-Content -Raw -Encoding ASCII $loginCpp
$accessText = Get-Content -Raw -Encoding ASCII $accessCpp
$ltzkText = Get-Content -Raw -Encoding Default $ltzkCpp
$projectText = Get-Content -Raw -Encoding UTF8 $project
$sqlText = Get-Content -Raw -Encoding UTF8 $setupSql
$docText = Get-Content -Raw -Encoding UTF8 $setupDoc
$dbIniText = Get-Content -Raw -Encoding ASCII $dbIni

foreach ($needle in @('DataOperator', 'DataUser', 'DatabaseAdmin', 'canOperateProductStore', 'canManageUsers')) {
    if ($authText -notmatch [regex]::Escape($needle)) {
        throw "Auth role/session missing expected marker: $needle"
    }
}

foreach ($needle in @(
    'databaseConfigPath',
    'QDir(appDir).absoluteFilePath(QStringLiteral("../config/database.ini"))',
    'authenticateUser',
    'ensureConnectedAndInitialized',
    'ensureDefaultAdmin',
    'user_permissions',
    'QCryptographicHash::Sha256',
    'CREATE EXTENSION IF NOT EXISTS postgis',
    'admin',
    '123456'
)) {
    if (($authImpl + $authText) -notmatch [regex]::Escape($needle)) {
        throw "Auth implementation missing expected marker: $needle"
    }
}

if ($authImpl -notmatch 'QDir::toNativeSeparators\(databaseConfigPath\(\)\)') {
    throw 'Database connection error must show the resolved configuration path.'
}
if ($authImpl -match '\\x0045\\x003A\\x005C\\x0041\\x0041\\x0041\\x004C\\x0054\\x005A\\x004B') {
    throw 'Database connection error must not contain an encoded machine-specific path.'
}
foreach ($needle in @('mUserNameEdit', 'mPasswordEdit', 'mLoginButton', 'authenticateUser')) {
    if ($loginText -notmatch [regex]::Escape($needle)) {
        throw "Login dialog missing expected marker: $needle"
    }
}

foreach ($needle in @('LTZKAccessControlDialog', 'addUserWithPassword', 'changeUserRole', 'deleteUserPermission', 'AccessRole::DatabaseAdmin')) {
    if ($accessText -notmatch [regex]::Escape($needle)) {
        throw "Access control dialog missing expected marker: $needle"
    }
}

foreach ($needle in @(
    'showStartupLogin',
    'ensureAuthenticatedForModule',
    'canAccessModule',
    'showAccessControlPage',
    'moduleButton_accessControl',
    'gCurrentUserSession.canManageUsers()',
    'gCurrentUserSession.canOperateProductStore()'
)) {
    if ($ltzkText -notmatch [regex]::Escape($needle)) {
        throw "LTZK integration missing expected marker: $needle"
    }
}

foreach ($needle in @(
    'auth\LTZKAuthManager.cpp',
    'auth\LTZKLoginDialog.cpp',
    'auth\LTZKAccessControlDialog.cpp',
    'auth\LTZKAuthManager.h',
    'auth\LTZKLoginDialog.h',
    'auth\LTZKAccessControlDialog.h',
    '$(ProjectDir)auth',
    'libpq.lib',
    'ws2_32.lib',
    'config\database.ini'
)) {
    if ($projectText -notmatch [regex]::Escape($needle)) {
        throw "Project file missing expected marker: $needle"
    }
}

foreach ($needle in @('CREATE DATABASE ltzk_platform', 'CREATE EXTENSION IF NOT EXISTS postgis', 'CREATE TABLE IF NOT EXISTS user_permissions', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92')) {
    if ($sqlText -notmatch [regex]::Escape($needle)) {
        throw "Setup SQL missing expected marker: $needle"
    }
}

foreach ($needle in @('config\database.ini', 'ltzk_platform', 'admin', '123456')) {
    if ($docText -notmatch [regex]::Escape($needle)) {
        throw "Setup documentation missing expected marker: $needle"
    }
}
foreach ($forbidden in @('E:/AAALTZK/config/database.ini', 'E:\AAALTZK')) {
    if (($authImpl + $authText + $docText) -match [regex]::Escape($forbidden)) {
        throw "Auth config path must not be hardcoded to source machine path: $forbidden"
    }
}

foreach ($needle in @('[db]', 'host=localhost', 'database=ltzk_platform', 'user=YOUR_PASSWORD')) {
    if ($dbIniText -notmatch [regex]::Escape($needle)) {
        throw "database.ini missing expected marker: $needle"
    }
}

Write-Host 'Login, permissions, and PostGIS integration verified.'


