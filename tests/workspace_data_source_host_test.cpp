#include "../LTZK.h"
#include <QAction>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QPointer>
#include <QListWidget>
#include <QStackedWidget>
#include <QTimer>
#include <QTranslator>
#include <qgsapplication.h>
#include <qgsdatasourcemanagerdialog.h>
#include <qgseditorwidgetregistry.h>
#include <qgsgui.h>
#include <qgsmapcanvas.h>
#include <qgsmaprendererparalleljob.h>
#include <qgsrasterlayer.h>
#include <qgsmessagebar.h>
#include <qgssublayersdialog.h>
#include <qgslayertree.h>
#include <QTreeWidget>
#include <QCheckBox>
#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <qgsproject.h>
#include <qgssourceselectprovider.h>
#include <qgssourceselectproviderregistry.h>
#include <qgsvectorlayer.h>
#include <cstdio>

static bool makeGeoPdf(const QString& path)
{
    auto* mem = GetGDALDriverManager()->GetDriverByName("MEM")->Create("", 32, 32, 3, GDT_Byte, nullptr);
    if (!mem) return false;
    double transform[] = {100, .01, 0, 40, 0, -.01};
    mem->SetGeoTransform(transform);
    OGRSpatialReference srs;
    srs.importFromEPSG(4326);
    srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    mem->SetSpatialRef(&srs);
    mem->GetRasterBand(1)->Fill(30);
    mem->GetRasterBand(2)->Fill(90);
    mem->GetRasterBand(3)->Fill(180);
    auto* driver = GetGDALDriverManager()->GetDriverByName("PDF");
    auto* output = driver ? driver->CreateCopy(path.toUtf8().constData(), mem, FALSE, nullptr, nullptr, nullptr) : nullptr;
    GDALClose(mem);
    if (output) GDALClose(output);
    return output != nullptr;
}

static int runHostTest(QgsApplication& app)
{
    LTZK window;
    QAction* openSources = window.findChild<QAction*>("actionDataSourceManager");
    if (!openSources)
    {
        std::fprintf(stderr, "FAIL: platform has no native data source manager entry\n");
        return 1;
    }
    openSources->trigger();
    auto* dialog = window.findChild<QgsDataSourceManagerDialog*>();
    if (!dialog || !dialog->isVisible())
    {
        std::fprintf(stderr, "FAIL: entry did not open the QGIS source manager\n");
        return 2;
    }
    window.on_actionAddVectorData_triggered();
    app.processEvents();
    auto* pages = dialog->findChild<QStackedWidget*>("mOptionsStackedWidget");
    auto* optionsList = dialog->findChild<QListWidget*>("mOptionsListWidget");
    std::printf("vector page row=%d widget=%s\n", optionsList->currentRow(), pages->currentWidget()->metaObject()->className());
    if (!QByteArray(pages->currentWidget()->metaObject()->className()).contains("OgrSourceSelect")) return 14;
    if (window.findChildren<QgsDataSourceManagerDialog*>().size() != 1)
        return 3;
    window.on_actionAddRasterData_triggered();
    app.processEvents();
    std::printf("raster page row=%d widget=%s\n", optionsList->currentRow(), pages->currentWidget()->metaObject()->className());
    if (!QByteArray(pages->currentWidget()->metaObject()->className()).contains("GdalSourceSelect"))
    {
        std::fprintf(stderr, "FAIL: raster entry left manager on browser page\n");
        return 14;
    }
    if (window.findChildren<QgsDataSourceManagerDialog*>().size() != 1)
        return 4;
    // Native refresh emits providerDialogsRefreshRequested itself. A host must
    // never connect that signal back to refresh(), which recursively crashes.
    dialog->refresh();
    window.on_actionAddWmsLayer_triggered();
    app.processEvents();
    if (optionsList->currentItem()->data(Qt::UserRole).toString().compare("wms", Qt::CaseInsensitive) != 0) return 15;
    window.on_actionAddWfsLayer_triggered();
    app.processEvents();
    if (optionsList->currentItem()->data(Qt::UserRole).toString().compare("WFS", Qt::CaseInsensitive) != 0) return 16;
    window.on_actionAddWcsLayer_triggered();
    app.processEvents();
    if (optionsList->currentItem()->data(Qt::UserRole).toString().compare("wcs", Qt::CaseInsensitive) != 0) return 17;
    std::printf("PASS: refresh and service page routing\n");
    std::fflush(stdout);

    const QString path = QCoreApplication::applicationDirPath() + "/host_points.geojson";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return 5;
    file.write(R"({"type":"FeatureCollection","features":[{"type":"Feature","properties":{"label":"one"},"geometry":{"type":"Point","coordinates":[10,20]}},{"type":"Feature","properties":{"label":"two"},"geometry":{"type":"Point","coordinates":[11,21]}}]})");
    file.close();
    const int before = QgsProject::instance()->count();
    window.mapCanvas()->setDestinationCrs(QgsCoordinateReferenceSystem("EPSG:3857"));
    dialog->vectorLayersAdded({path}, "UTF-8", "file");
    if (QgsProject::instance()->count() != before + 1)
    {
        std::fprintf(stderr, "FAIL: native vector batch signal did not register a layer\n");
        return 6;
    }
    auto* layer = qobject_cast<QgsVectorLayer*>(window.layerTreeView()->currentLayer());
    if (!layer || layer->featureCount() != 2 || window.currentMapLayer() != layer)
        return 7;
    const QgsPointXY center = window.mapCanvas()->extent().center();
    if (center.x() < 1000000 || center.x() > 1300000 || center.y() < 2100000 || center.y() > 2600000)
    {
        std::fprintf(stderr, "FAIL: source extent was not transformed into canvas CRS (%f,%f)\n", center.x(), center.y());
        return 8;
    }

    // A provider URI must never be filtered as a nonexistent local file.
    dialog->vectorLayerAdded("Point?crs=EPSG:4326&field=name:string", "memory source", "memory");
    if (QgsProject::instance()->count() != before + 2)
        return 9;

    QPointer<QgsDataSourceManagerDialog> original(dialog);
    dialog->close();
    window.on_actionAddVectorData_triggered();
    if (window.findChild<QgsDataSourceManagerDialog*>() != original.data()) return 10;
    // QPointer ownership must also survive explicit destruction and recreation.
    delete original.data();
    if (original) return 11;
    window.on_actionAddRasterData_triggered();
    app.processEvents();
    dialog = window.findChild<QgsDataSourceManagerDialog*>();
    if (!dialog || !dialog->isVisible()) return 12;
    dialog->rasterLayerAdded("missing-input.tif", "invalid", "gdal");
    if (QgsProject::instance()->count() != before + 2) return 13;

    const QString pdf = QCoreApplication::applicationDirPath() + "/host_georeferenced.pdf";
    if (!makeGeoPdf(pdf)) return 18;
    dialog->rasterLayersAdded({pdf});
    auto* raster = qobject_cast<QgsRasterLayer*>(window.currentMapLayer());
    if (!raster || !raster->isValid() || raster->crs().authid() != "EPSG:4326"
        || QgsProject::instance()->count() != before + 3) return 19;
    const auto pdfCenter = window.mapCanvas()->extent().center();
    if (pdfCenter.x() < 11000000 || pdfCenter.x() > 11300000 || pdfCenter.y() < 4700000 || pdfCenter.y() > 5000000)
        return 20;
    QgsMapSettings settings;
    settings.setOutputSize(QSize(320, 320));
    settings.setDestinationCrs(raster->crs());
    settings.setLayers({raster});
    settings.setExtent(raster->extent());
    QgsMapRendererParallelJob render(settings);
    render.start();
    render.waitForFinished();
    const auto rendered = render.renderedImage();
    const QColor centerColor = rendered.pixelColor(160, 160);
    if (!render.errors().isEmpty() || qAbs(centerColor.red() - 30) > 5
        || qAbs(centerColor.green() - 90) > 5 || qAbs(centerColor.blue() - 180) > 5) return 21;
    rendered.save(QCoreApplication::applicationDirPath() + "/geopdf_render.png");

    // Replacement must retain the original layer on cancellation or failure.
    const QString originalId = layer->id();
    dialog->replaceSelectedVectorLayer(originalId, "missing.geojson", "bad replacement", "ogr");
    if (!QgsProject::instance()->mapLayer(originalId)) return 22;
    dialog->replaceSelectedVectorLayer(originalId, path, "replacement", "ogr");
    if (QgsProject::instance()->mapLayer(originalId) != layer || window.currentMapLayer() != layer
        || layer->name() != "replacement" || QgsProject::instance()->count() != before + 3) return 23;
    auto* replacement = layer;
    if (!replacement->startEditing() || !replacement->changeAttributeValue(0, 0, "unsaved")) return 24;
    const QString replacementId = replacement->id();
    dialog->replaceSelectedVectorLayer(replacementId, path, "must not replace edits", "ogr");
    if (QgsProject::instance()->mapLayer(replacementId) != replacement || !replacement->isModified()) return 25;
    replacement->rollBack();

    // Opening a QGIS project from the source browser must preserve the bridge
    // used by all subsequent imports.
    const QString projectFile = QCoreApplication::applicationDirPath() + "/host_sources.qgs";
    if (!QgsProject::instance()->write(projectFile)) return 26;
    dialog->openFile(projectFile);
    if (!window.layerTreeCanvasBridge()) return 27;
    dialog->vectorLayersAdded({path}, "UTF-8", "file");
    if (!window.mapCanvas()->layers().contains(window.currentMapLayer())) return 28;

    dialog->messageBar()->clearWidgets();
    dialog->resize(1200, 780);
    dialog->setProperty("ltzkUiScaleOverride", 1.0);
    window.setProperty("ltzkUiScaleOverride", 1.0);
    dialog->setFont(QFont("Microsoft YaHei", 10));
    window.on_actionAddVectorData_triggered();
    for (int i = 0; i < 8; ++i) app.processEvents();
    if (pages && dialog != original.data()) pages = dialog->findChild<QStackedWidget*>("mOptionsStackedWidget");
    std::printf("manager width=%d current source width=%d\n", dialog->width(), pages ? pages->currentWidget()->width() : -1);
    dialog->grab().save(QCoreApplication::applicationDirPath() + "/data_source_manager.png");
    dialog->close();
    std::printf("PASS: native entry, vector/raster/service pages, refresh, signal and URI loading, active layer, CRS extent, window reuse/recreation, invalid input, GeoPDF placement and rendered pixels, safe replacement\n");
    return 0;
}

int main(int argc, char** argv)
{
    QgsApplication app(argc, argv, true);
    app.setOrganizationName("LTZKDataSourceHostTest");
    app.setApplicationName("LTZKDataSourceHostTest");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    app.setFont(QFont("Microsoft YaHei", 10));
    app.setProperty("ltzkBaseApplicationFont", QVariant::fromValue(app.font()));
    QgsApplication::setPrefixPath("E:/extern/QGIS_32815", true);
    QgsApplication::setPluginPath("E:/extern/QGIS_32815/output/plugins");
    QgsApplication::initQgis();
    QgsGui::instance();
    QgsGui::editorWidgetRegistry()->initEditors();
    QTranslator translator;
    translator.load("E:/extern/QGIS_32815/output/i18n/qgis_zh-Hans.qm");
    app.installTranslator(&translator);
    const int result = runHostTest(app);
    if (result) std::fprintf(stderr, "FAIL: host integration case %d\n", result);
    QgsProject::instance()->clear();
    QgsApplication::exitQgis();
    return result;
}
