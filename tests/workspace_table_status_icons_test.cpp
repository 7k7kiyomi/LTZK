#include "../LTZK.h"
// Exercise the actual internal icon resolver and tree builder without exposing
// implementation helpers in the production public API. Exclude its host object
// when linking this test translation unit.
#include "../DataCatalogWidget.cpp"
#include <QCryptographicHash>
#include <QDialog>
#include <QEvent>
#include <QFontDatabase>
#include <QLineEdit>
#include <QMessageBox>
#include <QSqlError>
#include <QStatusBar>
#include <qgsgui.h>
#include <qgseditorwidgetregistry.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsprojectionselectiondialog.h>
#include <QTemporaryDir>
#include <qgsscalewidget.h>
#include <qgsvectorlayer.h>
#include <cstdio>

template<typename Tag, typename Tag::type Member> struct MemberAccess
{ friend typename Tag::type member(Tag) { return Member; } };
struct TreeLoader
{
    using type = bool(DataCatalogWidget::*)(
        QStandardItem*,
        int,
        QSqlDatabase,
        const QSet<QString>&,
        const QList<ProductRecord>&);
    friend type member(TreeLoader);
};
template struct MemberAccess<TreeLoader, &DataCatalogWidget::appendProductDirectoryChildren>;

static int failures = 0;
static void check(bool passed, const char* message)
{
    std::printf("%s: %s\n", passed ? "PASS" : "FAIL", message);
    std::fflush(stdout);
    if (!passed) ++failures;
}
static QByteArray pixels(const QIcon& icon)
{
    const QImage image = icon.pixmap(24, 24).toImage().convertToFormat(QImage::Format_ARGB32);
    return QCryptographicHash::hash(QByteArray(reinterpret_cast<const char*>(image.constBits()),
        image.sizeInBytes()), QCryptographicHash::Sha256);
}
static QStandardItem* findItemNamed(QStandardItem* parent, const QString& name)
{
    if (!parent) return nullptr;
    if (parent->text() == name) return parent;
    for (int row = 0; row < parent->rowCount(); ++row)
    {
        if (QStandardItem* found = findItemNamed(parent->child(row), name)) return found;
    }
    return nullptr;
}
static void flushDeletes()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

static void testTables(LTZK& window)
{
    auto* firstLayer = new QgsVectorLayer("Point?crs=EPSG:4326&field=name:string", "first", "memory");
    auto* secondLayer = new QgsVectorLayer("Polygon?crs=EPSG:4326", "second", "memory");
    QgsProject::instance()->addMapLayers({firstLayer, secondLayer});
    QPointer<QDialog> first = window.openAttributeTable(firstLayer);
    auto* expression = first->findChild<QLineEdit*>("attributeTableExpressionFilter");
    expression->setText("name IS NOT NULL");
    check(window.openAttributeTable(firstLayer) == first, "same layer reuses dialog");
    check(expression->text() == "name IS NOT NULL", "reopen preserves filter text");
    first->showMinimized();
    check(window.openAttributeTable(firstLayer) == first && !first->isMinimized(), "reopen restores minimized dialog");
    QPointer<QDialog> second = window.openAttributeTable(secondLayer);
    check(first != second && first->isVisible() && second->isVisible(), "different layers have independent dialogs");
    check(window.openAttributeTable(firstLayer, "name = 'new'") == first
        && expression->text() == "name = 'new'", "plugin filter updates reused dialog");
    check(firstLayer->startEditing() && firstLayer->addAttribute(QgsField("unsaved", QVariant::String)),
        "prepare an unsaved layer edit");
    QTimer::singleShot(0, []()
    {
        for (auto* widget : QApplication::topLevelWidgets())
            if (auto* message = qobject_cast<QMessageBox*>(widget)) message->done(QMessageBox::Cancel);
    });
    check(!first->close() && window.openAttributeTable(firstLayer) == first && firstLayer->isModified(),
        "cancelled close preserves edits and dialog registration");
    firstLayer->rollBack();
    first->close();
    flushDeletes();
    check(!first, "closed table releases guarded dialog");
    QPointer<QDialog> reopened = window.openAttributeTable(firstLayer);
    check(reopened && reopened->isVisible(), "closed layer table can reopen");
    reopened->close();
    QPointer<QDialog> immediateReopen = window.openAttributeTable(firstLayer);
    check(immediateReopen && immediateReopen != reopened, "reopen before deferred deletion creates a live replacement");
    flushDeletes();
    check(!reopened && window.openAttributeTable(firstLayer) == immediateReopen,
        "old dialog deletion leaves replacement registered");
    QgsProject::instance()->removeMapLayer(firstLayer);
    flushDeletes();
    check(!immediateReopen, "removing layer closes its table");
    second->close();
    // Close any duplicate windows left by the baseline implementation.
    for (auto* dialog : window.findChildren<QDialog*>("workspaceAttributeTableDialog")) dialog->close();
    flushDeletes();
    QgsProject::instance()->removeMapLayer(secondLayer);
}

static void testStatus(LTZK& window)
{
    auto* canvas = window.mapCanvas();
    auto* row = window.findChild<QWidget*>("workspaceMapStatusBar");
    auto* coordinate = window.findChild<QLineEdit*>("workspaceMapCoordinate");
    auto* crs = window.findChild<QLabel*>("workspaceMapCrs");
    auto* scale = window.findChild<QgsScaleWidget*>("workspaceMapScale");
    check(row && coordinate && crs && scale, "map-local coordinate/CRS/scale controls exist");
    if (!row || !coordinate || !crs || !scale) return;
    check(row->parentWidget() == window.statusBar(), "map controls belong to the bottom application status bar");
    canvas->resize(800, 500);
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem("EPSG:4326"));
    canvas->setExtent(QgsRectangle(100, 30, 110, 40));
    window.mouseCoordinateChanged(QgsPointXY(104.25, 36.125));
    check(coordinate->text().contains("104.250000") && coordinate->text().contains("36.125000")
        && crs->text().contains("4326"), "geographic coordinates use canvas CRS");
    auto* geographic = window.findChild<QLabel*>("applicationGeographicCoordinate");
    check(geographic && geographic->parentWidget() == window.statusBar(), "geographic readout shares the bottom status bar");
    check(geographic && geographic->text().contains("104.250000")
        && geographic->text().contains("36.125000"), "geographic readout transforms from actual canvas CRS");
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem("EPSG:3857"));
    canvas->setExtent(QgsRectangle(11000000, 3500000, 12000000, 4500000));
    window.mouseCoordinateChanged(QgsPointXY(11600000, 4300000));
    check(coordinate->text().contains("11600000.00") && coordinate->text().contains("4300000.00")
        && crs->text().contains("3857"), "projected coordinates display native X/Y");
    canvas->zoomScale(250000);
    check(qAbs(scale->scale() - canvas->scale()) / canvas->scale() < .0001, "map zoom updates displayed scale");
    auto* scaleCombo = scale->findChild<QgsScaleComboBox*>();
    scaleCombo->lineEdit()->setText("1:500000");
    scaleCombo->lineEdit()->editingFinished();
    check(qAbs(canvas->scale() - 500000) / 500000 < .0001, "scale input zooms map");
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem("EPSG:4326"));
    check(coordinate->text().contains("--"), "CRS change clears stale coordinates");
    window.mouseCoordinateChanged(QgsPointXY(104.25, 36.125));
}

static void testProjectCrs(LTZK& window)
{
    auto* canvas = window.mapCanvas();
    auto* label = window.findChild<QLabel*>("workspaceMapCrs");
    check(label && label->textInteractionFlags().testFlag(Qt::LinksAccessibleByMouse)
        && label->textInteractionFlags().testFlag(Qt::LinksAccessibleByKeyboard),
        "EPSG entry supports mouse and keyboard activation");
    if (!label) return;
    auto* project = QgsProject::instance();
    project->setCrs(QgsCoordinateReferenceSystem("EPSG:3857"));
    canvas->setExtent(QgsRectangle(11000000, 3500000, 12000000, 4500000));
    auto* layer = new QgsVectorLayer("Point?crs=EPSG:4326", "CRS source", "memory");
    project->addMapLayer(layer);
    const auto originalSource = layer->source();
    QTimer::singleShot(100, [&]()
    {
        auto* dialog = qobject_cast<QgsProjectionSelectionDialog*>(QApplication::activeModalWidget());
        check(dialog && dialog->crs().authid() == "EPSG:3857", "CRS selector opens with current project CRS");
        if (dialog)
        {
            dialog->grab().save("E:/AAALTZK/workspace_table_status_icons_build/project_crs_dialog.png");
            dialog->setCrs(QgsCoordinateReferenceSystem("EPSG:4326"));
            dialog->accept();
        }
    });
    QMetaObject::invokeMethod(label, "linkActivated", Q_ARG(QString, "project-crs"));
    check(project->crs().authid() == "EPSG:4326"
        && canvas->mapSettings().destinationCrs() == project->crs()
        && label->text().contains("4326"), "accept changes project, canvas and EPSG display together");
    check(canvas->extent().center().x() > 90 && canvas->extent().center().x() < 120,
        "CRS change transforms the displayed map extent");
    check(layer->crs().authid() == "EPSG:4326" && layer->source() == originalSource,
        "project CRS selection preserves layer source CRS and data");
    QTimer::singleShot(100, [&]()
    {
        auto* dialog = qobject_cast<QgsProjectionSelectionDialog*>(QApplication::activeModalWidget());
        check(dialog != nullptr, "CRS selector reopens");
        if (dialog)
        {
            dialog->setCrs(QgsCoordinateReferenceSystem("EPSG:3857"));
            dialog->reject();
        }
    });
    QMetaObject::invokeMethod(label, "linkActivated", Q_ARG(QString, "project-crs"));
    check(project->crs().authid() == "EPSG:4326" && label->text().contains("4326"),
        "cancel preserves project CRS and status display");
    QTemporaryDir saved;
    const auto projectFile = saved.filePath("crs.qgs");
    check(project->write(projectFile), "save selected project CRS");
    project->setCrs(QgsCoordinateReferenceSystem("EPSG:3857"));
    check(project->read(projectFile) && project->crs().authid() == "EPSG:4326"
        && canvas->mapSettings().destinationCrs() == project->crs(),
        "reopening project restores selected CRS to project and canvas");
}

static void testIcons()
{
    const QString path = "E:/data/example.gdb";
    for (const auto& geometry : {QString("Point"), QString("MultiLineString"), QString("MultiPolygonZ")})
    {
        const QString expected = geometry == "Point" ? "point" : geometry.contains("Line") ? "line" : "polygon";
        check(pixels(productCatalogItemIcon("vector", geometry, "OpenFileGDB", path, "layer"))
            == pixels(catalogIcon(expected)), qPrintable("GDB vector leaf icon: " + expected));
    }
    check(pixels(productCatalogItemIcon("raster", "", "GDB", path, "image")) == pixels(imageryCatalogIcon()),
        "GDB raster leaf has raster icon");
    check(pixels(productCatalogItemIcon("table", "NoGeometry", "GDB", path, "attributes"))
        == pixels(catalogIcon("table")), "GDB nonspatial table has table icon");
    check(pixels(productCatalogItemIcon("", "", "GDB", path, "example.gdb"))
        == pixels(QgsDataCollectionItem::iconDataCollection()), "GDB container retains collection icon");
    check(pixels(productCatalogItemIcon("file", "", "pdf", "E:/manual.pdf", "manual.pdf"))
        == pixels(catalogIcon("pdf")), "ordinary PDF retains file-format icon");

    auto database = QSqlDatabase::addDatabase("QSQLITE", "workspace-icon-fixture");
    database.setDatabaseName(":memory:");
    check(database.open(), "open isolated SQLite icon fixture");
    QSqlQuery query(database);
    check(query.exec("CREATE TABLE product_directory (id INTEGER, parent_id INTEGER, name TEXT, description TEXT, sort_order INTEGER, node_type INTEGER)"), "create directory fixture");
    check(query.exec("CREATE TABLE product_metadata (id INTEGER, product_name TEXT, product_type TEXT, file_format TEXT, file_path TEXT, file_oid INTEGER, geometry_type TEXT, parent_dir_id INTEGER, directory_path TEXT, layer_table_name TEXT, current_version INTEGER, security_level TEXT)"), "create product fixture");
    check(query.exec("INSERT INTO product_directory VALUES (1,0,'ordinary','',0,0),(2,1,'example.gdb','',0,0),(3,2,'roads','',0,1),(4,0,'ordinaryDirect','',0,0)"), "insert tree fixture");
    check(query.exec("INSERT INTO product_metadata VALUES (1,'roads','vector','GDB','E:/data/example.gdb',0,'MultiLineString',3,'','roads',1,''),(2,'points','vector','GDB','E:/data/example.gdb',0,'Point',4,'','points',1,'')"), "insert GDB leaf fixture");
    check(query.exec(QStringLiteral("INSERT INTO product_directory VALUES (5,2,'%1','',1,1)")
        .arg(QStringLiteral("\u5f71\u50cf"))), "insert layer with category-like name");
    check(query.exec("INSERT INTO product_metadata VALUES (3,'named layer','vector','GDB','E:/data/example.gdb',0,'Point',5,'','named_layer',1,'')"),
        "insert geometry metadata for category-like name");
    query.prepare("INSERT INTO product_directory VALUES (6,0,:name,'',0,0)");
    query.bindValue(":name", QStringLiteral("\u6c38\u767b\u53bf"));
    check(query.exec(), "insert orphan GDB directory fixture");
    query.prepare("INSERT INTO product_metadata VALUES (4,:name,'vector','GDB',:path,0,'MultiLineString',-1,'','roads',1,'')");
    query.bindValue(":name", QStringLiteral("\u6c38\u767b\u53bf\u9053\u8def"));
    query.bindValue(":path", QStringLiteral("E:/data/\u6c38\u767b\u53bf.gdb"));
    check(query.exec(), "insert first unassigned GDB product fixture");
    query.prepare("INSERT INTO product_metadata VALUES (5,:name,'vector','GDB',:path,0,'Polygon',-1,'','residential',1,'')");
    query.bindValue(":name", QStringLiteral("\u6c38\u767b\u53bf\u5c45\u6c11\u5730"));
    query.bindValue(":path", QStringLiteral("E:/data/\u6c38\u767b\u53bf.gdb"));
    check(query.exec(), "insert second unassigned GDB product fixture");
    check(query.exec("INSERT INTO product_metadata VALUES (6,'unrelated layer','vector','GDB','E:/data/unrelated.gdb',0,'Point',-1,'','points',1,'')"),
        "insert unrelated unassigned GDB product fixture");
    {
        DataCatalogWidget widget;
        for (auto* timer : widget.findChildren<QTimer*>()) timer->stop();
        QStandardItem root("fixture");
        DataCatalogProductCatalog catalog(database);
        (widget.*member(TreeLoader{}))(
            &root,
            0,
            database,
            catalogGdbSourceDirectoryNames(database),
            catalog.unassignedGdbProducts());
        auto* ancestor = root.child(0);
        auto* container = ancestor ? ancestor->child(0) : nullptr;
        auto* leaf = container ? container->child(0) : nullptr;
        check(leaf && pixels(leaf->icon()) == pixels(catalogIcon("line")), "layer directory keeps geometry icon after recursion");
        check(container && pixels(container->icon()) == pixels(QgsDataCollectionItem::iconDataCollection()), "real GDB directory retains collection icon");
        check(container && container->child(1)
            && pixels(container->child(1)->icon()) == pixels(catalogIcon("point")),
            "layer with category-like name retains geometry icon");
        check(ancestor && pixels(ancestor->icon()) == pixels(catalogIcon("folder")), "ordinary ancestor does not inherit GDB icon");
        auto* directFolder = root.child(1);
        check(directFolder && pixels(directFolder->icon()) == pixels(catalogIcon("folder")),
            "ordinary folder with GDB product retains folder icon");
        check(directFolder && directFolder->child(0)
            && pixels(directFolder->child(0)->icon()) == pixels(catalogIcon("point")),
            "ordinary folder's GDB product uses geometry icon");
        auto* orphanGdbDirectory = findItemNamed(&root, QStringLiteral("\u6c38\u767b\u53bf"));
        check(orphanGdbDirectory && orphanGdbDirectory->rowCount() == 2,
            "unassigned GDB products appear under the matching source directory");
        QSet<int> orphanProductIds;
        if (orphanGdbDirectory)
        {
            for (int row = 0; row < orphanGdbDirectory->rowCount(); ++row)
            {
                orphanProductIds.insert(
                    orphanGdbDirectory->child(row)->data(Qt::UserRole + 7).toInt());
            }
        }
        check(orphanProductIds == QSet<int>({4, 5}),
            "GDB container children retain their product identities");
        check(!findItemNamed(&root, QStringLiteral("unrelated layer")),
            "unassigned GDB products do not appear beneath unrelated directories");
    }
    database.close();
}

int main(int argc, char** argv)
{
    QgsApplication app(argc, argv, true);
    app.setOrganizationName("LTZKWorkspaceRegressionTest");
    app.setApplicationName("LTZKWorkspaceRegressionTest");
    const int fontId = QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    const auto fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
    if (!fontFamilies.isEmpty()) app.setFont(QFont(fontFamilies.first(), 10));
    app.setProperty("ltzkBaseApplicationFont", QVariant::fromValue(app.font()));
    QgsApplication::setPrefixPath("E:/extern/QGIS_32815", true);
    QgsApplication::setPluginPath("E:/extern/QGIS_32815/output/plugins");
    QgsApplication::initQgis();
    QgsGui::instance();
    QgsGui::editorWidgetRegistry()->initEditors();
    {
        LTZK window;
        for (auto* timer : window.findChildren<QTimer*>()) timer->stop();
        testTables(window);
        testStatus(window);
        testProjectCrs(window);
        testIcons();
    }
    QgsProject::instance()->clear();
    QgsApplication::exitQgis();
    std::printf("%d regression failure(s)\n", failures);
    return failures ? 1 : 0;
}
