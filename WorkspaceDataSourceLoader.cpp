#include "WorkspaceDataSourceLoader.h"
#include <qgsproviderregistry.h>
#include <qgsprovidermetadata.h>
#include <qgsprovidersublayerdetails.h>
#include <qgssublayersdialog.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsrasterlayer.h>
#include <qgsmeshlayer.h>
#include <qgspointcloudlayer.h>
#include <qgsvectortilelayer.h>
#include <qgswkbtypes.h>
#include <qgsdatasourceuri.h>
#include <QFileInfo>
#include <gdal_priv.h>
#include <cmath>
#include <limits>

namespace
{
QString displayName(const QString& uri, const QString& name)
{
    if (!name.isEmpty()) return name;
    const QString base = QFileInfo(QgsDataSourceUri::removePassword(uri)).completeBaseName();
    return base.isEmpty() ? QString::fromUtf8("新图层") : base;
}

std::unique_ptr<QgsMapLayer> createLayer(const QString& uri, const QString& provider,
    QgsMapLayerType type, const QString& name, const QgsCoordinateTransformContext& context)
{
    switch (type)
    {
    case QgsMapLayerType::VectorLayer:
    {
        QgsVectorLayer::LayerOptions options(context);
        options.skipCrsValidation = true;
        return std::make_unique<QgsVectorLayer>(uri, name, provider, options);
    }
    case QgsMapLayerType::RasterLayer:
    {
        QgsRasterLayer::LayerOptions options(true, context);
        options.skipCrsValidation = true;
        return std::make_unique<QgsRasterLayer>(uri, name, provider, options);
    }
    case QgsMapLayerType::MeshLayer:
        return std::make_unique<QgsMeshLayer>(uri, name, provider, QgsMeshLayer::LayerOptions(context));
    case QgsMapLayerType::PointCloudLayer:
        return std::make_unique<QgsPointCloudLayer>(uri, name, provider, QgsPointCloudLayer::LayerOptions(context));
    case QgsMapLayerType::VectorTileLayer:
        return std::make_unique<QgsVectorTileLayer>(uri, name, QgsVectorTileLayer::LayerOptions(context));
    default:
        return {};
    }
}

// Inspect the raw dataset before QGIS has a chance to validate or default its CRS.
// A valid CRS on a QgsMapLayer alone does not prove that a PDF is georeferenced.
bool pdfHasGeoreferencing(const QgsProviderSublayerDetails& detail, QString& error)
{
    if (detail.providerKey() != "gdal" && detail.providerKey() != "ogr") return true;
    const auto* metadata = QgsProviderRegistry::instance()->providerMetadata(detail.providerKey());
    const QVariantMap parts = metadata ? metadata->decodeUri(detail.uri()) : QVariantMap();
    QString path = parts.value("path").toString();
    if (path.isEmpty()) path = detail.uri();
    const bool pdfHint = detail.driverName().compare("PDF", Qt::CaseInsensitive) == 0 ||
        path.endsWith(".pdf", Qt::CaseInsensitive) || path.startsWith("PDF:", Qt::CaseInsensitive);
    if (!pdfHint) return true;

    char** options = nullptr;
    for (const auto& option : parts.value("openOptions").toStringList())
        options = CSLAddString(options, option.toUtf8().constData());
    CPLPushErrorHandler(CPLQuietErrorHandler);
    auto* dataset = static_cast<GDALDataset*>(GDALOpenEx(path.toUtf8().constData(),
        GDAL_OF_RASTER | GDAL_OF_READONLY, nullptr, options, nullptr));
    CPLPopErrorHandler();
    CSLDestroy(options);
    bool valid = false;
    if (dataset)
    {
        double transform[6] = {};
        valid = dataset->GetSpatialRef() && !dataset->GetSpatialRef()->IsEmpty() &&
            dataset->GetGeoTransform(transform) == CE_None;
        for (double value : transform) valid = valid && std::isfinite(value);
        valid = valid && std::abs(transform[1] * transform[5] - transform[2] * transform[4]) > 0;
        GDALClose(dataset);
    }
    if (!valid) error = QString::fromUtf8("PDF 缺少可验证的地理参考（坐标系或定位变换），未添加图层：%1")
        .arg(QgsDataSourceUri::removePassword(path));
    return valid;
}
}

WorkspaceDataSourceLoader::Result WorkspaceDataSourceLoader::load(
    const QString& uri, const QString& providerKey, int layerType, const QString& name, const QString& encoding,
    const QgsCoordinateTransformContext& context, QWidget* parent)
{
    Result result;
    if (uri.trimmed().isEmpty())
    {
        result.errors << QString::fromUtf8("数据源地址为空。");
        return result;
    }
    auto* registry = QgsProviderRegistry::instance();
    const auto* metadata = providerKey.isEmpty() ? nullptr : registry->providerMetadata(providerKey);
    if (!providerKey.isEmpty() && !metadata)
    {
        result.errors << QString::fromUtf8("数据提供者不可用：%1").arg(providerKey);
        return result;
    }
    QList<QgsProviderSublayerDetails> candidates;
    if (providerKey.isEmpty()) candidates = registry->querySublayers(uri);
    else if (metadata->capabilities().testFlag(QgsProviderMetadata::QuerySublayers))
        candidates = metadata->querySublayers(uri);
    else if (layerType >= 0)
    {
        QgsProviderSublayerDetails detail;
        detail.setProviderKey(providerKey);
        detail.setType(static_cast<QgsMapLayerType>(layerType));
        detail.setUri(uri);
        detail.setName(displayName(uri, name));
        candidates << detail;
    }
    for (int i = candidates.size() - 1; i >= 0; --i)
        if (layerType >= 0 && int(candidates[i].type()) != layerType) candidates.removeAt(i);
    if (candidates.isEmpty())
    {
        result.errors << QString::fromUtf8("无法读取数据源，或没有可用的子图层：%1（提供者：%2）")
            .arg(QgsDataSourceUri::removePassword(uri), providerKey.isEmpty() ? QString::fromUtf8("自动识别") : providerKey);
        return result;
    }

    QList<int> selected;
    if (candidates.size() > 1)
    {
        const auto dialogType = providerKey == "gdal" ? QgsSublayersDialog::Gdal :
            providerKey == "mdal" ? QgsSublayersDialog::Mdal : QgsSublayersDialog::Ogr;
        // This is the installed QGIS 3.28 native selector required by the host UI.
        // Keep its intentional compatibility warning local to this constructor.
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_DEPRECATED
        QgsSublayersDialog dialog(dialogType, QString::fromUtf8("选择子图层"), parent, {},
            QgsDataSourceUri::removePassword(uri));
        QT_WARNING_POP
        dialog.setShowAddToGroupCheckbox(true);
        QgsSublayersDialog::LayerDefinitionList definitions;
        for (int i = 0; i < candidates.size(); ++i)
        {
            const auto& candidate = candidates[i];
            QgsSublayersDialog::LayerDefinition definition;
            definition.layerId = i;
            definition.layerName = QgsDataSourceUri::removePassword(candidate.name());
            definition.count = candidate.featureCount() < 0 ? -1 :
                int(qMin<qlonglong>(candidate.featureCount(), std::numeric_limits<int>::max()));
            definition.type = candidate.type() == QgsMapLayerType::VectorLayer ?
                QString::fromUtf8("矢量 / %1").arg(QgsWkbTypes::displayString(candidate.wkbType())) :
                candidate.type() == QgsMapLayerType::RasterLayer ? QString::fromUtf8("栅格") : candidate.providerKey();
            definition.description = candidate.providerKey() + " / " + QgsDataSourceUri::removePassword(candidate.description());
            definitions << definition;
        }
        dialog.populateLayerTable(definitions);
        if (dialog.exec() != QDialog::Accepted)
        {
            result.canceled = true;
            return result;
        }
        for (const auto& definition : dialog.selection())
            if (definition.layerId >= 0 && definition.layerId < candidates.size() && !selected.contains(definition.layerId))
                selected << definition.layerId;
        if (dialog.addToGroupCheckbox() && !selected.isEmpty()) result.groupName = displayName(uri, name);
    }
    else selected << 0;

    for (int index : selected)
    {
        const auto& detail = candidates[index];
        QString error;
        if (!pdfHasGeoreferencing(detail, error))
        {
            if (!result.errors.contains(error)) result.errors << error;
            continue;
        }
        const QString layerName = candidates.size() == 1 && !name.isEmpty() ? name :
            displayName(detail.uri(), detail.name());
        auto layer = createLayer(detail.uri(), detail.providerKey(), detail.type(), layerName, context);
        if (!layer || !layer->isValid())
        {
            result.errors << QString::fromUtf8("图层加载失败：%1（提供者：%2；地址：%3）")
                .arg(QgsDataSourceUri::removePassword(layerName), detail.providerKey(),
                    QgsDataSourceUri::removePassword(detail.uri()));
            continue;
        }
        if (!encoding.isEmpty())
            if (auto* vector = qobject_cast<QgsVectorLayer*>(layer.get()))
                if (vector->dataProvider()) vector->dataProvider()->setEncoding(encoding);
        result.layers.push_back(std::move(layer));
    }
    return result;
}
