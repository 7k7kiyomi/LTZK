#pragma once

#include "qgsmimedatautils.h"

#include <QModelIndex>
#include <QHash>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

#include <functional>

class QStandardItem;
class QStandardItemModel;
class QMenu;
class QPoint;
class QSqlDatabase;
class QToolButton;
class QTreeView;
class QLabel;
class QTimer;
struct ProductRecord;
class QgsBrowserGuiModel;
class QgsBrowserProxyModel;
class QgsBrowserTreeView;
class QgsDataItem;
class QgsFavoritesItem;

class DataCatalogWidget : public QWidget
{
public:
    using LoadCallback = std::function<void(const QgsMimeDataUtils::Uri&)>;
    using DataClipCallback = std::function<void()>;
    using DataImportCallback = std::function<void(int)>;
    using ProductOperationCallback = std::function<void(
        const QString& operation,
        int directoryId,
        int productId,
        const QList<int>& productIds)>;
    using LocalProductOperationCallback = std::function<void(
        const QString& operation,
        const QgsMimeDataUtils::UriList& localUris)>;

    explicit DataCatalogWidget(QWidget* parent = nullptr);
    ~DataCatalogWidget() override;

    QgsBrowserGuiModel* browserModel() const;
    void refreshDatabaseData();
    void setLoadCallback(LoadCallback callback);
    void setDataClipCallback(DataClipCallback callback);
    void setDataImportCallback(DataImportCallback callback);
    void setProductOperationCallback(ProductOperationCallback callback);
    void setLocalProductOperationCallback(LocalProductOperationCallback callback);
    QVariantMap databaseConnectionParameters() const;
    void applyMenuDatabaseConnection(const QVariantMap& parameters);
    void applySuccessfulMenuDatabaseConnection(
        const QVariantMap& parameters,
        bool remembered);

private:
    enum class CatalogSource
    {
        Database,
        Local
    };

    struct CatalogContext
    {
        CatalogSource source = CatalogSource::Database;
        QModelIndex viewIndex;
        QModelIndex sourceIndex;
        QStandardItem* databaseItem = nullptr;
        QgsDataItem* localItem = nullptr;
    };

    enum DatabaseItemRole
    {
        DatabaseLayerTypeRole = Qt::UserRole + 1,
        DatabaseSchemaRole,
        DatabaseTableRole,
        DatabaseGeometryColumnRole,
        DatabaseNameRole,
        ProductDirectoryIdRole,
        ProductIdRole,
        ProductNodeKindRole,
        ProductTypeRole,
        ProductGeometryTypeRole,
        ProductLayerTableRole,
        ProductFileOidRole,
        ProductFileFormatRole,
        ProductFilePathRole,
        ProductSecurityLevelRole,
        ProductDirectoryNodeTypeRole
    };

    enum class ProductCatalogNodeKind
    {
        None = 0,
        Directory = 1,
        Product = 2
    };

    enum class DataRemovalMode
    {
        PlatformOnly,
        Permanent
    };

    void buildUi();
    QVariantMap menuDatabaseConnectionParametersFromSettings() const;
    QString menuDatabaseConnectionSettingsSignature(const QVariantMap& parameters) const;
    bool synchronizeMenuDatabaseConnection(bool forceRetry);
    void beginSavedDatabaseConnection(const QVariantMap& parameters);
    bool openDatabaseConnection(
        const QVariantMap& parameters,
        QString* errorMessage);
    void closeDatabaseConnection();
    void loadDatabaseData();
    void initializeLocalBrowser();
    QModelIndex favoritesRootIndex();
    void showFavoritesInTree();
    void connectLocalFolder();
    void disconnectSelectedLocalFolder();
    void refreshLocalFolders();
    void activateDatabaseItem(const QModelIndex& index);
    void activateLocalItem(const QModelIndex& index);
    void showDatabaseMessage(const QString& message, bool isError);
    QStandardItem* schemaItem(const QString& schemaName, QStandardItem* databaseItem);
    bool loadProductDirectoryTree(
        QSqlDatabase database,
        QStandardItem* databaseItem,
        QString* errorMessage = nullptr);
    bool appendProductDirectoryChildren(
        QStandardItem* parentItem,
        int parentDirectoryId,
        QSqlDatabase database,
        const QSet<QString>& gdbSourceDirectoryNames,
        const QList<ProductRecord>& orphanGdbProducts);
    bool isProductCatalogDirectory(const QStandardItem* item) const;
    bool isProductCatalogProduct(const QStandardItem* item) const;
    bool isProductCatalogRoot(const QStandardItem* item) const;
    int productDirectoryId(const QStandardItem* item) const;
    int productId(const QStandardItem* item) const;
    QList<int> productIdsForContext(const CatalogContext& context) const;
    QList<int> allProductIdsUnderItem(const QStandardItem* item) const;
    void setProductSubtreeCheck(QStandardItem* item, Qt::CheckState state);
    void synchronizeProductCheckCache(
        const QStandardItem* item,
        const QList<int>& productIds,
        Qt::CheckState state);
    void refreshProductParentCheckStates(QStandardItem* item);
    void createProductDirectory(const CatalogContext& context);
    void deleteProductDirectory(const CatalogContext& context);
    void renameProductDirectory(const CatalogContext& context);
    void dispatchProductOperation(
        const QString& operation,
        const CatalogContext& context,
        const QList<int>& productIds = QList<int>(),
        const QgsMimeDataUtils::UriList& localUris =
            QgsMimeDataUtils::UriList());
    void addProductIdsToMap(const QList<int>& productIds);
    void addProductContextItemsToMap(const QVector<CatalogContext>& contexts);
    void addProductItemToMap(const CatalogContext& context);

    void showDatabaseContextMenu(const QPoint& position);
    void showLocalContextMenu(const QPoint& position);
    CatalogContext databaseContextAt(const QPoint& position) const;
    CatalogContext localContextAt(const QPoint& position) const;
    void populateContextMenu(QMenu& menu, const CatalogContext& context);
    void populateProductContextMenu(
        QMenu& menu,
        const CatalogContext& context);
    bool isProductMenuContext(const CatalogContext& context) const;
    bool isProductMenuDirectory(const CatalogContext& context) const;
    bool isProductMenuRoot(const CatalogContext& context) const;
    QgsMimeDataUtils::UriList localProductUrisForContext(
        const CatalogContext& context,
        bool checkedOnly) const;
    QVector<CatalogContext> checkedLocalContextsUnder(
        const CatalogContext& context) const;
    void setLocalProductSubtreeCheck(
        const CatalogContext& context,
        Qt::CheckState state);
    QString databaseCheckKey(const QStandardItem* item) const;
    bool isCheckableContext(const CatalogContext& context) const;
    bool isCheckedContext(const CatalogContext& context) const;
    QVector<CatalogContext> checkedContextsFor(
        const CatalogContext& context) const;
    QVector<CatalogContext> checkedDatabaseContexts() const;
    QVector<CatalogContext> checkedLocalContexts() const;
    void collectCheckedDatabaseContexts(
        const QStandardItem* item,
        QVector<CatalogContext>* contexts) const;
    void collectCheckedLocalContexts(
        const QModelIndex& parent,
        QVector<CatalogContext>* contexts) const;
    bool isCatalogLayer(const CatalogContext& context) const;
    bool isCatalogCollection(const CatalogContext& context) const;
    QgsMimeDataUtils::Uri databaseUri(const QStandardItem* item) const;
    QgsMimeDataUtils::UriList localUris(const QgsDataItem* item) const;
    void addContextItemToMap(const CatalogContext& context);
    void addContextItemsToMap(const QVector<CatalogContext>& contexts);
    void refreshContextItem(const CatalogContext& context);
    void refreshContextItems(const QVector<CatalogContext>& contexts);
    void showContextItemInfo(const CatalogContext& context);
    void importProductData(int targetDirectoryId);
    void importContextItem(const CatalogContext& context);
    void exportUri(const QgsMimeDataUtils::Uri& uri);
    void exportContextItem(const CatalogContext& context);
    void batchExportUris(
        const QgsMimeDataUtils::UriList& uris,
        int unloadedCount = 0,
        const QString& title = QStringLiteral("批量导出数据"));
    void batchExportContextItem(const CatalogContext& context);
    void batchExportContextItems(const QVector<CatalogContext>& contexts);
    void renameContextItem(const CatalogContext& context);
    void removeData(const CatalogContext& context);
    bool chooseDataRemovalMode(
        const CatalogContext& context,
        const QStringList& permanentTargets,
        DataRemovalMode* mode);
    void loadPlatformRemovalState();
    bool savePlatformRemovalState();
    QString databaseRemovalKey(
        const QString& databaseName,
        const QString& schemaName,
        const QString& tableName,
        const QString& layerType,
        const QString& geometryColumn) const;
    QString databaseRemovalKey(const QStandardItem* item) const;
    QString productRemovalKey(
        const QString& databaseName,
        int productId) const;
    QString localRemovalKey(const QgsDataItem* item) const;
    QStringList localDataPathsForContext(
        const CatalogContext& context) const;
    QStringList localDeleteTargetsForPath(const QString& sourcePath) const;
    bool permanentlyDeleteDatabaseData(
        const CatalogContext& context,
        QString* errorMessage);
    bool permanentlyDeleteLocalData(
        const CatalogContext& context,
        QStringList* deletedPaths,
        QStringList* failedPaths);
    bool isDeletableDataContext(const CatalogContext& context) const;
    bool canImportLocalDirectory(const CatalogContext& context) const;
    QString localImportTargetPath(const CatalogContext& context) const;
    QString uniqueLocalImportPath(const QString& targetDirectory, const QString& fileName) const;
    QStringList localImportCompanionFiles(const QString& sourcePath) const;
    bool copyLocalImportDataset(const QString& sourcePath, const QString& targetDirectory, QString* errorMessage) const;
    void refreshLocalImportTarget(const CatalogContext& context, const QString& targetDirectory);
    bool exportVectorUri(const QgsMimeDataUtils::Uri& uri, const QString& outputPath, QString* errorMessage) const;
    bool exportRasterUri(const QgsMimeDataUtils::Uri& uri, const QString& outputPath, QString* errorMessage) const;
    QgsMimeDataUtils::UriList exportUrisForContexts(
        const QVector<CatalogContext>& contexts,
        int* unloadedCount) const;
    QgsMimeDataUtils::UriList collectLocalUris(QgsDataItem* item, int* unloadedCount) const;
    QgsMimeDataUtils::UriList collectDatabaseUris(const QStandardItem* item) const;
    QString databaseDisplayAliasKey(const QStandardItem* item) const;
    QString displayNameForItem(const CatalogContext& context) const;

    QStandardItemModel* m_databaseModel = nullptr;
    QTreeView* m_databaseTree = nullptr;
    QgsBrowserGuiModel* m_browserModel = nullptr;
    QgsBrowserProxyModel* m_localProxyModel = nullptr;
    QgsBrowserTreeView* m_localTree = nullptr;
    QLabel* m_localStatusLabel = nullptr;
    QToolButton* m_disconnectFolderButton = nullptr;
    QSet<QString> m_checkedDatabaseKeys;
    QSet<QString> m_checkedLocalPaths;
    QSet<QString> m_removedDatabaseKeys;
    QSet<QString> m_removedLocalPaths;
    bool m_productCheckSyncing = false;
    QHash<QString, QString> m_databaseDisplayAliases;
    QHash<QString, QString> m_localDisplayAliases;
    QString m_databaseConnectionName;
    int m_databaseConnectionSerial = 0;
    quint64 m_savedDatabaseProbeGeneration = 0;
    bool m_savedDatabaseProbePending = false;
    QTimer* m_menuDatabaseSettingsTimer = nullptr;
    QString m_menuDatabaseSettingsSignature;
    QVariantMap m_liveMenuConnectionParameters;
    LoadCallback m_loadCallback;
    DataClipCallback m_dataClipCallback;
    DataImportCallback m_dataImportCallback;
    ProductOperationCallback m_productOperationCallback;
    LocalProductOperationCallback m_localProductOperationCallback;
};
