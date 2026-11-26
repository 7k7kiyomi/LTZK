from pathlib import Path
source = (Path(__file__).resolve().parent.parent / "LTZK.cpp").read_text(encoding="utf-8")
trace = source.split("void startupTrace(", 1)[1].split("QString localGansuLabelField", 1)[0]
assert "LTZK_STARTUP_TRACE" in trace, "Startup tracing must be opt-in to avoid synchronous disk writes during module entry"
activation = source.split("void LTZK::activateBusinessModule(", 1)[1].split("void LTZK::scheduleQualityResultPostActionScan()", 1)[0]
basemap = activation.split("if (!isMapServiceModule)", 1)[1].split("if (m_activeModuleTitle)", 1)[0]
assert basemap.count("loadLocalGansuShpBasemap()") == 1
assert "refreshMapCanvasLayerStack()" not in basemap and "setGansuInitialMapView()" not in basemap, "Basemap initialization already updates the canvas and view"
print("Module entry performance checks passed.")
