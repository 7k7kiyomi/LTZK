# LTZK Developer Package Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** From the current LTZK workspace, produce a clean, shareable developer package with source, an optional current runtime, documentation, and a verified ZIP archive, without shipping local build artifacts or database credentials.

**Architecture:** Keep the existing workspace untouched as the source of truth and build a dated staging tree under `dist`. `01_Source` contains the Visual Studio/Qt application and plugin source plus tests/resources; `02_Runtime` contains the latest available Release executable and plugin DLLs; `03_Docs` contains setup and handoff guidance. A reusable PowerShell packager defines the inclusion/exclusion rules and creates a ZIP only after the staging tree passes validation.

**Tech Stack:** PowerShell, Windows x64, Visual Studio 2019/MSVC v142, Qt 5, QGIS 3.28.15/OSGeo4W, 7-Zip.

---

### Task 1: Add the reproducible package builder

**Files:**
- Create: `tools/build_developer_package.ps1`
- Create: `docs/superpowers/plans/2026-09-28-ltzk-developer-package.md`

- [x] **Step 1: Define the package boundary.**

The script must fail if the dated staging directory already exists, copy only the explicitly selected application source files and source directories, omit build/cache/history/test-data paths, copy only DLLs into the runtime plugin directory, and write a sanitized `config/database.ini` with an empty password.

- [x] **Step 2: Build the staging tree.**

Create `dist/LTZK_Developer_Package_20260928/{01_Source,02_Runtime/LTZK,03_Docs}`; copy the current `x64/Release/LTZK.exe`, current `plugins/*.dll`, current resource/config files, source launch files, project sources, tests, tools, plugin source, SQL/templates, and handoff documentation into their corresponding locations.

- [x] **Step 3: Write package metadata.**

Write `PACKAGE_MANIFEST.md` at the package root with the snapshot date, included components, excluded material, build prerequisites, machine-specific path caveat, database credential caveat, and the fact that the quality/generalization plugin source is not present in this workspace.

### Task 2: Generate the package from the current workspace

**Files:**
- Create: `dist/LTZK_Developer_Package_20260928/**`
- Create: `dist/LTZK_Developer_Package_20260928.zip`

- [x] **Step 1: Run the packager.**

Run `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build_developer_package.ps1` from `E:\AAALTZK` and stop on any copy or validation error.

- [x] **Step 2: Create the archive.**

Run `D:\7za.exe a -tzip -mx=5 .\dist\LTZK_Developer_Package_20260928.zip .\dist\LTZK_Developer_Package_20260928` so the archive keeps its package-root directory, and keep the staging directory for inspection.

### Task 3: Verify the deliverable

**Files:**
- Inspect: `dist/LTZK_Developer_Package_20260928/**`
- Inspect: `dist/LTZK_Developer_Package_20260928.zip`

- [x] **Step 1: Verify required files and forbidden paths.**

Check that the solution/project files, current executable, three configured plugin DLLs, launch scripts, resources, configuration, plugin source, tests, and docs exist; assert that no `node_modules`, `GeneratedFiles`, `x64`, `package_build`, `.vs`, `__pycache__`, `.bak`, old archive, log, or local data path exists in the package.

- [x] **Step 2: Verify sanitization and archive integrity.**

Search the staging tree for the original `password=YOUR_PASSWORD` value and local absolute build paths where they must not be shipped; confirm the package config has `password=`; run `D:\7za.exe t .\dist\LTZK_Developer_Package_20260928.zip` and record the file count and exit code.

- [x] **Step 3: Report scope honestly.**

Report the package paths and verification evidence, and explicitly state that compilation/runtime GUI validation was not performed unless a fresh build or launch command produces success output.
