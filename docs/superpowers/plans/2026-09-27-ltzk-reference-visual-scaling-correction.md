# LTZK Reference Visual Scaling Correction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the platform render the external-monitor reference screenshots proportionally on the laptop's 2880×1800/200% display and other logical resolutions without changing business-plugin code.

**Architecture:** Treat the final external-monitor appearance as the visual reference, convert its measured geometry into one platform metric set, calculate one logical screen scale per window, and let Qt perform the physical-DPI mapping. Keep the home canvas full-width while keeping cards, login panel, fonts, icons, utility buttons, and workspace chrome uniformly scaled. Remove permanent workspace scrollbar/hover height reservations.

**Tech Stack:** C++17, Qt Widgets/Gui, QGIS 3.28, PowerShell source-contract tests, MSBuild Release x64.

---

## Scope

- Modify only platform files: `LTZK.cpp`, `LTZK.h`, `LTZKUiScale.h`, platform QSS, and platform tests.
- Do not modify `map_product_manager`, `map_product_manager_tools`, or their source/assets.
- Preserve existing login/authentication, module dispatch, map canvas, and plugin action behavior.

## Tasks

### Task 1: Add red regression contracts for the measured reference metrics

**Files:**
- Create: `tests/verify_reference_visual_metrics.ps1`

- [x] Assert the source contains the measured final visual metrics: home login panel `540×390`, module card `462×234`, utility button `172×54`, utility font `24`, grid spacing `80/74`, utility gap `45`.
- [x] Assert the home refresh does not use page contents height to derive the global scale or force a fixed canvas narrower than the full-width shell.
- [x] Assert icon rendering normalizes source DPR before centering and uses a label slot at least as tall as the rendered icon.
- [x] Assert workspace top-bar layout does not add permanent scrollbar reserve or hover overflow to the action-row height.
- [x] Assert no files under the business-plugin directories are changed.
- [x] Run the script and observe failure against the current source before production edits.

### Task 2: Centralize final reference metrics and home scaling

**Files:**
- Modify: `LTZK.cpp`
- Modify: `LTZKUiScale.h` only if a helper is required

- [x] Replace stale login and utility dimensions with the measured reference values.
- [x] Use one screen/logical scale for platform metrics; do not multiply layout values by `devicePixelRatio`.
- [x] Keep the full-width home background/header/footer in the full-width home canvas.
- [x] Prevent the message-log dock from changing the global home scale; use the same reference scale when the dock is toggled.
- [x] Reapply all home metrics idempotently from reference values after screen/resize/theme changes.

### Task 3: Correct home card icon rendering and utility controls

**Files:**
- Modify: `LTZK.cpp`
- Modify: `Resources/styles/ltzk_field_notes.qss` and matching platform QSS only if the reference border/padding requires it

- [x] Keep cards fixed at the scaled reference width and height.
- [x] Set icon label geometry before assigning the pixmap.
- [x] Normalize the source DPR before centering into the logical target pixmap.
- [x] Set utility buttons to the measured width, height, font, and spacing; prevent QSS padding from changing their outer geometry.

### Task 4: Rebuild workspace top-bar height ownership

**Files:**
- Modify: `LTZK.cpp`
- Modify: platform QSS only when required by the top-bar minimum-height rule

- [x] Define one scaled action-row height from the reference button height.
- [x] Remove permanent scrollbar-height and hover-overflow additions from the scroll-area height.
- [x] Keep shadows visually clipped/painted without reserving layout space.
- [x] Align title and action row to the same top-bar center line and keep title horizontal placement independent.
- [x] Show the horizontal scrollbar only when plugin actions overflow.

### Task 5: Verify and build

**Files:**
- Verify changed platform files and tests.

- [x] Run the new regression script and focused scaling/UI contracts.
- [x] Confirm no business-plugin scaling references were added.
- [x] Build `Release|x64` with MSBuild.
- [ ] Final visual acceptance still requires screenshots on the laptop and, when available, the external monitor using identical log-dock state.
