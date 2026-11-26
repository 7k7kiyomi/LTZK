#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QTextBrowser>
#include <QTextStream>
#include <QTableView>
#include <QTreeWidget>
#include <QTranslator>
#include <QTimer>
#include <QTextDocument>
#include <QTextFrame>
#include <QStyleOptionGroupBox>
#include <qgsapplication.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerproperties.h>
#include <qgsmapcanvas.h>
#include <qgsmessagebar.h>
#include <qgscollapsiblegroupbox.h>
#include <qgspropertyoverridebutton.h>
#include <qgstextformatwidget.h>
#include "../LTZKNativeDialogStyle.h"

int main(int argc, char **argv)
{
    QgsApplication app(argc, argv, true);
    app.setOrganizationName("LTZKThemeProbe");
    app.setApplicationName("LTZKThemeProbe");
    QgsApplication::setPrefixPath("E:/extern/QGIS_32815", true);
    QgsApplication::setPluginPath("E:/extern/QGIS_32815/output/plugins");
    QgsApplication::initQgis();
    QTranslator translator;
    translator.load("E:/extern/QGIS_32815/output/i18n/qgis_zh-Hans.qm");
    app.installTranslator(&translator);
    QFile theme(argc > 1 ? argv[1] : "Resources/styles/ltzk_field_notes.qss");
    if (!theme.open(QIODevice::ReadOnly)) return 2;
    const qreal scale = argc > 3 ? QString::fromLocal8Bit(argv[3]).toDouble() : 1.0;
    app.setFont(LTZKUiScale::scaledFont(app.font(), scale));
    app.setStyleSheet(LTZKUiScale::scaleStyleSheetPixels(QString::fromUtf8(theme.readAll()), scale));
    const bool applyFix = !qEnvironmentVariableIsSet("LTZK_PROBE_BASELINE");
    if (applyFix)
        app.setStyleSheet(app.styleSheet() + LTZKNativeDialogStyle::embeddedEditorStyleSheet());
    // Small QGIS tool buttons must retain enough content area to paint their icons.
    QPixmap iconPixmap(16, 16);
    iconPixmap.fill(Qt::red);
    const QIcon probeIcon(iconPixmap);
    QgsPropertyOverrideButton overrideButton;
    QgsTextFormatWidget textFormat;
    QToolButton formatButton(&textFormat);
    auto iconPainted = [&](QToolButton *button) {
        button->setIcon(probeIcon);
        button->setIconSize(QSize(16, 16));
        button->setFixedSize(30, 30);
        button->ensurePolished();
        const QImage withIcon = button->grab().toImage();
        button->setIcon(QIcon());
        const QImage withoutIcon = button->grab().toImage();
        int changed = 0;
        for (int y = 0; y < withIcon.height(); ++y)
            for (int x = 0; x < withIcon.width(); ++x)
                if (withIcon.pixel(x, y) != withoutIcon.pixel(x, y)) ++changed;
        return changed >= 50;
    };
    const bool overrideIconPainted = iconPainted(&overrideButton);
    const bool formatIconPainted = iconPainted(&formatButton);
    QTextStream(stdout) << (overrideIconPainted ? "PASS" : "FAIL") << " override icon painted\n"
                        << (formatIconPainted ? "PASS" : "FAIL") << " text format icon painted\n";
    QgsVectorLayer layer("Polygon?crs=EPSG:4326&field=name:string", "theme probe", "memory");
    QgsMapCanvas canvas;
    QgsMessageBar bar;
    QgsVectorLayerProperties dialog(&canvas, &bar, &layer);
    dialog.setAttribute(Qt::WA_DontShowOnScreen);
    dialog.resize(1000, 920);
    if (applyFix)
    {
        LTZKNativeDialogStyle::prepareHeaders(&dialog);
        dialog.setStyleSheet(app.styleSheet() + LTZKNativeDialogStyle::nativeHeaderStyleSheet(scale));
    }
    QTextStream out(stdout);
    const QString outputDir = argc > 2 ? QString::fromLocal8Bit(argv[2]) : "native_dialog_theme_render_build";
    QDir().mkpath(outputDir);
    QFile metadata(outputDir + "/metadata_original.html");
    metadata.open(QIODevice::WriteOnly);
    metadata.write(layer.htmlMetadata().toUtf8());
    for (const QString &page : {QString("mOptsPage_Information"), QString("mOptsPage_Style"), QString("mOptsPage_Server"), QString("mOptsPage_Rendering")})
    {
        dialog.setCurrentPage(page);
        dialog.show();
        for (int i = 0; i < 8; ++i) app.processEvents();
        if (applyFix)
        {
            LTZKNativeDialogStyle::polish(&dialog, scale);
            for (int i = 0; i < 8; ++i) app.processEvents();
        }
        dialog.grab().save(outputDir + (applyFix ? "/after_" : "/before_") + page + ".png");
    }
    for (auto *browser : dialog.findChildren<QTextBrowser *>())
    {
        out << "BROWSER " << browser->objectName() << " base=" << browser->palette().color(QPalette::Base).name() << " text=" << browser->palette().color(QPalette::Text).name() << '\n';
        if (browser->objectName() == "teMetadataViewer")
        {
            out << "DOC background=" << browser->document()->rootFrame()->frameFormat().background().color().name() << '\n';
            QFile html(outputDir + "/metadata_viewer.html");
            html.open(QIODevice::WriteOnly);
            html.write(browser->toHtml().toUtf8());
        }
    }
    auto *group = dialog.findChild<QGroupBox *>("mMetaDescriptionGrpBx");
    const QImage groupImage = group->grab().toImage();
    const QColor headerBackground = groupImage.pixelColor(qMin(110, groupImage.width() - 1), 3);
    int titleLeft = groupImage.width();
    for (int y = 2; y < qMin(group->fontMetrics().height() + 5, groupImage.height()); ++y)
        for (int x = 5; x < qMin(100, groupImage.width()); ++x)
        {
            const QColor pixel = groupImage.pixelColor(x, y);
            if (qAbs(pixel.red() - headerBackground.red()) + qAbs(pixel.green() - headerBackground.green())
                + qAbs(pixel.blue() - headerBackground.blue()) > 90)
                titleLeft = qMin(titleLeft, x);
        }
    out << "DESCRIPTION first title ink=" << titleLeft << '\n';
    // The collapse affordance needs a small gutter; theme padding must not add a second indent.
    const bool aligned = titleLeft <= LTZKUiScale::scaledInt(28, scale) + 2;
    out << (aligned ? "PASS" : "FAIL") << " native group title left alignment\n";
    auto *browser = dialog.findChild<QTextBrowser *>("teMetadataViewer");
    const bool themed = browser->document()->rootFrame()->frameFormat().background().color() == browser->palette().color(QPalette::Base);
    out << (themed ? "PASS" : "FAIL") << " metadata document theme background\n";
    auto *dimensions = dialog.findChild<QTreeWidget *>("mWmsDimensionsTreeWidget");
    const bool fits = dimensions->height() >= dimensions->header()->height() + 2 * dimensions->frameWidth();
    out << (fits ? "PASS" : "FAIL") << " table header fits its view\n";
    auto *styles = dialog.findChild<QComboBox *>("groupsCombo");
    const int editorFrame = styles && styles->lineEdit() ? styles->lineEdit()->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, styles->lineEdit()) : -1;
    out << "EDITOR frame=" << editorFrame << '\n';
    const bool singleFrame = editorFrame == 0;
    out << (singleFrame ? "PASS" : "FAIL") << " compound editor has a single frame\n";
    browser->document()->setDefaultStyleSheet(QgsApplication::reportStyleSheet());
    const QString sourceHtml = layer.htmlMetadata();
    browser->setHtml(sourceHtml);
    for (int i = 0; i < 8; ++i) app.processEvents();
    const bool refreshed = browser->document()->rootFrame()->frameFormat().background().color() == browser->palette().color(QPalette::Base)
        && browser->toPlainText().contains(layer.name());
    out << (refreshed ? "PASS" : "FAIL") << " metadata refresh preserves theme and content\n";
    dialog.setCurrentPage("mOptsPage_Server");
    for (int i = 0; i < 8; ++i) app.processEvents();
    if (applyFix)
        LTZKNativeDialogStyle::polish(&dialog, scale);
    for (int i = 0; i < 8; ++i) app.processEvents();
    auto *keywordLabel = dialog.findChild<QLabel *>("mLayerKeywordListLabel");
    const bool regularLabel = keywordLabel && keywordLabel->fontMetrics().height() < LTZKUiScale::scaledInt(24, scale);
    out << (regularLabel ? "PASS" : "FAIL") << " form label keeps regular font metrics\n";
    bool stable = true;
    bool nativeState = true;
    if (applyFix)
    {
        auto *collapse = qobject_cast<QgsCollapsibleGroupBoxBasic *>(group);
        const QString title = group->title();
        collapse->setCollapsed(true);
        LTZKNativeDialogStyle::polish(&dialog, scale);
        nativeState = collapse->isCollapsed() && group->title() == title;
        collapse->setCollapsed(false);
        LTZKNativeDialogStyle::polish(&dialog, scale);
        nativeState = nativeState && !collapse->isCollapsed();
        const QString style = group->styleSheet();
        for (int i = 0; i < 4; ++i)
            LTZKNativeDialogStyle::polish(&dialog, scale);
        stable = style == group->styleSheet();
        auto *checkGroup = dialog.findChild<QGroupBox *>("mSimplifyDrawingGroupBox");
        checkGroup->setChecked(false);
        LTZKNativeDialogStyle::polish(&dialog, scale);
        nativeState = nativeState && !checkGroup->isChecked();
        checkGroup->setChecked(true);
        LTZKNativeDialogStyle::polish(&dialog, scale);
        nativeState = nativeState && checkGroup->isChecked();
    }
    out << (stable ? "PASS" : "FAIL") << " repeated polish does not accumulate styles\n";
    out << (nativeState ? "PASS" : "FAIL") << " native collapse and checkbox states are preserved\n";
    bool arrowsVisible = true;
    bool arrowsClickable = true;
    for (const QString &name : {QString("mMetaDescriptionGrpBx"), QString("mMetaAttributionGrpBx"),
        QString("mMetaLegendGrpBx"), QString("mWmsDimensionsGrpBx")})
    {
        auto *section = dialog.findChild<QgsCollapsibleGroupBoxBasic *>(name);
        auto *button = LTZKNativeDialogStyle::collapseButton(section);
        if (!section || !button)
        {
            arrowsVisible = arrowsClickable = false;
            continue;
        }
        section->setCollapsed(false);
        if (applyFix)
            LTZKNativeDialogStyle::polish(&dialog, scale);
        for (int i = 0; i < 8; ++i) app.processEvents();
        for (int state = 0; state < 2; ++state)
        {
            // Compare the parent rendering with and without its native arrow.
            // A valid QIcon or a visible button alone does not prove it is painted.
            const QIcon icon = button->icon();
            const QImage painted = section->grab().toImage();
            button->setIcon(QIcon());
            const QImage blank = section->grab().toImage();
            button->setIcon(icon);
            const QRect bounds = QRect(button->mapTo(section, QPoint()), button->size()).intersected(painted.rect());
            int inkPixels = 0;
            for (int y = bounds.top(); y <= bounds.bottom(); ++y)
                for (int x = bounds.left(); x <= bounds.right(); ++x)
                    if (painted.pixel(x, y) != blank.pixel(x, y))
                        ++inkPixels;
            const bool visibleArrow = button->isVisible() && !icon.isNull() && inkPixels >= 4;
            arrowsVisible = arrowsVisible && visibleArrow;
            out << (visibleArrow ? "PASS" : "FAIL") << " arrow painted " << name
                << " collapsed=" << section->isCollapsed() << " ink=" << inkPixels << '\n';
            const bool previous = section->isCollapsed();
            button->click();
            for (int i = 0; i < 8; ++i) app.processEvents();
            arrowsClickable = arrowsClickable && section->isCollapsed() != previous;
        }
    }
    out << (arrowsClickable ? "PASS" : "FAIL") << " arrow clicks toggle native collapse state\n";
    dialog.grab().save(outputDir + (applyFix ? "/after_server_revisit.png" : "/before_server_revisit.png"));
    return aligned && themed && fits && singleFrame && refreshed && regularLabel && stable && nativeState
        && arrowsVisible && arrowsClickable && overrideIconPainted && formatIconPainted ? 0 : 1;
}
