#include "LTZKPluginManager.h"

#include "LTZKQgisInterface.h"

#include <QLibrary>
#include <QFileInfo>
#include <QMetaObject>

LTZKPluginManager::LTZKPluginManager(LTZK *app, QObject *parent)
    : QObject(parent)
    , mIface(new LTZKQgisInterface(app))
{
}

LTZKPluginManager::~LTZKPluginManager()
{
    unloadAll();
    delete mIface;
    mIface = nullptr;
}

bool LTZKPluginManager::loadPlugin(const QString &pluginPath, QString *errorMessage)
{
    const QString canonicalPath = QFileInfo(pluginPath).canonicalFilePath();
    if (canonicalPath.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Plugin file does not exist: %1").arg(pluginPath);
        }
        return false;
    }

    for (const LoadedPlugin &loaded : std::as_const(mPlugins))
    {
        if (loaded.path.compare(canonicalPath, Qt::CaseInsensitive) == 0)
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("Plugin is already loaded: %1").arg(canonicalPath);
            }
            return false;
        }
    }

    QLibrary *library = new QLibrary(canonicalPath);
    // Qt may retain SVG icon engines whose strings refer to plugin resources
    // after plugin actions are destroyed. Keep their backing module mapped.
    QLibrary::LoadHints loadHints = QLibrary::ResolveAllSymbolsHint;
    if (QFileInfo(canonicalPath).completeBaseName().compare(
            QStringLiteral("map_product_manager_tools"), Qt::CaseInsensitive) == 0)
    {
        loadHints |= QLibrary::PreventUnloadHint;
    }
    library->setLoadHints(loadHints);
    if (!library->load())
    {
        if (errorMessage)
        {
            *errorMessage = library->errorString();
        }
        delete library;
        return false;
    }

    auto classFactory = reinterpret_cast<create_t *>(library->resolve("classFactory"));
    auto unloadPlugin = reinterpret_cast<unload_t *>(library->resolve("unload"));
    if (!classFactory || !unloadPlugin)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("This is not a standard QGIS C++ plugin DLL. Missing classFactory or unload export.");
        }
        library->unload();
        delete library;
        return false;
    }

    QgisPlugin *plugin = classFactory(mIface);
    if (!plugin)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("classFactory failed to create the plugin instance.");
        }
        library->unload();
        delete library;
        return false;
    }

    plugin->initGui();

    LoadedPlugin loaded;
    loaded.path = canonicalPath;
    loaded.name = pluginMetadata(library, "name");
    if (loaded.name.isEmpty())
    {
        loaded.name = plugin->name();
    }
    if (loaded.name.isEmpty())
    {
        loaded.name = QFileInfo(canonicalPath).baseName();
    }
    loaded.library = library;
    loaded.plugin = plugin;
    loaded.unload = unloadPlugin;
    mPlugins.push_back(loaded);
    return true;
}

bool LTZKPluginManager::invokeProductCatalogOperation(
    const QString &pluginPath,
    const LTZKProductCatalogOperationRequest &request,
    int *resultCode,
    QString *errorMessage)
{
    const QString canonicalPath = QFileInfo(pluginPath).canonicalFilePath();
    if (canonicalPath.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Plugin file does not exist: %1").arg(pluginPath);
        }
        return false;
    }

    for (const LoadedPlugin &loaded : std::as_const(mPlugins))
    {
        if (loaded.path.compare(canonicalPath, Qt::CaseInsensitive) != 0 || !loaded.library)
        {
            continue;
        }

        const auto operationApi = reinterpret_cast<LTZKProductCatalogOpenOperationFn>(
            loaded.library->resolve("ltzk_product_catalog_open_operation"));
        if (!operationApi)
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral(
                    "Plugin does not export ltzk_product_catalog_open_operation: %1")
                    .arg(loaded.library->errorString());
            }
            return false;
        }

        const int result = operationApi(&request);
        if (resultCode)
        {
            *resultCode = result;
        }
        return true;
    }

    if (errorMessage)
    {
        *errorMessage = QStringLiteral("Plugin is not loaded: %1").arg(canonicalPath);
    }
    return false;
}

bool LTZKPluginManager::invokeProductCatalogAction(
    const QString &pluginPath,
    QString *errorMessage)
{
    const QString canonicalPath = QFileInfo(pluginPath).canonicalFilePath();
    if (canonicalPath.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Plugin file does not exist: %1").arg(pluginPath);
        }
        return false;
    }

    for (const LoadedPlugin &loaded : std::as_const(mPlugins))
    {
        if (loaded.path.compare(canonicalPath, Qt::CaseInsensitive) != 0 || !loaded.plugin)
        {
            continue;
        }

        QObject *pluginObject = dynamic_cast<QObject *>(loaded.plugin);
        if (!pluginObject)
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral(
                    "Loaded product catalog plugin is not a QObject: %1")
                    .arg(canonicalPath);
            }
            return false;
        }

        if (!QMetaObject::invokeMethod(
                pluginObject,
                "DataCatalogOperation",
                Qt::DirectConnection))
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral(
                    "Product catalog action slot is unavailable: DataCatalogOperation");
            }
            return false;
        }
        return true;
    }

    if (errorMessage)
    {
        *errorMessage = QStringLiteral("Plugin is not loaded: %1").arg(canonicalPath);
    }
    return false;
}

bool LTZKPluginManager::unloadPluginIfLoaded(const QString &pluginPath)
{
    const QString canonicalPath = QFileInfo(pluginPath).canonicalFilePath();
    if (canonicalPath.isEmpty())
    {
        return false;
    }

    for (int index = 0; index < mPlugins.size(); ++index)
    {
        LoadedPlugin &loaded = mPlugins[index];
        if (loaded.path.compare(canonicalPath, Qt::CaseInsensitive) != 0)
        {
            continue;
        }

        if (loaded.plugin)
        {
            loaded.plugin->unload();
            if (loaded.unload)
            {
                loaded.unload(loaded.plugin);
            }
            loaded.plugin = nullptr;
        }

        if (loaded.library)
        {
            loaded.library->unload();
            delete loaded.library;
            loaded.library = nullptr;
        }

        mPlugins.remove(index);
        return true;
    }

    return false;
}

void LTZKPluginManager::unloadAll()
{
    for (LoadedPlugin &loaded : mPlugins)
    {
        if (loaded.plugin)
        {
            loaded.plugin->unload();
            if (loaded.unload)
            {
                loaded.unload(loaded.plugin);
            }
            loaded.plugin = nullptr;
        }

        if (loaded.library)
        {
            loaded.library->unload();
            delete loaded.library;
            loaded.library = nullptr;
        }
    }

    mPlugins.clear();
}

QString LTZKPluginManager::pluginMetadata(QLibrary *library, const char *symbolName) const
{
    if (!library)
    {
        return QString();
    }

    auto metadata = reinterpret_cast<name_t *>(library->resolve(symbolName));
    if (!metadata)
    {
        return QString();
    }

    const QString *value = metadata();
    return value ? *value : QString();
}
