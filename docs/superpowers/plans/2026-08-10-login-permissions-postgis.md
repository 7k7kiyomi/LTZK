# Login Permissions PostGIS Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a database-backed login page, role-based module access, administrator-only permission management, and PostGIS/user_permissions setup guidance.

**Architecture:** Reuse the map product manager permission model and database patterns locally in this LTZK application. A small authentication layer owns PostGIS connection settings, schema initialization, admin bootstrap, password hashing, user authentication, and user permission CRUD; LTZK gates module entry and plugin feature execution through the current session.

**Tech Stack:** C++/Qt 5, Visual Studio `.vcxproj`, Qt Widgets, Qt SQL/libpq/PostGIS, PowerShell static verification scripts.

---

### Task 1: Permission Behavior Verification

**Files:**
- Create: `tests/verify_login_permissions_postgis.ps1`

- [ ] **Step 1: Write the failing test**

Create a PowerShell test that checks the codebase contains a database-backed login path, role constants, module gates, admin-only permission management, PostGIS initialization SQL, and Visual Studio project entries for new files and `libpq.lib`.

- [ ] **Step 2: Run test to verify it fails**

Run: `.\tests\verify_login_permissions_postgis.ps1`

Expected: FAIL because the new authentication files and LTZK integration do not exist yet.

### Task 2: Authentication Core

**Files:**
- Create: `auth/LTZKAuthManager.h`
- Create: `auth/LTZKAuthManager.cpp`

- [ ] **Step 1: Implement role/session structures**

Define `AccessRole`, `UserPermission`, `UserSession`, and helpers using the three Chinese role names: `数据操作员`, `数据使用者`, `数据库管理员`.

- [ ] **Step 2: Implement database connection from QSettings**

Use `config\database.ini` under the development output or LTZK installation directory; packaged `bin\LTZK.exe` reads `..\config\database.ini`, with keys `db/host`, `db/port`, `db/database`, `db/schema`, `db/user`, `db/password`, and defaults `localhost`, `5432`, `ltzk_platform`, `public`, `YOUR_PASSWORD`, empty password.

- [ ] **Step 3: Implement schema bootstrap**

On successful connection, run `CREATE EXTENSION IF NOT EXISTS postgis`, create `user_permissions`, migrate `password_hash`, and seed `admin/123456` as `数据库管理员`.

- [ ] **Step 4: Implement user authentication and permission CRUD**

Authenticate by `SELECT * FROM user_permissions WHERE user_name = ? AND password_hash = ?`. Add, delete, update password, update role, update permissions, and list users for the permission management dialog.

### Task 3: Login and Permission Dialogs

**Files:**
- Create: `auth/LTZKLoginDialog.h`
- Create: `auth/LTZKLoginDialog.cpp`
- Create: `auth/LTZKAccessControlDialog.h`
- Create: `auth/LTZKAccessControlDialog.cpp`

- [ ] **Step 1: Implement login dialog**

Build a modal dialog with username, password, and login button only. On submit, ensure database connection/schema/admin first, then authenticate against `user_permissions`.

- [ ] **Step 2: Implement access control dialog**

Build an administrator-only user table and edit form reusing the same permissions behavior from `E:\map_product_manager_tools`: add user with initial password, delete user, update role, optional password reset, and optional JSON permissions.

### Task 4: LTZK Integration

**Files:**
- Modify: `LTZK.h`
- Modify: `LTZK.cpp`
- Modify: `LTZK.vcxproj`

- [ ] **Step 1: Add LTZK auth methods and state**

Add startup login, login-required warning, access-denied warning, module permission checks, permission-management launcher, and UI refresh hooks.

- [ ] **Step 2: Gate module entry**

Require login for all module buttons. Allow `地图成果管理` only for `数据操作员` and `数据库管理员`. Allow `权限管理` only for `数据库管理员`.

- [ ] **Step 3: Gate plugin feature execution**

Check login and role before triggering plugin actions, so hidden menus and toolbar actions cannot bypass module permissions.

- [ ] **Step 4: Register new files in the project**

Add auth `.h/.cpp` files to the Visual Studio project, include the `auth` directory, and add `libpq.lib`/`ws2_32.lib` to linker dependencies.

### Task 5: Database Setup Documentation

**Files:**
- Create: `database/postgis_user_permissions_setup.sql`
- Create: `docs/postgis_login_permissions_setup.md`

- [ ] **Step 1: Add SQL setup script**

Provide SQL to create the database externally, enable PostGIS, create `user_permissions`, and seed `admin/123456`.

- [ ] **Step 2: Add operator instructions**

Explain how to create the database, how LTZK connects through QSettings, and how to change the database connection values.

### Task 6: Verification

**Files:**
- Run: `tests/verify_login_permissions_postgis.ps1`
- Run: `msbuild LTZK.sln /p:Configuration=Release /p:Platform=x64`

- [ ] **Step 1: Run static verification**

Expected: PASS with a message confirming login/permissions/PostGIS integration.

- [ ] **Step 2: Run build**

Expected: Visual Studio build exits with code 0.

