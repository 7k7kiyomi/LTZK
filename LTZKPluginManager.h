#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "qgisplugin.h"
#include "product_catalog_plugin_api.h"

class LTZK;
class LTZKQgisInterface;
class QLibrary;

class LTZKPluginManager : public QObject
{


public:
    explicit LTZKPluginManager(LTZK *app, QObject *parent = nullptr);
    ~LTZKPluginManager() override;

    bool loadPlugin(const QString &pluginPath, QString *errorMessage = nullptr);
    bool invokeProductCatalogOperation(
        const QString &pluginPath,
        const LTZKProductCatalogOperationRequest &request,
        int *resultCode = nullptr,
        QString *errorMessage = nullptr);
    bool invokeProductCatalogAction(
        const QString &pluginPath,
        QString *errorMessage = nullptr);
    bool unloadPluginIfLoaded(const QString &pluginPath);
    void unloadAll();

private:
    struct LoadedPlugin
    {
        QString path;
        QString name;
        QLibrary *library = nullptr;
        QgisPlugin *plugin = nullptr;
        unload_t *unload = nullptr;
    };

    QString pluginMetadata(QLibrary *library, const char *symbolName) const;

    LTZKQgisInterface *mIface = nullptr;
    QVector<LoadedPlugin> mPlugins;
};
