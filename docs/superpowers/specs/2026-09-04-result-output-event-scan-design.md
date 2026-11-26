# Result Output Event Scan Design

## Goal

Keep automatic SHP result loading for every configured plugin feature that produces SHP output, including format conversion, merge, clipping, edge matching, map generalization, and quality checks, without recursively scanning while the feature dialog is merely open.

## Confirmed behavior

- A plugin-level `watchResultOutputs` flag means that the plugin may expose SHP-producing features. It must not cause periodic scans by itself.
- The only accepted result root is the current feature window's semantic output-directory field and its actual text value selected by the user.
- Input paths, loaded layer directories, project directories, other top-level windows, and guessed fallback directories are never result roots.
- Opening a feature records its output-directory control and arms an operating-system directory watcher. Arming is not scanning.
- Recursive SHP discovery runs only after the selected output directory reports a real filesystem change. Multiple rapid changes are debounced into one scan.
- The result board continues to load SHP only. Other output formats are ignored.
- Leaving the owning business module or closing/replacing the monitored feature window removes stale watcher state.

## Architecture

`executePluginFeatureAction` establishes a pending result-monitoring context before triggering the plugin action so modal and non-modal dialogs can both be observed. The application event filter associates only a plugin window that contains a semantically identified output-directory line edit. Text changes in that exact control update the stored selected directory and the `QFileSystemWatcher` path.

`QFileSystemWatcher::directoryChanged` starts a single-shot debounce timer. Its timeout performs the existing bounded recursive SHP discovery and result-layer loading. The former unconditional two-second repeating scan is removed.

## Verification

Static regression tests verify event-driven wiring, absence of pre-action/immediate/repeating scans, and scoping to the actual selected output control. The Release target must compile with zero errors, focused result-monitor tests must pass, and the resulting executable/config must be synchronized to the actual `E:\LTZK` runtime.
