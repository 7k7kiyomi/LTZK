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
$sourceText = Get-Content -Raw -Encoding ASCII $source

foreach ($needle in @(
    'bool promptLoginForModule(const QString& moduleTitle)',
    'void logoutCurrentUser()',
    'QToolButton* m_homeLogoutButton',
    'QToolButton* m_workspaceLogoutButton'
)) {
    if ($headerText -notmatch [regex]::Escape($needle)) {
        throw "Logout deferred relogin header marker missing: $needle"
    }
}

foreach ($needle in @(
    'LTZK::promptLoginForModule(const QString& moduleTitle)',
    'LTZK::logoutCurrentUser()',
    'gCurrentUserSession.clear()',
    'showLoginRequiredMessage()',
    'showHomeLoginPage();',
    'authenticateFromHomePage()',
    'LTZKAuthManager::instance().authenticateUser',
    'gCurrentUserSession = session;',
    'refreshAuthUiState()',
    'm_workspaceStack->setCurrentWidget(m_homePage)',
    'm_pluginToolBar->hide()',
    'moduleButton_logout',
    'workspaceLogoutButton',
    'm_homeLogoutButton->setVisible(gCurrentUserSession.isLoggedIn)',
    'm_workspaceLogoutButton->setVisible(gCurrentUserSession.isLoggedIn)',
    'connect(m_homeLogoutButton, &QToolButton::clicked, this, &LTZK::logoutCurrentUser)',
    'connect(m_workspaceLogoutButton, &QToolButton::clicked, this, &LTZK::logoutCurrentUser)'
)) {
    if ($sourceText -notmatch [regex]::Escape($needle)) {
        throw "Logout deferred relogin source marker missing: $needle"
    }
}

$ensurePattern = 'if \(!gCurrentUserSession\.isLoggedIn\)\s*\{\s*return promptLoginForModule\(moduleTitle\);\s*\}'
if ($sourceText -notmatch $ensurePattern) {
    throw 'ensureAuthenticatedForModule must defer to promptLoginForModule when logged out.'
}

Write-Host 'Logout deferred relogin verified.'
