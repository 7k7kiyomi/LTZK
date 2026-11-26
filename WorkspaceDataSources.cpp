#include "LTZK.h"
#include "WorkspaceDataSourceLoader.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStatusBar>
#include <qgsbrowserguimodel.h>
#include <qgscoordinatetransform.h>
#include <qgsdatasourcemanagerdialog.h>
#include <qgsgui.h>
#include <qgslayertree.h>
#include <qgslayertreegroup.h>
#include <qgslayertreelayer.h>
#include <qgsmapcanvas.h>
#include <qgsmessagebar.h>
#include <qgsmessagelog.h>
#include <qgsproject.h>
#include <qgsproviderregistry.h>
#include <qgssourceselectprovider.h>
#include <qgssourceselectproviderregistry.h>
#include <qgsvectorlayer.h>

namespace
{
    int sourceLayerType(const QString& type)
    {
        if (type == QStringLiteral("vector")) return static_cast<int>(QgsMapLayerType::VectorLayer);
        if (type == QStringLiteral("raster")) return static_cast<int>(QgsMapLayerType::RasterLayer);
        if (type == QStringLiteral("mesh")) return static_cast<int>(QgsMapLayerType::MeshLayer);
        if (type == QStringLiteral("pointcloud")) return static_cast<int>(QgsMapLayerType::PointCloudLayer);
        if (type == QStringLiteral("vector-tile")) return static_cast<int>(QgsMapLayerType::VectorTileLayer);
        return -1;
    }

    bool matchesSourceFilter(const QString& fileName, const QString& filter)
    {
        static const QRegularExpression pattern(QStringLiteral(R"(\*\.[^()\s;]+)"));
        auto matches = pattern.globalMatch(filter);
        while (matches.hasNext())
            if (QDir::match(matches.next().captured(), fileName)) return true;
        return false;
    }
}

bool LTZK::isSupportedDataSourceFile(const QString& filePath) const
{
    const auto* registry = QgsProviderRegistry::instance();
    const QString name = QFileInfo(filePath).fileName();
    return isVectorLayerFile(filePath) || isRasterLayerFile(filePath)
        || QFileInfo(filePath).suffix().compare(QStringLiteral("gdb"), Qt::CaseInsensitive) == 0
        || matchesSourceFilter(name, registry->fileVectorFilters())
        || matchesSourceFilter(name, registry->fileRasterFilters())
        || matchesSourceFilter(name, registry->fileMeshFilters())
        || matchesSourceFilter(name, registry->filePointCloudFilters());
}

void LTZK::on_actionDataSourceManager_triggered()
{
    openDataSourceManager();
}

void LTZK::openDataSourceManager(const QString& page)
{
    if (!m_dataSourceManager)
    {
        // Share connections and browser refreshes with the platform's data catalog.
        if (!m_sourceBrowserModel)
        {
            m_sourceBrowserModel = browserModel();
            if (!m_sourceBrowserModel)
            {
                m_sourceBrowserModel = new QgsBrowserGuiModel(this);
                m_sourceBrowserModel->initialize();
            }
        }
        auto* dialog = new QgsDataSourceManagerDialog(m_sourceBrowserModel, this, m_mapCanvas);
        m_dataSourceManager = dialog;
        // Closing hides this reusable window; QPointer also handles explicit deletion.
        dialog->setAttribute(Qt::WA_DeleteOnClose, false);
        dialog->setObjectName(QStringLiteral("workspaceDataSourceManager"));
        const auto vectorType = static_cast<int>(QgsMapLayerType::VectorLayer);
        const auto rasterType = static_cast<int>(QgsMapLayerType::RasterLayer);

        connect(dialog, &QgsDataSourceManagerDialog::addVectorLayers, this,
            [this, vectorType](const QStringList& sources, const QString& encoding, const QString& sourceType)
            {
                // sourceType controls the native source selector; its emitted OGR URIs
                // already contain file, directory, database or protocol information.
                Q_UNUSED(sourceType);
                loadDataSources(sources, QStringLiteral("ogr"), vectorType, QString(), encoding);
            });
        connect(dialog, &QgsDataSourceManagerDialog::addVectorLayer, this,
            [this, vectorType](const QString& uri, const QString& name, const QString& provider)
            { loadDataSources({uri}, provider, vectorType, name); });
        connect(dialog, &QgsDataSourceManagerDialog::addRasterLayers, this,
            [this, rasterType](const QStringList& sources)
            { loadDataSources(sources, QStringLiteral("gdal"), rasterType); });
        connect(dialog, &QgsDataSourceManagerDialog::addRasterLayer, this,
            [this, rasterType](const QString& uri, const QString& name, const QString& provider)
            { loadDataSources({uri}, provider.isEmpty() ? QStringLiteral("gdal") : provider, rasterType, name); });
        connect(dialog, &QgsDataSourceManagerDialog::addDatabaseLayers, this,
            [this, vectorType](const QStringList& sources, const QString& provider)
            { loadDataSources(sources, provider, vectorType); });
        connect(dialog, &QgsDataSourceManagerDialog::addMeshLayer, this,
            [this](const QString& uri, const QString& name, const QString& provider)
            { loadDataSources({uri}, provider, static_cast<int>(QgsMapLayerType::MeshLayer), name); });
        connect(dialog, &QgsDataSourceManagerDialog::addPointCloudLayer, this,
            [this](const QString& uri, const QString& name, const QString& provider)
            { loadDataSources({uri}, provider, static_cast<int>(QgsMapLayerType::PointCloudLayer), name); });
        connect(dialog, &QgsDataSourceManagerDialog::addVectorTileLayer, this,
            [this](const QString& uri, const QString& name)
            { loadDataSources({uri}, QStringLiteral("vectortile"), static_cast<int>(QgsMapLayerType::VectorTileLayer), name); });
        connect(dialog, &QgsDataSourceManagerDialog::replaceSelectedVectorLayer,
            this, &LTZK::replaceSourceVectorLayer);
        connect(dialog, &QgsDataSourceManagerDialog::handleDropUriList, this,
            [this](const QgsMimeDataUtils::UriList& uris)
            { for (const auto& uri : uris) loadCatalogDataSource(uri); });
        connect(dialog, &QgsDataSourceManagerDialog::openFile, this,
            [this](const QString& file, const QString& hint)
            {
                Q_UNUSED(hint);
                if (isProjectFile(file)) openProjectFile(file);
                else if (QFileInfo(file).isDir()) addLayerFiles(collectSupportedLayerFiles(file));
                else addLayerFiles({file});
            });
        connect(dialog, &QgsDataSourceManagerDialog::showStatusMessage, this,
            [this](const QString& message) { statusBar()->showMessage(message, 6000); });
        connect(dialog, &QgsDataSourceManagerDialog::connectionsChanged, this,
            [this]() { if (m_sourceBrowserModel) m_sourceBrowserModel->refresh(); });
        // QGIS already forwards providerDialogsRefreshRequested to its source
        // widgets. refresh() emits that signal, so connecting it back recurses.
        connect(dialog, &QgsDataSourceManagerDialog::updateProjectHome, this,
            [this]() { if (m_sourceBrowserModel) m_sourceBrowserModel->updateProjectHome(); });
    }
    if (!m_dataSourceManager->isVisible()) m_dataSourceManager->reset();
    if (!page.isEmpty())
    {
        // Despite the 3.28 header's "raster" example, openPage uses provider keys.
        QString providerPage = page == QStringLiteral("raster") ? QStringLiteral("gdal") : page;
        for (auto* provider : QgsGui::sourceSelectProviderRegistry()->providers())
            if (provider->providerKey().compare(providerPage, Qt::CaseInsensitive) == 0)
            { providerPage = provider->providerKey(); break; }
        m_dataSourceManager->openPage(providerPage);
    }
    m_dataSourceManager->show();
    m_dataSourceManager->activate();
}

void LTZK::reportDataSourceErrors(const QStringList& errors)
{
    if (errors.isEmpty()) return;
    const QString detail = errors.join(QLatin1Char('\n'));
    QgsMessageLog::logMessage(detail, QStringLiteral("数据源"), Qgis::Warning);
    if (m_dataSourceManager && m_dataSourceManager->isVisible())
        m_dataSourceManager->messageBar()->pushWarning(QStringLiteral("部分数据未加载"), detail);
    else if (m_messageBar)
        m_messageBar->pushWarning(QStringLiteral("数据加载失败"), detail);
    statusBar()->showMessage(detail, 10000);
}

QList<QgsMapLayer*> LTZK::loadDataSources(const QStringList& sources, const QString& providerKey,
    int layerType, const QString& name, const QString& encoding)
{
    QList<QgsMapLayer*> added;
    QStringList errors;
    for (const QString& source : sources)
    {
        if (source.trimmed().isEmpty()) continue;
        QWidget* dialogParent = m_dataSourceManager && m_dataSourceManager->isVisible()
            ? static_cast<QWidget*>(m_dataSourceManager.data()) : this;
        auto result = WorkspaceDataSourceLoader::load(source, providerKey, layerType, name,
            encoding, QgsProject::instance()->transformContext(), dialogParent);
        errors.append(result.errors);
        QList<QgsMapLayer*> layers;
        for (auto& layer : result.layers) layers.append(layer.release());
        registerSourceLayers(layers, result.groupName);
        added.append(layers);
    }
    reportDataSourceErrors(errors);
    if (!added.isEmpty() && errors.isEmpty())
        statusBar()->showMessage(QStringLiteral("已添加 %1 个图层").arg(added.size()), 5000);
    return added;
}

void LTZK::registerSourceLayers(const QList<QgsMapLayer*>& layers, const QString& groupName)
{
    if (layers.isEmpty()) return;
    QgsProject* project = QgsProject::instance();
    QgsLayerTreeGroup* group = groupName.isEmpty() ? nullptr
        : project->layerTreeRoot()->insertGroup(0, groupName);
    // The host already has its own canvas CRS and navigation state.
    if (m_layerTreeCanvasBridge) m_layerTreeCanvasBridge->setAutoSetupOnFirstLayer(false);
    project->addMapLayers(layers, !group);
    if (group)
        for (QgsMapLayer* layer : layers) group->addLayer(layer);

    m_curMapLayer = layers.first();
    if (m_layerTreeView) m_layerTreeView->setCurrentLayer(m_curMapLayer);
    if (!m_mapCanvas) return;
    m_mapCanvas->setCurrentLayer(m_curMapLayer);
    if (m_layerTreeCanvasBridge) m_layerTreeCanvasBridge->setCanvasLayers();

    QgsRectangle combined;
    bool hasExtent = false;
    for (QgsMapLayer* layer : layers)
    {
        if (!layer || !layer->isSpatial() || !layer->crs().isValid()) continue;
        if (auto* vector = qobject_cast<QgsVectorLayer*>(layer))
            if (vector->featureCount() == 0) continue;
        const QgsRectangle sourceExtent = layer->extent();
        if (sourceExtent.isNull() || !sourceExtent.isFinite()) continue;
        if (!m_mapCanvas->mapSettings().destinationCrs().isValid())
            m_mapCanvas->setDestinationCrs(layer->crs());
        try
        {
            QgsCoordinateTransform transform(layer->crs(), m_mapCanvas->mapSettings().destinationCrs(), project);
            const QgsRectangle extent = transform.transformBoundingBox(sourceExtent);
            if (!extent.isFinite()) continue;
            if (hasExtent) combined.combineExtentWith(extent);
            else { combined = extent; hasExtent = true; }
        }
        catch (const QgsCsException&)
        {
            reportDataSourceErrors({QStringLiteral("图层“%1”已加载，但无法转换范围至当前地图坐标系。").arg(layer->name())});
        }
    }
    if (hasExtent)
    {
        m_mapCanvas->zoomToFeatureExtent(combined);
        m_hasInitialMapView = false;
        rememberInitialMapView();
    }
    m_mapCanvas->refresh();
}

void LTZK::replaceSourceVectorLayer(const QString& oldId, const QString& uri,
    const QString& name, const QString& providerKey)
{
    auto* project = QgsProject::instance();
    auto* oldLayer = qobject_cast<QgsVectorLayer*>(project->mapLayer(oldId));
    if (!oldLayer)
    {
        reportDataSourceErrors({QStringLiteral("待替换的矢量图层已不存在，请重新选择。")});
        return;
    }
    if (oldLayer->isEditable() && oldLayer->isModified())
    {
        reportDataSourceErrors({QStringLiteral("请先保存或撤销“%1”的编辑，再替换数据源。").arg(oldLayer->name())});
        return;
    }
    // Prepare the replacement before touching the old layer or its tree position.
    auto result = WorkspaceDataSourceLoader::load(uri, providerKey,
        static_cast<int>(QgsMapLayerType::VectorLayer), name, QString(), project->transformContext(),
        m_dataSourceManager ? static_cast<QWidget*>(m_dataSourceManager.data()) : this);
    reportDataSourceErrors(result.errors);
    if (result.layers.size() != 1 || !result.errors.isEmpty()) return;
    QgsMapLayer* replacement = result.layers.front().get();
    const QString previousSource = oldLayer->source();
    const QString previousName = oldLayer->name();
    const QString previousProvider = oldLayer->providerType();
    QgsDataProvider::ProviderOptions options;
    options.transformContext = project->transformContext();

    // Keep the project layer object and ID so its styles, tree position, and
    // references survive the same way they do with QGIS' native replacement.
    oldLayer->setDataSource(replacement->source(), replacement->name(),
        replacement->providerType(), options, false);
    if (!oldLayer->isValid())
    {
        // The temporary layer was valid, but retain a recovery path if the
        // target layer cannot adopt its provider for any reason.
        oldLayer->setDataSource(previousSource, previousName, previousProvider, options, false);
        reportDataSourceErrors({QStringLiteral("替换矢量图层失败，已恢复原数据源：%1").arg(previousName)});
        return;
    }
    m_curMapLayer = oldLayer;
    if (m_layerTreeView) m_layerTreeView->setCurrentLayer(oldLayer);
    if (m_mapCanvas) { m_mapCanvas->setCurrentLayer(oldLayer); m_mapCanvas->refresh(); }
}

void LTZK::loadCatalogDataSource(const QgsMimeDataUtils::Uri& uri)
{
    if (uri.uri.isEmpty()) return;
    if (uri.layerType == QStringLiteral("project")) { openProjectFile(uri.uri); return; }
    if (uri.layerType == QStringLiteral("directory")) { addLayerFiles(collectSupportedLayerFiles(uri.uri)); return; }
    for (QgsMapLayer* layer : QgsProject::instance()->mapLayers())
    {
        if (layer && layer->providerType() == uri.providerKey && layer->source() == uri.uri)
        {
            m_curMapLayer = layer;
            if (m_layerTreeView) m_layerTreeView->setCurrentLayer(layer);
            if (m_mapCanvas) m_mapCanvas->setCurrentLayer(layer);
            statusBar()->showMessage(QStringLiteral("已切换至图层：%1").arg(layer->name()), 4000);
            return;
        }
    }
    const int type = sourceLayerType(uri.layerType);
    if (type < 0)
    {
        reportDataSourceErrors({QStringLiteral("此来源类型无法作为地图图层加载：%1").arg(uri.layerType)});
        return;
    }
    loadDataSources({uri.uri}, uri.providerKey, type, uri.name);
}
