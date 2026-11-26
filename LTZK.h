#pragma once

#include <QtWidgets/QMainWindow>
#include <QMap>
#include "ui_LTZK.h"
#include "PluginBindingConfig.h"
#include "MapServiceWorkspace.h"
#include "qgsmaplayer.h"
#include "qgslayertreemapcanvasbridge.h"
#include "QgsLayerTreeView.h"
#include "qgsmimedatautils.h"
#include "QLabel.h"
#include "qgsmaptoolzoom.h"
#include "qgsmaptoolpan.h"
#include "qgsrectangle.h"
#include <QDateTime>
#include <QColor>
#include <QList>
#include <QMetaObject>
#include <QPointer>
#include <QSet>
#include <QVariantMap>

class QDragEnterEvent;
class QDropEvent;
class QCloseEvent;
class QIcon;
class QFrame;
class QAction;
class QMenu;
class QDialog;
class QPlainTextEdit;
class QPushButton;
class QLineEdit;
class QTabWidget;
class QToolButton;
class QToolBar;
class QVBoxLayout;
class QTimer;
class QFileSystemWatcher;
class QThread;
class QScreen;
class QStackedWidget;
class QWidget;
class QgsDockWidget;
class QgsBrowserGuiModel;
class QgsDataSourceManagerDialog;
class QgsMessageBar;
class QgsRasterLayer;
class QgsScaleWidget;
class QgsVectorLayer;
class QgsFeature;
class QgsLayerTreeGroup;
class QgsLayerTreeViewDefaultActions;
class LTZKPluginManager;
class LTZKAccessControlDialog;
class DataCatalogWidget;
struct PluginBindingConfig;

class LTZK : public QMainWindow
{
    friend struct SeriesTableTypographyProbe;
    Q_OBJECT


public:
    LTZK(QWidget* parent = nullptr);
    ~LTZK();

    // 初始化图层树视图。
    void initLayerTreeView();


    QgsMapCanvas* mapCanvas() const;
    QgsLayerTreeView* layerTreeView() const;
    QgsLayerTreeMapCanvasBridge* layerTreeCanvasBridge() const;
    QgsBrowserGuiModel* browserModel() const;
    QgsMapLayer* currentMapLayer() const;
    QgsMessageBar* messageBarWidget() const;
    QMenu* pluginMenu() const;
    QToolBar* pluginToolBar() const;
    QMenu* ensureMenu(const QString& title);
    QMenu* ensurePluginSubMenu(const QString& pluginName);
    QgsVectorLayer* addPluginVectorLayer(const QString& vectorLayerPath,
                                         const QString& baseName,
                                         const QString& providerKey);
    QDialog* openAttributeTable(QgsVectorLayer* layer,
                                const QString& filterExpression = QString());

private:
    Ui::LTZKClass ui;

    struct PluginFeatureWindowEntry
    {
        QString pluginId;
        QString featureName;
        QString moduleTitle;
        QString signature;
        QPointer<QWidget> window;
        bool duplicate = false;
    };

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

public slots:
    // 打开工程。
    void on_actionOpenProject_triggered();
    // 保存工程。
    void on_actionSaveProject_triggered();
    // 另存工程。
    void on_actionSaveAsProject_triggered();
    // 退出。
    void on_actionExit_triggered();

    // 图层树控制。
    void on_actionLayerTreeControl_triggered();
    // 放大。
    void on_actionZoomIn_triggered();
    // 缩小。
    void on_actionZoomOut_triggered();
    // 浏览/平移。
    void on_actionPan_triggered();
    // 缩放至初始比例。
    void on_actionZoomActual_triggered();
    // 缩放至图层范围。
    void on_actionFullExtent_triggered();

    // 添加矢量数据。
    void on_actionAddVectorData_triggered();
    void on_actionDataSourceManager_triggered();
    // 添加栅格数据。
    void on_actionAddRasterData_triggered();
    // 当前图层变化。
    void onCurrentLayerChanged(QgsMapLayer* layer);
    // 移除图层。
    void removeLayer();
    // 添加 WMS 图层。
    void on_actionAddWmsLayer_triggered();
    // 添加 WFS 图层。
    void on_actionAddWfsLayer_triggered();
    // 添加 WCS 图层。
    void on_actionAddWcsLayer_triggered();
    // 打开属性表。
    void on_actionOpenAttrTable_triggered();
    // 打开字段管理器。
    void on_actionOpenFields_triggered();


    void mouseCoordinateChanged(const QgsPointXY& newCoordinate);
    // 将 Web Mercator 坐标转换为 WGS84 经纬度。
    void convertMercatorToWGS84(const QgsPoint& curPoint, QgsPoint& newPoint);

    // shp 坐标转换。
    void on_actionCoordTransShp_triggered();
    void on_actionCoordTransqgis_triggered();
    void on_actionCoordTranstif_triggered();
    void on_actionCoordTranspoint_triggered();
    void applyMenuDatabaseConnection(const QVariantMap& parameters);


private:
    enum class PlatformTheme
    {
        White,
        Black,
        SciFi,
        FieldNotes,
        SteelWarmWhite
    };


    void openProjectFile(const QString& filename);
    void addLayerFiles(const QStringList& filePaths);
    void openDataSourceManager(const QString& page = QString());
    QList<QgsMapLayer*> loadDataSources(const QStringList& sources,
        const QString& providerKey = QString(), int layerType = -1,
        const QString& name = QString(), const QString& encoding = QString());
    void registerSourceLayers(const QList<QgsMapLayer*>& layers, const QString& groupName = QString());
    void reportDataSourceErrors(const QStringList& errors);
    void replaceSourceVectorLayer(const QString& oldId, const QString& uri,
        const QString& name, const QString& providerKey);
    void renameLayerTreeGroup(QgsLayerTreeGroup* groupNode);
    void removeLayerTreeNodes(const QList<QgsLayerTreeNode*>& nodes);
    void removeSelectedLayerTreeNodes();
    void removeCheckedLayerTreeNodes();
    void initWorkspaceToolbars();
    void initWorkspaceMapStatusBar(QVBoxLayout* mapLayout);
    void updateWorkspaceMapCoordinates(const QgsPointXY& coordinate);
    void updateWorkspaceMapCrs();
    void showWorkspaceProjectCrsDialog();
    void setWorkspaceToolbarsActive(bool active);
    void resetWorkspaceToolbarLayout();
    void applyDefaultWorkspaceToolbarLayout();
    void saveMapWorkspaceToolbarState() const;
    void saveWorkspaceToolbarState() const;
    void showCurrentLayerProperties();
    void showLayerProperties(QgsMapLayer* layer);
    bool layerCanStartEditing(const QgsVectorLayer* layer) const;
    bool setLayerEditing(QgsVectorLayer* layer, bool enabled, bool confirmDiscard = true);
    bool saveLayerEdits(QgsVectorLayer* layer, bool keepEditing = true);
    bool rollbackLayerEdits(QgsVectorLayer* layer, bool confirm = true);
    void updateWorkspaceEditingActions();
    void addDigitizedFeature(QgsVectorLayer* layer, const QgsFeature& feature);
    void zoomToCurrentLayer();
    void identifyCurrentLayerFeature();
    void selectFeatureAtCanvasPoint(const QgsPointXY& point);
    void refreshLayerLegendMapFilter();
    void populateMapThemeMenu(QMenu* menu);
    void applyMapTheme(const QString& name);
    void createMapTheme();
    void updateMapTheme(const QString& name);
    void renameMapTheme(const QString& name);
    void removeMapTheme(const QString& name);
    QStringList collectSupportedLayerFiles(const QString& path) const;
    bool isProjectFile(const QString& filePath) const;
    bool isVectorLayerFile(const QString& filePath) const;
    bool isRasterLayerFile(const QString& filePath) const;
    bool isSupportedDataSourceFile(const QString& filePath) const;
    void openOwsSourceSelect(const QString& service);
    void loadLocalGansuShpBasemap();
    void styleLocalGansuBasemapLayer(QgsVectorLayer* layer);
    void setGansuInitialMapView();
    void setDefaultPanMapTool();
    void rememberInitialMapView();
    void resetInitialMapView();
    void restoreInitialMapView();
    void initMessageLogDock();
    QPlainTextEdit* messageLogTextEdit(const QString& tag);
    void appendMessageLogEntry(const QString& message, const QString& tag, int level);
    void bindPluginFeatureActions(const PluginBindingConfig& config);
    void refreshConfiguredPluginActionIcons();
    void executeConfiguredPluginFeature(const QString& pluginId, const QString& featureName);
    void executePluginFeatureAction(const PluginDefinition& plugin, const FeatureBinding& binding);
    void cachePluginActionsForDll(const QString& pluginPath, const QList<QAction*>& actions);
    QList<QAction*> pluginActionsForDll(const QString& pluginPath) const;
    void applySciFiStyleToTopLevelWidgets();
    void applySciFiStyleToPluginWindow(QWidget* widget);
    void applyPlatformStyleToTopLevelWidgets();
    void applyPlatformStyleToPluginWindow(QWidget* widget);
    void schedulePluginWindowPolish(QWidget* window);
    void polishPluginFrameHierarchy(QWidget* widget);
    void polishPluginGroupBoxTitles(QWidget* widget);
    void applyPluginWindowGlowEffects(QWidget* widget);
    void normalizeQualityWindowTableColumnWidths(QWidget* widget);
    void scheduleQualityWindowTableColumnWidthNormalization(QWidget* widget);
    void installPluginTableEmbeddedWidgetDelegates(QWidget* widget);
    void schedulePluginTableEmbeddedTextSuppression();
    void scheduleSeriesContentTablePolish();
    void polishSeriesContentTableWindows();
    void polishSeriesContentTableWindow(QWidget* widget);
    void normalizePluginTableEmbeddedControls(QWidget* widget);
    QString platformStyleSheet() const;
    QString platformThemeKey(PlatformTheme theme) const;
    QString platformThemeLabel(PlatformTheme theme) const;
    PlatformTheme platformThemeFromKey(const QString& key) const;
    bool isLightPlatformTheme() const;
    void loadSavedPlatformTheme();
    void savePlatformTheme() const;
    void setPlatformTheme(PlatformTheme theme);
    void showPlatformThemeDialog();
    void refreshPlatformThemeStyles();
    void bindUiScaleToCurrentScreen();
    void refreshUiScale();
    void refreshHomePageScaledMetrics();
    void refreshWorkspaceScaledMetrics();
    void showStartupLogin();
    bool promptLoginForModule(const QString& moduleTitle);
    void logoutCurrentUser();
    bool ensureAuthenticatedForModule(const QString& moduleTitle);
    bool canAccessModule(const QString& moduleTitle) const;
    void showLoginRequiredMessage();
    void showAccessDeniedMessage(const QString& moduleTitle);
    void showAccessControlPage();
    void authenticateFromHomePage();
    bool resetProductManagerPluginSession();
    bool leaveCurrentBusinessModule();
    bool closePluginFeatureWindowsForModule(const QString& moduleTitle);
    bool closeAllPluginFeatureWindows();
    bool destroyPluginFeatureWindowsForPlugin(const QString& pluginId);
    void destroyAllPluginFeatureWindows();
    bool activatePluginFeatureWindows(
        const QString& pluginId,
        const QString& featureName,
        const QString& moduleTitle);
    void capturePluginFeatureWindow(QWidget* window);
    bool isPluginWindowOrEmbeddedService(QWidget* widget) const;
    QString pluginFeatureWindowSignature(QWidget* window) const;
    void prunePluginFeatureWindows();
    void showHomeLoginPage();
    void showHomeModulesPage();
    void refreshAuthUiState();
    void publishCurrentUserSession();
    bool clearRemoteLayersForGuest();
    QString userLogPath() const;
    QString platformSettingsPath() const;
    void saveUserLog(const QString& userName, const QString& password);
    void clearUserLog();
    void initSciFiWorkspace();
    QWidget* buildSciFiHomePage();
    QWidget* buildHomeLoginPanel(QWidget* parent);
    QWidget* buildHomeModulesPanel(QWidget* parent);
    void updateHomeUtilityButtonMetrics();
    QWidget* buildAccessControlPage();
    QWidget* buildSciFiWorkspacePage();
    QToolButton* createModuleLaunchButton(const QString& moduleTitle, const QString& moduleCode, const QString& subtitle, const QString& objectName, const QIcon& icon);
    QToolButton* createWorkspaceToolButton(const QString& title, const QString& objectName, const QIcon& icon);
    QToolButton* createTopBarToolButton(QAction* action, QWidget* parent);
    void refreshModuleActionsFromMenus();
    QAction* firstExecutableActionForMenu(const QString& menuTitle) const;
    QList<QAction*> configuredFeatureActionsForModule(const QString& moduleTitle) const;
    void setWorkspacePluginActions(const QList<QAction*>& actions);
    void activateBusinessModule(const QString& moduleTitle);
    void updateWorkspaceTopBarThemeBackground(const QString& moduleTitle);
    void updateDataCatalogVisibility(const QString& moduleTitle);
    void scheduleQualityResultPostActionScan();
    void beginQualityResultMonitoring(
        const QString& moduleTitle,
        const QString& resultOrganization = QString());
    void bindQualityResultOutputWindow(QWidget* window);
    void refreshQualityResultOutputDirectories();
    void updateQualityResultOutputWatcher();
    void handleQualityResultDirectoryChanged(const QString& path);
    void handleQualityResultWorkerFinished();
    void armQualityResultScanAfterCompletion();
    void rememberQualityResultGroupPaths(QgsLayerTreeGroup* group);
    bool isQualityResultGroupPathSuppressed(const QString& folderPath) const;
    bool hasRunningQualityResultWorker() const;
    QStringList collectQualityResultSearchRoots() const;
    bool usesMapQualityErrorResultOrganization() const;
    QStringList collectMapQualityErrorSearchRoots() const;
    QStringList collectQualityErrorDirectories(const QString& searchBase) const;
    bool isQualityResultLayerInSearchRoots(
        const QString& sourcePath,
        const QStringList& searchRoots) const;
    bool isQualityResultLayerInSelectedOutputDirs(const QString& sourcePath) const;
    bool isQualityResultShpFileStable(const QString& shpPath);
    bool registerQualityResultLayer(QgsVectorLayer* layer, const QString& moduleTitle);
    QgsLayerTreeGroup* ensureQualityResultLayerGroup(const QString& sourcePath);
    void addQualityResultLayerToTree(QgsVectorLayer* layer);
    bool moveQualityResultLayerToGroupSafely(QgsVectorLayer* layer);
    int removeMapQualitySeriesIntermediateResultNodes();
    int regroupQualityResultLayersBySourcePath();
    void pruneQualityResultLayerRegistry();
    int syncQualityResultShpLayers();
    void refreshMapCanvasLayerStack();
    void hideQualityResultCanvasLayers();
    bool visibleLayersContainShp(const QList<QgsMapLayer*>& layers, bool includeSystemBasemap = false) const;
    QColor defaultMapCanvasColor() const;
    void applyWorkspaceShpCanvasBackground(bool useLightCanvas);
    void setShpLayerDisplayColor(QgsVectorLayer* layer);
    void chooseMapCanvasColor();
    void setQualityResultLayerVisible(const QString& resultPath, bool visible);
    void locateQualityResultFeature(const QString& resultPath, qint64 featureId);
    void loadCatalogDataSource(const QgsMimeDataUtils::Uri& uri);
    QString qualityResultFeatureIdField(const QgsVectorLayer* layer) const;
    QString qualityResultMessageField(const QgsVectorLayer* layer) const;
    QString qualityResultErrorTypeField(const QgsVectorLayer* layer) const;
    void on_actionCoordTrans_triggered();
    void on_actionLoadQgisPlugin_triggered();



private:
    QgsMapLayer* m_curMapLayer = nullptr; // 当前图层指针。

    QgsLayerTreeMapCanvasBridge*
        m_layerTreeCanvasBridge = nullptr;

    QgsMapCanvas* m_mapCanvas = nullptr; // 地图画布。

    QgsLayerTreeView* m_layerTreeView = nullptr; // 图层树视图。
    QgsLayerTreeViewDefaultActions* m_layerTreeDefaultActions = nullptr;
    QWidget* m_layerTreePanelWidget = nullptr; // 图层树面板容器。
    DataCatalogWidget* m_dataCatalogWidget = nullptr;
    QPointer<QgsDataSourceManagerDialog> m_dataSourceManager;
    QMap<QString, QPointer<QDialog>> m_attributeTableDialogs;
    QLineEdit* m_mapCoordinateEdit = nullptr;
    QLabel* m_mapCrsLabel = nullptr;
    QgsScaleWidget* m_mapScaleWidget = nullptr;
    QPointer<QgsBrowserGuiModel> m_sourceBrowserModel;
    QTabWidget* m_workspaceSideTabs = nullptr;


    QLabel* m_CurrentCoordinage = nullptr;

    // 地图交互工具。
    QgsMapTool* m_zoomInTool = nullptr;
    QgsMapTool* m_zoomOutTool = nullptr;

    QgsMapTool* m_panTool = nullptr;

    QMenu* m_pluginMenu = nullptr;
    QToolBar* m_pluginToolBar = nullptr;
    LTZKPluginManager* m_pluginManager = nullptr;
    QMap<QString, QList<QPointer<QAction>>> m_pluginActionsByDllPath;
    QList<PluginFeatureWindowEntry> m_pluginFeatureWindows;
    QList<QPointer<QWidget>> m_shutdownAuxiliaryWindows;
    bool m_closeEventInProgress = false;
    QString m_pluginWindowCapturePluginId;
    QString m_pluginWindowCaptureFeatureName;
    QString m_pluginWindowCaptureModuleTitle;
    QSet<QWidget*> m_pluginWindowCaptureBaseline;
    quint64 m_businessModuleGeneration = 0;
    quint64 m_pluginWindowCaptureBusinessModuleGeneration = 0;
    quint64 m_pluginWindowCaptureGeneration = 0;
    QStackedWidget* m_workspaceStack = nullptr;
    QWidget* m_homePage = nullptr;
    QWidget* m_homeLoginPage = nullptr;
    bool m_homeLoginPending = false;
    QWidget* m_homeModulesPage = nullptr;
    QWidget* m_accessControlPage = nullptr;
    QWidget* m_mapCanvasPage = nullptr;
    MapServiceWorkspace* m_workspaceContentStack = nullptr;
    QStackedWidget* m_homeContentStack = nullptr;
    QLabel* m_activeModuleTitle = nullptr;
    QString m_currentBusinessModuleTitle;
    QToolBar* m_layerTreeToolBar = nullptr;
    QLineEdit* m_layerTreeFilterEdit = nullptr;
    QList<QToolBar*> m_workspaceToolBars;
    QMainWindow* m_mapCanvasToolBarHost = nullptr;
    QToolButton* m_mapCanvasToolBarMenuButton = nullptr;
    QMap<QString, bool> m_mapWorkspaceToolbarVisibility;
    bool m_mapWorkspaceToolbarsActive = false;
    bool m_synchronizingWorkspaceToolbarVisibility = false;
    QList<QAction*> m_visibilityActions;
    bool m_layerLegendMapFilterEnabled = false;
    bool m_layerLegendExpressionFilterEnabled = false;
    QString m_activeMapThemeName;
    QgsMapTool* m_identifyFeatureTool = nullptr;
    QAction* m_identifyFeatureAction = nullptr;
    QgsMapTool* m_selectFeatureTool = nullptr;
    QgsMapTool* m_measureDistanceTool = nullptr;
    QgsMapTool* m_measureAreaTool = nullptr;
    QgsMapTool* m_capturePointTool = nullptr;
    QgsMapTool* m_captureLineTool = nullptr;
    QgsMapTool* m_capturePolygonTool = nullptr;
    QgsMapTool* m_vertexEditTool = nullptr;
    QAction* m_editModeAction = nullptr;
    QAction* m_saveLayerEditsAction = nullptr;
    QAction* m_rollbackLayerEditsAction = nullptr;
    QAction* m_endLayerEditingAction = nullptr;
    QAction* m_addPointFeatureAction = nullptr;
    QAction* m_addLineFeatureAction = nullptr;
    QAction* m_addPolygonFeatureAction = nullptr;
    QAction* m_vertexEditAction = nullptr;
    QAction* m_deleteSelectedFeaturesAction = nullptr;
    QAction* m_undoLayerEditAction = nullptr;
    QAction* m_redoLayerEditAction = nullptr;
    QPointer<QgsVectorLayer> m_workspaceEditingObservedLayer;
    QList<QMetaObject::Connection> m_workspaceEditingConnections;
    QList<QMetaObject::Connection> m_workspaceUndoConnections;
    QTimer* m_qualityResultScanTimer = nullptr;
    QFileSystemWatcher* m_qualityResultOutputWatcher = nullptr;
    QDateTime m_qualityResultScanStartedAt;
    bool m_qualityResultPostActionScanActive = false;
    bool m_qualityResultPostActionScanPending = false;
    bool m_qualityResultScanReady = false;
    int m_qualityResultPostActionScanAttempts = 0;
    QString m_qualityResultMonitoringModuleTitle;
    QPointer<QWidget> m_qualityResultMonitoringWindow;
    bool m_qualityResultWindowBindingPending = false;
    QMap<QString, QAction*> m_moduleActions;
    PluginBindingConfig m_pluginBindingConfig;
    QString m_qualityResultOrganization;
    QStringList m_qualityResultOutputDirs;
    QMap<QString, QString> m_qualityResultFileSignatures;
    QSet<QString> m_qualityResultStablePaths;
    QSet<QString> m_removedQualityResultGroupPaths;
    QMap<QString, QPointer<QgsVectorLayer>> m_qualityResultLayers;
    QMap<QString, QString> m_resultLayerModules;
    QSet<QString> m_qualityResultVisiblePaths;
    QgsMessageBar* m_messageBar = nullptr;
    QgsDockWidget* m_messageLogDock = nullptr;
    QTabWidget* m_messageLogTabs = nullptr;
    QAction* m_messageLogToggleAction = nullptr;
    QToolButton* m_messageLogToggleButton = nullptr;
    QToolButton* m_homeAccessControlButton = nullptr;
    QToolButton* m_homeLogoutButton = nullptr;
    QToolButton* m_homeThemeButton = nullptr;
    QToolButton* m_workspaceLogoutButton = nullptr;
    QLineEdit* m_homeLoginUserNameEdit = nullptr;
    QLineEdit* m_homeLoginPasswordEdit = nullptr;
    QLabel* m_homeLoginStatusLabel = nullptr;
    LTZKAccessControlDialog* m_accessControlWidget = nullptr;

    QgsRectangle m_initialMapExtent;
    double m_initialMapScale = 0.0;
    bool m_hasInitialMapView = false;
    PlatformTheme m_currentTheme = PlatformTheme::SciFi;
    bool m_constrainingMapView = false;
    bool m_workspaceUsesLightShpCanvas = false;
    QColor m_customCanvasColor;
    bool m_hasCustomCanvasColor = false;
    QPointer<QScreen> m_uiScaleScreen;
    QMetaObject::Connection m_uiScaleScreenChangedConnection;
    QMetaObject::Connection m_uiScaleGeometryChangedConnection;
    QMetaObject::Connection m_uiScaleDpiChangedConnection;
    qreal m_lastUiScale = 0.0;
    qreal m_lastHomeScale = 0.0;
};



