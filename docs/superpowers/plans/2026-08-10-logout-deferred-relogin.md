# Logout Deferred Relogin Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an in-app logout entry that clears the current session without closing the program, then prompts for login only when the user next opens a protected module.

**Architecture:** Keep authentication state in the existing global `gCurrentUserSession`. Add main-window methods for deferred login and logout, then expose logout through home/workspace buttons. Extend static verification to lock the behavior.

**Tech Stack:** C++17, Qt 5.15 widgets, PowerShell static verification, MSBuild Debug|x64.

---

### Task 1: Verification Script

**Files:**
- Create: `tests/verify_logout_deferred_relogin.ps1`

- [ ] Write a static verification script that checks for `logoutCurrentUser`, `promptLoginForModule`, `gCurrentUserSession.clear()`, `refreshAuthUiState()`, `moduleButton_logout`, and `workspaceLogoutButton`.
- [ ] Run `.	ests\verify_logout_deferred_relogin.ps1`; expected failure before implementation is a missing marker such as `logoutCurrentUser`.

### Task 2: Main Window Auth Flow

**Files:**
- Modify: `LTZK.h`
- Modify: `LTZK.cpp`

- [ ] Declare `promptLoginForModule(const QString& moduleTitle)` and `logoutCurrentUser()` in `LTZK.h`.
- [ ] Change `ensureAuthenticatedForModule()` so logged-out users see the login-required message and then receive `LTZKLoginDialog`; successful login refreshes auth UI and continues the requested module action.
- [ ] Add `logoutCurrentUser()` to clear `gCurrentUserSession`, refresh auth UI, clear the active module title, hide plugin toolbar, and return to the home page.

### Task 3: Logout UI

**Files:**
- Modify: `LTZK.h`
- Modify: `LTZK.cpp`

- [ ] Store logout button pointers as members so `refreshAuthUiState()` can toggle visibility.
- [ ] Add a home page logout button with object name `moduleButton_logout`.
- [ ] Add a workspace top bar logout button with object name `workspaceLogoutButton`.
- [ ] Update `refreshAuthUiState()` to show logout buttons only when logged in.

### Task 4: Verification

**Files:**
- Run: `tests/verify_logout_deferred_relogin.ps1`
- Run: `tests/verify_login_permissions_postgis.ps1`
- Run: `LTZK.sln`

- [ ] Run `.	ests\verify_logout_deferred_relogin.ps1`; expected output: `Logout deferred relogin verified.`
- [ ] Run `.	ests\verify_login_permissions_postgis.ps1`; expected output: existing login/permissions verification passes.
- [ ] Run Debug|x64 MSBuild; expected result: successful build with 0 errors.