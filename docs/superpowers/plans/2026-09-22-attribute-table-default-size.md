# Attribute Table Default Size Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the platform's “打开属性表” window open at a consistently usable size instead of relying on the small or variable Qt/QGIS default size.

**Architecture:** Keep the existing platform-side `LTZK::on_actionOpenAttrTable_triggered()` flow and add only window sizing before `show()`. The implementation will derive a large default size from the current screen's available geometry, cap it to the screen, and set a smaller minimum size; it will not force maximization or modify `map_product_manager_tools`.

**Tech Stack:** C++/Qt 5, QScreen, existing PowerShell static regression tests, MSBuild/Visual Studio Release x64.

---

### Task 1: Add the failing regression check

**Files:**
- Create: `tests/verify_attribute_table_default_size.ps1`

- [ ] **Step 1: Write the failing test**

The test extracts only `on_actionOpenAttrTable_triggered()` and requires the platform code to calculate available screen geometry, set a minimum size, and apply a default size before showing the view. It also rejects forced maximization because the requirement is a larger default, not mandatory maximization.

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
.\tests\verify_attribute_table_default_size.ps1
```

Expected: FAIL because the current slot creates and shows `QgsAttributeTableView` without any size configuration.

### Task 2: Set a bounded default and minimum size in the platform

**Files:**
- Modify: `LTZK.cpp:9671-9689`

- [ ] **Step 1: Add screen-aware sizing before `show()`**

Insert the following logic after `tv->setAttribute(Qt::WA_DeleteOnClose);`:

```cpp
    QScreen* attributeTableScreen = screen();
    if (!attributeTableScreen)
    {
        attributeTableScreen = QApplication::primaryScreen();
    }

    const QRect availableGeometry = attributeTableScreen
        ? attributeTableScreen->availableGeometry()
        : QRect();
    const int availableWidth = availableGeometry.width() > 0 ? availableGeometry.width() : 1000;
    const int availableHeight = availableGeometry.height() > 0 ? availableGeometry.height() : 650;
    const QSize attributeTableMinimumSize(
        qMin(availableWidth, 800),
        qMin(availableHeight, 500));
    const QSize attributeTableDefaultSize(
        qMin(availableWidth, qMax(800, qMin(1100, qRound(availableWidth * 0.78)))),
        qMin(availableHeight, qMax(500, qMin(700, qRound(availableHeight * 0.68)))));
    tv->setMinimumSize(attributeTableMinimumSize);
    tv->resize(attributeTableDefaultSize);
```

This gives normal screens an approximately `1000x650`-class window, keeps at least `800x500` where the screen permits it, and prevents the window from exceeding a small screen's available area.

- [ ] **Step 2: Run the focused regression test**

Run:

```powershell
.\tests\verify_attribute_table_default_size.ps1
```

Expected: PASS.

### Task 3: Verify the platform build and related attribute-table contracts

**Files:**
- Verify: `LTZK.cpp`, `tests/verify_attribute_table_actions.ps1`, `tests/verify_attribute_table_loads_layer.ps1`, `tests/verify_attribute_table_multiple_layers.ps1`, `tests/verify_attribute_table_default_size.ps1`

- [ ] **Step 1: Run all attribute-table static checks**

Run:

```powershell
.\tests\verify_attribute_table_actions.ps1
.\tests\verify_attribute_table_loads_layer.ps1
.\tests\verify_attribute_table_multiple_layers.ps1
.\tests\verify_attribute_table_default_size.ps1
```

Expected: all four scripts exit successfully.

- [ ] **Step 2: Build the platform Release x64 target**

Run the existing isolated-output MSBuild command:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' '.\LTZK.sln' /m /t:LTZK /p:Configuration=Release /p:Platform=x64 /p:IntDir='E:\AAALTZK\attribute_table_size_build\host\obj\' /p:OutDir='E:\AAALTZK\attribute_table_size_build\host\'
```

Expected: exit code `0` and a built `LTZK.exe` in `E:\AAALTZK\attribute_table_size_build\host\`.

- [ ] **Step 3: Review the final diff and confirm scope**

Confirm the only production source change is in `LTZK.cpp`, the new check is under `tests`, and no file under `map_product_manager_tools` changed.
