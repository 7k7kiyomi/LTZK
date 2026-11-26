#include "../WorkspaceDataSourceLoader.h"
#include <qgsapplication.h>
#include <qgsgui.h>
#include <qgsvectorlayer.h>
#include <qgssublayersdialog.h>
#include <QCheckBox>
#include <QTreeWidget>
#include <QTimer>
#include <QTemporaryDir>
#include <QFile>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <cstdio>
#include <cmath>

namespace
{
int failures = 0;
void check(bool condition, const char* message)
{
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
    std::fflush(stdout);
    if (!condition) ++failures;
}
struct DialogDriver
{
    QTimer timer;
    int seen = 0;
    int candidates = 0;
    bool cancel;
    bool all;
    explicit DialogDriver(bool cancel = false, bool all = true) : cancel(cancel), all(all)
    {
        QObject::connect(&timer, &QTimer::timeout, [this]()
        {
            auto* dialog = qobject_cast<QgsSublayersDialog*>(QApplication::activeModalWidget());
            if (!dialog) return;
            ++seen;
            auto* table = dialog->findChild<QTreeWidget*>("layersTable");
            candidates = table ? table->topLevelItemCount() : 0;
            if (this->cancel) dialog->reject();
            else
            {
                if (table)
                {
                    table->clearSelection();
                    if (this->all) table->selectAll();
                    else if (table->topLevelItemCount()) table->topLevelItem(0)->setSelected(true);
                }
                if (auto* group = dialog->findChild<QCheckBox*>("mCbxAddToGroup")) group->setChecked(this->all);
                dialog->accept();
            }
        });
        timer.start(25);
    }
};
using Result = WorkspaceDataSourceLoader::Result;
Result load(const QString& uri, const QString& provider, int type, const QString& name = {})
{
    return WorkspaceDataSourceLoader::load(uri, provider, type, name, "UTF-8", {}, nullptr);
}
bool makeVectors(const QString& path)
{
    auto* driver = GetGDALDriverManager()->GetDriverByName("GPKG");
    auto* ds = driver ? driver->Create(path.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr) : nullptr;
    if (!ds) return false;
    OGRSpatialReference srs;
    srs.importFromEPSG(4326);
    srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    for (int i = 0; i < 2; ++i)
    {
        auto* layer = ds->CreateLayer(i ? "towns" : "stations", &srs, wkbPoint);
        if (!layer) { GDALClose(ds); return false; }
        OGRFieldDefn field("label", OFTString);
        layer->CreateField(&field);
        auto* feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
        feature->SetField("label", i ? "乙站" : "甲站");
        OGRPoint point(100.05 + i * .02, 39.95);
        feature->SetGeometry(&point);
        const bool ok = layer->CreateFeature(feature) == OGRERR_NONE;
        OGRFeature::DestroyFeature(feature);
        if (!ok) { GDALClose(ds); return false; }
    }
    GDALClose(ds);
    return true;
}
bool makeRaster(const QString& path, const char* format, bool georef, const QString& vectors = {})
{
    auto* mem = GetGDALDriverManager()->GetDriverByName("MEM")->Create("", 16, 16, 3, GDT_Byte, nullptr);
    if (georef)
    {
        double transform[] = {100, .01, 0, 40, 0, -.01};
        mem->SetGeoTransform(transform);
        OGRSpatialReference srs;
        srs.importFromEPSG(4326);
        srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        mem->SetSpatialRef(&srs);
    }
    for (int i = 1; i <= 3; ++i) mem->GetRasterBand(i)->Fill(i * 60);
    char** options = nullptr;
    if (format == QByteArray("PDF")) options = CSLSetNameValue(options, "DPI", "72");
    if (!vectors.isEmpty())
    {
        options = CSLSetNameValue(options, "OGR_DATASOURCE", vectors.toUtf8().constData());
        options = CSLSetNameValue(options, "OGR_WRITE_ATTRIBUTES", "YES");
    }
    auto* driver = GetGDALDriverManager()->GetDriverByName(format);
    auto* output = driver ? driver->CreateCopy(path.toUtf8().constData(), mem, FALSE, options, nullptr, nullptr) : nullptr;
    CSLDestroy(options);
    GDALClose(mem);
    if (output) GDALClose(output);
    return output != nullptr;
}
bool makeMultipagePdf(const QString& path, const QString& raster)
{
    QString xml = "<PDFComposition>";
    for (int page = 0; page < 2; ++page)
    {
        const int x = 100 + page * 10;
        xml += QString("<Page><DPI>72</DPI><Width>16</Width><Height>16</Height>"
            "<Georeferencing id=\"geo%1\"><SRS dataAxisToSRSAxisMapping=\"2,1\">EPSG:4326</SRS>"
            "<BoundingBox x1=\"0\" y1=\"0\" x2=\"16\" y2=\"16\"/>"
            "<ControlPoint x=\"0\" y=\"0\" GeoX=\"%2\" GeoY=\"39.84\"/>"
            "<ControlPoint x=\"0\" y=\"16\" GeoX=\"%2\" GeoY=\"40\"/>"
            "<ControlPoint x=\"16\" y=\"0\" GeoX=\"%3\" GeoY=\"39.84\"/>"
            "<ControlPoint x=\"16\" y=\"16\" GeoX=\"%3\" GeoY=\"40\"/>"
            "</Georeferencing><Content><Raster dataset=\"%4\" x1=\"0\" y1=\"0\" x2=\"16\" y2=\"16\"/>"
            "</Content></Page>").arg(page).arg(x).arg(x + .16, 0, 'f', 2).arg(raster.toHtmlEscaped());
    }
    xml += "</PDFComposition>";
    const QString composition = path + ".xml";
    QFile file(composition);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(xml.toUtf8());
    file.close();
    char** options = CSLSetNameValue(nullptr, "COMPOSITION_FILE", composition.toUtf8().constData());
    auto* dataset = GetGDALDriverManager()->GetDriverByName("PDF")->Create(
        path.toUtf8().constData(), 0, 0, 0, GDT_Unknown, options);
    CSLDestroy(options);
    if (dataset) GDALClose(dataset);
    return dataset != nullptr;
}
}

int main(int argc, char** argv)
{
    QgsApplication app(argc, argv, true);
    QgsApplication::setPrefixPath("E:/extern/QGIS_32815", true);
    QgsApplication::setPluginPath("E:/extern/QGIS_32815/plugins");
    QgsApplication::initQgis();
    QgsGui::instance();
    GDALAllRegister();
    // Fixtures are disposable: suppress PAM sidecars for PDF:page:path URIs,
    // which cannot form valid Windows sidecar filenames.
    CPLSetConfigOption("GDAL_PAM_ENABLED", "NO");
    QTemporaryDir temp;
    check(temp.isValid(), "temporary fixture directory");
    const QString gpkg = temp.filePath(QString::fromUtf8("中文测试.gpkg"));
    const QString tif = temp.filePath("reference.tif");
    const QString pdf = temp.filePath("georeferenced.pdf");
    const QString plain = temp.filePath("unreferenced.pdf");
    const QString multipage = temp.filePath("multipage.pdf");
    check(makeVectors(gpkg), "create actual multi-layer GeoPackage");
    check(makeRaster(tif, "GTiff", true), "create reference GeoTIFF");
    check(makeRaster(pdf, "PDF", true, gpkg), "create raster and vector GeoPDF");
    auto* pdfVectors = static_cast<GDALDataset*>(GDALOpenEx(pdf.toUtf8().constData(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
    check(pdfVectors && pdfVectors->GetLayerCount() >= 2, "fixture has two GDAL-readable PDF vector tables");
    if (pdfVectors) GDALClose(pdfVectors);
    check(makeRaster(plain, "PDF", false), "create unreferenced PDF");
    check(makeMultipagePdf(multipage, tif), "create two-page georeferenced PDF");
    {
        auto result = load(tif, "gdal", int(QgsMapLayerType::RasterLayer));
        check(result.layers.size() == 1 && result.errors.isEmpty(), "load actual GeoTIFF");
        if (!result.layers.empty())
        {
            check(result.layers[0]->crs().authid() == "EPSG:4326", "preserve raster CRS");
            check(std::abs(result.layers[0]->extent().xMinimum() - 100) < .001, "preserve raster extent");
        }
    }
    {
        DialogDriver driver;
        auto result = load(gpkg, "ogr", int(QgsMapLayerType::VectorLayer));
        check(driver.seen == 1 && driver.candidates == 2, "show native selector for two GPKG tables");
        check(result.layers.size() == 2 && result.errors.isEmpty(), "load all selected GPKG tables");
        check(!result.groupName.isEmpty(), "honor native add-to-group checkbox");
        if (result.layers.size() == 2)
        {
            auto* vector = qobject_cast<QgsVectorLayer*>(result.layers[0].get());
            check(vector && vector->featureCount() == 1 && vector->fields().indexOf("label") >= 0,
                "retain vector geometry and attribute schema");
        }
    }
    {
        DialogDriver driver(false, false);
        auto result = load(gpkg, "ogr", int(QgsMapLayerType::VectorLayer));
        check(result.layers.size() == 1 && result.groupName.isEmpty(), "load only selected table without grouping");
    }
    {
        DialogDriver driver(true);
        auto result = load(gpkg, "ogr", int(QgsMapLayerType::VectorLayer));
        check(driver.seen == 1 && result.canceled && result.layers.empty() && result.errors.isEmpty(),
            "cancel native selector without importing layers");
    }
    {
        const QString uri = gpkg + "|layername=stations|option:LIST_ALL_TABLES=YES";
        auto result = load(uri, "ogr", int(QgsMapLayerType::VectorLayer), "chosen name");
        check(result.layers.size() == 1 && result.errors.isEmpty(), "load provider URI with table and open options");
        if (!result.layers.empty()) check(result.layers[0]->name() == "chosen name" &&
            result.layers[0]->source().contains("option:LIST_ALL_TABLES=YES"), "preserve name and OGR open option");
    }
    {
        auto result = load("Point?crs=EPSG:4326&field=name:string", "memory", int(QgsMapLayerType::VectorLayer));
        check(result.layers.size() == 1 && result.errors.isEmpty(), "accept non-file provider URI via direct fallback");
    }
    {
        auto result = load(pdf, "gdal", int(QgsMapLayerType::RasterLayer));
        check(result.layers.size() == 1 && result.errors.isEmpty(), "load GeoPDF raster");
        if (!result.layers.empty()) check(result.layers[0]->crs().authid() == "EPSG:4326" &&
            std::abs(result.layers[0]->extent().xMinimum() - 100) < .001, "GeoPDF overlays reference location");
    }
    {
        DialogDriver driver;
        auto result = load(pdf, "ogr", int(QgsMapLayerType::VectorLayer));
        check(result.layers.size() >= 2 && result.errors.isEmpty(), "load readable vector GeoPDF tables");
    }
    {
        DialogDriver driver;
        auto result = load(pdf, {}, -1);
        bool raster = false, vector = false;
        for (const auto& layer : result.layers)
        {
            raster |= layer->type() == QgsMapLayerType::RasterLayer;
            vector |= layer->type() == QgsMapLayerType::VectorLayer;
        }
        check(driver.seen == 1 && raster && vector, "auto GeoPDF selector exposes raster and vector candidates");
    }
    {
        DialogDriver driver;
        auto result = load(multipage, "gdal", int(QgsMapLayerType::RasterLayer));
        check(driver.seen == 1 && driver.candidates == 2 && result.layers.size() == 2 && result.errors.isEmpty(),
            "query and load both actual PDF page subdatasets");
        if (result.layers.size() == 2)
        {
            const double a = result.layers[0]->extent().xMinimum();
            const double b = result.layers[1]->extent().xMinimum();
            check(std::abs(std::abs(a - b) - 10) < .001,
                "PDF page provider URIs preserve distinct georeferencing");
        }
    }
    {
        auto result = load(plain, "gdal", int(QgsMapLayerType::RasterLayer));
        check(result.layers.empty() && !result.errors.isEmpty() && result.errors.join(" ").contains(QString::fromUtf8("地理参考")),
            "reject ordinary PDF lacking raw georeferencing");
    }
    {
        auto result = load(plain, "ogr", int(QgsMapLayerType::VectorLayer));
        check(result.layers.empty() && !result.errors.isEmpty(), "empty PDF vector query does not fall back to false layer");
        result = load(temp.filePath("broken.pdf"), "gdal", int(QgsMapLayerType::RasterLayer));
        check(result.layers.empty() && !result.errors.isEmpty(), "report broken data source");
    }
    {
        const QString uri = "dbname='missing_fixture' password='runtimeSecretValue' table='missing'";
        auto result = load(uri, "ogr", int(QgsMapLayerType::VectorLayer));
        check(result.layers.empty() && !result.errors.isEmpty() &&
            !result.errors.join(" ").contains("runtimeSecretValue") && result.errors.join(" ").contains("missing_fixture"),
            "failed provider URI diagnostics remove passwords while retaining useful source context");
    }
    std::printf("%d failures\n", failures);
    return failures ? 1 : 0;
}
