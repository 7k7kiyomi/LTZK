#include <qgsapplication.h>
#include <qgsmapcanvas.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsgeometry.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QElapsedTimer>
#include <QThread>
#include <cstdio>
#if __has_include("../WorkspaceLayerStyleRefresh.h")
#include "../WorkspaceLayerStyleRefresh.h"
#endif
static void process(int ms) { QElapsedTimer t; t.start(); while (t.elapsed() < ms) { QCoreApplication::processEvents(); QThread::msleep(5); } }
int main(int argc, char **argv) {
 QgsApplication app(argc, argv, true); QgsApplication::setPrefixPath("E:/extern/QGIS_32815", true); QgsApplication::initQgis();
 QgsVectorLayer layer("Polygon?crs=EPSG:4326", "test", "memory"); if (!layer.isValid()) return 2;
 QgsFeature f(layer.fields()); f.setGeometry(QgsGeometry::fromWkt("POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))")); layer.dataProvider()->addFeatures(QgsFeatureList() << f); layer.updateExtents();
 auto *renderer = static_cast<QgsSingleSymbolRenderer *>(layer.renderer()); renderer->symbol()->setColor(Qt::red);
 QgsMapCanvas canvas; canvas.resize(240,240); canvas.setCachingEnabled(true); canvas.setLayers({&layer}); canvas.setExtent(QgsRectangle(-1,-1,11,11)); canvas.show(); canvas.refresh(); process(1500);
 const QColor before = canvas.grab().toImage().pixelColor(120,120);
 QDialog dialog; QDialogButtonBox buttons(QDialogButtonBox::Apply | QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog); buttons.setObjectName("buttonBox");
 QObject::connect(&buttons, &QDialogButtonBox::clicked, &dialog, [&](QAbstractButton *button) { if (buttons.standardButton(button) == QDialogButtonBox::Apply) renderer->symbol()->setColor(Qt::blue); else if (buttons.standardButton(button) == QDialogButtonBox::Ok) renderer->symbol()->setColor(Qt::green); });
#if __has_include("../WorkspaceLayerStyleRefresh.h")
 connectLayerStyleRefresh(&dialog, &canvas);
#endif
 buttons.button(QDialogButtonBox::Apply)->click(); process(1500);
 const QColor after = canvas.grab().toImage().pixelColor(120,120);
 const bool pass = before.red() > before.blue() && after.blue() > after.red();
 std::printf("%s cached canvas updates on Apply: before %s after %s\n", pass ? "PASS" : "FAIL", qPrintable(before.name()), qPrintable(after.name()));
 buttons.button(QDialogButtonBox::Ok)->click(); process(1500);
 const QColor ok = canvas.grab().toImage().pixelColor(120,120);
 const bool okPass = ok.green() > ok.blue() && ok.green() > ok.red();
 std::printf("%s cached canvas updates on OK: %s\n", okPass ? "PASS" : "FAIL", qPrintable(ok.name()));
 int refreshes = 0; QObject::connect(&canvas, &QgsMapCanvas::mapCanvasRefreshed, &canvas, [&]() { ++refreshes; });
 buttons.button(QDialogButtonBox::Cancel)->click(); process(500);
 const bool cancelPass = refreshes == 0;
 std::printf("%s Cancel does not redraw\n", cancelPass ? "PASS" : "FAIL");
 return pass && okPass && cancelPass ? 0 : 1;
}
