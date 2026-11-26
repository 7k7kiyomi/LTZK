from pathlib import Path
root = Path(__file__).resolve().parent.parent
source = (root / "LTZK.cpp").read_text(encoding="utf-8")
auth = (root / "auth/LTZKAuthManager.cpp").read_text(encoding="utf-8")
catalog = (root / "DataCatalogWidget.cpp").read_text(encoding="utf-8")
login = source.split("void LTZK::authenticateFromHomePage()", 1)[1].split("bool LTZK::resetProductManagerPluginSession()", 1)[0]
assert "QThread::create" in login, "Database authentication must run off the GUI thread"
assert "closeCurrentThreadConnection" in login, "Worker database connection must be closed in its own thread"
assert "m_dataCatalogWidget->refreshDatabaseData()" not in login, "Login must not synchronously load the catalog"
assert "QThread::currentThreadId()" in auth, "Database connections must belong to their own thread"
assert "isVisible()" in catalog.split("m_menuDatabaseSettingsTimer->setInterval(500)", 1)[1].split("m_menuDatabaseSettingsTimer->start()", 1)[0], "Do not connect hidden catalogs on the home screen"
print("Login responsiveness checks passed.")
