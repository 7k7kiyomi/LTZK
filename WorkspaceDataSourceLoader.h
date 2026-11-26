#pragma once
#include <memory>
#include <vector>
#include <QStringList>
#include <qgsmaplayer.h>
#include <qgscoordinatetransformcontext.h>

class QWidget;
namespace WorkspaceDataSourceLoader
{
struct Result
{
    // Unregistered layers stay owned here until the host transfers them to QgsProject.
    std::vector<std::unique_ptr<QgsMapLayer>> layers;
    QStringList errors;
    QString groupName;
    bool canceled = false;
};
// layerType is a QgsMapLayerType value, or -1 to offer every readable type.
// providerKey may be empty for automatic detection; URIs are never treated as
// ordinary file paths. The native selector is modal only for multiple candidates.
Result load(const QString& uri, const QString& providerKey, int layerType,
    const QString& name, const QString& encoding,
    const QgsCoordinateTransformContext& context, QWidget* parent);
}
