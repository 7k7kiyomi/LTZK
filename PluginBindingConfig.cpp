#include "PluginBindingConfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace
{
QStringList readStringArray(const QJsonObject& object, const QString& key)
{
    QStringList values;
    const QJsonValue value = object.value(key);
    if (value.isArray())
    {
        const QJsonArray array = value.toArray();
        for (const QJsonValue& item : array)
        {
            if (item.isString())
            {
                values.append(item.toString());
            }
        }
    }
    else if (value.isString())
    {
        values.append(value.toString());
    }
    return values;
}

PluginActionIcon readActionIcon(const QJsonObject& object)
{
    PluginActionIcon icon;
    icon.enabled = object.value(QStringLiteral("enabled")).toBool(true);
    icon.actionKeywords = readStringArray(object, QStringLiteral("actionKeywords"));
    icon.fallbackActionIndex = object.value(QStringLiteral("fallbackActionIndex")).toInt(-1);
    icon.icon = object.value(QStringLiteral("icon")).toString();
    return icon;
}

FeatureBinding readFeatureBinding(const QJsonObject& object)
{
    FeatureBinding binding;
    binding.enabled = object.value(QStringLiteral("enabled")).toBool(true);
    binding.showInMenu = object.value(QStringLiteral("showInMenu")).toBool(true);
    binding.watchResultOutputs = object.value(QStringLiteral("watchResultOutputs")).toBool(true);
    binding.requiresAccount = object.value(QStringLiteral("requiresAccount")).toBool(false);
    binding.resultOrganization = object.value(QStringLiteral("resultOrganization")).toString().trimmed();
    binding.menuPath = readStringArray(object, QStringLiteral("menuPath"));
    binding.featureName = object.value(QStringLiteral("featureName")).toString();
    binding.actionKeywords = readStringArray(object, QStringLiteral("actionKeywords"));
    binding.fallbackActionIndex = object.value(QStringLiteral("fallbackActionIndex")).toInt(-1);
    binding.icon = object.value(QStringLiteral("icon")).toString();
    return binding;
}
}

bool loadPluginBindingConfig(const QString& configPath, PluginBindingConfig* config, QString* errorMessage)
{
    if (!config)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Plugin binding config output is null.");
        }
        return false;
    }

    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Plugin binding config cannot be opened: %1").arg(configPath);
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Plugin binding config is invalid JSON: %1").arg(parseError.errorString());
        }
        return false;
    }

    PluginBindingConfig parsed;
    const QJsonArray plugins = document.object().value(QStringLiteral("plugins")).toArray();
    for (const QJsonValue& pluginValue : plugins)
    {
        if (!pluginValue.isObject())
        {
            continue;
        }

        const QJsonObject pluginObject = pluginValue.toObject();
        PluginDefinition plugin;
        plugin.enabled = pluginObject.value(QStringLiteral("enabled")).toBool(true);
        plugin.watchResultOutputs = pluginObject.value(QStringLiteral("watchResultOutputs")).toBool(false);
        plugin.id = pluginObject.value(QStringLiteral("id")).toString();
        plugin.dll = pluginObject.value(QStringLiteral("dll")).toString();

        const QJsonArray actionIcons = pluginObject.value(QStringLiteral("actionIcons")).toArray();
        for (const QJsonValue& iconValue : actionIcons)
        {
            if (iconValue.isObject())
            {
                plugin.actionIcons.append(readActionIcon(iconValue.toObject()));
            }
        }

        const QJsonArray bindings = pluginObject.value(QStringLiteral("bindings")).toArray();
        for (const QJsonValue& bindingValue : bindings)
        {
            if (bindingValue.isObject())
            {
                FeatureBinding binding = readFeatureBinding(bindingValue.toObject());
                // Product management persists to or queries PostGIS. The
                // quality data downloader also uses a remote service.
                binding.requiresAccount = binding.requiresAccount
                    || plugin.id == QStringLiteral("map_product_manager_tools")
                    || binding.featureName == QStringLiteral("地图数据下载");
                plugin.bindings.append(binding);
            }
        }

        if (!plugin.id.isEmpty() && !plugin.dll.isEmpty())
        {
            parsed.plugins.append(plugin);
        }
    }

    *config = parsed;
    return true;
}
