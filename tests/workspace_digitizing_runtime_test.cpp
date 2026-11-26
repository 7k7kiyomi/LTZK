#include <QTimer>
#include <QUndoStack>
#include <qgsapplication.h>
#include <qgsgui.h>
#include <qgsmapcanvas.h>
#include <qgsmapmouseevent.h>
#include <qgsmaptooldigitizefeature.h>
#include <qgsadvanceddigitizingdockwidget.h>
#include <qgsattributedialog.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerutils.h>
#include <cstdio>
#include <memory>
#ifdef Q_OS_WIN
#include <Windows.h>
#endif

static int runCase(const QString& geometry, bool nullCad)
{
    const auto mode = geometry == "Point" ? QgsMapToolCapture::CapturePoint
        : geometry == "LineString" ? QgsMapToolCapture::CaptureLine
        : QgsMapToolCapture::CapturePolygon;
    QgsVectorLayer layer(geometry + "?crs=EPSG:4326&field=name:string",
        "digitizing runtime test", "memory");
    if (!layer.isValid() || !layer.startEditing()) return 2;
    QgsMapCanvas canvas;
    canvas.resize(600, 400);
    canvas.setDestinationCrs(layer.crs());
    canvas.setLayers({&layer});
    canvas.setCurrentLayer(&layer);
    canvas.setExtent(QgsRectangle(0, 0, 10, 10));
    canvas.show();
    QgsApplication::processEvents();
    std::unique_ptr<QgsAdvancedDigitizingDockWidget> cad;
    if (!nullCad)
    {
        cad.reset(new QgsAdvancedDigitizingDockWidget(&canvas));
        cad->hide();
    }
    std::printf("%s: constructing capture tool (CAD=%s)\n",
        geometry.toUtf8().constData(), nullCad ? "null" : "initialized");
    std::fflush(stdout);
    QgsMapToolDigitizeFeature tool(&canvas, cad.get(), mode);
    tool.setLayer(&layer);
    int completed = 0;
    bool formSaved = false;
    QObject::connect(&tool, &QgsMapToolDigitizeFeature::digitizingCompleted,
        &canvas, [&](const QgsFeature& captured)
        {
            ++completed;
            std::printf("%s: captured geometry, opening attribute form\n", geometry.toUtf8().constData());
            std::fflush(stdout);
            QgsFeature feature = QgsVectorLayerUtils::createFeature(&layer, captured.geometry());
            QgsAttributeEditorContext context;
            context.setMapCanvas(&canvas);
            QgsAttributeDialog form(&layer, &feature, false, &canvas, true, context);
            form.setMode(QgsAttributeEditorContext::AddFeatureMode);
            QTimer::singleShot(0, &form, &QgsAttributeDialog::accept);
            QTimer::singleShot(2000, &form, &QgsAttributeDialog::reject);
            formSaved = form.exec() == QDialog::Accepted;
        });
    canvas.setMapTool(&tool);
    const auto click = [&](QPoint position, Qt::MouseButton button)
    {
        QgsMapMouseEvent move(&canvas, QEvent::MouseMove, position);
        tool.canvasMoveEvent(&move);
        QgsMapMouseEvent press(&canvas, QEvent::MouseButtonPress, position, button, button);
        tool.canvasPressEvent(&press);
        QgsMapMouseEvent release(&canvas, QEvent::MouseButtonRelease, position, button);
        tool.canvasReleaseEvent(&release);
    };
    std::printf("%s: clicking first vertex\n", geometry.toUtf8().constData());
    std::fflush(stdout);
    click(QPoint(120, 120), Qt::LeftButton);
    if (mode != QgsMapToolCapture::CapturePoint)
    {
        click(QPoint(350, 120), Qt::LeftButton);
        if (mode == QgsMapToolCapture::CapturePolygon)
            click(QPoint(350, 280), Qt::LeftButton);
        click(QPoint(350, 280), Qt::RightButton);
    }
    if (completed != 1 || !formSaved || layer.featureCount() != 1) return 3;
    QgsFeature saved;
    if (!layer.getFeatures().nextFeature(saved) || saved.geometry().isEmpty()
        || saved.geometry().type() != layer.geometryType()) return 4;
    if (!layer.undoStack()->canUndo()) return 5;
    layer.undoStack()->undo();
    if (layer.featureCount() != 0) return 6;
    layer.undoStack()->redo();
    if (layer.featureCount() != 1) return 7;

    QgsFeature canceled = QgsVectorLayerUtils::createFeature(&layer, saved.geometry());
    QgsAttributeDialog cancelForm(&layer, &canceled, false, &canvas);
    cancelForm.setMode(QgsAttributeEditorContext::AddFeatureMode);
    QTimer::singleShot(0, &cancelForm, &QgsAttributeDialog::reject);
    if (cancelForm.exec() != QDialog::Rejected || layer.featureCount() != 1) return 8;
    canvas.unsetMapTool(&tool);
    canvas.setLayers({});
    layer.rollBack();
    std::printf("PASS %s capture, form acceptance/cancellation, undo and redo\n",
        geometry.toUtf8().constData());
    std::fflush(stdout);
    return 0;
}

int main(int argc, char** argv)
{
#ifdef Q_OS_WIN
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
#endif
    QgsApplication app(argc, argv, true);
    app.setOrganizationName("LTZKDigitizingTest");
    app.setApplicationName("LTZKDigitizingTest");
    QgsApplication::setPrefixPath("E:/extern/QGIS_32815", true);
    QgsApplication::setPluginPath("E:/extern/QGIS_32815/output/plugins");
    QgsApplication::initQgis();
    QgsGui::instance();
    const bool nullCad = app.arguments().contains("--null-cad");
    int result = 0;
    // --null-cad deliberately reproduces the pre-fix access violation.
    for (const QString& geometry : {QString("Point"), QString("LineString"),
        QString("Polygon"), QString("MultiPolygon")})
    {
        result = runCase(geometry, nullCad);
        if (result != 0) break;
    }
    QgsApplication::exitQgis();
    return result;
}
