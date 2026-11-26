from pathlib import Path

root = Path(__file__).resolve().parent.parent
source = (root / "LTZK.cpp").read_text(encoding="utf-8")
manager = (root / "LTZKPluginManager.cpp").read_text(encoding="utf-8")
workspace = (root / "MapServiceWorkspace.h").read_text(encoding="utf-8")
assert "QLibrary::PreventUnloadHint" in manager, "Plugin code and icon resources must remain mapped after session reset"
assert "Qt::WA_DontShowOnScreen" in source and "Qt::WA_DontShowOnScreen" in workspace, "Suppress the temporary top-level service dialog and restore display on embedding"
assert "setWorkspacePluginActions(isMapServiceModule ? QList<QAction*>() : workspacePluginActions)" in source, "Service workspace must hide the redundant feature button"
print("Map service lifecycle checks passed.")
