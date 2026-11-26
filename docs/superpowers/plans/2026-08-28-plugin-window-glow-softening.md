# Plugin Window Glow Softening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce static plugin-window outer-frame glow while preserving a soft sci-fi halo and clear interaction-state feedback across all three platform themes.

**Architecture:** Keep the existing centralized `LTZK::applyPluginWindowGlowEffects` traversal and theme-specific color selection. Reduce the effect alpha/radii in `LTZK.cpp`, reduce static outer-border alpha in the three QSS theme files, and make the existing PowerShell verification reject the old strong values.

**Tech Stack:** C++/Qt, Qt Style Sheets, PowerShell verification scripts, Visual Studio solution.

---

### Task 1: Convert the glow verification to the softened contract

**Files:**
- Modify: `E:\AAALTZK\tests\verify_plugin_window_glow_edges_all_themes.ps1:8-76`
- Test: `E:\AAALTZK\tests\verify_plugin_window_glow_edges_all_themes.ps1`

- [ ] **Step 1: Replace the strong-glow assertions with exact softened assertions.**

The test must require these C++ values:

```powershell
return QColor(37, 99, 235, 80);
return QColor(245, 158, 11, 105);
return QColor(42, 212, 255, 125);
applyGlow(groupBox, m_currentTheme == PlatformTheme::White ? 15 : 20);
applyGlow(view, m_currentTheme == PlatformTheme::White ? 12 : 16);
applyGlow(frame, m_currentTheme == PlatformTheme::White ? 13 : 18);
```

The test must require the following static outer-border tokens in the matching theme file; the exact values are listed by theme below:

```powershell
'border: 1px solid rgba(132, 236, 255, 150);'
'border: 1px solid rgba(132, 236, 255, 140);'
'border: 1px solid rgba(245, 158, 11, 135);'
'border: 1px solid rgba(245, 158, 11, 120);'
'border: 1px solid rgba(37, 99, 235, 130);'
'border: 1px solid rgba(37, 99, 235, 115);'
```

Keep the existing checks for `QGroupBox`, `qlineargradient`, item views, `QTabWidget::pane`, `QScrollArea`, `rgba(`, the centralized function, theme switching, and `setOffset(0, 0)`. Add a negative assertion that fails if any old strong C++ value or radius remains.

- [ ] **Step 2: Run the modified verification before implementation.**

Run:

```powershell
.\tests\verify_plugin_window_glow_edges_all_themes.ps1
```

Expected: FAIL because `LTZK.cpp` and the QSS files still contain the old strong values. This is the TDD red check.

### Task 2: Soften centralized Qt glow effects

**Files:**
- Modify: `E:\AAALTZK\LTZK.cpp:5559-5633`
- Test: `E:\AAALTZK\tests\verify_plugin_window_glow_edges_all_themes.ps1`

- [ ] **Step 1: Lower theme glow alpha while keeping the existing RGB identity.**

Change `LTZK::pluginWindowGlowColor()` to:

```cpp
if (m_currentTheme == PlatformTheme::White)
{
    return QColor(37, 99, 235, 80);
}
if (m_currentTheme == PlatformTheme::Black)
{
    return QColor(245, 158, 11, 105);
}
return QColor(42, 212, 255, 125);
```

- [ ] **Step 2: Halve the static glow radius for the three existing target categories.**

Use these existing expressions:

```cpp
applyGlow(groupBox, m_currentTheme == PlatformTheme::White ? 15 : 20);
applyGlow(view, m_currentTheme == PlatformTheme::White ? 12 : 16);
applyGlow(frame, m_currentTheme == PlatformTheme::White ? 13 : 18);
```

Do not change target discovery, skip properties, theme-key caching, existing effects, offset, or interaction styles.

- [ ] **Step 3: Run the verification after the C++ change.**

Run:

```powershell
.\tests\verify_plugin_window_glow_edges_all_themes.ps1
```

Expected: FAIL only on the QSS softened-border assertions. This confirms the centralized C++ values are updated before changing the theme files.

### Task 3: Reduce static outer-border intensity in all themes

**Files:**
- Modify: `E:\AAALTZK\Resources\styles\ltzk_sci_fi.qss:8,21,47,108`
- Modify: `E:\AAALTZK\Resources\styles\ltzk_black.qss:8,20,41,97`
- Modify: `E:\AAALTZK\Resources\styles\ltzk_white.qss:8,20,41,97`
- Test: `E:\AAALTZK\tests\verify_plugin_window_glow_edges_all_themes.ps1`

- [ ] **Step 1: Apply the softened sci-fi border values.**

Change only the static outer-border alpha values in these selectors:

```qss
QGroupBox ... border: 1px solid rgba(116, 238, 255, 150);
QTableView, QTableWidget, QTreeView, QTreeWidget, QListView ... border: 1px solid rgba(132, 236, 255, 140);
QTabWidget::pane { border: 1px solid rgba(132, 236, 255, 140); ... }
QScrollArea { ... border: 1px solid rgba(132, 236, 255, 105); }
```

Preserve backgrounds, layout values, hover/pressed states, and all non-outer-control borders.

- [ ] **Step 2: Apply the softened black border values.**

Change the corresponding static outer-border alpha values to:

```qss
QGroupBox ... border: 1px solid rgba(245, 158, 11, 135);
QTableView, QTableWidget, QTreeView, QTreeWidget, QListView ... border: 1px solid rgba(245, 158, 11, 120);
QTabWidget::pane { border: 1px solid rgba(245, 158, 11, 120); ... }
QScrollArea { ... border: 1px solid rgba(245, 158, 11, 95); }
```

- [ ] **Step 3: Apply the softened white border values.**

Change the corresponding static outer-border alpha values to:

```qss
QGroupBox ... border: 1px solid rgba(37, 99, 235, 130);
QTableView, QTableWidget, QTreeView, QTreeWidget, QListView ... border: 1px solid rgba(37, 99, 235, 115);
QTabWidget::pane { border: 1px solid rgba(37, 99, 235, 115); ... }
QScrollArea { ... border: 1px solid rgba(37, 99, 235, 95); }
```

- [ ] **Step 4: Run the verification after all theme changes.**

Run:

```powershell
.\tests\verify_plugin_window_glow_edges_all_themes.ps1
```

Expected: `Plugin window glow edges verified for all platform themes.` and exit code 0.

### Task 4: Review the final change and build when available

**Files:**
- Review: `E:\AAALTZK\LTZK.cpp`
- Review: `E:\AAALTZK\Resources\styles\ltzk_sci_fi.qss`
- Review: `E:\AAALTZK\Resources\styles\ltzk_black.qss`
- Review: `E:\AAALTZK\Resources\styles\ltzk_white.qss`
- Review: `E:\AAALTZK\tests\verify_plugin_window_glow_edges_all_themes.ps1`

- [ ] **Step 1: Confirm the diff is limited to glow values and their verification contract.**

Run:

```powershell
git -C E:\AAALTZK diff -- LTZK.cpp Resources\styles\ltzk_sci_fi.qss Resources\styles\ltzk_black.qss Resources\styles\ltzk_white.qss tests\verify_plugin_window_glow_edges_all_themes.ps1
```

Expected: only glow color alpha, glow radius, static outer-border alpha, and test expectations differ. If Git metadata is unavailable, inspect the changed file contents and report that limitation.

- [ ] **Step 2: Run the focused verification again as the final fresh check.**

Run:

```powershell
.\tests\verify_plugin_window_glow_edges_all_themes.ps1
```

Expected: exit code 0 with the success message.

- [ ] **Step 3: Attempt the existing Release build only if its toolchain is available.**

Run:

```powershell
msbuild E:\AAALTZK\LTZK.sln /t:Build /p:Configuration=Release /m
```

Expected: exit code 0. If the environment lacks the configured Qt/QGIS or Visual Studio toolchain, report the exact build failure separately; the focused source/style verification remains required.

