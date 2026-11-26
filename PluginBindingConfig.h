#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct PluginActionIcon
{
    bool enabled = true;
    QStringList actionKeywords;
    int fallbackActionIndex = -1;
    QString icon;
};

struct FeatureBinding
{
    bool enabled = true;
    bool showInMenu = true;
    bool watchResultOutputs = true;
    bool requiresAccount = false;
    QString resultOrganization;
    QStringList menuPath;
    QString featureName;
    QStringList actionKeywords;
    int fallbackActionIndex = -1;
    QString icon;
};

struct PluginDefinition
{
    bool enabled = true;
    bool watchResultOutputs = false;
    QString id;
    QString dll;
    QVector<PluginActionIcon> actionIcons;
    QVector<FeatureBinding> bindings;
};

struct PluginBindingConfig
{
    QVector<PluginDefinition> plugins;
};

bool loadPluginBindingConfig(const QString& configPath, PluginBindingConfig* config, QString* errorMessage = nullptr);
