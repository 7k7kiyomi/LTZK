# Logout And Deferred Relogin Design

## Goal
Add an in-app logout entry so users can leave the current account without closing LTZK. After logout the application remains open in an unauthenticated state. The next protected module launch should prompt for login and continue the requested action only after a successful login.

## UI
Provide a logout entry on the home page and the workspace top bar. It should be visible only when a user is logged in. Existing admin-only permission management remains visible only for database administrators.

## Behavior
Logout clears `gCurrentUserSession`, refreshes auth-dependent UI, hides admin-only controls, clears the active business module, and returns to the home page. It does not open the login dialog immediately and does not close the program.

When a protected module is launched while logged out, show the existing login-required prompt, then open `LTZKLoginDialog`. If login succeeds, refresh auth UI and continue the original module launch. If login is cancelled, remain logged out and do not launch the module.

## Testing
Add a static verification script that checks for a logout action, session clearing, deferred login from `ensureAuthenticatedForModule`, auth UI refresh, and UI markers for home and workspace logout buttons.