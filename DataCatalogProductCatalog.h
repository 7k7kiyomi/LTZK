#pragma once

#include <QList>
#include <QSqlDatabase>
#include <QSet>
#include <QString>

struct ProductDirectoryRecord
{
    int id = -1;
    int parentId = 0;
    QString name;
    QString description;
    int sortOrder = 0;
    int nodeType = 0;
};

struct ProductRecord
{
    int id = -1;
    QString productName;
    QString productType;
    QString fileFormat;
    QString filePath;
    int fileOid = 0;
    QString geometryType;
    int parentDirId = 0;
    QString directoryPath;
    QString layerTableName;
    int currentVersion = 1;
    QString securityLevel;
};

struct ProductImportSnapshot
{
    QSet<int> directoryIds;
    QSet<int> productIds;
};

class DataCatalogProductCatalog
{
public:
    explicit DataCatalogProductCatalog(const QSqlDatabase& database);

    bool hasProductCatalogSchema(QString* errorMessage = nullptr) const;
    bool ensureFixedDirectories(QString* errorMessage = nullptr) const;
    QSet<int> fixedProductLeafDirectoryIds(QString* errorMessage = nullptr) const;

    QList<ProductDirectoryRecord> childDirectories(
        int parentId,
        QString* errorMessage = nullptr) const;
    QList<ProductRecord> productsForDirectory(
        int directoryId,
        QString* errorMessage = nullptr) const;
    QList<ProductRecord> unassignedGdbProducts(
        QString* errorMessage = nullptr) const;
    ProductRecord product(int productId, QString* errorMessage = nullptr) const;
    bool captureImportSnapshot(
        ProductImportSnapshot* snapshot,
        QString* errorMessage = nullptr) const;
    bool relocateImportedProducts(
        int targetDirectoryId,
        const ProductImportSnapshot& before,
        QString* errorMessage = nullptr) const;

    bool createDirectory(
        int parentId,
        const QString& name,
        const QString& createdBy,
        int* directoryId,
        QString* errorMessage = nullptr) const;
    bool renameDirectory(
        int directoryId,
        const QString& name,
        QString* errorMessage = nullptr) const;
    bool deleteProduct(
        int productId,
        QString* errorMessage = nullptr) const;
    bool deleteProducts(
        const QList<int>& productIds,
        QString* errorMessage = nullptr) const;
    bool deleteDirectory(
        int directoryId,
        QString* errorMessage = nullptr) const;

    bool exportBlobProduct(
        const ProductRecord& product,
        const QString& outputDirectory,
        QString* outputPath,
        QString* errorMessage = nullptr) const;

private:
    bool relationExists(const QString& relationName) const;
    bool ensureDirectory(
        const QString& name,
        int parentId,
        int sortOrder,
        QString* errorMessage) const;
    bool deleteProductInTransaction(
        int productId,
        QString* errorMessage) const;
    QList<int> subtreeDirectoryIds(int directoryId, QString* errorMessage) const;
    bool queryFailed(const class QSqlQuery& query, QString* errorMessage) const;

    mutable QSqlDatabase m_database;
};
