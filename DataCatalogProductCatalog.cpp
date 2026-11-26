#include "DataCatalogProductCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace
{
QString quotedSqlIdentifier(const QString& identifier)
{
    return QStringLiteral("\"%1\"")
        .arg(identifier.trimmed().replace(QStringLiteral("\""), QStringLiteral("\"\"")));
}

QString quotedSqlIdentifierPath(const QString& identifierPath)
{
    QStringList parts = identifierPath.split(QChar('.'), Qt::SkipEmptyParts);
    if (parts.isEmpty())
    {
        return QString();
    }

    QStringList quoted;
    for (const QString& part : parts)
    {
        quoted.append(quotedSqlIdentifier(part));
    }
    return quoted.join(QChar('.'));
}

QString safeFileName(QString name)
{
    name = name.trimmed();
    if (name.isEmpty())
    {
        name = QStringLiteral("product");
    }
    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    return name;
}

QString productOutputFileName(const ProductRecord& product)
{
    QString name = safeFileName(product.productName);
    QString suffix = product.fileFormat.trimmed();
    if (!suffix.isEmpty() && !name.endsWith(QStringLiteral(".") + suffix, Qt::CaseInsensitive))
    {
        name += QStringLiteral(".") + suffix;
    }
    return name;
}

bool execQuery(QSqlQuery& query, QString* errorMessage)
{
    if (query.exec())
    {
        return true;
    }
    if (errorMessage)
    {
        *errorMessage = query.lastError().text();
    }
    return false;
}

ProductRecord productRecordFromQuery(const QSqlQuery& query)
{
    ProductRecord product;
    product.id = query.value(0).toInt();
    product.productName = query.value(1).toString();
    product.productType = query.value(2).toString();
    product.fileFormat = query.value(3).toString();
    product.filePath = query.value(4).toString();
    product.fileOid = query.value(5).toInt();
    product.geometryType = query.value(6).toString();
    product.parentDirId = query.value(7).toInt();
    product.directoryPath = query.value(8).toString();
    product.layerTableName = query.value(9).toString();
    product.currentVersion = query.value(10).toInt();
    product.securityLevel = query.value(11).toString();
    return product;
}
}

DataCatalogProductCatalog::DataCatalogProductCatalog(const QSqlDatabase& database)
    : m_database(database)
{
}

bool DataCatalogProductCatalog::queryFailed(
    const QSqlQuery& query,
    QString* errorMessage) const
{
    if (errorMessage)
    {
        *errorMessage = query.lastError().text();
    }
    return true;
}

bool DataCatalogProductCatalog::hasProductCatalogSchema(QString* errorMessage) const
{
    if (!m_database.isValid() || !m_database.isOpen())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("数据库连接未打开。");
        }
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT COUNT(DISTINCT table_name) = 2 "
        "FROM information_schema.tables "
        "WHERE table_schema = current_schema() "
        "  AND table_name IN ('product_directory', 'product_metadata')"));
    if (!execQuery(query, errorMessage) || !query.next())
    {
        if (query.lastError().isValid())
        {
            queryFailed(query, errorMessage);
        }
        return false;
    }
    return query.value(0).toBool();
}

bool DataCatalogProductCatalog::relationExists(const QString& relationName) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT EXISTS ("
        "  SELECT 1 FROM information_schema.tables "
        "  WHERE table_schema = current_schema() AND table_name = :table_name"
        ")"));
    query.bindValue(QStringLiteral(":table_name"), relationName);
    return execQuery(query, nullptr) && query.next() && query.value(0).toBool();
}

bool DataCatalogProductCatalog::ensureDirectory(
    const QString& name,
    int parentId,
    int sortOrder,
    QString* errorMessage) const
{
    QSqlQuery find(m_database);
    find.prepare(QStringLiteral(
        "SELECT id FROM product_directory "
        "WHERE parent_id = :parent_id AND name = :name LIMIT 1"));
    find.bindValue(QStringLiteral(":parent_id"), parentId);
    find.bindValue(QStringLiteral(":name"), name);
    if (!execQuery(find, errorMessage))
    {
        return false;
    }
    if (find.next())
    {
        return true;
    }

    QSqlQuery insert(m_database);
    insert.prepare(QStringLiteral(
        "INSERT INTO product_directory "
        "(parent_id, name, description, sort_order, created_by, created_at, node_type) "
        "VALUES (:parent_id, :name, '', :sort_order, 'system', CURRENT_TIMESTAMP, 0)"));
    insert.bindValue(QStringLiteral(":parent_id"), parentId);
    insert.bindValue(QStringLiteral(":name"), name);
    insert.bindValue(QStringLiteral(":sort_order"), sortOrder);
    return execQuery(insert, errorMessage);
}

bool DataCatalogProductCatalog::ensureFixedDirectories(QString* errorMessage) const
{
    if (!hasProductCatalogSchema(errorMessage))
    {
        return false;
    }

    struct DirectoryDefinition
    {
        QString name;
        int parentId;
        int sortOrder;
    };

    // The first-level and second-level names intentionally mirror
    // DirectoryHelper::ensureFixedDirectories in map_product_manager_tools.
    const QString drawingResults = QStringLiteral("制图成果");
    const QString drawingElements = QStringLiteral("制图要素");
    const QString drawingMaterials = QStringLiteral("制图资料");
    if (!ensureDirectory(drawingResults, 0, 0, errorMessage) ||
        !ensureDirectory(drawingElements, 0, 1, errorMessage) ||
        !ensureDirectory(drawingMaterials, 0, 2, errorMessage))
    {
        return false;
    }

    auto directoryId = [this](const QString& name, int parentId, QString* error) -> int
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "SELECT id FROM product_directory "
            "WHERE parent_id = :parent_id AND name = :name LIMIT 1"));
        query.bindValue(QStringLiteral(":parent_id"), parentId);
        query.bindValue(QStringLiteral(":name"), name);
        if (!execQuery(query, error) || !query.next())
        {
            if (query.lastError().isValid())
            {
                *error = query.lastError().text();
            }
            return -1;
        }
        return query.value(0).toInt();
    };

    const int resultsId = directoryId(drawingResults, 0, errorMessage);
    const int elementsId = directoryId(drawingElements, 0, errorMessage);
    const int materialsId = directoryId(drawingMaterials, 0, errorMessage);
    if (resultsId <= 0 || elementsId <= 0 || materialsId <= 0)
    {
        if (errorMessage && errorMessage->isEmpty())
        {
            *errorMessage = QStringLiteral("读取固定成果目录失败。");
        }
        return false;
    }

    const QList<DirectoryDefinition> children = {
        {QStringLiteral("AI"), resultsId, 0},
        {QStringLiteral("PDF"), resultsId, 1},
        {QStringLiteral("其它"), resultsId, 2},
        {QStringLiteral("影像"), elementsId, 0},
        {QStringLiteral("晕渲"), elementsId, 1},
        {QStringLiteral("要素集"), elementsId, 2},
        {QStringLiteral("文档资料"), materialsId, 0},
        {QStringLiteral("表格资料"), materialsId, 1},
        {QStringLiteral("其它"), materialsId, 2}
    };
    for (const DirectoryDefinition& child : children)
    {
        if (!ensureDirectory(child.name, child.parentId, child.sortOrder, errorMessage))
        {
            return false;
        }
    }
    return true;
}

QSet<int> DataCatalogProductCatalog::fixedProductLeafDirectoryIds(
    QString* errorMessage) const
{
    QSet<int> result;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT leaf.id "
        "FROM product_directory leaf "
        "JOIN product_directory parent ON parent.id = leaf.parent_id "
        "WHERE parent.parent_id = 0 AND ("
        "  (parent.name = '制图成果' AND leaf.name IN ('AI', 'PDF', '其它')) OR "
        "  (parent.name = '制图要素' AND leaf.name IN ('影像', '晕渲', '要素集')) OR "
        "  (parent.name = '制图资料' AND leaf.name IN ('文档资料', '表格资料', '其它'))"
        ") "
        "ORDER BY parent.sort_order, leaf.sort_order, leaf.name"));
    if (!execQuery(query, errorMessage))
    {
        return result;
    }

    while (query.next())
    {
        const int directoryId = query.value(0).toInt();
        if (directoryId > 0)
        {
            result.insert(directoryId);
        }
    }
    return result;
}

QList<ProductDirectoryRecord> DataCatalogProductCatalog::childDirectories(
    int parentId,
    QString* errorMessage) const
{
    QList<ProductDirectoryRecord> result;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT id, parent_id, name, description, sort_order, node_type "
        "FROM product_directory "
        "WHERE parent_id = :parent_id "
        "ORDER BY sort_order, name"));
    query.bindValue(QStringLiteral(":parent_id"), parentId);
    if (!execQuery(query, errorMessage))
    {
        return result;
    }

    while (query.next())
    {
        ProductDirectoryRecord directory;
        directory.id = query.value(0).toInt();
        directory.parentId = query.value(1).toInt();
        directory.name = query.value(2).toString();
        directory.description = query.value(3).toString();
        directory.sortOrder = query.value(4).toInt();
        directory.nodeType = query.value(5).toInt();
        result.append(directory);
    }
    return result;
}

QList<ProductRecord> DataCatalogProductCatalog::productsForDirectory(
    int directoryId,
    QString* errorMessage) const
{
    QList<ProductRecord> result;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT id, product_name, product_type, file_format, file_path, file_oid, "
        "       geometry_type, parent_dir_id, directory_path, layer_table_name, "
        "       current_version, security_level "
        "FROM product_metadata "
        "WHERE parent_dir_id = :parent_id "
        "ORDER BY product_name, id"));
    query.bindValue(QStringLiteral(":parent_id"), directoryId);
    if (!execQuery(query, errorMessage))
    {
        return result;
    }

    while (query.next())
    {
        result.append(productRecordFromQuery(query));
    }
    return result;
}

QList<ProductRecord> DataCatalogProductCatalog::unassignedGdbProducts(
    QString* errorMessage) const
{
    QList<ProductRecord> result;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT id, product_name, product_type, file_format, file_path, file_oid, "
        "       geometry_type, parent_dir_id, directory_path, layer_table_name, "
        "       current_version, security_level "
        "FROM product_metadata "
        "WHERE parent_dir_id = -1 "
        "  AND (LOWER(COALESCE(file_format, '')) LIKE '%gdb%' "
        "       OR LOWER(COALESCE(file_format, '')) LIKE '%geodatabase%' "
        "       OR LOWER(COALESCE(file_path, '')) LIKE '%.gdb%') "
        "ORDER BY file_path, product_name, id"));
    if (!execQuery(query, errorMessage))
    {
        return result;
    }

    while (query.next())
    {
        result.append(productRecordFromQuery(query));
    }
    return result;
}

bool DataCatalogProductCatalog::captureImportSnapshot(
    ProductImportSnapshot* snapshot,
    QString* errorMessage) const
{
    if (!snapshot || !m_database.isValid() || !m_database.isOpen())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("无法读取导入前的成果目录状态。");
        }
        return false;
    }

    snapshot->directoryIds.clear();
    snapshot->productIds.clear();
    QSqlQuery directories(m_database);
    if (!directories.exec(QStringLiteral("SELECT id FROM product_directory")))
    {
        queryFailed(directories, errorMessage);
        return false;
    }
    while (directories.next())
    {
        snapshot->directoryIds.insert(directories.value(0).toInt());
    }

    QSqlQuery products(m_database);
    if (!products.exec(QStringLiteral("SELECT id FROM product_metadata")))
    {
        queryFailed(products, errorMessage);
        return false;
    }
    while (products.next())
    {
        snapshot->productIds.insert(products.value(0).toInt());
    }
    return true;
}

bool DataCatalogProductCatalog::relocateImportedProducts(
    int targetDirectoryId,
    const ProductImportSnapshot& before,
    QString* errorMessage) const
{
    if (!m_database.isValid() || !m_database.isOpen() ||
        targetDirectoryId <= 0 ||
        !before.directoryIds.contains(targetDirectoryId))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("选中的目标目录已不存在。");
        }
        return false;
    }

    QHash<int, int> parentByDirectory;
    QHash<int, QString> nameByDirectory;
    QSqlQuery directories(m_database);
    if (!directories.exec(QStringLiteral(
            "SELECT id, parent_id, name FROM product_directory")))
    {
        queryFailed(directories, errorMessage);
        return false;
    }
    while (directories.next())
    {
        const int id = directories.value(0).toInt();
        parentByDirectory.insert(id, directories.value(1).toInt());
        nameByDirectory.insert(id, directories.value(2).toString());
    }
    if (!parentByDirectory.contains(targetDirectoryId))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("选中的目标目录已被删除。");
        }
        return false;
    }

    QSet<int> newDirectoryRoots;
    QSet<int> directProductIds;
    QSqlQuery products(m_database);
    if (!products.exec(QStringLiteral(
            "SELECT id, parent_dir_id FROM product_metadata")))
    {
        queryFailed(products, errorMessage);
        return false;
    }
    while (products.next())
    {
        const int productId = products.value(0).toInt();
        if (before.productIds.contains(productId))
        {
            continue;
        }

        int directoryId = products.value(1).toInt();
        int highestNewDirectoryId = 0;
        QSet<int> visited;
        bool alreadyUnderTarget = false;
        while (directoryId > 0 && parentByDirectory.contains(directoryId) &&
               !visited.contains(directoryId))
        {
            visited.insert(directoryId);
            if (directoryId == targetDirectoryId)
            {
                alreadyUnderTarget = true;
                break;
            }
            if (!before.directoryIds.contains(directoryId))
            {
                highestNewDirectoryId = directoryId;
            }
            directoryId = parentByDirectory.value(directoryId);
        }
        if (alreadyUnderTarget)
        {
            continue;
        }
        if (highestNewDirectoryId > 0)
        {
            newDirectoryRoots.insert(highestNewDirectoryId);
        }
        else
        {
            directProductIds.insert(productId);
        }
    }

    if (newDirectoryRoots.isEmpty() && directProductIds.isEmpty())
    {
        return true;
    }

    QSqlQuery collision(m_database);
    collision.prepare(QStringLiteral(
        "SELECT id FROM product_directory "
        "WHERE parent_id = :target_id AND name = :name AND id <> :directory_id "
        "LIMIT 1"));
    for (const int directoryId : newDirectoryRoots)
    {
        collision.bindValue(QStringLiteral(":target_id"), targetDirectoryId);
        collision.bindValue(QStringLiteral(":name"), nameByDirectory.value(directoryId));
        collision.bindValue(QStringLiteral(":directory_id"), directoryId);
        if (!execQuery(collision, errorMessage))
        {
            return false;
        }
        if (collision.next())
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("目标目录下已存在同名子目录：%1")
                    .arg(nameByDirectory.value(directoryId));
            }
            return false;
        }
    }

    if (!m_database.transaction())
    {
        if (errorMessage)
        {
            *errorMessage = m_database.lastError().text();
        }
        return false;
    }
    QSqlQuery moveDirectory(m_database);
    moveDirectory.prepare(QStringLiteral(
        "UPDATE product_directory SET parent_id = :target_id WHERE id = :directory_id"));
    for (const int directoryId : newDirectoryRoots)
    {
        moveDirectory.bindValue(QStringLiteral(":target_id"), targetDirectoryId);
        moveDirectory.bindValue(QStringLiteral(":directory_id"), directoryId);
        if (!execQuery(moveDirectory, errorMessage))
        {
            m_database.rollback();
            return false;
        }
    }
    QSqlQuery moveProduct(m_database);
    moveProduct.prepare(QStringLiteral(
        "UPDATE product_metadata SET parent_dir_id = :target_id WHERE id = :product_id"));
    for (const int productId : directProductIds)
    {
        moveProduct.bindValue(QStringLiteral(":target_id"), targetDirectoryId);
        moveProduct.bindValue(QStringLiteral(":product_id"), productId);
        if (!execQuery(moveProduct, errorMessage))
        {
            m_database.rollback();
            return false;
        }
    }
    if (!m_database.commit())
    {
        if (errorMessage)
        {
            *errorMessage = m_database.lastError().text();
        }
        m_database.rollback();
        return false;
    }
    return true;
}

ProductRecord DataCatalogProductCatalog::product(
    int productId,
    QString* errorMessage) const
{
    const QList<ProductRecord> products = [&]()
    {
        QList<ProductRecord> result;
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "SELECT id, product_name, product_type, file_format, file_path, file_oid, "
            "       geometry_type, parent_dir_id, directory_path, layer_table_name, "
            "       current_version, security_level "
            "FROM product_metadata WHERE id = :id"));
        query.bindValue(QStringLiteral(":id"), productId);
        if (!execQuery(query, errorMessage))
        {
            return result;
        }
        if (query.next())
        {
            ProductRecord item;
            item.id = query.value(0).toInt();
            item.productName = query.value(1).toString();
            item.productType = query.value(2).toString();
            item.fileFormat = query.value(3).toString();
            item.filePath = query.value(4).toString();
            item.fileOid = query.value(5).toInt();
            item.geometryType = query.value(6).toString();
            item.parentDirId = query.value(7).toInt();
            item.directoryPath = query.value(8).toString();
            item.layerTableName = query.value(9).toString();
            item.currentVersion = query.value(10).toInt();
            item.securityLevel = query.value(11).toString();
            result.append(item);
        }
        return result;
    }();
    return products.isEmpty() ? ProductRecord() : products.first();
}

bool DataCatalogProductCatalog::createDirectory(
    int parentId,
    const QString& name,
    const QString& createdBy,
    int* directoryId,
    QString* errorMessage) const
{
    if (directoryId)
    {
        *directoryId = -1;
    }
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("目录名称不能为空。");
        }
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO product_directory "
        "(parent_id, name, description, sort_order, created_by, created_at, node_type) "
        "VALUES (:parent_id, :name, '', 0, :created_by, CURRENT_TIMESTAMP, 0) "
        "RETURNING id"));
    query.bindValue(QStringLiteral(":parent_id"), qMax(parentId, 0));
    query.bindValue(QStringLiteral(":name"), trimmedName);
    query.bindValue(QStringLiteral(":created_by"), createdBy.trimmed().isEmpty()
        ? QStringLiteral("system") : createdBy.trimmed());
    if (!execQuery(query, errorMessage) || !query.next())
    {
        if (query.lastError().isValid())
        {
            queryFailed(query, errorMessage);
        }
        return false;
    }
    if (directoryId)
    {
        *directoryId = query.value(0).toInt();
    }
    return true;
}

bool DataCatalogProductCatalog::renameDirectory(
    int directoryId,
    const QString& name,
    QString* errorMessage) const
{
    if (directoryId <= 0 || name.trimmed().isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("目录参数无效。");
        }
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE product_directory SET name = :name WHERE id = :id"));
    query.bindValue(QStringLiteral(":name"), name.trimmed());
    query.bindValue(QStringLiteral(":id"), directoryId);
    return execQuery(query, errorMessage);
}

QList<int> DataCatalogProductCatalog::subtreeDirectoryIds(
    int directoryId,
    QString* errorMessage) const
{
    QList<int> result;
    if (directoryId <= 0)
    {
        return result;
    }

    QList<int> pending;
    pending.append(directoryId);
    while (!pending.isEmpty())
    {
        const int currentId = pending.takeFirst();
        result.append(currentId);
        const QList<ProductDirectoryRecord> children = childDirectories(currentId, errorMessage);
        if (errorMessage && !errorMessage->isEmpty())
        {
            return QList<int>();
        }
        for (const ProductDirectoryRecord& child : children)
        {
            pending.append(child.id);
        }
    }
    return result;
}

bool DataCatalogProductCatalog::deleteProductInTransaction(
    int productId,
    QString* errorMessage) const
{
    QSqlQuery productQuery(m_database);
    productQuery.prepare(QStringLiteral(
        "SELECT file_oid, layer_table_name FROM product_metadata WHERE id = :id"));
    productQuery.bindValue(QStringLiteral(":id"), productId);
    if (!execQuery(productQuery, errorMessage) || !productQuery.next())
    {
        if (errorMessage && errorMessage->isEmpty())
        {
            *errorMessage = QStringLiteral("成果数据不存在：%1").arg(productId);
        }
        return false;
    }

    QSet<int> fileOids;
    const int currentOid = productQuery.value(0).toInt();
    if (currentOid > 0)
    {
        fileOids.insert(currentOid);
    }
    const QString layerTableName = productQuery.value(1).toString().trimmed();

    if (relationExists(QStringLiteral("version_records")))
    {
        QSqlQuery versionQuery(m_database);
        versionQuery.prepare(QStringLiteral(
            "SELECT file_oid FROM version_records WHERE product_id = :product_id"));
        versionQuery.bindValue(QStringLiteral(":product_id"), productId);
        if (!execQuery(versionQuery, errorMessage))
        {
            return false;
        }
        while (versionQuery.next())
        {
            const int oid = versionQuery.value(0).toInt();
            if (oid > 0)
            {
                fileOids.insert(oid);
            }
        }
    }

    for (const int oid : fileOids)
    {
        QSqlQuery unlinkQuery(m_database);
        unlinkQuery.prepare(QStringLiteral("SELECT lo_unlink(:oid)"));
        unlinkQuery.bindValue(QStringLiteral(":oid"), oid);
        if (!execQuery(unlinkQuery, errorMessage))
        {
            return false;
        }
    }

    if (!layerTableName.isEmpty())
    {
        const QString quotedTable = quotedSqlIdentifierPath(layerTableName);
        if (quotedTable.isEmpty())
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("成果图层表名无效：%1").arg(layerTableName);
            }
            return false;
        }
        QSqlQuery dropQuery(m_database);
        if (!dropQuery.exec(QStringLiteral("DROP TABLE IF EXISTS %1 CASCADE").arg(quotedTable)))
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("删除图层表 %1 失败：%2")
                    .arg(layerTableName, dropQuery.lastError().text());
            }
            return false;
        }
    }

    if (relationExists(QStringLiteral("layer_registry")))
    {
        QSqlQuery registryQuery(m_database);
        registryQuery.prepare(QStringLiteral(
            "DELETE FROM layer_registry WHERE product_id = :product_id"));
        registryQuery.bindValue(QStringLiteral(":product_id"), productId);
        if (!execQuery(registryQuery, errorMessage))
        {
            return false;
        }
    }

    QSqlQuery deleteQuery(m_database);
    deleteQuery.prepare(QStringLiteral("DELETE FROM product_metadata WHERE id = :id"));
    deleteQuery.bindValue(QStringLiteral(":id"), productId);
    return execQuery(deleteQuery, errorMessage);
}

bool DataCatalogProductCatalog::deleteProduct(
    int productId,
    QString* errorMessage) const
{
    return deleteProducts(QList<int>() << productId, errorMessage);
}

bool DataCatalogProductCatalog::deleteProducts(
    const QList<int>& productIds,
    QString* errorMessage) const
{
    if (errorMessage)
    {
        errorMessage->clear();
    }

    QList<int> uniqueProductIds;
    for (const int productId : productIds)
    {
        if (productId > 0 && !uniqueProductIds.contains(productId))
        {
            uniqueProductIds.append(productId);
        }
    }
    if (uniqueProductIds.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("没有可删除的成果数据。");
        }
        return false;
    }
    if (!m_database.isValid() || !m_database.isOpen())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("数据库连接未打开。");
        }
        return false;
    }
    if (!m_database.transaction())
    {
        if (errorMessage)
        {
            *errorMessage = m_database.lastError().text();
        }
        return false;
    }

    for (const int productId : uniqueProductIds)
    {
        if (!deleteProductInTransaction(productId, errorMessage))
        {
            m_database.rollback();
            return false;
        }
    }

    if (!m_database.commit())
    {
        if (errorMessage)
        {
            *errorMessage = m_database.lastError().text();
        }
        m_database.rollback();
        return false;
    }
    return true;
}

bool DataCatalogProductCatalog::deleteDirectory(
    int directoryId,
    QString* errorMessage) const
{
    if (directoryId <= 0)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("根目录不允许删除。");
        }
        return false;
    }

    const QList<int> directoryIds = subtreeDirectoryIds(directoryId, errorMessage);
    if (directoryIds.isEmpty())
    {
        return false;
    }
    if (!m_database.transaction())
    {
        if (errorMessage)
        {
            *errorMessage = m_database.lastError().text();
        }
        return false;
    }

    for (const int currentDirId : directoryIds)
    {
        const QList<ProductRecord> products = productsForDirectory(currentDirId, errorMessage);
        if (errorMessage && !errorMessage->isEmpty())
        {
            m_database.rollback();
            return false;
        }
        for (const ProductRecord& productRecord : products)
        {
            if (!deleteProductInTransaction(productRecord.id, errorMessage))
            {
                m_database.rollback();
                return false;
            }
        }
    }

    for (int index = directoryIds.size() - 1; index >= 0; --index)
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("DELETE FROM product_directory WHERE id = :id"));
        query.bindValue(QStringLiteral(":id"), directoryIds.at(index));
        if (!execQuery(query, errorMessage))
        {
            m_database.rollback();
            return false;
        }
    }

    if (!m_database.commit())
    {
        if (errorMessage)
        {
            *errorMessage = m_database.lastError().text();
        }
        m_database.rollback();
        return false;
    }
    return true;
}

bool DataCatalogProductCatalog::exportBlobProduct(
    const ProductRecord& productRecord,
    const QString& outputDirectory,
    QString* outputPath,
    QString* errorMessage) const
{
    if (outputPath)
    {
        outputPath->clear();
    }
    if (productRecord.fileOid <= 0)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("成果没有可导出的数据库文件对象。");
        }
        return false;
    }

    QDir outputDir(outputDirectory);
    if (!outputDir.exists() && !outputDir.mkpath(QStringLiteral(".")))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("无法创建导出目录：%1").arg(outputDirectory);
        }
        return false;
    }

    const QString path = outputDir.filePath(productOutputFileName(productRecord));
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT lo_get(:oid)"));
    query.bindValue(QStringLiteral(":oid"), productRecord.fileOid);
    if (!execQuery(query, errorMessage) || !query.next())
    {
        return false;
    }
    const QByteArray data = query.value(0).toByteArray();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("写入导出文件失败：%1").arg(file.errorString());
        }
        return false;
    }
    file.close();
    if (outputPath)
    {
        *outputPath = path;
    }
    return true;
}
