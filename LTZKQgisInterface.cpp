#include "LTZKQgisInterface.h"

#include "LTZK.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDockWidget>
#include <QFont>
#include <QMenuBar>
#include <QStatusBar>
#include <QUrl>
#include <QWidget>

#include "qgsmapcanvas.h"
#include "qgsmaplayer.h"
#include "qgslayertree.h"
#include "qgsproject.h"
#include "qgsrasterlayer.h"
#include "qgsvectorlayer.h"

LTZKQgisInterface::LTZKQgisInterface(LTZK *app)
    : mApp(app)
{
}

QMenu *LTZKQgisInterface::menuByTitle(const QString &title) const
{
    return mApp ? mApp->ensureMenu(title) : nullptr;
}

void LTZKQgisInterface::addActionToNamedMenu(const QString &menuTitle, const QString &pluginName, QAction *action)
{
    if (!mApp || !action)
    {
        return;
    }

    QMenu *menu = menuTitle.isEmpty() || menuTitle == QStringLiteral("Plugins")
        ? mApp->pluginMenu()
        : mApp->ensureMenu(menuTitle);

    if (!pluginName.isEmpty() && menu == mApp->pluginMenu())
    {
        menu = mApp->ensurePluginSubMenu(pluginName);
    }

    if (menu && !menu->actions().contains(action))
    {
        menu->addAction(action);
    }
}

void LTZKQgisInterface::removeActionFromNamedMenu(const QString &menuTitle, const QString &pluginName, QAction *action)
{
    if (!mApp || !action)
    {
        return;
    }

    QMenu *menu = menuTitle.isEmpty() || menuTitle == QStringLiteral("Plugins")
        ? mApp->pluginMenu()
        : mApp->ensureMenu(menuTitle);

    if (!pluginName.isEmpty() && menu == mApp->pluginMenu())
    {
        menu = mApp->ensurePluginSubMenu(pluginName);
    }

    if (menu)
    {
        menu->removeAction(action);
    }
}

int LTZKQgisInterface::addActionToToolbar(QToolBar *toolbar, QAction *action) const
{
    if (!toolbar || !action)
    {
        return -1;
    }

    if (!toolbar->actions().contains(action))
    {
        toolbar->addAction(action);
    }
    return toolbar->actions().indexOf(action);
}

QAction *LTZKQgisInterface::addWidgetToToolbar(QToolBar *toolbar, QWidget *widget) const
{
    if (!toolbar || !widget)
    {
        return nullptr;
    }

    return toolbar->addWidget(widget);
}


QAction *LTZKQgisInterface::safeAction(const QString &objectName, const QString &text) const
{
    if (mApp && !objectName.isEmpty())
    {
        if (QAction *existingAction = mApp->findChild<QAction *>(objectName))
        {
            return existingAction;
        }
    }

    const QString key = objectName.isEmpty() ? text : objectName;
    QAction *fallback = mFallbackActions.value(key, nullptr);
    if (!fallback)
    {
        fallback = new QAction(text, const_cast<LTZKQgisInterface *>(this));
        fallback->setObjectName(key);
        fallback->setEnabled(false);
        mFallbackActions.insert(key, fallback);
    }
    return fallback;
}
QgsPluginManagerInterface * LTZKQgisInterface::pluginManagerInterface(  )
{
    return nullptr;
}

QgsLayerTreeView * LTZKQgisInterface::layerTreeView(  )
{
    return mApp ? mApp->layerTreeView() : nullptr;
}

void LTZKQgisInterface::addCustomActionForLayerType( QAction *action, QString menu, QgsMapLayerType type, bool allLayers )
{
    Q_UNUSED(type); Q_UNUSED(allLayers); addActionToNamedMenu(menu, QString(), action);
}

void LTZKQgisInterface::addCustomActionForLayer( QAction *action, QgsMapLayer *layer )
{
    Q_UNUSED(layer); addActionToNamedMenu(QString(), QString(), action);
}

bool LTZKQgisInterface::removeCustomActionForLayerType( QAction *action )
{
    removeActionFromNamedMenu(QString(), QString(), action); return true;
}

QList< QgsMapCanvas * > LTZKQgisInterface::mapCanvases(  )
{
    return mApp && mApp->mapCanvas() ? QList<QgsMapCanvas *>{ mApp->mapCanvas() } : QList<QgsMapCanvas *>();
}

QgsMapCanvas * LTZKQgisInterface::createNewMapCanvas( const QString &name )
{
    Q_UNUSED(name); return nullptr;
}

void LTZKQgisInterface::closeMapCanvas( const QString &name )
{
    Q_UNUSED(name);
}

QSize LTZKQgisInterface::iconSize( bool dockedToolbar ) const
{
    Q_UNUSED(dockedToolbar); return QSize(24, 24);
}

QList<QgsMapLayer *> LTZKQgisInterface::editableLayers( bool modified ) const
{
    Q_UNUSED(modified); return QList<QgsMapLayer *>();
}

QgsMapLayer * LTZKQgisInterface::activeLayer(  )
{
    return mApp ? mApp->currentMapLayer() : nullptr;
}

QgsMapCanvas * LTZKQgisInterface::mapCanvas(  )
{
    return mApp ? mApp->mapCanvas() : nullptr;
}

QList<QgsMapDecoration *> LTZKQgisInterface::activeDecorations(  )
{
    return QList<QgsMapDecoration *>();
}

QgsLayerTreeMapCanvasBridge * LTZKQgisInterface::layerTreeCanvasBridge(  )
{
    return mApp ? mApp->layerTreeCanvasBridge() : nullptr;
}

QWidget * LTZKQgisInterface::mainWindow(  )
{
    return mApp;
}

QgsMessageBar * LTZKQgisInterface::messageBar(  )
{
    return mApp ? mApp->messageBarWidget() : nullptr;
}

QList<QgsLayoutDesignerInterface *> LTZKQgisInterface::openLayoutDesigners(  )
{
    return QList<QgsLayoutDesignerInterface *>();
}

QMap<QString, QVariant> LTZKQgisInterface::defaultStyleSheetOptions(  )
{
    return QMap<QString, QVariant>();
}

QFont LTZKQgisInterface::defaultStyleSheetFont(  )
{
    return qApp ? qApp->font() : QFont();
}

QgsAdvancedDigitizingDockWidget * LTZKQgisInterface::cadDockWidget(  )
{
    return nullptr;
}

QMenu * LTZKQgisInterface::projectMenu(  )
{
    return menuByTitle(QStringLiteral("Plugins"));
}

QMenu * LTZKQgisInterface::editMenu(  )
{
    return menuByTitle(QStringLiteral("Plugins"));
}

QMenu * LTZKQgisInterface::viewMenu(  )
{
    return menuByTitle(QStringLiteral("Plugins"));
}

QMenu * LTZKQgisInterface::layerMenu(  )
{
    return menuByTitle(QStringLiteral("Plugins"));
}

QMenu * LTZKQgisInterface::newLayerMenu(  )
{
    return menuByTitle(QStringLiteral("Plugins"));
}

QMenu * LTZKQgisInterface::addLayerMenu(  )
{
    return menuByTitle(QStringLiteral("Plugins"));
}

QMenu * LTZKQgisInterface::settingsMenu(  )
{
    return menuByTitle(QStringLiteral("Plugins"));
}

QMenu * LTZKQgisInterface::pluginMenu(  )
{
    return mApp ? mApp->pluginMenu() : nullptr;
}

QMenu * LTZKQgisInterface::pluginHelpMenu(  )
{
    return menuByTitle(QStringLiteral("Plugins"));
}

QMenu * LTZKQgisInterface::rasterMenu(  )
{
    return menuByTitle(QStringLiteral("Plugins"));
}

QMenu * LTZKQgisInterface::databaseMenu(  )
{
    return menuByTitle(QStringLiteral("Plugins"));
}

QMenu * LTZKQgisInterface::vectorMenu(  )
{
    return menuByTitle(QStringLiteral("Plugins"));
}

QMenu * LTZKQgisInterface::webMenu(  )
{
    return menuByTitle(QStringLiteral("Web"));
}

QMenu * LTZKQgisInterface::firstRightStandardMenu(  )
{
    return menuByTitle(QStringLiteral("Plugins"));
}

QMenu * LTZKQgisInterface::windowMenu(  )
{
    return menuByTitle(QStringLiteral("Plugins"));
}

QMenu * LTZKQgisInterface::helpMenu(  )
{
    return menuByTitle(QStringLiteral("Plugins"));
}

QToolBar * LTZKQgisInterface::fileToolBar(  )
{
    return mApp ? mApp->pluginToolBar() : nullptr;
}

QToolBar * LTZKQgisInterface::layerToolBar(  )
{
    return mApp ? mApp->pluginToolBar() : nullptr;
}

QToolBar * LTZKQgisInterface::dataSourceManagerToolBar(  )
{
    return mApp ? mApp->pluginToolBar() : nullptr;
}

QToolBar * LTZKQgisInterface::mapNavToolToolBar(  )
{
    return mApp ? mApp->pluginToolBar() : nullptr;
}

QToolBar * LTZKQgisInterface::digitizeToolBar(  )
{
    return mApp ? mApp->pluginToolBar() : nullptr;
}

QToolBar * LTZKQgisInterface::advancedDigitizeToolBar(  )
{
    return mApp ? mApp->pluginToolBar() : nullptr;
}

QToolBar * LTZKQgisInterface::shapeDigitizeToolBar(  )
{
    return mApp ? mApp->pluginToolBar() : nullptr;
}

QToolBar * LTZKQgisInterface::attributesToolBar(  )
{
    return mApp ? mApp->pluginToolBar() : nullptr;
}

QToolBar * LTZKQgisInterface::selectionToolBar(  )
{
    return mApp ? mApp->pluginToolBar() : nullptr;
}

QToolBar * LTZKQgisInterface::pluginToolBar(  )
{
    return mApp ? mApp->pluginToolBar() : nullptr;
}

QToolBar * LTZKQgisInterface::helpToolBar(  )
{
    return mApp ? mApp->pluginToolBar() : nullptr;
}

QToolBar * LTZKQgisInterface::rasterToolBar(  )
{
    return mApp ? mApp->pluginToolBar() : nullptr;
}

QToolBar * LTZKQgisInterface::vectorToolBar(  )
{
    return mApp ? mApp->pluginToolBar() : nullptr;
}

QToolBar * LTZKQgisInterface::databaseToolBar(  )
{
    return mApp ? mApp->pluginToolBar() : nullptr;
}

QToolBar * LTZKQgisInterface::webToolBar(  )
{
    return mApp ? mApp->pluginToolBar() : nullptr;
}

QAction * LTZKQgisInterface::actionNewProject(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionOpenProject(  )
{
    return safeAction(QStringLiteral("actionOpenProject"), QStringLiteral("Open Project"));
}

QAction * LTZKQgisInterface::actionSaveProject(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionSaveProjectAs(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionSaveMapAsImage(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionProjectProperties(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionCreatePrintLayout(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionShowLayoutManager(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionExit(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionCutFeatures(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionCopyFeatures(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionPasteFeatures(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionAddFeature(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionDeleteSelected(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionMoveFeature(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionSplitFeatures(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionSplitParts(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionAddRing(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionAddPart(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionSimplifyFeature(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionDeleteRing(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionDeletePart(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionVertexTool(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionVertexToolActiveLayer(  )
{
    return nullptr;
}

QActionGroup * LTZKQgisInterface::mapToolActionGroup(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionPan(  )
{
    return safeAction(QStringLiteral("actionPan"), QStringLiteral("Pan"));
}

QAction * LTZKQgisInterface::actionPanToSelected(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionZoomIn(  )
{
    return safeAction(QStringLiteral("actionZoomIn"), QStringLiteral("Zoom In"));
}

QAction * LTZKQgisInterface::actionZoomOut(  )
{
    return safeAction(QStringLiteral("actionZoomOut"), QStringLiteral("Zoom Out"));
}

QAction * LTZKQgisInterface::actionSelect(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionSelectRectangle(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionSelectPolygon(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionSelectFreehand(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionSelectRadius(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionIdentify(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionFeatureAction(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionMeasure(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionMeasureArea(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionZoomFullExtent(  )
{
    return safeAction(QStringLiteral("actionFullExtent"), QStringLiteral("Full Extent"));
}

QAction * LTZKQgisInterface::actionZoomToLayer(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionZoomToLayers(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionZoomToSelected(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionZoomLast(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionZoomNext(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionZoomActualSize(  )
{
    return safeAction(QStringLiteral("actionZoomActual"), QStringLiteral("Actual Size"));
}

QAction * LTZKQgisInterface::actionMapTips(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionNewBookmark(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionShowBookmarks(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionDraw(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionNewVectorLayer(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionAddOgrLayer(  )
{
    return safeAction(QStringLiteral("actionAddVectorData"), QStringLiteral("Add Vector Layer"));
}

QAction * LTZKQgisInterface::actionAddRasterLayer(  )
{
    return safeAction(QStringLiteral("actionAddRasterData"), QStringLiteral("Add Raster Layer"));
}

QAction * LTZKQgisInterface::actionAddPgLayer(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionAddWmsLayer(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionAddXyzLayer(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionAddVectorTileLayer(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionAddPointCloudLayer(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionAddAfsLayer(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionAddAmsLayer(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionCopyLayerStyle(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionPasteLayerStyle(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionOpenTable(  )
{
    return safeAction(QStringLiteral("actionOpenAttrTable"), QStringLiteral("Open Attribute Table"));
}

QAction * LTZKQgisInterface::actionOpenFieldCalculator(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionOpenStatisticalSummary(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionToggleEditing(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionSaveActiveLayerEdits(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionAllEdits(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionSaveEdits(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionSaveAllEdits(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionRollbackEdits(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionRollbackAllEdits(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionCancelEdits(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionCancelAllEdits(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionLayerSaveAs(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionDuplicateLayer(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionLayerProperties(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionAddToOverview(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionAddAllToOverview(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionRemoveAllFromOverview(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionHideAllLayers(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionShowAllLayers(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionHideSelectedLayers(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionToggleSelectedLayers(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionToggleSelectedLayersIndependently(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionHideDeselectedLayers(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionShowSelectedLayers(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionManagePlugins(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionPluginListSeparator(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionShowPythonDialog(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionToggleFullScreen(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionOptions(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionCustomProjection(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionHelpContents(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionQgisHomePage(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionCheckQgisVersion(  )
{
    return nullptr;
}

QAction * LTZKQgisInterface::actionAbout(  )
{
    return nullptr;
}

QgsVectorLayerTools * LTZKQgisInterface::vectorLayerTools(  )
{
    return nullptr;
}

int LTZKQgisInterface::messageTimeout(  )
{
    return 5;
}

QgsStatusBar * LTZKQgisInterface::statusBarIface(  )
{
    return nullptr;
}

QgsLayerTreeRegistryBridge::InsertionPoint LTZKQgisInterface::layerTreeInsertionPoint(  )
{
    QgsLayerTreeRegistryBridge::InsertionPoint point(static_cast<QgsLayerTreeGroup *>(QgsProject::instance()->layerTreeRoot()), 0); return point;
}

void LTZKQgisInterface::zoomFull(  )
{
    if (mApp && mApp->mapCanvas()) mApp->mapCanvas()->zoomToFullExtent();
}

void LTZKQgisInterface::zoomToPrevious(  )
{
    if (mApp && mApp->mapCanvas()) mApp->mapCanvas()->zoomToPreviousExtent();
}

void LTZKQgisInterface::zoomToNext(  )
{
    if (mApp && mApp->mapCanvas()) mApp->mapCanvas()->zoomToNextExtent();
}

void LTZKQgisInterface::zoomToActiveLayer(  )
{
    if (mApp && mApp->mapCanvas() && mApp->currentMapLayer()) mApp->mapCanvas()->setExtent(mApp->currentMapLayer()->extent());
}

QgsVectorLayer * LTZKQgisInterface::addVectorLayer( const QString &vectorLayerPath, const QString &baseName, const QString &providerKey )
{
    if (mApp)
    {
        return mApp->addPluginVectorLayer(vectorLayerPath, baseName, providerKey);
    }

    QgsVectorLayer *layer = new QgsVectorLayer(vectorLayerPath, baseName, providerKey);
    if (!layer->isValid())
    {
        delete layer;
        return nullptr;
    }
    QgsProject::instance()->addMapLayer(layer);
    return layer;
}

QgsRasterLayer * LTZKQgisInterface::addRasterLayer( const QString &rasterLayerPath, const QString &baseName )
{
    QgsRasterLayer *layer = new QgsRasterLayer(rasterLayerPath, baseName); if (!layer->isValid()) { delete layer; return nullptr; } QgsProject::instance()->addMapLayer(layer); return layer;
}

QgsRasterLayer * LTZKQgisInterface::addRasterLayer( const QString &url, const QString &layerName, const QString &providerKey )
{
    QgsRasterLayer *layer = new QgsRasterLayer(url, layerName, providerKey); if (!layer->isValid()) { delete layer; return nullptr; } QgsProject::instance()->addMapLayer(layer); return layer;
}

QgsMeshLayer * LTZKQgisInterface::addMeshLayer( const QString &url, const QString &baseName, const QString &providerKey )
{
    Q_UNUSED(url); Q_UNUSED(baseName); Q_UNUSED(providerKey); return nullptr;
}

QgsVectorTileLayer * LTZKQgisInterface::addVectorTileLayer( const QString &url, const QString &baseName )
{
    Q_UNUSED(url); Q_UNUSED(baseName); return nullptr;
}

QgsPointCloudLayer * LTZKQgisInterface::addPointCloudLayer( const QString &url, const QString &baseName, const QString &providerKey )
{
    Q_UNUSED(url); Q_UNUSED(baseName); Q_UNUSED(providerKey); return nullptr;
}

bool LTZKQgisInterface::addProject( const QString &project )
{
    return QgsProject::instance()->read(project);
}

bool LTZKQgisInterface::newProject( bool promptToSaveFlag )
{
    Q_UNUSED(promptToSaveFlag); QgsProject::instance()->clear(); return true;
}

void LTZKQgisInterface::reloadConnections(  )
{
    
}

bool LTZKQgisInterface::setActiveLayer( QgsMapLayer * arg0 )
{
    Q_UNUSED(arg0); return false;
}

void LTZKQgisInterface::copySelectionToClipboard( QgsMapLayer * arg0 )
{
    Q_UNUSED(arg0);
}

void LTZKQgisInterface::pasteFromClipboard( QgsMapLayer * arg0 )
{
    Q_UNUSED(arg0);
}

int LTZKQgisInterface::addToolBarIcon( QAction *qAction )
{
    return addActionToToolbar(mApp ? mApp->pluginToolBar() : nullptr, qAction);
}

QAction * LTZKQgisInterface::addToolBarWidget( QWidget *widget )
{
    return addWidgetToToolbar(mApp ? mApp->pluginToolBar() : nullptr, widget);
}

void LTZKQgisInterface::removeToolBarIcon( QAction *qAction )
{
    if (mApp && mApp->pluginToolBar()) mApp->pluginToolBar()->removeAction(qAction);
}

QAction * LTZKQgisInterface::addRasterToolBarWidget( QWidget *widget )
{
    return addWidgetToToolbar(mApp ? mApp->pluginToolBar() : nullptr, widget);
}

int LTZKQgisInterface::addRasterToolBarIcon( QAction *qAction )
{
    return addActionToToolbar(mApp ? mApp->pluginToolBar() : nullptr, qAction);
}

void LTZKQgisInterface::removeRasterToolBarIcon( QAction *qAction )
{
    if (mApp && mApp->pluginToolBar()) mApp->pluginToolBar()->removeAction(qAction);
}

int LTZKQgisInterface::addVectorToolBarIcon( QAction *qAction )
{
    return addActionToToolbar(mApp ? mApp->pluginToolBar() : nullptr, qAction);
}

QAction * LTZKQgisInterface::addVectorToolBarWidget( QWidget *widget )
{
    return addWidgetToToolbar(mApp ? mApp->pluginToolBar() : nullptr, widget);
}

void LTZKQgisInterface::removeVectorToolBarIcon( QAction *qAction )
{
    if (mApp && mApp->pluginToolBar()) mApp->pluginToolBar()->removeAction(qAction);
}

int LTZKQgisInterface::addDatabaseToolBarIcon( QAction *qAction )
{
    return addActionToToolbar(mApp ? mApp->pluginToolBar() : nullptr, qAction);
}

QAction * LTZKQgisInterface::addDatabaseToolBarWidget( QWidget *widget )
{
    return addWidgetToToolbar(mApp ? mApp->pluginToolBar() : nullptr, widget);
}

void LTZKQgisInterface::removeDatabaseToolBarIcon( QAction *qAction )
{
    if (mApp && mApp->pluginToolBar()) mApp->pluginToolBar()->removeAction(qAction);
}

int LTZKQgisInterface::addWebToolBarIcon( QAction *qAction )
{
    return addActionToToolbar(mApp ? mApp->pluginToolBar() : nullptr, qAction);
}

QAction * LTZKQgisInterface::addWebToolBarWidget( QWidget *widget )
{
    return addWidgetToToolbar(mApp ? mApp->pluginToolBar() : nullptr, widget);
}

void LTZKQgisInterface::removeWebToolBarIcon( QAction *qAction )
{
    if (mApp && mApp->pluginToolBar()) mApp->pluginToolBar()->removeAction(qAction);
}

QToolBar * LTZKQgisInterface::addToolBar( const QString &name )
{
    return mApp ? mApp->addToolBar(name) : nullptr;
}

void LTZKQgisInterface::addToolBar( QToolBar *toolbar, Qt::ToolBarArea area )
{
    if (mApp && toolbar) mApp->addToolBar(area, toolbar);
}

void LTZKQgisInterface::openMessageLog(  )
{
    
}

void LTZKQgisInterface::addUserInputWidget( QWidget *widget )
{
    Q_UNUSED(widget);
}

void LTZKQgisInterface::showLayoutManager(  )
{
    
}

QgsLayoutDesignerInterface * LTZKQgisInterface::openLayoutDesigner( QgsMasterLayoutInterface *layout )
{
    Q_UNUSED(layout); return nullptr;
}

void LTZKQgisInterface::showOptionsDialog( QWidget *parent, const QString &currentPage )
{
    Q_UNUSED(parent); Q_UNUSED(currentPage);
}

void LTZKQgisInterface::showProjectPropertiesDialog( const QString &currentPage )
{
    Q_UNUSED(currentPage);
}

void LTZKQgisInterface::buildStyleSheet( const QMap<QString, QVariant> &opts )
{
    Q_UNUSED(opts);
}

void LTZKQgisInterface::saveStyleSheetOptions( const QMap<QString, QVariant> &opts )
{
    Q_UNUSED(opts);
}

void LTZKQgisInterface::addPluginToMenu( const QString &name, QAction *action )
{
    addActionToNamedMenu(QStringLiteral("Plugins"), name, action);
}

void LTZKQgisInterface::removePluginMenu( const QString &name, QAction *action )
{
    removeActionFromNamedMenu(QStringLiteral("Plugins"), name, action);
}

void LTZKQgisInterface::insertAddLayerAction( QAction *action )
{
    addActionToNamedMenu(QStringLiteral("Plugins"), QString(), action);
}

void LTZKQgisInterface::removeAddLayerAction( QAction *action )
{
    removeActionFromNamedMenu(QStringLiteral("Plugins"), QString(), action);
}

void LTZKQgisInterface::addPluginToDatabaseMenu( const QString &name, QAction *action )
{
    addActionToNamedMenu(QStringLiteral("Plugins"), name, action);
}

void LTZKQgisInterface::removePluginDatabaseMenu( const QString &name, QAction *action )
{
    removeActionFromNamedMenu(QStringLiteral("Plugins"), name, action);
}

void LTZKQgisInterface::addPluginToRasterMenu( const QString &name, QAction *action )
{
    addActionToNamedMenu(QStringLiteral("Plugins"), name, action);
}

void LTZKQgisInterface::removePluginRasterMenu( const QString &name, QAction *action )
{
    removeActionFromNamedMenu(QStringLiteral("Plugins"), name, action);
}

void LTZKQgisInterface::addPluginToVectorMenu( const QString &name, QAction *action )
{
    addActionToNamedMenu(QStringLiteral("Plugins"), name, action);
}

void LTZKQgisInterface::removePluginVectorMenu( const QString &name, QAction *action )
{
    removeActionFromNamedMenu(QStringLiteral("Plugins"), name, action);
}

void LTZKQgisInterface::addPluginToWebMenu( const QString &name, QAction *action )
{
    addActionToNamedMenu(QStringLiteral("Web"), name, action);
}

void LTZKQgisInterface::removePluginWebMenu( const QString &name, QAction *action )
{
    removeActionFromNamedMenu(QStringLiteral("Web"), name, action);
}

void LTZKQgisInterface::addPluginToMeshMenu( const QString &name, QAction *action )
{
    addActionToNamedMenu(QStringLiteral("Plugins"), name, action);
}

void LTZKQgisInterface::removePluginMeshMenu( const QString &name, QAction *action )
{
    removeActionFromNamedMenu(QStringLiteral("Plugins"), name, action);
}

void LTZKQgisInterface::addDockWidget( Qt::DockWidgetArea area, QDockWidget *dockwidget )
{
    if (mApp && dockwidget) mApp->addDockWidget(area, dockwidget);
}

void LTZKQgisInterface::addTabifiedDockWidget( Qt::DockWidgetArea area, QDockWidget *dockwidget, const QStringList &tabifyWith, bool raiseTab )
{
    Q_UNUSED(tabifyWith); if (mApp && dockwidget) { mApp->addDockWidget(area, dockwidget); if (raiseTab) dockwidget->raise(); }
}

void LTZKQgisInterface::removeDockWidget( QDockWidget *dockwidget )
{
    if (mApp && dockwidget) mApp->removeDockWidget(dockwidget);
}

void LTZKQgisInterface::showLayerProperties( QgsMapLayer *l, const QString &page )
{
    Q_UNUSED(l); Q_UNUSED(page);
}

QDialog * LTZKQgisInterface::showAttributeTable( QgsVectorLayer *l, const QString &filterExpression )
{
    return mApp ? mApp->openAttributeTable(l, filterExpression) : nullptr;
}

void LTZKQgisInterface::addWindow( QAction *action )
{
    if (mApp && action) mApp->menuBar()->addAction(action);
}

void LTZKQgisInterface::removeWindow( QAction *action )
{
    if (mApp && action) mApp->menuBar()->removeAction(action);
}

bool LTZKQgisInterface::registerMainWindowAction( QAction *action, const QString &defaultShortcut )
{
    Q_UNUSED(defaultShortcut); if (mApp && action) mApp->addAction(action); return true;
}

bool LTZKQgisInterface::unregisterMainWindowAction( QAction *action )
{
    if (mApp && action) mApp->removeAction(action); return true;
}

void LTZKQgisInterface::registerMapLayerConfigWidgetFactory( QgsMapLayerConfigWidgetFactory *factory )
{
    Q_UNUSED(factory);
}

void LTZKQgisInterface::unregisterMapLayerConfigWidgetFactory( QgsMapLayerConfigWidgetFactory *factory )
{
    Q_UNUSED(factory);
}

void LTZKQgisInterface::registerOptionsWidgetFactory( QgsOptionsWidgetFactory *factory )
{
    Q_UNUSED(factory);
}

void LTZKQgisInterface::unregisterOptionsWidgetFactory( QgsOptionsWidgetFactory *factory )
{
    Q_UNUSED(factory);
}

void LTZKQgisInterface::registerProjectPropertiesWidgetFactory( QgsOptionsWidgetFactory *factory )
{
    Q_UNUSED(factory);
}

void LTZKQgisInterface::unregisterProjectPropertiesWidgetFactory( QgsOptionsWidgetFactory *factory )
{
    Q_UNUSED(factory);
}

void LTZKQgisInterface::registerDevToolWidgetFactory( QgsDevToolWidgetFactory *factory )
{
    Q_UNUSED(factory);
}

void LTZKQgisInterface::unregisterDevToolWidgetFactory( QgsDevToolWidgetFactory *factory )
{
    Q_UNUSED(factory);
}

void LTZKQgisInterface::registerApplicationExitBlocker( QgsApplicationExitBlockerInterface *blocker )
{
    Q_UNUSED(blocker);
}

void LTZKQgisInterface::unregisterApplicationExitBlocker( QgsApplicationExitBlockerInterface *blocker )
{
    Q_UNUSED(blocker);
}

void LTZKQgisInterface::registerMapToolHandler( QgsAbstractMapToolHandler *handler )
{
    Q_UNUSED(handler);
}

void LTZKQgisInterface::unregisterMapToolHandler( QgsAbstractMapToolHandler *handler )
{
    Q_UNUSED(handler);
}

void LTZKQgisInterface::registerCustomDropHandler( QgsCustomDropHandler *handler )
{
    Q_UNUSED(handler);
}

void LTZKQgisInterface::unregisterCustomDropHandler( QgsCustomDropHandler *handler )
{
    Q_UNUSED(handler);
}

void LTZKQgisInterface::registerCustomProjectOpenHandler( QgsCustomProjectOpenHandler *handler )
{
    Q_UNUSED(handler);
}

void LTZKQgisInterface::unregisterCustomProjectOpenHandler( QgsCustomProjectOpenHandler *handler )
{
    Q_UNUSED(handler);
}

void LTZKQgisInterface::registerCustomLayoutDropHandler( QgsLayoutCustomDropHandler *handler )
{
    Q_UNUSED(handler);
}

void LTZKQgisInterface::unregisterCustomLayoutDropHandler( QgsLayoutCustomDropHandler *handler )
{
    Q_UNUSED(handler);
}

void LTZKQgisInterface::openURL( const QString &url, bool useQgisDocDirectory )
{
    Q_UNUSED(useQgisDocDirectory); QDesktopServices::openUrl(QUrl(url));
}

bool LTZKQgisInterface::openFeatureForm( QgsVectorLayer *l, QgsFeature &f, bool updateFeatureOnly, bool showModal )
{
    Q_UNUSED(l); Q_UNUSED(f); Q_UNUSED(updateFeatureOnly); Q_UNUSED(showModal); return false;
}

QgsAttributeDialog * LTZKQgisInterface::getFeatureForm( QgsVectorLayer *l, QgsFeature &f )
{
    Q_UNUSED(l); Q_UNUSED(f); return nullptr;
}

void LTZKQgisInterface::preloadForm( const QString &uifile )
{
    Q_UNUSED(uifile);
}

void LTZKQgisInterface::locatorSearch( const QString &searchText )
{
    Q_UNUSED(searchText);
}

void LTZKQgisInterface::registerLocatorFilter( QgsLocatorFilter *filter )
{
    Q_UNUSED(filter);
}

void LTZKQgisInterface::deregisterLocatorFilter( QgsLocatorFilter *filter )
{
    Q_UNUSED(filter);
}

void LTZKQgisInterface::invalidateLocatorResults(  )
{
    
}

bool LTZKQgisInterface::askForDatumTransform( QgsCoordinateReferenceSystem sourceCrs, QgsCoordinateReferenceSystem destinationCrs )
{
    Q_UNUSED(sourceCrs); Q_UNUSED(destinationCrs); return true;
}

QgsBrowserGuiModel * LTZKQgisInterface::browserModel(  )
{
    return mApp ? mApp->browserModel() : nullptr;
}

void LTZKQgisInterface::setGpsPanelConnection( QgsGpsConnection *connection )
{
    Q_UNUSED(connection);
}
