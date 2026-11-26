#include "DataCatalogWidget.h"
#include "PlatformMenuDatabaseConnectionObserver.h"

#include "CatalogCheckedSelection.h"
#include "DataCatalogProductCatalog.h"
#include "auth/LTZKAuthManager.h"

#include "qgsbrowserguimodel.h"
#include "qgsbrowserproxymodel.h"
#include "qgsbrowsertreeview.h"
#include "qgsapplication.h"
#include "qgsdatacollectionitem.h"
#include "qgsdataitem.h"
#include "qgsdatasourceuri.h"
#include "qgsdirectoryitem.h"
#include "qgsfavoritesitem.h"
#include "qgslayeritem.h"
#include "qgsproject.h"
#include "qgsrasterfilewriter.h"
#include "qgsrasterlayer.h"
#include "qgsrasterpipe.h"
#include "qgsvectorfilewriter.h"
#include "qgsvectorlayer.h"
#include "qgswkbtypes.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QSplitter>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QSize>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStyle>
#include <QSettings>
#include <QStringList>
#include <QToolButton>
#include <QTreeView>
#include <QTimer>
#include <QThread>
#include <QUuid>
#include <memory>
#include <QVBoxLayout>
#include <QVariant>

namespace
{
    const char* kCatalogDatabaseConnectionPrefix = "LTZKDataCatalogPostgisConnection";

    QSqlDatabase catalogDatabase(const QString& connectionName)
    {
        if (connectionName.isEmpty() || !QSqlDatabase::contains(connectionName))
        {
            return QSqlDatabase();
        }
        return QSqlDatabase::database(connectionName, false);
    }

    QString databaseIdentityKey(
        const QSqlDatabase& database,
        const QString& databaseName)
    {
        return QStringLiteral("%1\x1f%2\x1f%3\x1f%4")
            .arg(database.hostName().trimmed().toLower())
            .arg(database.port())
            .arg(databaseName.trimmed())
            .arg(database.userName().trimmed().toLower());
    }

    QString quotedSqlIdentifier(const QString& identifier)
    {
        return QStringLiteral("\"%1\"")
            .arg(identifier.trimmed().replace(QStringLiteral("\""), QStringLiteral("\"\"")));
    }

    QString databaseCatalogQuery()
    {
        return QStringLiteral(
            "SELECT t.table_schema, t.table_name "
            "FROM information_schema.tables t "
            "WHERE t.table_type IN ('BASE TABLE', 'VIEW', 'FOREIGN') "
            "  AND t.table_schema NOT IN ('pg_catalog', 'information_schema', 'topology') "
            "  AND t.table_schema NOT LIKE 'pg_toast%' "
            "  AND t.table_name NOT IN ("
            "    'user_permissions', 'spatial_ref_sys', 'geometry_columns', "
            "    'geography_columns', 'raster_columns', 'raster_overviews'"
            "  ) "
            "  AND has_table_privilege(format('%I.%I', t.table_schema, t.table_name), 'SELECT') "
            "ORDER BY t.table_schema, t.table_name");
    }

    bool databaseMetadataRelationExists(
        QSqlDatabase database,
        const QString& relationName,
        QString* errorMessage)
    {
        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "SELECT to_regclass(:relation_name) IS NOT NULL"));
        query.bindValue(QStringLiteral(":relation_name"), relationName);
        if (!query.exec())
        {
            if (errorMessage)
            {
                *errorMessage = query.lastError().text();
            }
            return false;
        }
        if (!query.next())
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("未返回元数据关系检查结果。");
            }
            return false;
        }
        return query.value(0).toBool();
    }

    QString databaseItemSuffix(
        const QString& layerType,
        const QString& geometryType,
        int srid,
        const QString& dataColumn)
    {
        if (layerType == QStringLiteral("raster"))
        {
            if (!dataColumn.isEmpty())
            {
                return QStringLiteral(" [栅格 · %1]").arg(dataColumn);
            }
            return QStringLiteral(" [栅格]");
        }
        if (layerType == QStringLiteral("table"))
        {
            return QStringLiteral(" [数据表]");
        }

        QString detail = geometryType.trimmed();
        if (detail.isEmpty())
        {
            detail = QStringLiteral("空间图层");
        }
        if (!dataColumn.isEmpty())
        {
            detail += QStringLiteral(" · %1").arg(dataColumn);
        }
        if (srid > 0)
        {
            detail += QStringLiteral(" · EPSG:%1").arg(srid);
        }
        return QStringLiteral(" [%1]").arg(detail);
    }

    QString databaseVectorMetadataQuery()
    {
        return QStringLiteral(
            "SELECT f_table_schema, f_table_name, f_geometry_column, type, srid "
            "FROM geometry_columns");
    }

    QString databaseRasterMetadataQuery()
    {
        return QStringLiteral(
            "SELECT r_table_schema, r_table_name, r_raster_column, 'RASTER', srid "
            "FROM raster_columns");
    }

    struct DatabaseCatalogEntry
    {
        QString schemaName;
        QString tableName;
        QString dataColumn;
        QString dataType;
        int srid = 0;
        QString layerType = QStringLiteral("table");
    };

    QString databaseCatalogKey(const QString& schemaName, const QString& tableName)
    {
        return schemaName + QChar(0x1f) + tableName;
    }

    QIcon catalogIcon(const QString& name)
    {
        const QIcon icon(QStringLiteral(":/LTZK/Resources/catalog_icons/%1.svg").arg(name));
        return icon.isNull()
            ? QApplication::style()->standardIcon(QStyle::SP_FileIcon)
            : icon;
    }

    QIcon imageryCatalogIcon()
    {
        return QgsLayerItem::iconRaster();
    }

    QIcon hillshadeCatalogIcon()
    {
        QIcon icon = QgsApplication::getThemeIcon(
            QStringLiteral("/propertyicons/rendering.svg"));
        if (icon.isNull())
        {
            icon = QgsApplication::getThemeIcon(
                QStringLiteral("/propertyicons/render.svg"));
        }
        return icon;
    }

    bool isGdbFormatOrPath(const QString& value)
    {
        QString normalized = value.trimmed().toLower();
        if (normalized.isEmpty())
        {
            return false;
        }

        normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
        const QStringList pathParts = normalized.split(QLatin1Char('/'));
        for (const QString& part : pathParts)
        {
            if (part.endsWith(QStringLiteral(".gdb")))
            {
                return true;
            }
        }

        QString compactFormat = normalized;
        compactFormat.remove(QRegularExpression(QStringLiteral("[^a-z0-9]")));
        return QStringList({
            QStringLiteral("gdb"),
            QStringLiteral("filegdb"),
            QStringLiteral("openfilegdb"),
            QStringLiteral("filegeodatabase"),
            QStringLiteral("esrifilegeodatabase")}).contains(compactFormat);
    }

    bool isGdbProductFormat(const QString& value)
    {
        const QString normalized = value.trimmed().toLower();
        return isGdbFormatOrPath(normalized) ||
            normalized.contains(QStringLiteral("gdb")) ||
            normalized.contains(QStringLiteral("geodatabase"));
    }

    QSet<QString> catalogGdbSourceDirectoryNames(QSqlDatabase database)
    {
        QSet<QString> sourceNames;
        QSqlQuery query(database);
        // Imported GDB layers can be unassigned (parent_dir_id = -1) while
        // their source directory remains. Resolve source names independently
        // of those directory links so the container keeps its GDB identity.
        if (!query.exec(QStringLiteral(
                "SELECT DISTINCT file_path FROM product_metadata "
                "WHERE LOWER(COALESCE(file_format, '')) LIKE '%gdb%' "
                "   OR LOWER(COALESCE(file_format, '')) LIKE '%geodatabase%' "
                "   OR LOWER(COALESCE(file_path, '')) LIKE '%.gdb%'")))
        {
            return sourceNames;
        }
        while (query.next())
        {
            QString sourcePath = query.value(0).toString().trimmed();
            sourcePath.replace(QLatin1Char('\\'), QLatin1Char('/'));
            const QStringList parts = sourcePath.split(QLatin1Char('/'));
            for (const QString& part : parts)
            {
                if (part.endsWith(QStringLiteral(".gdb"), Qt::CaseInsensitive))
                {
                    sourceNames.insert(
                        QFileInfo(part).completeBaseName().trimmed().toCaseFolded());
                }
            }
        }
        return sourceNames;
    }

    bool catalogGdbSourceMatchesDirectory(
        const QString& sourcePath,
        const QString& directoryName)
    {
        const QString directoryKey = QFileInfo(directoryName.trimmed())
            .completeBaseName().trimmed().toCaseFolded();
        if (directoryKey.isEmpty())
        {
            return false;
        }

        QString normalizedPath = QDir::fromNativeSeparators(sourcePath.trimmed());
        normalizedPath.replace(QLatin1Char('\\'), QLatin1Char('/'));
        const QStringList parts = normalizedPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        for (const QString& part : parts)
        {
            if (part.endsWith(QStringLiteral(".gdb"), Qt::CaseInsensitive) &&
                QFileInfo(part).completeBaseName().trimmed().toCaseFolded() == directoryKey)
            {
                return true;
            }
        }
        return false;
    }

    bool isCatalogCategoryDirectory(const QString& name)
    {
        const QString normalized = name.trimmed();
        if (normalized.compare(QStringLiteral("AI"), Qt::CaseInsensitive) == 0 ||
            normalized.compare(QStringLiteral("PDF"), Qt::CaseInsensitive) == 0)
        {
            return true;
        }
        return QStringList({
            QStringLiteral("\u5236\u56fe\u6210\u679c"),
            QStringLiteral("\u5236\u56fe\u8981\u7d20"),
            QStringLiteral("\u5236\u56fe\u8d44\u6599"),
            QStringLiteral("\u5f71\u50cf"),
            QStringLiteral("\u6655\u6e32"),
            QStringLiteral("\u8981\u7d20\u96c6"),
            QStringLiteral("\u6587\u6863\u8d44\u6599"),
            QStringLiteral("\u8868\u683c\u8d44\u6599"),
            QStringLiteral("\u5176\u5b83")}).contains(normalized);
    }

    QIcon catalogDirectoryIcon(const QString& name)
    {
        const QString normalized = name.trimmed();
        if (normalized == QStringLiteral("\u5f71\u50cf")) return catalogIcon(QStringLiteral("imagery"));
        if (normalized == QStringLiteral("\u6655\u6e32")) return hillshadeCatalogIcon();
        if (normalized == QStringLiteral("\u5236\u56fe\u6210\u679c")) return catalogIcon(QStringLiteral("results"));
        if (normalized == QStringLiteral("\u5236\u56fe\u8981\u7d20")) return catalogIcon(QStringLiteral("elements"));
        if (normalized == QStringLiteral("\u5236\u56fe\u8d44\u6599")) return catalogIcon(QStringLiteral("materials"));
        if (normalized.compare(QStringLiteral("AI"), Qt::CaseInsensitive) == 0) return catalogIcon(QStringLiteral("ai"));
        if (normalized.compare(QStringLiteral("PDF"), Qt::CaseInsensitive) == 0) return catalogIcon(QStringLiteral("pdf"));
        if (normalized == QStringLiteral("\u8981\u7d20\u96c6")) return catalogIcon(QStringLiteral("feature_set"));
        if (normalized == QStringLiteral("\u6587\u6863\u8d44\u6599")) return catalogIcon(QStringLiteral("document"));
        if (normalized == QStringLiteral("\u8868\u683c\u8d44\u6599")) return catalogIcon(QStringLiteral("spreadsheet"));
        if (normalized == QStringLiteral("\u5176\u5b83")) return catalogIcon(QStringLiteral("other"));
        return catalogIcon(QStringLiteral("folder"));
    }

    QIcon catalogFileFormatIcon(const QString& formatOrPath)
    {
        if (isGdbFormatOrPath(formatOrPath))
        {
            return QgsDataCollectionItem::iconDataCollection();
        }
        const QString suffix = QFileInfo(formatOrPath.trimmed()).suffix().toLower();
        const QString format = suffix.isEmpty()
            ? formatOrPath.trimmed().remove(QChar('.')).toLower()
            : suffix;
        if (format == QStringLiteral("ai") || format == QStringLiteral("ait"))
            return catalogIcon(QStringLiteral("ai"));
        if (format == QStringLiteral("pdf"))
            return catalogIcon(QStringLiteral("pdf"));
        if (QStringList({QStringLiteral("doc"), QStringLiteral("docx"),
            QStringLiteral("odt"), QStringLiteral("rtf"), QStringLiteral("txt")}).contains(format))
            return catalogIcon(QStringLiteral("document"));
        if (QStringList({QStringLiteral("xls"), QStringLiteral("xlsx"),
            QStringLiteral("ods"), QStringLiteral("csv")}).contains(format))
            return catalogIcon(QStringLiteral("spreadsheet"));
        if (QStringList({QStringLiteral("png"), QStringLiteral("jpg"),
            QStringLiteral("jpeg"), QStringLiteral("tif"), QStringLiteral("tiff")}).contains(format))
            return imageryCatalogIcon();
        return QIcon();
    }

    QIcon databaseItemIcon(const QString& layerType)
    {
        if (layerType == QStringLiteral("raster"))
        {
            return imageryCatalogIcon();
        }
        if (layerType == QStringLiteral("table"))
        {
            return catalogIcon(QStringLiteral("table"));
        }
        return catalogIcon(QStringLiteral("elements"));
    }

    bool isProductRasterType(const QString& productType)
    {
        const QString normalizedType = productType.trimmed().toLower();
        return normalizedType.contains(QStringLiteral("栅格")) ||
            normalizedType.contains(QStringLiteral("raster"));
    }

    bool isProductVectorType(const QString& productType)
    {
        const QString normalizedType = productType.trimmed().toLower();
        return normalizedType.contains(QStringLiteral("矢量")) ||
            normalizedType.contains(QStringLiteral("vector"));
    }

    QIcon productCatalogItemIcon(
        const QString& productType,
        const QString& geometryType,
        const QString& fileFormat,
        const QString& filePath,
        const QString& productName)
    {
        // Resolve the dataset before its storage container. A source path
        // containing .gdb does not turn a feature class into a geodatabase.
        const QString normalizedType = productType.trimmed().toLower();
        const QString normalizedGeometry = geometryType.trimmed().toLower();
        if (isProductRasterType(productType))
        {
            return imageryCatalogIcon();
        }
        if (normalizedType.contains(QStringLiteral("table"))
            || normalizedType.contains(QStringLiteral("\u8868"))
            || normalizedGeometry == QStringLiteral("nogeometry")
            || normalizedGeometry == QStringLiteral("none")
            || normalizedGeometry == QStringLiteral("null"))
        {
            return catalogIcon(QStringLiteral("table"));
        }
        if (isProductVectorType(productType) || !normalizedGeometry.isEmpty())
        {
            if (normalizedGeometry.contains(QStringLiteral("point")) ||
                normalizedGeometry.contains(QStringLiteral("点")))
                return catalogIcon(QStringLiteral("point"));
            if (normalizedGeometry.contains(QStringLiteral("line")) ||
                normalizedGeometry.contains(QStringLiteral("curve")) ||
                normalizedGeometry.contains(QStringLiteral("线")))
                return catalogIcon(QStringLiteral("line"));
            if (normalizedGeometry.contains(QStringLiteral("polygon")) ||
                normalizedGeometry.contains(QStringLiteral("面")))
            {
                return catalogIcon(QStringLiteral("polygon"));
            }
            if (isProductVectorType(productType))
                return catalogIcon(QStringLiteral("elements"));
        }
        for (const QString& candidate : {fileFormat, filePath, productName})
        {
            if (isGdbFormatOrPath(candidate) || isGdbProductFormat(candidate)) continue;
            const QIcon fileIcon = catalogFileFormatIcon(candidate);
            if (!fileIcon.isNull()) return fileIcon;
        }
        if (productName.endsWith(QStringLiteral(".gdb"), Qt::CaseInsensitive)
            && (isGdbProductFormat(fileFormat) || isGdbFormatOrPath(filePath)))
        {
            return QgsDataCollectionItem::iconDataCollection();
        }
        return catalogIcon(QStringLiteral("file"));
    }

    QIcon productCatalogItemIconForDirectory(
        const ProductRecord& product,
        const QString& directoryName)
    {
        if (directoryName == QStringLiteral("\u5f71\u50cf"))
        {
            return catalogIcon(QStringLiteral("raster"));
        }
        if (directoryName == QStringLiteral("\u6655\u6e32"))
        {
            return hillshadeCatalogIcon();
        }
        return productCatalogItemIcon(
            product.productType,
            product.geometryType,
            product.fileFormat,
            product.filePath,
            product.productName);
    }

    QIcon drawProductLayerIcon(int kind)
    {
        // 0=point, 1=line, 2=polygon, 3=raster; shared by both trees.
        if (kind == 0) return catalogIcon(QStringLiteral("point"));
        if (kind == 1) return catalogIcon(QStringLiteral("line"));
        if (kind == 2) return catalogIcon(QStringLiteral("polygon"));
        return imageryCatalogIcon();
    }

    int productLayerIconKind(const QString& productType, const QString& geometryType)
    {
        if (isProductRasterType(productType))
        {
            return 3;
        }
        if (!isProductVectorType(productType))
        {
            return -1;
        }

        const QString normalizedGeometry = geometryType.trimmed().toLower();
        if (normalizedGeometry.contains(QStringLiteral("point")) ||
            normalizedGeometry.contains(QStringLiteral("点")))
        {
            return 0;
        }
        if (normalizedGeometry.contains(QStringLiteral("line")) ||
            normalizedGeometry.contains(QStringLiteral("curve")) ||
            normalizedGeometry.contains(QStringLiteral("线")))
        {
            return 1;
        }
        if (normalizedGeometry.contains(QStringLiteral("polygon")) ||
            normalizedGeometry.contains(QStringLiteral("surface")) ||
            normalizedGeometry.contains(QStringLiteral("面")))
        {
            return 2;
        }
        return -1;
    }

    QFrame* createSectionHeader(
        const QString& text,
        QWidget* parent,
        const QList<QToolButton*>& buttons,
        QToolButton** titleButton)
    {
        QFrame* header = new QFrame(parent);
        header->setProperty("catalogSectionHeader", true);
        QHBoxLayout* layout = new QHBoxLayout(header);
        layout->setContentsMargins(8, 4, 4, 4);
        layout->setSpacing(4);

        QToolButton* title = new QToolButton(header);
        title->setProperty("catalogSectionTitle", true);
        title->setText(QString());
        title->setAccessibleName(text);
        title->setToolButtonStyle(Qt::ToolButtonTextOnly);
        title->setAutoRaise(true);
        title->setCursor(Qt::PointingHandCursor);
        title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        QHBoxLayout* titleLayout = new QHBoxLayout(title);
        titleLayout->setContentsMargins(0, 0, 0, 0);
        titleLayout->setSpacing(0);
        QLabel* titleLabel = new QLabel(text, title);
        titleLabel->setProperty("catalogSectionTitle", true);
        titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        titleLayout->addWidget(titleLabel);

        layout->addWidget(title, 1);
        for (QToolButton* button : buttons)
        {
            layout->addWidget(button, 0, Qt::AlignVCenter);
        }

        if (titleButton)
        {
            *titleButton = title;
        }
        return header;
    }

    QToolButton* createCatalogButton(
        const QString& iconPath,
        const QString& accessibleName,
        const QString& tooltip,
        QWidget* parent)
    {
        QToolButton* button = new QToolButton(parent);
        button->setObjectName(QStringLiteral("catalogToolbarButton"));
        button->setProperty("catalogToolbarButton", true);
        button->setIcon(QIcon(iconPath));
        button->setIconSize(QSize(18, 18));
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setFixedSize(28, 28);
        button->setAccessibleName(accessibleName);
        button->setText(QString());
        button->setToolTip(tooltip);
        button->setCursor(Qt::PointingHandCursor);
        button->setAutoRaise(true);
        return button;
    }

    bool isSupportedCatalogUri(const QgsMimeDataUtils::Uri& uri)
    {
        return uri.layerType == QStringLiteral("vector") ||
               uri.layerType == QStringLiteral("raster");
    }

    QString normalizedLocalPath(const QString& path)
    {
        const QFileInfo fileInfo(path.trimmed());
        if (fileInfo.filePath().isEmpty())
        {
            return QString();
        }

        const QString absolutePath = QDir::cleanPath(fileInfo.absoluteFilePath());
        if (fileInfo.isSymLink())
        {
            return absolutePath;
        }

        const QString canonicalPath = fileInfo.canonicalFilePath();
        return QDir::cleanPath(
            canonicalPath.isEmpty() ? absolutePath : canonicalPath);
    }

    bool isSafeLocalDeleteTarget(
        const QString& targetPath,
        const QString& sourcePath)
    {
        const QFileInfo sourceInfo(sourcePath);
        const QFileInfo targetInfo(targetPath);
        if (!sourceInfo.exists() || sourceInfo.isSymLink() ||
            (!sourceInfo.isFile() && !sourceInfo.isDir()) ||
            !targetInfo.exists() || targetInfo.isSymLink() ||
            (!targetInfo.isFile() && !targetInfo.isDir()))
        {
            return false;
        }

        const QString targetAbsolutePath = QDir::cleanPath(
            targetInfo.absoluteFilePath());
        const QString rootPath = QDir::cleanPath(
            QDir(targetInfo.absolutePath()).rootPath());
        if (targetAbsolutePath.compare(rootPath, Qt::CaseInsensitive) == 0)
        {
            return false;
        }

        if (sourceInfo.isDir())
        {
            return targetInfo.isDir() &&
                targetAbsolutePath.compare(
                    QDir::cleanPath(sourceInfo.absoluteFilePath()),
                    Qt::CaseInsensitive) == 0;
        }

        return targetInfo.isFile() &&
            targetInfo.absolutePath().compare(
                sourceInfo.absolutePath(),
                Qt::CaseInsensitive) == 0;
    }

    QString stableLocalItemKey(const QgsDataItem* item)
    {
        if (!item)
        {
            return QString();
        }

        for (const QgsMimeDataUtils::Uri& uri : item->mimeUris())
        {
            const QString filePath = uri.filePath.trimmed();
            if (filePath.isEmpty())
            {
                continue;
            }

            return normalizedLocalPath(filePath);
        }

        const QString itemPath = item->path().trimmed();
        if (!itemPath.isEmpty())
        {
            return normalizedLocalPath(itemPath);
        }
        return QString();
    }

    class CatalogBrowserProxyModel final : public QgsBrowserProxyModel
    {
    public:
        explicit CatalogBrowserProxyModel(QObject* parent = nullptr)
            : QgsBrowserProxyModel(parent)
        {
        }

        void setCheckedPaths(QSet<QString>* paths)
        {
            m_checkedPaths = paths;
        }

        void setRemovedPaths(const QSet<QString>* paths)
        {
            m_removedPaths = paths;
            invalidateFilter();
        }

        void setDisplayAliases(const QHash<QString, QString>* aliases)
        {
            m_displayAliases = aliases;
            invalidate();
        }

        void refreshCatalogState()
        {
            invalidateFilter();
            invalidate();
        }

        Qt::ItemFlags flags(const QModelIndex& index) const override
        {
            Qt::ItemFlags itemFlags = QgsBrowserProxyModel::flags(index);
            if (isCheckable(index))
            {
                itemFlags |= Qt::ItemIsUserCheckable;
            }
            return itemFlags;
        }

        QVariant data(const QModelIndex& index, int role) const override
        {
            if (role == Qt::DisplayRole)
            {
                const QgsDataItem* item = dataItem(index);
                const QString key = stableLocalItemKey(item);
                if (m_displayAliases && !key.isEmpty())
                {
                    const auto alias = m_displayAliases->constFind(key);
                    if (alias != m_displayAliases->constEnd())
                    {
                        return alias.value();
                    }
                }
            }
            if (role == Qt::CheckStateRole && isCheckable(index))
            {
                const QgsDataItem* item = dataItem(index);
                const bool checked = m_checkedPaths && item &&
                    m_checkedPaths->contains(item->path());
                return checked ? Qt::Checked : Qt::Unchecked;
            }
            if (role == Qt::DecorationRole)
            {
                QgsDataItem* item = dataItem(index);
                if (QgsDirectoryItem* directory = qobject_cast<QgsDirectoryItem*>(item))
                {
                    return catalogDirectoryIcon(QFileInfo(directory->dirPath()).fileName());
                }
                if (QgsLayerItem* layerItem = qobject_cast<QgsLayerItem*>(item))
                {
                    const QgsMimeDataUtils::UriList uris = item->mimeUris();
                    for (const QgsMimeDataUtils::Uri& uri : uris)
                    {
                        const QIcon formatIcon = catalogFileFormatIcon(uri.filePath);
                        if (!formatIcon.isNull()) return formatIcon;
                    }
                    const QIcon pathIcon = catalogFileFormatIcon(item->path());
                    if (!pathIcon.isNull()) return pathIcon;

                    // DecorationRole is queried while painting and laying out
                    // the tree on the GUI thread. Browser items already carry
                    // their geometry type; never open a data provider for an icon.
                    for (const QgsMimeDataUtils::Uri& uri : uris)
                    {
                        if (uri.layerType == QStringLiteral("raster"))
                            return drawProductLayerIcon(3);
                        if (uri.layerType != QStringLiteral("vector"))
                            continue;
                        switch (QgsWkbTypes::geometryType(uri.wkbType))
                        {
                        case QgsWkbTypes::PointGeometry:
                            return drawProductLayerIcon(0);
                        case QgsWkbTypes::LineGeometry:
                            return drawProductLayerIcon(1);
                        case QgsWkbTypes::PolygonGeometry:
                            return drawProductLayerIcon(2);
                        default:
                            break;
                        }
                    }
                    return layerItem->icon();
                }
            }
            return QgsBrowserProxyModel::data(index, role);
        }

        bool setData(
            const QModelIndex& index,
            const QVariant& value,
            int role = Qt::EditRole) override
        {
            if (role == Qt::CheckStateRole && isCheckable(index))
            {
                QgsDataItem* item = dataItem(index);
                if (!item || !m_checkedPaths)
                {
                    return false;
                }

                const bool checked = value.toInt() == Qt::Checked;
                if (checked)
                {
                    m_checkedPaths->insert(item->path());
                }
                else
                {
                    m_checkedPaths->remove(item->path());
                }

                emit dataChanged(index, index, QVector<int>() << role);
                return true;
            }
            return QgsBrowserProxyModel::setData(index, value, role);
        }

        bool hasChildren(
            const QModelIndex& parent = QModelIndex()) const override
        {
            if (qobject_cast<QgsLayerItem*>(dataItem(parent)))
            {
                return false;
            }
            return QgsBrowserProxyModel::hasChildren(parent);
        }

    protected:
        bool filterAcceptsRow(
            int sourceRow,
            const QModelIndex& sourceParent) const override
        {
            if (mModel)
            {
                const QModelIndex sourceIndex = mModel->index(
                    sourceRow,
                    0,
                    sourceParent);
                const QgsDataItem* item = mModel->dataItem(sourceIndex);
                const QString key = stableLocalItemKey(item);
                if (m_removedPaths && !key.isEmpty() &&
                    m_removedPaths->contains(key))
                {
                    return false;
                }
            }
            if (mModel &&
                qobject_cast<QgsLayerItem*>(mModel->dataItem(sourceParent)))
            {
                return false;
            }
            return QgsBrowserProxyModel::filterAcceptsRow(
                sourceRow,
                sourceParent);
        }

    private:
        bool isCheckable(const QModelIndex& index) const
        {
            QgsDataItem* item = dataItem(index);
            if (!item || item->path().isEmpty())
            {
                return false;
            }

            const QgsMimeDataUtils::UriList uris = item->mimeUris();
            for (const QgsMimeDataUtils::Uri& uri : uris)
            {
                if (isSupportedCatalogUri(uri))
                {
                    return true;
                }
            }
            return false;
        }

        QSet<QString>* m_checkedPaths = nullptr;
        const QSet<QString>* m_removedPaths = nullptr;
        const QHash<QString, QString>* m_displayAliases = nullptr;
    };

    void appendUniqueUri(
        QgsMimeDataUtils::UriList* destination,
        const QgsMimeDataUtils::Uri& candidate)
    {
        if (!destination || !isSupportedCatalogUri(candidate))
        {
            return;
        }

        for (const QgsMimeDataUtils::Uri& existing : *destination)
        {
            if (existing.layerType == candidate.layerType &&
                existing.providerKey == candidate.providerKey &&
                existing.uri == candidate.uri)
            {
                return;
            }
        }
        destination->append(candidate);
    }

    QString sanitizedExportBaseName(const QString& value)
    {
        QString result = value.trimmed();
        const QString forbidden = QStringLiteral("\\/:*?\"<>|");
        for (int index = 0; index < result.size(); ++index)
        {
            const QChar character = result.at(index);
            if (character.unicode() < 0x20 || forbidden.contains(character))
            {
                result[index] = QChar('_');
            }
        }
        while (result.endsWith(QChar('.')) || result.endsWith(QChar(' ')))
        {
            result.chop(1);
        }
        return result.isEmpty() ? QStringLiteral("layer") : result;
    }

    QString uniqueExportPath(
        const QString& directory,
        const QString& baseName,
        const QString& extension)
    {
        const QString safeBaseName = sanitizedExportBaseName(baseName);
        const QString normalizedExtension = extension.startsWith(QChar('.'))
            ? extension
            : QChar('.') + extension;
        QString candidate = QDir(directory).filePath(safeBaseName + normalizedExtension);
        int suffix = 2;
        while (QFileInfo::exists(candidate))
        {
            candidate = QDir(directory).filePath(
                QStringLiteral("%1_%2%3")
                    .arg(safeBaseName)
                    .arg(suffix)
                    .arg(normalizedExtension));
            ++suffix;
        }
        return candidate;
    }

    QString rasterWriterErrorText(QgsRasterFileWriter::WriterError error)
    {
        switch (error)
        {
        case QgsRasterFileWriter::NoError:
            return QString();
        case QgsRasterFileWriter::SourceProviderError:
            return QStringLiteral("栅格源数据提供者错误。");
        case QgsRasterFileWriter::DestProviderError:
            return QStringLiteral("栅格目标数据提供者错误。");
        case QgsRasterFileWriter::CreateDatasourceError:
            return QStringLiteral("无法创建栅格输出数据源。");
        case QgsRasterFileWriter::WriteError:
            return QStringLiteral("写入栅格输出失败。");
        case QgsRasterFileWriter::NoDataConflict:
            return QStringLiteral("栅格 NoData 值冲突。");
        case QgsRasterFileWriter::WriteCanceled:
            return QStringLiteral("栅格导出已取消。");
        }
        return QStringLiteral("未知的栅格导出错误。");
    }
}

DataCatalogWidget::DataCatalogWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("workspaceDataCatalog"));
    loadPlatformRemovalState();
    buildUi();
    initializeLocalBrowser();

    m_menuDatabaseSettingsTimer = new QTimer(this);
    m_menuDatabaseSettingsTimer->setInterval(500);
    connect(m_menuDatabaseSettingsTimer, &QTimer::timeout, this, [this]()
    {
        if (isVisible())
        {
            synchronizeMenuDatabaseConnection(false);
        }
    });
    m_menuDatabaseSettingsTimer->start();
    QTimer::singleShot(0, this, [this]()
    {
        if (isVisible())
        {
            synchronizeMenuDatabaseConnection(false);
        }
    });
}

DataCatalogWidget::~DataCatalogWidget()
{
    closeDatabaseConnection();
}

void DataCatalogWidget::loadPlatformRemovalState()
{
    QSettings settings(
        QStringLiteral("GarMap"),
        QStringLiteral("LTZK"));
    const QStringList removedDatabaseKeys = settings.value(
        QStringLiteral("dataCatalog/removedDatabaseKeys")).toStringList();
    const QStringList removedLocalPaths = settings.value(
        QStringLiteral("dataCatalog/removedLocalPaths")).toStringList();

    m_removedDatabaseKeys.clear();
    for (const QString& key : removedDatabaseKeys)
    {
        if (!key.trimmed().isEmpty())
        {
            m_removedDatabaseKeys.insert(key);
        }
    }

    m_removedLocalPaths.clear();
    for (const QString& path : removedLocalPaths)
    {
        if (!path.trimmed().isEmpty())
        {
            m_removedLocalPaths.insert(normalizedLocalPath(path));
        }
    }
}

bool DataCatalogWidget::savePlatformRemovalState()
{
    QSettings settings(
        QStringLiteral("GarMap"),
        QStringLiteral("LTZK"));

    QStringList removedDatabaseKeys = m_removedDatabaseKeys.values();
    removedDatabaseKeys.sort();
    settings.setValue(
        QStringLiteral("dataCatalog/removedDatabaseKeys"),
        removedDatabaseKeys);

    QStringList removedLocalPaths = m_removedLocalPaths.values();
    removedLocalPaths.sort();
    settings.setValue(
        QStringLiteral("dataCatalog/removedLocalPaths"),
        removedLocalPaths);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

QVariantMap DataCatalogWidget::menuDatabaseConnectionParametersFromSettings() const
{
    QSettings settings(
        QStringLiteral("GarMap"),
        QStringLiteral("MapProductManager"));
    settings.sync();

    const QString host = settings.value(QStringLiteral("db/host")).toString().trimmed();
    bool portOk = false;
    const int port = settings.value(QStringLiteral("db/port")).toInt(&portOk);
    const QString databaseName = settings.value(
        QStringLiteral("db/database")).toString().trimmed();
    const QString userName = settings.value(
        QStringLiteral("db/user")).toString().trimmed();
    if (host.isEmpty() ||
        !portOk ||
        port < 1 ||
        port > 65535 ||
        databaseName.isEmpty() ||
        userName.isEmpty())
    {
        return QVariantMap();
    }

    const QString schema = settings.value(
        QStringLiteral("db/schema"),
        QStringLiteral("public")).toString().trimmed();
    QVariantMap parameters;
    parameters.insert(QStringLiteral("host"), host);
    parameters.insert(QStringLiteral("port"), port);
    parameters.insert(QStringLiteral("database"), databaseName);
    parameters.insert(
        QStringLiteral("schema"),
        schema.isEmpty() ? QStringLiteral("public") : schema);
    parameters.insert(QStringLiteral("user"), userName);
    parameters.insert(
        QStringLiteral("password"),
        settings.value(QStringLiteral("db/password")).toString());
    parameters.insert(
        QStringLiteral("savePassword"),
        settings.value(QStringLiteral("db/savePassword"), false).toBool());
    return parameters;
}

QString DataCatalogWidget::menuDatabaseConnectionSettingsSignature(
    const QVariantMap& parameters) const
{
    QByteArray payload;
    const QStringList keys = {
        QStringLiteral("host"),
        QStringLiteral("port"),
        QStringLiteral("database"),
        QStringLiteral("schema"),
        QStringLiteral("user"),
        QStringLiteral("password"),
        QStringLiteral("savePassword")};
    for (const QString& key : keys)
    {
        const QByteArray value = parameters.value(key).toString().toUtf8();
        payload.append(QByteArray::number(value.size()));
        payload.append(':');
        payload.append(value);
        payload.append('|');
    }
    return QString::fromLatin1(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

bool DataCatalogWidget::synchronizeMenuDatabaseConnection(bool forceRetry)
{
    if (!m_databaseModel || !gCurrentUserSession.isLoggedIn)
    {
        return false;
    }

    // The plugin writes QSettings before showing its success message. Wait
    // until the dialog is accepted so a settings poll cannot briefly clear
    // the catalog when this connection did not remember its password.
    if (PlatformMenuDatabaseConnectionObserver::isConnectionDialogVisible())
    {
        return false;
    }

    // A successful connection without "remember password" exists only in
    // memory during this platform session. The saved settings have no
    // password, so polling them must not replace this live connection.
    if (!m_liveMenuConnectionParameters.isEmpty())
    {
        const bool currentConnectionOpen = [&]()
        {
            const QSqlDatabase currentDatabase = catalogDatabase(m_databaseConnectionName);
            return currentDatabase.isValid() && currentDatabase.isOpen();
        }();
        if (forceRetry && !currentConnectionOpen)
        {
            beginSavedDatabaseConnection(m_liveMenuConnectionParameters);
            return true;
        }
        return false;
    }

    const QVariantMap parameters = menuDatabaseConnectionParametersFromSettings();
    if (parameters.isEmpty())
    {
        if (!m_menuDatabaseSettingsSignature.isEmpty() ||
            !m_databaseConnectionName.isEmpty())
        {
            applyMenuDatabaseConnection(QVariantMap());
            return true;
        }
        return false;
    }

    const QString signature = menuDatabaseConnectionSettingsSignature(parameters);
    const bool currentConnectionOpen = [&]()
    {
        const QSqlDatabase currentDatabase = catalogDatabase(m_databaseConnectionName);
        return currentDatabase.isValid() && currentDatabase.isOpen();
    }();
    if (m_savedDatabaseProbePending && signature == m_menuDatabaseSettingsSignature)
    {
        return true;
    }
    if (signature == m_menuDatabaseSettingsSignature && currentConnectionOpen)
    {
        return false;
    }
    if (signature == m_menuDatabaseSettingsSignature && !forceRetry)
    {
        return false;
    }

    if (parameters.value(QStringLiteral("password")).toString().isEmpty())
    {
        ++m_savedDatabaseProbeGeneration;
        m_savedDatabaseProbePending = false;
        closeDatabaseConnection();
        if (m_databaseModel)
        {
            m_databaseModel->removeRows(0, m_databaseModel->rowCount());
        }
        m_checkedDatabaseKeys.clear();
        m_databaseDisplayAliases.clear();
        m_menuDatabaseSettingsSignature = signature;
        showDatabaseMessage(
            QStringLiteral("菜单栏上次数据库配置未保存密码，请重新连接数据库；可不勾选“记住密码”。"),
            true);
        return true;
    }

    beginSavedDatabaseConnection(parameters);
    return true;
}

void DataCatalogWidget::beginSavedDatabaseConnection(const QVariantMap& parameters)
{
    const QString signature = menuDatabaseConnectionSettingsSignature(parameters);
    if (m_savedDatabaseProbePending && signature == m_menuDatabaseSettingsSignature) return;
    const quint64 generation = ++m_savedDatabaseProbeGeneration;
    m_savedDatabaseProbePending = true;
    m_menuDatabaseSettingsSignature = signature;
    showDatabaseMessage(QStringLiteral("\u6b63\u5728\u8fde\u63a5\u5df2\u4fdd\u5b58\u7684\u6570\u636e\u5e93\uff0c\u53ef\u7ee7\u7eed\u64cd\u4f5c\u672c\u5730\u6570\u636e\u3002"), false);

    struct ProbeResult { bool connected = false; QString error; };
    const auto result = std::make_shared<ProbeResult>();
    // Each worker owns its SQL connection. Never use a GUI-thread QSqlDatabase
    // from this thread or parent the running thread to the catalog widget.
    QThread* worker = QThread::create([parameters, result]()
    {
        const QString name = QStringLiteral("LTZKCatalogProbe_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), name);
            database.setHostName(parameters.value(QStringLiteral("host")).toString());
            database.setPort(parameters.value(QStringLiteral("port")).toInt());
            database.setDatabaseName(parameters.value(QStringLiteral("database")).toString());
            database.setUserName(parameters.value(QStringLiteral("user")).toString());
            database.setPassword(parameters.value(QStringLiteral("password")).toString());
            database.setConnectOptions(QStringLiteral("connect_timeout=3"));
            result->connected = database.open();
            if (!result->connected) result->error = database.lastError().text();
            database.close();
        }
        QSqlDatabase::removeDatabase(name);
    });
    connect(worker, &QThread::finished, this, [this, generation, parameters, result]()
    {
        if (generation != m_savedDatabaseProbeGeneration) return;
        m_savedDatabaseProbePending = false;
        if (!gCurrentUserSession.isLoggedIn || PlatformMenuDatabaseConnectionObserver::isConnectionDialogVisible()) return;
        const QVariantMap latest = m_liveMenuConnectionParameters.isEmpty()
            ? menuDatabaseConnectionParametersFromSettings() : m_liveMenuConnectionParameters;
        if (menuDatabaseConnectionSettingsSignature(latest) != menuDatabaseConnectionSettingsSignature(parameters)) return;
        if (!result->connected)
        {
            showDatabaseMessage(QStringLiteral("\u6570\u636e\u5e93\u81ea\u52a8\u8fde\u63a5\u5931\u8d25\uff0c\u8bf7\u68c0\u67e5\u8fde\u63a5\u914d\u7f6e\u540e\u5237\u65b0\u3002\n%1").arg(result->error), true);
            return;
        }
        applyMenuDatabaseConnection(parameters);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

bool DataCatalogWidget::openDatabaseConnection(
    const QVariantMap& parameters,
    QString* errorMessage)
{
    const QString host = parameters.value(QStringLiteral("host")).toString().trimmed();
    bool portOk = false;
    const int port = parameters.value(QStringLiteral("port")).toInt(&portOk);
    const QString databaseName = parameters.value(QStringLiteral("database"))
        .toString()
        .trimmed();
    const QString userName = parameters.value(QStringLiteral("user"))
        .toString()
        .trimmed();
    const QString password = parameters.value(QStringLiteral("password")).toString();
    if (host.isEmpty() ||
        !portOk ||
        port < 1 ||
        port > 65535 ||
        databaseName.isEmpty() ||
        userName.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("菜单栏数据库连接参数无效。请检查主机、端口、数据库和用户名。");
        }
        return false;
    }

    const QString connectionName = QStringLiteral("%1_%2_%3")
        .arg(QString::fromLatin1(kCatalogDatabaseConnectionPrefix))
        .arg(QString::number(reinterpret_cast<quintptr>(this), 16))
        .arg(QString::number(++m_databaseConnectionSerial));

    QSqlDatabase database = QSqlDatabase::addDatabase(
        QStringLiteral("QPSQL"),
        connectionName);
    database.setHostName(host);
    database.setPort(port);
    database.setDatabaseName(databaseName);
    database.setUserName(userName);
    database.setPassword(password);
    database.setConnectOptions(QStringLiteral("connect_timeout=3"));

    const auto discardConnection = [&database, &connectionName]()
    {
        database.close();
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
    };

    if (!database.open())
    {
        if (errorMessage)
        {
            *errorMessage = database.lastError().text();
        }
        discardConnection();
        return false;
    }

    const QString schema = parameters.value(QStringLiteral("schema"))
        .toString()
        .trimmed();
    const QString normalizedSchema = schema.isEmpty()
        ? QStringLiteral("public")
        : schema;
    bool searchPathSet = false;
    QString searchPathError;
    {
        const QString setPathSql = normalizedSchema.compare(
            QStringLiteral("public"), Qt::CaseInsensitive) == 0
            ? QStringLiteral("SET search_path TO public")
            : QStringLiteral("SET search_path TO %1, public")
                .arg(quotedSqlIdentifier(normalizedSchema));
        QSqlQuery searchPathQuery(database);
        searchPathSet = searchPathQuery.exec(setPathSql);
        if (!searchPathSet)
        {
            searchPathError = searchPathQuery.lastError().text();
        }
    }
    if (!searchPathSet)
    {
        if (errorMessage)
        {
            *errorMessage = searchPathError;
        }
        discardConnection();
        return false;
    }

    closeDatabaseConnection();
    m_databaseConnectionName = connectionName;
    return true;
}

void DataCatalogWidget::closeDatabaseConnection()
{
    const QString connectionName = m_databaseConnectionName;
    m_databaseConnectionName.clear();
    if (connectionName.isEmpty() || !QSqlDatabase::contains(connectionName))
    {
        return;
    }

    QSqlDatabase database = QSqlDatabase::database(connectionName, false);
    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

void DataCatalogWidget::applyMenuDatabaseConnection(const QVariantMap& parameters)
{
    ++m_savedDatabaseProbeGeneration;
    m_savedDatabaseProbePending = false;
    if (parameters.isEmpty())
    {
        m_liveMenuConnectionParameters.clear();
    }
    closeDatabaseConnection();
    if (m_databaseModel)
    {
        m_databaseModel->removeRows(0, m_databaseModel->rowCount());
    }
    m_checkedDatabaseKeys.clear();
    m_databaseDisplayAliases.clear();
    m_menuDatabaseSettingsSignature.clear();
    if (!parameters.isEmpty())
    {
        m_menuDatabaseSettingsSignature = menuDatabaseConnectionSettingsSignature(parameters);
    }

    if (!gCurrentUserSession.isLoggedIn)
    {
        showDatabaseMessage(QStringLiteral("请先登录后再显示数据库数据。"), false);
        return;
    }

    if (parameters.isEmpty())
    {
        showDatabaseMessage(
            QStringLiteral("请先通过菜单栏“数据库连接配置”连接数据库。"),
            false);
        return;
    }

    QString errorMessage;
    if (!openDatabaseConnection(parameters, &errorMessage))
    {
        showDatabaseMessage(
            QStringLiteral("菜单栏数据库连接同步失败：%1")
                .arg(errorMessage.isEmpty()
                    ? QStringLiteral("无法打开数据库查询连接。")
                    : errorMessage),
            true);
        return;
    }

    loadDatabaseData();
}

void DataCatalogWidget::applySuccessfulMenuDatabaseConnection(
    const QVariantMap& parameters,
    bool remembered)
{
    m_liveMenuConnectionParameters = remembered ? QVariantMap() : parameters;
    applyMenuDatabaseConnection(parameters);
}

QgsBrowserGuiModel* DataCatalogWidget::browserModel() const
{
    return m_browserModel;
}

void DataCatalogWidget::setLoadCallback(LoadCallback callback)
{
    m_loadCallback = std::move(callback);
}

void DataCatalogWidget::setDataClipCallback(DataClipCallback callback)
{
    m_dataClipCallback = std::move(callback);
}

void DataCatalogWidget::setDataImportCallback(DataImportCallback callback)
{
    m_dataImportCallback = std::move(callback);
}

void DataCatalogWidget::setProductOperationCallback(ProductOperationCallback callback)
{
    m_productOperationCallback = std::move(callback);
}

void DataCatalogWidget::setLocalProductOperationCallback(
    LocalProductOperationCallback callback)
{
    m_localProductOperationCallback = std::move(callback);
}

QVariantMap DataCatalogWidget::databaseConnectionParameters() const
{
    QVariantMap parameters;
    const QSqlDatabase database = catalogDatabase(m_databaseConnectionName);
    if (!database.isValid() || !database.isOpen())
    {
        return parameters;
    }

    parameters.insert(QStringLiteral("host"), database.hostName());
    parameters.insert(QStringLiteral("port"), database.port());
    parameters.insert(QStringLiteral("database"), database.databaseName());
    parameters.insert(QStringLiteral("schema"), QString());
    parameters.insert(QStringLiteral("user"), database.userName());
    parameters.insert(QStringLiteral("password"), database.password());

    QSqlQuery query(database);
    if (query.exec(QStringLiteral("SELECT current_schema()")) && query.next())
    {
        parameters.insert(QStringLiteral("schema"), query.value(0).toString());
    }
    return parameters;
}

void DataCatalogWidget::buildUi()
{
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QSplitter* sectionSplitter = new QSplitter(Qt::Vertical, this);
    sectionSplitter->setObjectName(QStringLiteral("dataCatalogSectionSplitter"));
    sectionSplitter->setChildrenCollapsible(false);

    QWidget* databaseSection = new QWidget(sectionSplitter);
    databaseSection->setObjectName(QStringLiteral("databaseCatalogSection"));
    QVBoxLayout* databaseLayout = new QVBoxLayout(databaseSection);
    databaseLayout->setContentsMargins(0, 0, 0, 0);
    databaseLayout->setSpacing(0);

    QToolButton* refreshDatabaseButton = createCatalogButton(
        QStringLiteral(":/LTZK/Resources/mActionRefresh.svg"),
        QStringLiteral("刷新"),
        QStringLiteral("刷新数据库数据"),
        databaseSection);
    QToolButton* databaseTitleButton = nullptr;
    QFrame* databaseHeader = createSectionHeader(
        QStringLiteral("数据库数据"),
        databaseSection,
        QList<QToolButton*>() << refreshDatabaseButton,
        &databaseTitleButton);
    databaseLayout->addWidget(databaseHeader);
    databaseTitleButton->setObjectName(QStringLiteral("databaseCatalogTitleButton"));

    QWidget* databaseContent = new QWidget(databaseSection);
    databaseContent->setObjectName(QStringLiteral("databaseCatalogContent"));
    QVBoxLayout* databaseContentLayout = new QVBoxLayout(databaseContent);
    databaseContentLayout->setContentsMargins(0, 0, 0, 0);
    databaseContentLayout->setSpacing(0);

    m_databaseModel = new QStandardItemModel(this);
    m_databaseModel->setHorizontalHeaderLabels(QStringList() << QStringLiteral("数据库数据"));
    m_databaseTree = new QTreeView(databaseContent);
    m_databaseTree->setObjectName(QStringLiteral("databaseCatalogTree"));
    m_databaseTree->setModel(m_databaseModel);
    m_databaseTree->setHeaderHidden(true);
    m_databaseTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_databaseTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_databaseTree->setUniformRowHeights(true);
    m_databaseTree->setContextMenuPolicy(Qt::CustomContextMenu);
    databaseContentLayout->addWidget(m_databaseTree, 1);
    databaseLayout->addWidget(databaseContent, 1);
    connect(
        m_databaseModel,
        &QStandardItemModel::itemChanged,
        this,
        [this](QStandardItem* item)
        {
            if (!item || !item->isCheckable())
            {
                return;
            }

            const int productNodeKind = item->data(ProductNodeKindRole).toInt();
            if (productNodeKind == static_cast<int>(ProductCatalogNodeKind::Directory) ||
                productNodeKind == static_cast<int>(ProductCatalogNodeKind::Product))
            {
                const QString key = databaseCheckKey(item);
                if (!key.isEmpty())
                {
                    if (item->checkState() == Qt::Checked)
                    {
                        m_checkedDatabaseKeys.insert(key);
                    }
                    else
                    {
                        m_checkedDatabaseKeys.remove(key);
                    }
                }

                if (!m_productCheckSyncing)
                {
                    m_productCheckSyncing = true;
                    if (productNodeKind == static_cast<int>(ProductCatalogNodeKind::Directory) &&
                        item->checkState() != Qt::PartiallyChecked)
                    {
                        setProductSubtreeCheck(item, item->checkState());
                    }
                    refreshProductParentCheckStates(item);
                    m_productCheckSyncing = false;
                }
                return;
            }

            const QString layerType = item->data(DatabaseLayerTypeRole).toString();
            if (layerType != QStringLiteral("vector") &&
                layerType != QStringLiteral("raster"))
            {
                return;
            }

            const QString key = databaseCheckKey(item);
            if (key.isEmpty())
            {
                return;
            }

            if (item->checkState() == Qt::Checked)
            {
                m_checkedDatabaseKeys.insert(key);
            }
            else
            {
                m_checkedDatabaseKeys.remove(key);
            }
        });

    QWidget* localSection = new QWidget(sectionSplitter);
    localSection->setObjectName(QStringLiteral("localCatalogSection"));
    QVBoxLayout* localLayout = new QVBoxLayout(localSection);
    localLayout->setContentsMargins(0, 0, 0, 0);
    localLayout->setSpacing(0);

    QToolButton* connectFolderButton = createCatalogButton(
        QStringLiteral(":/LTZK/Resources/mIconFolderLink.svg"),
        QStringLiteral("连接文件夹"),
        QStringLiteral("连接一个本地数据文件夹"),
        localSection);
    m_disconnectFolderButton = createCatalogButton(
        QStringLiteral(":/LTZK/Resources/mActionUnlink.svg"),
        QStringLiteral("断开连接"),
        QStringLiteral("断开当前选中的文件夹连接"),
        localSection);
    QToolButton* refreshLocalButton = createCatalogButton(
        QStringLiteral(":/LTZK/Resources/mActionRefresh.svg"),
        QStringLiteral("刷新"),
        QStringLiteral("刷新本地数据"),
        localSection);
    QToolButton* localTitleButton = nullptr;
    QFrame* localHeader = createSectionHeader(
        QStringLiteral("本地数据"),
        localSection,
        QList<QToolButton*>() << connectFolderButton << m_disconnectFolderButton << refreshLocalButton,
        &localTitleButton);
    localLayout->addWidget(localHeader);
    localTitleButton->setObjectName(QStringLiteral("localCatalogTitleButton"));

    QWidget* localContent = new QWidget(localSection);
    localContent->setObjectName(QStringLiteral("localCatalogContent"));
    QVBoxLayout* localContentLayout = new QVBoxLayout(localContent);
    localContentLayout->setContentsMargins(0, 0, 0, 0);
    localContentLayout->setSpacing(0);

    m_localStatusLabel = new QLabel(localContent);
    m_localStatusLabel->setObjectName(QStringLiteral("localCatalogStatusLabel"));
    m_localStatusLabel->setText(QStringLiteral("未连接本地数据文件夹"));
    m_localStatusLabel->setToolTip(QStringLiteral("使用“连接文件夹”选择本地数据目录"));
    m_localStatusLabel->setWordWrap(true);
    localContentLayout->addWidget(m_localStatusLabel);

    m_localTree = new QgsBrowserTreeView(localContent);
    m_localTree->setObjectName(QStringLiteral("localCatalogTree"));
    m_localTree->setHeaderHidden(true);
    m_localTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_localTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_localTree->setSettingsSection(QStringLiteral("ltzkWorkspaceDataCatalog"));
    m_localTree->setContextMenuPolicy(Qt::CustomContextMenu);
    localContentLayout->addWidget(m_localTree, 1);
    localLayout->addWidget(localContent, 1);

    sectionSplitter->addWidget(databaseSection);
    sectionSplitter->addWidget(localSection);
    sectionSplitter->setStretchFactor(0, 1);
    sectionSplitter->setStretchFactor(1, 2);
    sectionSplitter->setSizes(QList<int>() << 260 << 520);

    auto selectCatalogSource = [sectionSplitter, databaseContent, localContent,
        databaseHeader, localHeader](bool showDatabase)
    {
        databaseContent->setVisible(showDatabase);
        localContent->setVisible(!showDatabase);

        QTimer::singleShot(0, sectionSplitter, [sectionSplitter, databaseContent, localContent,
            databaseHeader, localHeader, showDatabase]()
        {
            const QList<int> currentSizes = sectionSplitter->sizes();
            const int totalSize = currentSizes.value(0) + currentSizes.value(1);
            if (totalSize <= 0)
            {
                return;
            }

            const int collapsedSize = qMax(
                databaseHeader->sizeHint().height(),
                localHeader->sizeHint().height());
            const int expandedSize = qMax(collapsedSize, totalSize - collapsedSize);
            const QList<int> targetSizes = showDatabase
                ? (QList<int>() << expandedSize << collapsedSize)
                : (QList<int>() << collapsedSize << expandedSize);
            sectionSplitter->setSizes(targetSizes);
        });
    };

    databaseContent->setVisible(true);
    localContent->setVisible(true);
    connect(databaseTitleButton, &QToolButton::clicked, this, [selectCatalogSource]()
    {
        selectCatalogSource(true);
    });
    connect(localTitleButton, &QToolButton::clicked, this, [selectCatalogSource]()
    {
        selectCatalogSource(false);
    });

    rootLayout->addWidget(sectionSplitter, 1);

    connect(refreshDatabaseButton, &QToolButton::clicked,
        this, &DataCatalogWidget::refreshDatabaseData);
    connect(connectFolderButton, &QToolButton::clicked,
        this, &DataCatalogWidget::connectLocalFolder);
    connect(m_disconnectFolderButton, &QToolButton::clicked,
        this, &DataCatalogWidget::disconnectSelectedLocalFolder);
    connect(refreshLocalButton, &QToolButton::clicked,
        this, &DataCatalogWidget::refreshLocalFolders);
    connect(m_databaseTree, &QTreeView::doubleClicked,
        this, &DataCatalogWidget::activateDatabaseItem);
    connect(m_localTree, &QTreeView::doubleClicked,
        this, &DataCatalogWidget::activateLocalItem);
    connect(m_databaseTree, &QTreeView::customContextMenuRequested,
        this, &DataCatalogWidget::showDatabaseContextMenu);
    connect(m_localTree, &QTreeView::customContextMenuRequested,
        this, &DataCatalogWidget::showLocalContextMenu);

    showDatabaseMessage(
        QStringLiteral("请先通过菜单栏“数据库连接配置”连接数据库。"),
        false);
}

void DataCatalogWidget::initializeLocalBrowser()
{
    m_browserModel = new QgsBrowserGuiModel(this);

    // QGIS calls connected folders "Favorites" internally.  The application
    // presents them as local data folders, so always rebind the tree after
    // QGIS finishes its lazy population.  This also handles folders selected
    // while the browser model is still initializing.
    connect(
        m_browserModel,
        &QgsBrowserModel::stateChanged,
        this,
        [this](const QModelIndex& index, Qgis::BrowserItemState)
        {
            if (!m_browserModel || !m_localTree || !index.isValid())
            {
                return;
            }

            QgsDataItem* item = m_browserModel->dataItem(index);
            if (!item || item->path() != QStringLiteral("favorites:"))
            {
                return;
            }

            QTimer::singleShot(0, this, &DataCatalogWidget::showFavoritesInTree);
        });

    // Populate the browser model before attaching the proxy.  This follows
    // QGIS' own data-source selector ordering and guarantees that the proxy
    // has a complete source tree when it first maps the Favorites index.
    m_browserModel->initialize();
    auto* catalogProxy = new CatalogBrowserProxyModel(this);
    catalogProxy->setCheckedPaths(&m_checkedLocalPaths);
    catalogProxy->setRemovedPaths(&m_removedLocalPaths);
    catalogProxy->setDisplayAliases(&m_localDisplayAliases);
    m_localProxyModel = catalogProxy;
    m_localProxyModel->setBrowserModel(m_browserModel);

    // QgsBrowserTreeView supports an optional proxy model. Keep the source
    // model for folder persistence and layer activation, while giving the
    // view its own mapped indexes. Passing a source index directly to a
    // proxy-backed view makes the model report rows but renders an empty tree.
    m_localTree->setModel(m_localProxyModel);
    m_localTree->setBrowserModel(m_browserModel);
    refreshLocalFolders();
}

QModelIndex DataCatalogWidget::favoritesRootIndex()
{
    if (!m_browserModel)
    {
        return QModelIndex();
    }

    return m_browserModel->findPath(QStringLiteral("favorites:"));
}

void DataCatalogWidget::showFavoritesInTree()
{
    if (!m_browserModel || !m_localTree)
    {
        return;
    }

    const QModelIndex favoritesIndex = favoritesRootIndex();
    if (!favoritesIndex.isValid())
    {
        if (m_localStatusLabel)
        {
            m_localStatusLabel->setText(QStringLiteral("正在初始化本地数据目录…"));
        }
        return;
    }

    const QModelIndex favoritesViewIndex = m_localProxyModel
        ? m_localProxyModel->mapFromSource(favoritesIndex)
        : favoritesIndex;
    if (!favoritesViewIndex.isValid())
    {
        if (m_localStatusLabel)
        {
            m_localStatusLabel->setText(QStringLiteral("正在刷新本地数据目录…"));
        }
        return;
    }

    m_localTree->setRootIndex(favoritesViewIndex);
    if (m_browserModel->canFetchMore(favoritesIndex))
    {
        m_browserModel->fetchMore(favoritesIndex);
    }
    m_localTree->collapseAll();
    m_localTree->expand(favoritesViewIndex);

    if (m_localStatusLabel)
    {
        const int folderCount = m_browserModel->rowCount(favoritesIndex);
        m_localStatusLabel->setText(
            folderCount > 0
                ? QStringLiteral("已连接 %1 个本地数据文件夹").arg(folderCount)
                : QStringLiteral("未连接本地数据文件夹"));
    }
}

void DataCatalogWidget::connectLocalFolder()
{
    const QString path = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("连接本地数据文件夹"),
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (path.isEmpty() || !m_browserModel)
    {
        return;
    }

    QString name = QFileInfo(path).fileName();
    if (name.isEmpty())
    {
        name = path;
    }

    m_browserModel->addFavoriteDirectory(path, name);
    if (m_localStatusLabel)
    {
        m_localStatusLabel->setText(
            QStringLiteral("正在连接本地数据文件夹：%1")
                .arg(QDir::toNativeSeparators(path)));
    }

    // addFavoriteDirectory inserts the selected item immediately when the
    // Favorites item is populated.  A queued refresh covers the startup case
    // where that item is populated asynchronously, without clearing the just
    // inserted row prematurely.
    QTimer::singleShot(0, this, [this]()
    {
        showFavoritesInTree();
    });
}

void DataCatalogWidget::disconnectSelectedLocalFolder()
{
    if (!m_browserModel || !m_localTree)
    {
        return;
    }

    const QModelIndex index = m_localTree->currentIndex();
    const QModelIndex sourceIndex = m_localProxyModel
        ? m_localProxyModel->mapToSource(index)
        : index;
    QgsDataItem* item = m_browserModel->dataItem(sourceIndex);
    const QModelIndex favoritesIndex = favoritesRootIndex();
    QgsDataItem* favoritesItem = m_browserModel->dataItem(favoritesIndex);
    if (!item || !favoritesItem || item->parent() != favoritesItem ||
        !qobject_cast<QgsDirectoryItem*>(item))
    {
        QMessageBox::information(
            this,
            QStringLiteral("断开文件夹连接"),
            QStringLiteral("请选择已连接文件夹的根节点。"));
        return;
    }

    m_browserModel->removeFavorite(sourceIndex);
    showFavoritesInTree();
}

void DataCatalogWidget::refreshLocalFolders()
{
    if (!m_browserModel || !m_localTree)
    {
        return;
    }

    QModelIndex favoritesIndex = favoritesRootIndex();
    if (!favoritesIndex.isValid())
    {
        m_browserModel->reload();
        showFavoritesInTree();
        return;
    }

    showFavoritesInTree();
    QgsDataItem* favoritesItem = m_browserModel->dataItem(favoritesIndex);
    if (favoritesItem && favoritesItem->state() == Qgis::BrowserItemState::Populated)
    {
        m_browserModel->refresh(favoritesIndex);
    }
    QTimer::singleShot(0, this, &DataCatalogWidget::showFavoritesInTree);
}

void DataCatalogWidget::refreshDatabaseData()
{
    if (!m_databaseModel)
    {
        return;
    }

    if (synchronizeMenuDatabaseConnection(true))
    {
        return;
    }

    loadDatabaseData();
}

void DataCatalogWidget::loadDatabaseData()
{
    if (!m_databaseModel)
    {
        return;
    }

    QSet<int> expandedDirectoryIds;
    std::function<void(QStandardItem*)> rememberExpanded =
        [this, &expandedDirectoryIds, &rememberExpanded](QStandardItem* item)
    {
        if (!item)
        {
            return;
        }
        if (isProductCatalogDirectory(item) &&
            m_databaseTree->isExpanded(m_databaseModel->indexFromItem(item)))
        {
            expandedDirectoryIds.insert(productDirectoryId(item));
        }
        for (int row = 0; row < item->rowCount(); ++row)
        {
            rememberExpanded(item->child(row));
        }
    };
    for (int row = 0; row < m_databaseModel->rowCount(); ++row)
    {
        rememberExpanded(m_databaseModel->item(row));
    }

    m_databaseModel->removeRows(0, m_databaseModel->rowCount());
    if (!gCurrentUserSession.isLoggedIn)
    {
        showDatabaseMessage(QStringLiteral("请先登录后再连接数据库。"), false);
        return;
    }

    if (m_databaseConnectionName.isEmpty())
    {
        showDatabaseMessage(
            QStringLiteral("请先通过菜单栏“数据库连接配置”连接数据库。"),
            false);
        return;
    }

    QSqlDatabase database = catalogDatabase(m_databaseConnectionName);
    if (!database.isValid() || !database.isOpen())
    {
        showDatabaseMessage(
            QStringLiteral("菜单栏数据库连接尚未打开，请通过“数据库连接配置”重新连接。"),
            true);
        return;
    }

    QStandardItem* databaseItem = new QStandardItem(
        catalogIcon(QStringLiteral("database")),
        database.databaseName());
    databaseItem->setEditable(false);
    databaseItem->setData(database.databaseName(), DatabaseNameRole);
    databaseItem->setData(
        static_cast<int>(ProductCatalogNodeKind::Directory),
        ProductNodeKindRole);
    databaseItem->setData(0, ProductDirectoryIdRole);
    databaseItem->setData(0, ProductDirectoryNodeTypeRole);
    databaseItem->setData(QStringLiteral("product-directory"), DatabaseLayerTypeRole);
    databaseItem->setFlags(databaseItem->flags() | Qt::ItemIsUserCheckable);
    databaseItem->setCheckState(
        m_checkedDatabaseKeys.contains(databaseCheckKey(databaseItem))
            ? Qt::Checked
            : Qt::Unchecked);
    const QString databaseAlias = m_databaseDisplayAliases.value(
        databaseDisplayAliasKey(databaseItem));
    if (!databaseAlias.isEmpty())
    {
        databaseItem->setText(databaseAlias);
    }
    m_databaseModel->appendRow(databaseItem);

    DataCatalogProductCatalog productCatalog(database);
    QString productCatalogError;
    if (productCatalog.hasProductCatalogSchema(&productCatalogError))
    {
        if (!loadProductDirectoryTree(database, databaseItem, &productCatalogError))
        {
            showDatabaseMessage(
                QStringLiteral("读取成果目录失败：%1")
                    .arg(productCatalogError.isEmpty()
                        ? QStringLiteral("数据库目录结构不可用。")
                        : productCatalogError),
                true);
            return;
        }
        const QModelIndex databaseIndex = m_databaseModel->indexFromItem(databaseItem);
        m_databaseTree->collapseAll();
        m_databaseTree->expand(databaseIndex);
        std::function<void(QStandardItem*)> restoreExpanded =
            [this, &expandedDirectoryIds, &restoreExpanded](QStandardItem* item)
        {
            if (!item)
            {
                return;
            }
            if (isProductCatalogDirectory(item) &&
                expandedDirectoryIds.contains(productDirectoryId(item)))
            {
                m_databaseTree->expand(m_databaseModel->indexFromItem(item));
            }
            for (int row = 0; row < item->rowCount(); ++row)
            {
                restoreExpanded(item->child(row));
            }
        };
        restoreExpanded(databaseItem);
        return;
    }

    // A database without the成果目录 schema keeps the legacy schema/table
    // view and its original context-menu behavior.
    databaseItem->setData(QVariant(), ProductNodeKindRole);
    databaseItem->setData(QVariant(), ProductDirectoryIdRole);
    databaseItem->setData(QVariant(), DatabaseLayerTypeRole);
    databaseItem->setFlags(databaseItem->flags() & ~Qt::ItemIsUserCheckable);

    QSqlQuery query(database);
    if (!query.exec(databaseCatalogQuery()))
    {
        showDatabaseMessage(
            QStringLiteral("读取数据库目录失败：%1").arg(query.lastError().text()),
            true);
        return;
    }

    QList<DatabaseCatalogEntry> entries;
    QHash<QString, int> firstEntryByTable;
    while (query.next())
    {
        DatabaseCatalogEntry entry;
        entry.schemaName = query.value(0).toString();
        entry.tableName = query.value(1).toString();
        firstEntryByTable.insert(
            databaseCatalogKey(entry.schemaName, entry.tableName),
            entries.size());
        entries.append(entry);
    }
    if (query.lastError().isValid())
    {
        showDatabaseMessage(
            QStringLiteral("读取数据库目录失败：%1").arg(query.lastError().text()),
            true);
        return;
    }

    QStringList metadataWarnings;
    const auto appendSpatialEntries = [&database, &entries, &firstEntryByTable](
        const QString& relationName,
        const QString& metadataQuery,
        const QString& layerType,
        QStringList* warnings)
    {
        QString probeError;
        if (!databaseMetadataRelationExists(
            database,
            relationName,
            &probeError))
        {
            if (!probeError.isEmpty())
            {
                warnings->append(
                    QStringLiteral("检查 %1 失败：%2").arg(relationName, probeError));
            }
            return;
        }

        QSqlQuery metadata(database);
        if (!metadata.exec(metadataQuery))
        {
            warnings->append(
                QStringLiteral("读取 %1 失败：%2")
                    .arg(relationName, metadata.lastError().text()));
            return;
        }

        while (metadata.next())
        {
            const QString schemaName = metadata.value(0).toString();
            const QString tableName = metadata.value(1).toString();
            const int entryIndex = firstEntryByTable.value(
                databaseCatalogKey(schemaName, tableName),
                -1);
            if (entryIndex < 0)
            {
                continue;
            }

            DatabaseCatalogEntry spatialEntry = entries.at(entryIndex);
            spatialEntry.dataColumn = metadata.value(2).toString();
            spatialEntry.dataType = metadata.value(3).toString();
            spatialEntry.srid = metadata.value(4).toInt();
            spatialEntry.layerType = layerType;
            if (entries.at(entryIndex).layerType == QStringLiteral("table"))
            {
                entries[entryIndex] = spatialEntry;
            }
            else
            {
                entries.append(spatialEntry);
            }
        }
        if (metadata.lastError().isValid())
        {
            warnings->append(
                QStringLiteral("读取 %1 失败：%2")
                    .arg(relationName, metadata.lastError().text()));
        }
    };

    appendSpatialEntries(
        QStringLiteral("geometry_columns"),
        databaseVectorMetadataQuery(),
        QStringLiteral("vector"),
        &metadataWarnings);
    appendSpatialEntries(
        QStringLiteral("raster_columns"),
        databaseRasterMetadataQuery(),
        QStringLiteral("raster"),
        &metadataWarnings);

    int dataItemCount = 0;
    for (const DatabaseCatalogEntry& entry : entries)
    {
        const QString& schemaName = entry.schemaName;
        const QString& tableName = entry.tableName;
        const QString& geometryColumn = entry.dataColumn;
        const QString& geometryType = entry.dataType;
        const int srid = entry.srid;
        const QString& layerType = entry.layerType;

        if ((layerType == QStringLiteral("vector") ||
             layerType == QStringLiteral("raster")) &&
            m_removedDatabaseKeys.contains(databaseRemovalKey(
                database.databaseName(),
                schemaName,
                tableName,
                layerType,
                geometryColumn)))
        {
            continue;
        }

        QStandardItem* schema = schemaItem(schemaName, databaseItem);
        QStandardItem* table = new QStandardItem(
            databaseItemIcon(layerType),
            tableName + databaseItemSuffix(layerType, geometryType, srid, geometryColumn));
        table->setEditable(false);
        if (layerType == QStringLiteral("table"))
        {
            table->setEnabled(false);
            table->setToolTip(QStringLiteral("普通表仅用于目录浏览，不能作为地图图层加载。"));
        }
        else
        {
            table->setToolTip(QStringLiteral("双击加载 %1.%2").arg(schemaName, tableName));
        }
        table->setData(layerType, DatabaseLayerTypeRole);
        table->setData(schemaName, DatabaseSchemaRole);
        table->setData(tableName, DatabaseTableRole);
        table->setData(geometryColumn, DatabaseGeometryColumnRole);
        table->setData(database.databaseName(), DatabaseNameRole);
        if (layerType == QStringLiteral("vector") ||
            layerType == QStringLiteral("raster"))
        {
            table->setFlags(table->flags() | Qt::ItemIsUserCheckable);
            table->setCheckState(
                m_checkedDatabaseKeys.contains(databaseCheckKey(table))
                    ? Qt::Checked
                    : Qt::Unchecked);
        }
        const QString tableAlias = m_databaseDisplayAliases.value(
            databaseDisplayAliasKey(table));
        if (!tableAlias.isEmpty())
        {
            table->setText(tableAlias);
        }
        schema->appendRow(table);
        ++dataItemCount;
    }

    for (const QString& warning : metadataWarnings)
    {
        QStandardItem* warningItem = new QStandardItem(
            style()->standardIcon(QStyle::SP_MessageBoxWarning),
            warning);
        warningItem->setEditable(false);
        warningItem->setEnabled(false);
        databaseItem->appendRow(warningItem);
    }

    if (dataItemCount == 0)
    {
        QStandardItem* emptyItem = new QStandardItem(QStringLiteral("当前数据库没有可读取的业务数据。"));
        emptyItem->setEditable(false);
        emptyItem->setEnabled(false);
        databaseItem->appendRow(emptyItem);
    }

    m_databaseTree->expand(databaseItem->index());
}

bool DataCatalogWidget::loadProductDirectoryTree(
    QSqlDatabase database,
    QStandardItem* databaseItem,
    QString* errorMessage)
{
    if (!database.isValid() || !database.isOpen() || !databaseItem)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("成果目录数据库连接无效。");
        }
        return false;
    }

    DataCatalogProductCatalog catalog(database);
    if (!catalog.ensureFixedDirectories(errorMessage))
    {
        return false;
    }

    QString orphanProductsError;
    const QList<ProductRecord> orphanGdbProducts = catalog.unassignedGdbProducts(
        &orphanProductsError);
    if (!orphanProductsError.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = orphanProductsError;
        }
        return false;
    }

    const QSet<QString> gdbSourceDirectoryNames = catalogGdbSourceDirectoryNames(database);
    appendProductDirectoryChildren(
        databaseItem,
        0,
        database,
        gdbSourceDirectoryNames,
        orphanGdbProducts);
    if (databaseItem->rowCount() == 0)
    {
        QStandardItem* emptyItem = new QStandardItem(
            QStringLiteral("当前数据库没有可读取的成果目录。"));
        emptyItem->setEditable(false);
        emptyItem->setEnabled(false);
        databaseItem->appendRow(emptyItem);
    }
    return true;
}

bool DataCatalogWidget::appendProductDirectoryChildren(
    QStandardItem* parentItem,
    int parentDirectoryId,
    QSqlDatabase database,
    const QSet<QString>& gdbSourceDirectoryNames,
    const QList<ProductRecord>& orphanGdbProducts)
{
    if (!parentItem || !database.isValid() || !database.isOpen())
    {
        return false;
    }

    bool subtreeHasGdb = false;

    QString schemaName;
    QSqlQuery schemaQuery(database);
    if (schemaQuery.exec(QStringLiteral("SELECT current_schema()")) && schemaQuery.next())
    {
        schemaName = schemaQuery.value(0).toString();
    }

    DataCatalogProductCatalog catalog(database);
    QString errorMessage;
    const QList<ProductDirectoryRecord> directories = catalog.childDirectories(
        parentDirectoryId,
        &errorMessage);
    for (const ProductDirectoryRecord& directory : directories)
    {
        const QList<ProductRecord> products = catalog.productsForDirectory(
            directory.id,
            &errorMessage);
        const QString databaseName = parentItem->data(DatabaseNameRole).toString();
        QList<ProductRecord> visibleProducts;
        for (const ProductRecord& product : products)
        {
            if (!m_removedDatabaseKeys.contains(
                    productRemovalKey(databaseName, product.id)))
            {
                visibleProducts.append(product);
            }
        }
        const bool directoryMatchesGdbSource = directory.nodeType != 1 &&
            gdbSourceDirectoryNames.contains(
                QFileInfo(directory.name.trimmed()).completeBaseName().trimmed().toCaseFolded());
        if (directoryMatchesGdbSource)
        {
            for (const ProductRecord& product : orphanGdbProducts)
            {
                if (catalogGdbSourceMatchesDirectory(product.filePath, directory.name) &&
                    !m_removedDatabaseKeys.contains(
                        productRemovalKey(databaseName, product.id)))
                {
                    visibleProducts.append(product);
                }
            }
        }
        if (directory.nodeType == 1 && visibleProducts.isEmpty())
        {
            continue;
        }
        bool directoryHasGdb = isGdbFormatOrPath(directory.name) ||
            gdbSourceDirectoryNames.contains(directory.name.trimmed().toCaseFolded());
        for (const ProductRecord& product : visibleProducts)
        {
            if (isGdbProductFormat(product.fileFormat) ||
                isGdbFormatOrPath(product.filePath))
            {
                directoryHasGdb = true;
                break;
            }
        }
        QIcon directoryIcon = catalogDirectoryIcon(directory.name);
        const bool isImageryOrHillshade =
            directory.name == QStringLiteral("\u5f71\u50cf") ||
            directory.name == QStringLiteral("\u6655\u6e32");
        if (directory.nodeType == 1 && !visibleProducts.isEmpty())
        {
            // This directory is a layer, even if its name matches a category.
            const ProductRecord& product = visibleProducts.first();
            directoryIcon = productCatalogItemIcon(product.productType, product.geometryType,
                product.fileFormat, product.filePath, product.productName);
        }
        else if (!isImageryOrHillshade && !visibleProducts.isEmpty())
        {
            for (const ProductRecord& product : visibleProducts)
            {
                // A GDB source is not evidence that this folder is a GDB.
                // Actual container identity is applied after building children.
                if (isGdbProductFormat(product.fileFormat) ||
                    isGdbFormatOrPath(product.filePath))
                {
                    continue;
                }
                bool formatMatched = false;
                for (const QString& candidate : {
                    product.fileFormat, product.filePath, product.productName})
                {
                    const QIcon formatIcon = catalogFileFormatIcon(candidate);
                    if (!formatIcon.isNull())
                    {
                        directoryIcon = formatIcon;
                        formatMatched = true;
                        break;
                    }
                }
                if (formatMatched)
                {
                    break;
                }
            }
        }

        QStandardItem* directoryItem = new QStandardItem(
            directoryIcon,
            directory.name);
        directoryItem->setEditable(false);
        directoryItem->setData(
            static_cast<int>(ProductCatalogNodeKind::Directory),
            ProductNodeKindRole);
        directoryItem->setData(directory.id, ProductDirectoryIdRole);
        directoryItem->setData(directory.nodeType, ProductDirectoryNodeTypeRole);
        directoryItem->setData(
            parentItem->data(DatabaseNameRole),
            DatabaseNameRole);
        directoryItem->setData(QStringLiteral("product-directory"), DatabaseLayerTypeRole);
        directoryItem->setFlags(directoryItem->flags() | Qt::ItemIsUserCheckable);
        directoryItem->setCheckState(
            m_checkedDatabaseKeys.contains(databaseCheckKey(directoryItem))
                ? Qt::Checked
                : Qt::Unchecked);
        directoryItem->setToolTip(QStringLiteral("成果目录：%1").arg(directory.name));

        // 普通目录下的成果都应可见，包括右键导入到自建目录或上级目录的成果。
        // node_type==1 的图层目录本身代表成果，避免再追加同名子节点。
        if (directory.nodeType != 1)
        {
            for (const ProductRecord& product : visibleProducts)
            {
                QStandardItem* productItem = new QStandardItem(
                    productCatalogItemIconForDirectory(product, directory.name),
                    product.productName);
                productItem->setEditable(false);
                productItem->setData(
                    static_cast<int>(ProductCatalogNodeKind::Product),
                    ProductNodeKindRole);
                productItem->setData(directory.id, ProductDirectoryIdRole);
                productItem->setData(product.id, ProductIdRole);
                productItem->setData(product.productType, ProductTypeRole);
                productItem->setData(product.geometryType, ProductGeometryTypeRole);
                productItem->setData(product.layerTableName, ProductLayerTableRole);
                productItem->setData(product.fileOid, ProductFileOidRole);
                productItem->setData(product.fileFormat, ProductFileFormatRole);
                productItem->setData(product.filePath, ProductFilePathRole);
                productItem->setData(product.securityLevel, ProductSecurityLevelRole);
                productItem->setData(schemaName, DatabaseSchemaRole);
                productItem->setData(product.layerTableName, DatabaseTableRole);
                const bool productIsRaster = isProductRasterType(product.productType);
                const bool productIsVector = isProductVectorType(product.productType);
                productItem->setData(
                    productIsRaster
                        ? QStringLiteral("product-raster")
                        : (productIsVector
                            ? QStringLiteral("product-vector")
                            : QStringLiteral("product-file")),
                    DatabaseLayerTypeRole);
                productItem->setData(
                    productIsRaster
                        ? QStringLiteral("rast")
                        : QStringLiteral("geom"),
                    DatabaseGeometryColumnRole);
                productItem->setData(
                    parentItem->data(DatabaseNameRole),
                    DatabaseNameRole);
                productItem->setToolTip(
                    QStringLiteral("成果名称：%1\n类型：%2\n格式：%3\n密级：%4")
                        .arg(product.productName)
                        .arg(product.productType)
                        .arg(product.fileFormat.toUpper())
                        .arg(product.securityLevel));
                productItem->setFlags(productItem->flags() | Qt::ItemIsUserCheckable);
                productItem->setCheckState(
                    m_checkedDatabaseKeys.contains(databaseCheckKey(productItem))
                        ? Qt::Checked
                        : Qt::Unchecked);
                directoryItem->appendRow(productItem);
            }
        }

        parentItem->appendRow(directoryItem);
        const bool childrenHaveGdb = appendProductDirectoryChildren(
            directoryItem,
            directory.id,
            database,
            gdbSourceDirectoryNames,
            orphanGdbProducts);
        // Only actual container nodes get collection icons. Descendant source
        // formats must not override a feature-class leaf or an ordinary folder.
        const bool isGdbContainer = isGdbFormatOrPath(directory.name)
            || gdbSourceDirectoryNames.contains(directory.name.trimmed().toCaseFolded());
        if (directory.nodeType != 1 && isGdbContainer && !isCatalogCategoryDirectory(directory.name))
        {
            directoryItem->setIcon(QgsDataCollectionItem::iconDataCollection());
        }
        subtreeHasGdb = subtreeHasGdb || directoryHasGdb || childrenHaveGdb;
    }
    return subtreeHasGdb;
}

bool DataCatalogWidget::isProductCatalogDirectory(const QStandardItem* item) const
{
    return item && item->data(ProductNodeKindRole).toInt() ==
        static_cast<int>(ProductCatalogNodeKind::Directory);
}

bool DataCatalogWidget::isProductCatalogProduct(const QStandardItem* item) const
{
    return item && item->data(ProductNodeKindRole).toInt() ==
        static_cast<int>(ProductCatalogNodeKind::Product);
}

bool DataCatalogWidget::isProductCatalogRoot(const QStandardItem* item) const
{
    return isProductCatalogDirectory(item) && productDirectoryId(item) == 0;
}

int DataCatalogWidget::productDirectoryId(const QStandardItem* item) const
{
    return item ? item->data(ProductDirectoryIdRole).toInt() : -1;
}

int DataCatalogWidget::productId(const QStandardItem* item) const
{
    return item ? item->data(ProductIdRole).toInt() : -1;
}

QList<int> DataCatalogWidget::allProductIdsUnderItem(const QStandardItem* item) const
{
    QList<int> result;
    if (!item)
    {
        return result;
    }
    if (isProductCatalogProduct(item))
    {
        const int id = productId(item);
        if (id > 0)
        {
            result.append(id);
        }
        return result;
    }

    if (isProductCatalogDirectory(item) && productDirectoryId(item) > 0)
    {
        const QSqlDatabase database = catalogDatabase(m_databaseConnectionName);
        if (database.isValid() && database.isOpen())
        {
            DataCatalogProductCatalog catalog(database);
            const QList<ProductRecord> products = catalog.productsForDirectory(
                productDirectoryId(item));
            for (const ProductRecord& product : products)
            {
                if (product.id > 0 && !result.contains(product.id))
                {
                    result.append(product.id);
                }
            }
        }
    }

    for (int row = 0; row < item->rowCount(); ++row)
    {
        const QList<int> childIds = allProductIdsUnderItem(item->child(row));
        for (const int childId : childIds)
        {
            if (!result.contains(childId))
            {
                result.append(childId);
            }
        }
    }
    return result;
}

QList<int> DataCatalogWidget::productIdsForContext(const CatalogContext& context) const
{
    QList<int> result;
    if (context.source != CatalogSource::Database || !context.databaseItem)
    {
        return result;
    }
    if (isProductCatalogProduct(context.databaseItem))
    {
        const int id = productId(context.databaseItem);
        if (id > 0)
        {
            result.append(id);
        }
        return result;
    }

    const QSqlDatabase database = catalogDatabase(m_databaseConnectionName);
    DataCatalogProductCatalog catalog(database);
    const auto appendDirectoryProducts =
        [&result, &catalog](const QStandardItem* item)
    {
        if (!item)
        {
            return;
        }
        const int directoryId = item->data(ProductDirectoryIdRole).toInt();
        if (directoryId <= 0)
        {
            return;
        }
        const QList<ProductRecord> products = catalog.productsForDirectory(directoryId);
        for (const ProductRecord& product : products)
        {
            if (product.id > 0 && !result.contains(product.id))
            {
                result.append(product.id);
            }
        }
    };

    std::function<void(const QStandardItem*)> collect = [&](const QStandardItem* item)
    {
        if (!item)
        {
            return;
        }
        if (isProductCatalogProduct(item))
        {
            if (item->checkState() == Qt::Checked)
            {
                const int id = productId(item);
                if (id > 0 && !result.contains(id))
                {
                    result.append(id);
                }
            }
            return;
        }

        if (isProductCatalogDirectory(item))
        {
            const bool isContextItem = item == context.databaseItem;
            const bool isLayerDirectory =
                item->data(ProductDirectoryNodeTypeRole).toInt() == 1;
            const Qt::CheckState state = item->checkState();

            // 原始目录树中的图层节点本身代表其目录下的成果，不再有隐藏的
            // product_metadata 子节点；右键图层节点时应直接取得其成果。
            if (isLayerDirectory &&
                (isContextItem || state == Qt::Checked))
            {
                const QList<int> ids = allProductIdsUnderItem(item);
                for (const int id : ids)
                {
                    if (id > 0 && !result.contains(id))
                    {
                        result.append(id);
                    }
                }
                return;
            }

            if (!isContextItem && state == Qt::Unchecked)
            {
                return;
            }
            if (isLayerDirectory || item->rowCount() == 0)
            {
                appendDirectoryProducts(item);
            }
        }

        for (int row = 0; row < item->rowCount(); ++row)
        {
            collect(item->child(row));
        }
    };
    collect(context.databaseItem);
    return result;
}

void DataCatalogWidget::setProductSubtreeCheck(
    QStandardItem* item,
    Qt::CheckState state)
{
    if (!item)
    {
        return;
    }
    item->setCheckState(state);
    for (int row = 0; row < item->rowCount(); ++row)
    {
        setProductSubtreeCheck(item->child(row), state);
    }
}

void DataCatalogWidget::synchronizeProductCheckCache(
    const QStandardItem* item,
    const QList<int>& productIds,
    Qt::CheckState state)
{
    if (!item || productIds.isEmpty())
    {
        return;
    }

    const QString databaseName = item->data(DatabaseNameRole).toString();
    if (databaseName.isEmpty())
    {
        return;
    }

    for (const int id : productIds)
    {
        if (id <= 0)
        {
            continue;
        }

        const QString key =
            QStringLiteral("product-check:%1\\x1f%2")
                .arg(databaseName)
                .arg(id);
        if (state == Qt::Checked)
        {
            m_checkedDatabaseKeys.insert(key);
        }
        else
        {
            m_checkedDatabaseKeys.remove(key);
        }
    }
}

void DataCatalogWidget::refreshProductParentCheckStates(QStandardItem* item)
{
    if (!item)
    {
        return;
    }
    QStandardItem* parent = item->parent();
    while (parent)
    {
        int checkableCount = 0;
        int checkedCount = 0;
        int partialCount = 0;
        for (int row = 0; row < parent->rowCount(); ++row)
        {
            QStandardItem* child = parent->child(row);
            if (!child || !child->isCheckable())
            {
                continue;
            }
            ++checkableCount;
            if (child->checkState() == Qt::Checked)
            {
                ++checkedCount;
            }
            else if (child->checkState() == Qt::PartiallyChecked)
            {
                ++partialCount;
            }
        }
        if (checkableCount > 0)
        {
            const Qt::CheckState state = checkedCount == checkableCount
                ? Qt::Checked
                : ((checkedCount > 0 || partialCount > 0)
                    ? Qt::PartiallyChecked
                    : Qt::Unchecked);
            parent->setCheckState(state);
        }
        parent = parent->parent();
    }
}

void DataCatalogWidget::createProductDirectory(const CatalogContext& context)
{
    if (!context.databaseItem || !isProductCatalogDirectory(context.databaseItem))
    {
        return;
    }
    if (!gCurrentUserSession.canOperateProductStore())
    {
        QMessageBox::warning(this, QStringLiteral("权限不足"),
            QStringLiteral("当前用户无权新建目录。"));
        return;
    }
    QInputDialog inputDialog(this);
    inputDialog.setWindowTitle(QStringLiteral("新建目录"));
    inputDialog.setLabelText(
        QStringLiteral("在当前节点「%1」下新建子目录，请输入目录名称：")
            .arg(context.databaseItem->text()));
    inputDialog.setInputMode(QInputDialog::TextInput);
    inputDialog.setTextEchoMode(QLineEdit::Normal);
    inputDialog.setTextValue(QString());
    inputDialog.setOkButtonText(QStringLiteral("确定"));
    inputDialog.setCancelButtonText(QStringLiteral("取消"));
    const bool accepted = inputDialog.exec() == QDialog::Accepted;
    const QString name = inputDialog.textValue().trimmed();
    if (!accepted || name.isEmpty())
    {
        return;
    }

    QSqlDatabase database = catalogDatabase(m_databaseConnectionName);
    DataCatalogProductCatalog catalog(database);
    QString errorMessage;
    int newDirectoryId = -1;
    if (!catalog.createDirectory(
            productDirectoryId(context.databaseItem),
            name,
            gCurrentUserSession.userName,
            &newDirectoryId,
            &errorMessage))
    {
        QMessageBox::warning(
            this,
            QStringLiteral("新建目录"),
            QStringLiteral("新建目录失败：%1").arg(errorMessage));
        return;
    }
    Q_UNUSED(newDirectoryId);
    refreshDatabaseData();
}

void DataCatalogWidget::deleteProductDirectory(const CatalogContext& context)
{
    if (!context.databaseItem || !isProductCatalogDirectory(context.databaseItem) ||
        isProductCatalogRoot(context.databaseItem))
    {
        return;
    }
    if (!gCurrentUserSession.canOperateProductStore())
    {
        QMessageBox::warning(this, QStringLiteral("权限不足"),
            QStringLiteral("当前用户无权删除目录。"));
        return;
    }

    const QList<int> productIds = allProductIdsUnderItem(context.databaseItem);
    QString message = QStringLiteral("确定要删除目录「%1」及其所有子目录吗？")
        .arg(context.databaseItem->text());
    if (!productIds.isEmpty())
    {
        message += QStringLiteral("\n该目录下共有 %1 条产品数据，将一并删除！")
            .arg(productIds.size());
    }
    message += QStringLiteral("\n\n此操作不可恢复，是否继续？");
    QMessageBox confirmation(
        QMessageBox::Question,
        QStringLiteral("确认删除"),
        message,
        QMessageBox::Yes | QMessageBox::No,
        this);
    confirmation.setDefaultButton(QMessageBox::No);
    confirmation.button(QMessageBox::Yes)->setText(QStringLiteral("是"));
    confirmation.button(QMessageBox::No)->setText(QStringLiteral("否"));
    if (confirmation.exec() != QMessageBox::Yes)
    {
        return;
    }

    QSqlDatabase database = catalogDatabase(m_databaseConnectionName);
    DataCatalogProductCatalog catalog(database);
    QString errorMessage;
    if (!catalog.deleteDirectory(
            productDirectoryId(context.databaseItem),
            &errorMessage))
    {
        QMessageBox::warning(
            this,
            QStringLiteral("删除目录"),
            QStringLiteral("删除目录失败：%1").arg(errorMessage));
        return;
    }
    refreshDatabaseData();
}

void DataCatalogWidget::renameProductDirectory(const CatalogContext& context)
{
    if (!context.databaseItem || !isProductCatalogDirectory(context.databaseItem) ||
        isProductCatalogRoot(context.databaseItem))
    {
        return;
    }

    bool accepted = false;
    const QString name = QInputDialog::getText(
        this,
        QStringLiteral("重命名目录"),
        QStringLiteral("请输入新的目录名称："),
        QLineEdit::Normal,
        context.databaseItem->text(),
        &accepted).trimmed();
    if (!accepted || name.isEmpty() || name == context.databaseItem->text())
    {
        return;
    }

    QSqlDatabase database = catalogDatabase(m_databaseConnectionName);
    DataCatalogProductCatalog catalog(database);
    QString errorMessage;
    if (!catalog.renameDirectory(
            productDirectoryId(context.databaseItem),
            name,
            &errorMessage))
    {
        QMessageBox::warning(
            this,
            QStringLiteral("重命名目录"),
            QStringLiteral("重命名失败：%1").arg(errorMessage));
        return;
    }
    refreshDatabaseData();
}

void DataCatalogWidget::dispatchProductOperation(
    const QString& operation,
    const CatalogContext& context,
    const QList<int>& productIds,
    const QgsMimeDataUtils::UriList& localUris)
{
    if (context.source == CatalogSource::Local)
    {
        if (operation == QStringLiteral("map"))
        {
            if (!m_loadCallback)
            {
                return;
            }
            for (const QgsMimeDataUtils::Uri& uri : localUris)
            {
                if (isSupportedCatalogUri(uri))
                {
                    m_loadCallback(uri);
                }
            }
        }
        else if (operation == QStringLiteral("export"))
        {
            if (isProductMenuDirectory(context))
            {
                batchExportUris(
                    localUris,
                    0,
                    QStringLiteral("导出所选数据"));
            }
            else if (!localUris.isEmpty())
            {
                exportUri(localUris.first());
            }
        }
        else if (operation == QStringLiteral("rename-directory"))
        {
            renameContextItem(context);
        }
        else if ((operation == QStringLiteral("range") ||
                  operation == QStringLiteral("main-area") ||
                  operation == QStringLiteral("theme")) &&
                 m_localProductOperationCallback && !localUris.isEmpty())
        {
            m_localProductOperationCallback(operation, localUris);
        }
        return;
    }

    if (context.source == CatalogSource::Database &&
        operation == QStringLiteral("map"))
    {
        const QList<int> mapProductIds = productIds.isEmpty()
            ? productIdsForContext(context)
            : productIds;
        addProductIdsToMap(mapProductIds);
        return;
    }

    if (!m_productOperationCallback || !context.databaseItem)
    {
        return;
    }
    m_productOperationCallback(
        operation,
        productDirectoryId(context.databaseItem),
        productId(context.databaseItem),
        productIds);
}

void DataCatalogWidget::addProductIdsToMap(const QList<int>& productIds)
{
    if (!m_loadCallback || productIds.isEmpty())
    {
        return;
    }
    if (!gCurrentUserSession.isLoggedIn)
    {
        QMessageBox::warning(
            this,
            QStringLiteral("添加到地图"),
            QStringLiteral("请先登录后再加载数据库成果。"));
        return;
    }

    const QSqlDatabase database = catalogDatabase(m_databaseConnectionName);
    if (!database.isValid() || !database.isOpen())
    {
        QMessageBox::warning(
            this,
            QStringLiteral("添加到地图"),
            QStringLiteral("数据库连接不可用，请重新连接后再试。"));
        return;
    }

    QString schemaName = QStringLiteral("public");
    QSqlQuery schemaQuery(database);
    if (schemaQuery.exec(QStringLiteral("SELECT current_schema()")) &&
        schemaQuery.next() && !schemaQuery.value(0).toString().isEmpty())
    {
        schemaName = schemaQuery.value(0).toString();
    }

    DataCatalogProductCatalog catalog(database);
    QSet<int> addedProductIds;
    QStringList skippedProducts;
    for (const int id : productIds)
    {
        if (id <= 0 || addedProductIds.contains(id))
        {
            continue;
        }
        addedProductIds.insert(id);

        QString errorMessage;
        const ProductRecord product = catalog.product(id, &errorMessage);
        if (product.id != id)
        {
            skippedProducts.append(
                QStringLiteral("ID %1：%2").arg(id).arg(
                    errorMessage.isEmpty()
                        ? QStringLiteral("找不到成果记录")
                        : errorMessage));
            continue;
        }

        const bool isVector = isProductVectorType(product.productType);
        const bool isRaster = isProductRasterType(product.productType);
        if ((!isVector && !isRaster) || product.layerTableName.isEmpty())
        {
            skippedProducts.append(
                QStringLiteral("%1：不是可直接加载的矢量或栅格图层")
                    .arg(product.productName));
            continue;
        }

        QgsDataSourceUri sourceUri;
        sourceUri.setConnection(
            database.hostName(),
            QString::number(database.port()),
            database.databaseName(),
            database.userName(),
            database.password());
        sourceUri.setDataSource(
            schemaName,
            product.layerTableName,
            isRaster ? QStringLiteral("rast") : QStringLiteral("geom"));

        QgsMimeDataUtils::Uri uri;
        uri.layerType = isRaster ? QStringLiteral("raster") : QStringLiteral("vector");
        uri.providerKey = isRaster
            ? QStringLiteral("YOUR_PASSWORDraster")
            : QStringLiteral("YOUR_PASSWORD");
        uri.name = product.productName;
        uri.uri = sourceUri.uri(false);
        m_loadCallback(uri);
    }

    if (!skippedProducts.isEmpty())
    {
        QMessageBox::warning(
            this,
            QStringLiteral("添加到地图"),
            QStringLiteral("以下成果未能提交加载：\n%1")
                .arg(skippedProducts.join(QChar('\n'))));
    }
}

void DataCatalogWidget::addProductItemToMap(const CatalogContext& context)
{
    if (!m_loadCallback || !context.databaseItem || !isProductCatalogProduct(context.databaseItem))
    {
        return;
    }
    const QgsMimeDataUtils::Uri uri = databaseUri(context.databaseItem);
    if (isSupportedCatalogUri(uri))
    {
        m_loadCallback(uri);
    }
    else
    {
        QMessageBox::information(
            this,
            QStringLiteral("添加到地图"),
            QStringLiteral("该成果不是可直接加载的矢量或栅格图层。"));
    }
}

void DataCatalogWidget::addProductContextItemsToMap(
    const QVector<CatalogContext>& contexts)
{
    QList<int> productIds;
    for (const CatalogContext& context : contexts)
    {
        if (context.source != CatalogSource::Database || !context.databaseItem)
        {
            continue;
        }
        const QList<int> ids = isProductCatalogProduct(context.databaseItem)
            ? QList<int>() << productId(context.databaseItem)
            : allProductIdsUnderItem(context.databaseItem);
        for (const int id : ids)
        {
            if (id > 0 && !productIds.contains(id))
            {
                productIds.append(id);
            }
        }
    }
    addProductIdsToMap(productIds);
}

QStandardItem* DataCatalogWidget::schemaItem(
    const QString& schemaName,
    QStandardItem* databaseItem)
{
    for (int row = 0; row < databaseItem->rowCount(); ++row)
    {
        QStandardItem* item = databaseItem->child(row);
        if (item && item->data(DatabaseSchemaRole).toString() == schemaName)
        {
            return item;
        }
    }

    QStandardItem* item = new QStandardItem(
        catalogIcon(QStringLiteral("folder")),
        schemaName);
    item->setEditable(false);
    item->setData(schemaName, DatabaseSchemaRole);
    item->setData(databaseItem->data(DatabaseNameRole), DatabaseNameRole);
    const QString schemaAlias = m_databaseDisplayAliases.value(
        databaseDisplayAliasKey(item));
    if (!schemaAlias.isEmpty())
    {
        item->setText(schemaAlias);
    }
    databaseItem->appendRow(item);
    return item;
}

void DataCatalogWidget::activateDatabaseItem(const QModelIndex& index)
{
    if (!index.isValid() || !m_databaseModel || !m_loadCallback)
    {
        return;
    }

    if (!gCurrentUserSession.isLoggedIn)
    {
        showDatabaseMessage(QStringLiteral("请先登录后再连接数据库。"), false);
        return;
    }

    const QStandardItem* item = m_databaseModel->itemFromIndex(index);
    const QgsMimeDataUtils::Uri uri = databaseUri(item);
    if (!isSupportedCatalogUri(uri))
    {
        return;
    }

    m_loadCallback(uri);
}

QgsMimeDataUtils::Uri DataCatalogWidget::databaseUri(
    const QStandardItem* item) const
{
    QgsMimeDataUtils::Uri uri;
    if (!item || !gCurrentUserSession.isLoggedIn)
    {
        return uri;
    }

    const QString layerType = item->data(DatabaseLayerTypeRole).toString();
    if (isProductCatalogProduct(item))
    {
        const QString productType = item->data(ProductTypeRole).toString();
        const bool isVector = isProductVectorType(productType);
        const bool isRaster = isProductRasterType(productType);
        if (!isVector && !isRaster)
        {
            return uri;
        }

        QSqlDatabase database = catalogDatabase(m_databaseConnectionName);
        if (!database.isValid() || !database.isOpen())
        {
            return uri;
        }

        QgsDataSourceUri sourceUri;
        sourceUri.setConnection(
            database.hostName(),
            QString::number(database.port()),
            database.databaseName(),
            database.userName(),
            database.password());
        sourceUri.setDataSource(
            item->data(DatabaseSchemaRole).toString().isEmpty()
                ? QStringLiteral("public")
                : item->data(DatabaseSchemaRole).toString(),
            item->data(ProductLayerTableRole).toString(),
            item->data(DatabaseGeometryColumnRole).toString().isEmpty()
                ? (isRaster ? QStringLiteral("rast") : QStringLiteral("geom"))
                : item->data(DatabaseGeometryColumnRole).toString());

        uri.layerType = isRaster ? QStringLiteral("raster") : QStringLiteral("vector");
        uri.providerKey = isRaster
            ? QStringLiteral("YOUR_PASSWORDraster")
            : QStringLiteral("YOUR_PASSWORD");
        uri.name = item->text();
        uri.uri = sourceUri.uri(false);
        return uri;
    }

    if (item->data(DatabaseTableRole).toString().isEmpty())
    {
        return uri;
    }
    if (item->data(DatabaseLayerTypeRole).toString() == QStringLiteral("table"))
    {
        return uri;
    }
    if (layerType != QStringLiteral("vector") &&
        layerType != QStringLiteral("raster"))
    {
        return uri;
    }

    QSqlDatabase database = catalogDatabase(m_databaseConnectionName);
    if (!database.isValid() || !database.isOpen())
    {
        return uri;
    }

    QgsDataSourceUri sourceUri;
    sourceUri.setConnection(
        database.hostName(),
        QString::number(database.port()),
        database.databaseName(),
        database.userName(),
        database.password());
    sourceUri.setDataSource(
        item->data(DatabaseSchemaRole).toString(),
        item->data(DatabaseTableRole).toString(),
        item->data(DatabaseGeometryColumnRole).toString());

    uri.layerType = layerType == QStringLiteral("raster")
        ? QStringLiteral("raster")
        : QStringLiteral("vector");
    uri.providerKey = uri.layerType == QStringLiteral("raster")
        ? QStringLiteral("YOUR_PASSWORDraster")
        : QStringLiteral("YOUR_PASSWORD");
    uri.name = item->data(DatabaseTableRole).toString();
    uri.uri = sourceUri.uri(false);
    return uri;
}

void DataCatalogWidget::activateLocalItem(const QModelIndex& index)
{
    if (!index.isValid() || !m_browserModel || !m_loadCallback)
    {
        return;
    }

    const QModelIndex sourceIndex = m_localProxyModel
        ? m_localProxyModel->mapToSource(index)
        : index;
    QgsDataItem* item = m_browserModel->dataItem(sourceIndex);
    if (!item)
    {
        return;
    }

    const QgsMimeDataUtils::UriList uris = localUris(item);
    for (const QgsMimeDataUtils::Uri& uri : uris)
    {
        if (isSupportedCatalogUri(uri))
        {
            m_loadCallback(uri);
        }
    }
}

QgsMimeDataUtils::UriList DataCatalogWidget::localUris(
    const QgsDataItem* item) const
{
    QgsMimeDataUtils::UriList result;
    if (!item)
    {
        return result;
    }

    const QgsMimeDataUtils::UriList uris = item->mimeUris();
    for (const QgsMimeDataUtils::Uri& uri : uris)
    {
        if (isSupportedCatalogUri(uri))
        {
            result.append(uri);
        }
    }
    return result;
}

DataCatalogWidget::CatalogContext DataCatalogWidget::databaseContextAt(
    const QPoint& position) const
{
    CatalogContext context;
    context.source = CatalogSource::Database;
    if (!m_databaseTree || !m_databaseModel)
    {
        return context;
    }

    context.viewIndex = m_databaseTree->indexAt(position);
    context.sourceIndex = context.viewIndex;
    if (context.viewIndex.isValid())
    {
        context.databaseItem = m_databaseModel->itemFromIndex(context.viewIndex);
    }
    return context;
}

DataCatalogWidget::CatalogContext DataCatalogWidget::localContextAt(
    const QPoint& position) const
{
    CatalogContext context;
    context.source = CatalogSource::Local;
    if (!m_localTree || !m_browserModel)
    {
        return context;
    }

    context.viewIndex = m_localTree->indexAt(position);
    context.sourceIndex = m_localProxyModel
        ? m_localProxyModel->mapToSource(context.viewIndex)
        : context.viewIndex;
    if (context.sourceIndex.isValid())
    {
        context.localItem = m_browserModel->dataItem(context.sourceIndex);
    }
    return context;
}

QString DataCatalogWidget::databaseCheckKey(const QStandardItem* item) const
{
    if (!item)
    {
        return QString();
    }

    const QString databaseName = item->data(DatabaseNameRole).toString();
    const int productNodeKind = item->data(ProductNodeKindRole).toInt();
    if (!databaseName.isEmpty() &&
        (productNodeKind == static_cast<int>(ProductCatalogNodeKind::Directory) ||
         productNodeKind == static_cast<int>(ProductCatalogNodeKind::Product)))
    {
        if (productNodeKind == static_cast<int>(ProductCatalogNodeKind::Product))
        {
            const int id = productId(item);
            return id > 0
                ? QStringLiteral("product-check:%1\\x1f%2").arg(databaseName).arg(id)
                : QString();
        }
        const int id = productDirectoryId(item);
        return id >= 0
            ? QStringLiteral("product-directory-check:%1\\x1f%2")
                .arg(databaseName)
                .arg(id)
            : QString();
    }

    const QString schemaName = item->data(DatabaseSchemaRole).toString();
    const QString tableName = item->data(DatabaseTableRole).toString();
    const QString layerType = item->data(DatabaseLayerTypeRole).toString();
    if (databaseName.isEmpty() || schemaName.isEmpty() ||
        tableName.isEmpty() ||
        (layerType != QStringLiteral("vector") &&
         layerType != QStringLiteral("raster")))
    {
        return QString();
    }

    return QStringLiteral("catalog-check:%1\x1f%2\x1f%3\x1f%4\x1f%5")
        .arg(databaseName)
        .arg(schemaName)
        .arg(tableName)
        .arg(layerType)
        .arg(item->data(DatabaseGeometryColumnRole).toString());
}

bool DataCatalogWidget::isCheckableContext(
    const CatalogContext& context) const
{
    if (context.source == CatalogSource::Database)
    {
        return !databaseCheckKey(context.databaseItem).isEmpty();
    }
    return context.localItem && !localUris(context.localItem).isEmpty();
}

bool DataCatalogWidget::isCheckedContext(
    const CatalogContext& context) const
{
    if (!isCheckableContext(context))
    {
        return false;
    }

    if (context.source == CatalogSource::Database)
    {
        return context.databaseItem->checkState() == Qt::Checked;
    }

    return context.localItem &&
           m_checkedLocalPaths.contains(context.localItem->path());
}

void DataCatalogWidget::collectCheckedDatabaseContexts(
    const QStandardItem* item,
    QVector<CatalogContext>* contexts) const
{
    if (!item || !contexts || !m_databaseModel)
    {
        return;
    }

    if (isCheckableContext(CatalogContext{
            CatalogSource::Database,
            m_databaseModel->indexFromItem(item),
            m_databaseModel->indexFromItem(item),
            const_cast<QStandardItem*>(item),
            nullptr}) &&
        item->checkState() == Qt::Checked)
    {
        CatalogContext context;
        context.source = CatalogSource::Database;
        context.viewIndex = m_databaseModel->indexFromItem(item);
        context.sourceIndex = context.viewIndex;
        context.databaseItem = const_cast<QStandardItem*>(item);
        contexts->append(context);
    }

    for (int row = 0; row < item->rowCount(); ++row)
    {
        collectCheckedDatabaseContexts(item->child(row), contexts);
    }
}

QVector<DataCatalogWidget::CatalogContext>
DataCatalogWidget::checkedDatabaseContexts() const
{
    QVector<CatalogContext> contexts;
    if (!m_databaseModel)
    {
        return contexts;
    }

    for (int row = 0; row < m_databaseModel->rowCount(); ++row)
    {
        collectCheckedDatabaseContexts(m_databaseModel->item(row), &contexts);
    }
    return contexts;
}

QVector<DataCatalogWidget::CatalogContext>
DataCatalogWidget::checkedLocalContexts() const
{
    QVector<CatalogContext> contexts;
    if (!m_browserModel || !m_localProxyModel || !m_localTree)
    {
        return contexts;
    }

    collectCheckedLocalContexts(m_localTree->rootIndex(), &contexts);
    return contexts;
}

void DataCatalogWidget::collectCheckedLocalContexts(
    const QModelIndex& parent,
    QVector<CatalogContext>* contexts) const
{
    if (!contexts || !m_localProxyModel || !m_browserModel)
    {
        return;
    }

    for (const QModelIndex& viewIndex :
         collectCheckedCatalogIndexes(m_localProxyModel, parent))
    {
        const QModelIndex sourceIndex = m_localProxyModel->mapToSource(viewIndex);
        QgsDataItem* item = m_localProxyModel->dataItem(viewIndex);
        if (item && !localUris(item).isEmpty())
        {
            CatalogContext context;
            context.source = CatalogSource::Local;
            context.viewIndex = viewIndex;
            context.sourceIndex = sourceIndex;
            context.localItem = item;
            contexts->append(context);
        }
    }
}

QVector<DataCatalogWidget::CatalogContext>
DataCatalogWidget::checkedContextsFor(
    const CatalogContext& context) const
{
    const QVector<CatalogContext> contexts = context.source == CatalogSource::Database
        ? checkedDatabaseContexts()
        : checkedLocalContexts();
    if (!contexts.isEmpty())
    {
        return contexts;
    }
    return QVector<CatalogContext>() << context;
}

bool DataCatalogWidget::isProductMenuContext(
    const CatalogContext& context) const
{
    if (context.source == CatalogSource::Database)
    {
        return isProductCatalogDirectory(context.databaseItem) ||
               isProductCatalogProduct(context.databaseItem);
    }

    return isCatalogCollection(context) || isCatalogLayer(context);
}

bool DataCatalogWidget::isProductMenuDirectory(
    const CatalogContext& context) const
{
    return context.source == CatalogSource::Database
        ? isProductCatalogDirectory(context.databaseItem)
        : isCatalogCollection(context) || isCatalogLayer(context);
}

bool DataCatalogWidget::isProductMenuRoot(
    const CatalogContext& context) const
{
    if (context.source == CatalogSource::Database)
    {
        return isProductCatalogRoot(context.databaseItem);
    }

    return context.localItem &&
           context.localItem->type() == Qgis::BrowserItemType::Favorites;
}

bool DataCatalogWidget::isDeletableDataContext(
    const CatalogContext& context) const
{
    if (context.source == CatalogSource::Database)
    {
        if (isProductCatalogProduct(context.databaseItem))
        {
            return productId(context.databaseItem) > 0;
        }
        if (isProductCatalogDirectory(context.databaseItem))
        {
            return context.databaseItem->data(ProductDirectoryNodeTypeRole).toInt() == 1 &&
                !allProductIdsUnderItem(context.databaseItem).isEmpty();
        }
        return isCatalogLayer(context);
    }

    return isCatalogLayer(context) &&
        !localDataPathsForContext(context).isEmpty();
}

QVector<DataCatalogWidget::CatalogContext>
DataCatalogWidget::checkedLocalContextsUnder(
    const CatalogContext& context) const
{
    QVector<CatalogContext> contexts;
    if (context.source != CatalogSource::Local ||
        !context.localItem ||
        !m_localProxyModel ||
        !context.viewIndex.isValid())
    {
        return contexts;
    }

    if (isCheckedContext(context))
    {
        contexts.append(context);
    }
    collectCheckedLocalContexts(context.viewIndex, &contexts);
    return contexts;
}

QgsMimeDataUtils::UriList DataCatalogWidget::localProductUrisForContext(
    const CatalogContext& context,
    bool checkedOnly) const
{
    QgsMimeDataUtils::UriList result;
    if (context.source != CatalogSource::Local || !context.localItem)
    {
        return result;
    }

    if (isCatalogLayer(context))
    {
        for (const QgsMimeDataUtils::Uri& uri : localUris(context.localItem))
        {
            appendUniqueUri(&result, uri);
        }
        return result;
    }

    if (checkedOnly)
    {
        const QVector<CatalogContext> contexts = checkedLocalContextsUnder(context);
        for (const CatalogContext& checkedContext : contexts)
        {
            for (const QgsMimeDataUtils::Uri& uri : localUris(checkedContext.localItem))
            {
                appendUniqueUri(&result, uri);
            }
        }
        return result;
    }

    int unloadedCount = 0;
    return collectLocalUris(context.localItem, &unloadedCount);
}

void DataCatalogWidget::setLocalProductSubtreeCheck(
    const CatalogContext& context,
    Qt::CheckState state)
{
    if (context.source != CatalogSource::Local ||
        !m_localProxyModel ||
        !context.viewIndex.isValid())
    {
        return;
    }

    std::function<void(const QModelIndex&)> setCheckState =
        [this, state, &setCheckState](const QModelIndex& index)
    {
        if (!index.isValid())
        {
            return;
        }

        if (m_localProxyModel->flags(index) & Qt::ItemIsUserCheckable)
        {
            m_localProxyModel->setData(index, state, Qt::CheckStateRole);
        }

        for (int row = 0; row < m_localProxyModel->rowCount(index); ++row)
        {
            setCheckState(m_localProxyModel->index(row, 0, index));
        }
    };
    setCheckState(context.viewIndex);
}

void DataCatalogWidget::showDatabaseContextMenu(const QPoint& position)
{
    const CatalogContext context = databaseContextAt(position);
    if (!context.databaseItem || !m_databaseTree)
    {
        return;
    }

    m_databaseTree->setCurrentIndex(context.viewIndex);
    QMenu menu(m_databaseTree);
    populateContextMenu(menu, context);
    if (!menu.isEmpty())
    {
        menu.exec(m_databaseTree->viewport()->mapToGlobal(position));
    }
}

void DataCatalogWidget::showLocalContextMenu(const QPoint& position)
{
    const CatalogContext context = localContextAt(position);
    if (!context.localItem || !m_localTree ||
        !isProductMenuContext(context))
    {
        return;
    }

    m_localTree->setCurrentIndex(context.viewIndex);
    QMenu menu(m_localTree);
    populateContextMenu(menu, context);
    if (!menu.isEmpty())
    {
        menu.exec(m_localTree->viewport()->mapToGlobal(position));
    }
}

void DataCatalogWidget::populateProductContextMenu(
    QMenu& menu,
    const CatalogContext& context)
{
    const bool database = context.source == CatalogSource::Database;
    const bool directory = isProductMenuDirectory(context);
    const bool root = directory && isProductMenuRoot(context);
    const QList<int> selectedProductIds = database
        ? productIdsForContext(context)
        : QList<int>();
    const QList<int> allProductIds = database
        ? (directory ? allProductIdsUnderItem(context.databaseItem)
                    : selectedProductIds)
        : QList<int>();
    const QgsMimeDataUtils::UriList selectedLocalUris = database
        ? QgsMimeDataUtils::UriList()
        : localProductUrisForContext(context, true);
    const QgsMimeDataUtils::UriList allLocalUris = database
        ? QgsMimeDataUtils::UriList()
        : localProductUrisForContext(context, false);
    const QVector<CatalogContext> checkedMapContexts =
        context.source == CatalogSource::Database
            ? checkedDatabaseContexts()
            : checkedLocalContexts();
    const bool hasCheckedMapContexts = !checkedMapContexts.isEmpty();
    const QVector<CatalogContext> mapContexts = hasCheckedMapContexts
        ? checkedMapContexts
        : QVector<CatalogContext>() << context;
    QList<int> mapProductIds;
    QgsMimeDataUtils::UriList mapLocalUris;
    if (hasCheckedMapContexts)
    {
        for (const CatalogContext& mapContext : mapContexts)
        {
            if (database)
            {
                const QList<int> mapContextProductIds =
                    productIdsForContext(mapContext);
                for (const int id : mapContextProductIds)
                {
                    if (id > 0 && !mapProductIds.contains(id))
                    {
                        mapProductIds.append(id);
                    }
                }
            }
            else
            {
                for (const QgsMimeDataUtils::Uri& uri :
                     localProductUrisForContext(mapContext, true))
                {
                    appendUniqueUri(&mapLocalUris, uri);
                }
            }
        }
    }
    else if (database)
    {
        mapProductIds = directory ? allProductIds : selectedProductIds;
    }
    else
    {
        mapLocalUris = allLocalUris;
    }
    const bool hasSelectedData = database
        ? !selectedProductIds.isEmpty()
        : !selectedLocalUris.isEmpty();
    const bool hasAllData = database
        ? !allProductIds.isEmpty()
        : !allLocalUris.isEmpty();
    const bool hasMapData = database
        ? !mapProductIds.isEmpty()
        : !mapLocalUris.isEmpty();

    if (directory)
    {
        QAction* createDirectory = menu.addAction(QStringLiteral("新建目录"));
        QAction* deleteDirectory = menu.addAction(QStringLiteral("删除目录"));
        QAction* renameDirectory = menu.addAction(QStringLiteral("重命名"));
        QAction* deleteData = nullptr;
        if (isDeletableDataContext(context))
        {
            deleteData = menu.addAction(QStringLiteral("删除数据"));
        }
        menu.addSeparator();
        QAction* importData = menu.addAction(QStringLiteral("导入数据"));
        QAction* exportData = menu.addAction(QStringLiteral("导出所选数据"));
        QAction* rangeExport = menu.addAction(QStringLiteral("按范围导出"));
        QAction* mainAreaExport = menu.addAction(QStringLiteral("按制图导出"));
        menu.addSeparator();
        QAction* addToMap = menu.addAction(QStringLiteral("添加到地图"));
        QAction* addToTheme = menu.addAction(QStringLiteral("添加到主题"));

        bool canRenameLocalDirectory = false;
        if (!database && context.localItem && m_browserModel)
        {
            const QModelIndex favoritesIndex = favoritesRootIndex();
            QgsDataItem* favoritesItem = m_browserModel->dataItem(favoritesIndex);
            canRenameLocalDirectory = favoritesItem &&
                context.localItem->parent() == favoritesItem &&
                qobject_cast<QgsDirectoryItem*>(context.localItem);
        }

        createDirectory->setEnabled(database);
        deleteDirectory->setEnabled(database && !root);
        renameDirectory->setEnabled(
            database
                ? (!root || isDeletableDataContext(context))
                : (canRenameLocalDirectory || isCatalogLayer(context)));
        importData->setEnabled(
            database
                ? static_cast<bool>(m_dataImportCallback)
                : canImportLocalDirectory(context));
        exportData->setEnabled(
            hasSelectedData &&
            (database ? static_cast<bool>(m_productOperationCallback) : true));
        const bool hasProductOperation = database
            ? static_cast<bool>(m_productOperationCallback)
            : static_cast<bool>(m_localProductOperationCallback);
        rangeExport->setEnabled(hasSelectedData && hasProductOperation);
        mainAreaExport->setEnabled(hasSelectedData && hasProductOperation);
        addToMap->setEnabled(
            hasMapData && static_cast<bool>(m_loadCallback));
        addToTheme->setEnabled(hasAllData && hasProductOperation);

        connect(createDirectory, &QAction::triggered, this,
            [this, context]() { createProductDirectory(context); });
        connect(deleteDirectory, &QAction::triggered, this,
            [this, context]() { deleteProductDirectory(context); });
        connect(renameDirectory, &QAction::triggered, this,
            [this, context]()
            {
                if (context.source == CatalogSource::Local &&
                    isCatalogLayer(context))
                {
                    renameContextItem(context);
                    return;
                }
                dispatchProductOperation(
                    QStringLiteral("rename-directory"),
                    context);
            });
        if (deleteData)
        {
            connect(deleteData, &QAction::triggered, this,
                [this, context]() { removeData(context); });
        }
        connect(importData, &QAction::triggered, this,
            [this, context, database]()
            {
                if (!database)
                {
                    importContextItem(context);
                    return;
                }
                if (m_dataImportCallback)
                {
                    const int targetDirectoryId =
                        qMax(productDirectoryId(context.databaseItem), 0);
                    importProductData(targetDirectoryId);
                }
            });
        connect(exportData, &QAction::triggered, this,
            [this, context, database, selectedProductIds, selectedLocalUris]()
            {
                if (database)
                {
                    dispatchProductOperation(
                        QStringLiteral("export"),
                        context,
                        selectedProductIds);
                }
                else
                {
                    dispatchProductOperation(
                        QStringLiteral("export"),
                        context,
                        QList<int>(),
                        selectedLocalUris);
                }
            });
        connect(rangeExport, &QAction::triggered, this,
            [this, context, selectedProductIds, selectedLocalUris]()
            {
                dispatchProductOperation(
                    QStringLiteral("range"),
                    context,
                    selectedProductIds,
                    selectedLocalUris);
            });
        connect(mainAreaExport, &QAction::triggered, this,
            [this, context, selectedProductIds, selectedLocalUris]()
            {
                dispatchProductOperation(
                    QStringLiteral("main-area"),
                    context,
                    selectedProductIds,
                    selectedLocalUris);
            });
        connect(addToMap, &QAction::triggered, this,
            [this, context, mapProductIds, mapLocalUris]()
            {
                dispatchProductOperation(
                    QStringLiteral("map"),
                    context,
                    mapProductIds,
                    mapLocalUris);
            });
        connect(addToTheme, &QAction::triggered, this,
            [this, context, database, selectedProductIds, allProductIds,
             selectedLocalUris, allLocalUris]()
            {
                dispatchProductOperation(
                    QStringLiteral("theme"),
                    context,
                    database
                        ? (selectedProductIds.isEmpty()
                            ? allProductIds
                            : selectedProductIds)
                        : QList<int>(),
                    database
                        ? QgsMimeDataUtils::UriList()
                        : (selectedLocalUris.isEmpty()
                            ? allLocalUris
                            : selectedLocalUris));
            });
        return;
    }

    QAction* rename = menu.addAction(QStringLiteral("重命名"));
    QAction* deleteData = menu.addAction(QStringLiteral("删除数据"));
    menu.addSeparator();
    QAction* exportData = menu.addAction(QStringLiteral("导出所选数据"));
    QAction* rangeExport = menu.addAction(QStringLiteral("按范围导出"));
    QAction* mainAreaExport = menu.addAction(QStringLiteral("按制图导出"));
    menu.addSeparator();
    QAction* addToMap = menu.addAction(QStringLiteral("添加到地图"));
    QAction* addToTheme = menu.addAction(QStringLiteral("添加到主题"));

    const bool canOperate = hasSelectedData &&
        (database ? static_cast<bool>(m_productOperationCallback) : true);
    const bool canProductOperation = hasSelectedData &&
        (database
            ? static_cast<bool>(m_productOperationCallback)
            : static_cast<bool>(m_localProductOperationCallback));
    exportData->setEnabled(canOperate);
    rangeExport->setEnabled(canProductOperation);
    mainAreaExport->setEnabled(canProductOperation);
    addToMap->setEnabled(
        hasMapData && static_cast<bool>(m_loadCallback));
    addToTheme->setEnabled(canProductOperation);
    rename->setEnabled(isDeletableDataContext(context));
    deleteData->setEnabled(isDeletableDataContext(context));

    connect(rename, &QAction::triggered, this,
        [this, context]() { renameContextItem(context); });
    connect(deleteData, &QAction::triggered, this,
        [this, context]() { removeData(context); });

    connect(exportData, &QAction::triggered, this,
        [this, context, database, selectedProductIds, selectedLocalUris]()
        {
            if (database)
            {
                dispatchProductOperation(
                    QStringLiteral("export"),
                    context,
                    selectedProductIds);
            }
            else
            {
                dispatchProductOperation(
                    QStringLiteral("export"),
                    context,
                    QList<int>(),
                    selectedLocalUris);
            }
        });
    connect(rangeExport, &QAction::triggered, this,
        [this, context, selectedProductIds, selectedLocalUris]()
        {
            dispatchProductOperation(
                QStringLiteral("range"),
                context,
                selectedProductIds,
                selectedLocalUris);
        });
    connect(mainAreaExport, &QAction::triggered, this,
        [this, context, selectedProductIds, selectedLocalUris]()
        {
            dispatchProductOperation(
                QStringLiteral("main-area"),
                context,
                selectedProductIds,
                selectedLocalUris);
        });
    connect(addToMap, &QAction::triggered, this,
        [this, context, mapProductIds, mapLocalUris]()
        {
            dispatchProductOperation(
                QStringLiteral("map"),
                context,
                mapProductIds,
                mapLocalUris);
        });
    connect(addToTheme, &QAction::triggered, this,
        [this, context, selectedProductIds, selectedLocalUris]()
        {
            dispatchProductOperation(
                QStringLiteral("theme"),
                context,
                selectedProductIds,
                selectedLocalUris);
        });
}

void DataCatalogWidget::populateContextMenu(
    QMenu& menu,
    const CatalogContext& context)
{
    if (isProductMenuContext(context))
    {
        populateProductContextMenu(menu, context);
        return;
    }

    QAction* addToMap = menu.addAction(QStringLiteral("添加到地图"));
    QAction* refresh = menu.addAction(QStringLiteral("刷新"));
    QAction* importData = menu.addAction(QStringLiteral("导入数据"));
    QAction* dataClip = menu.addAction(QStringLiteral("数据裁剪"));
    QAction* info = menu.addAction(QStringLiteral("基本信息"));
    QAction* exportData = menu.addAction(QStringLiteral("导出数据"));
    QAction* batchExport = menu.addAction(QStringLiteral("批量导出数据"));
    QAction* rename = menu.addAction(QStringLiteral("重命名"));
    QAction* deleteData = nullptr;
    if (isDeletableDataContext(context))
    {
        deleteData = menu.addAction(QStringLiteral("删除数据"));
    }

    const QVector<CatalogContext> operationContexts = checkedContextsFor(context);
    const bool layer = isCatalogLayer(context);
    const bool collection = isCatalogCollection(context);
    bool hasLayerTarget = false;
    bool hasRefreshTarget = false;
    for (const CatalogContext& operationContext : operationContexts)
    {
        if (isCatalogLayer(operationContext))
        {
            hasLayerTarget = true;
            hasRefreshTarget = true;
        }
        else if (isCatalogCollection(operationContext))
        {
            hasRefreshTarget = true;
        }
    }
    bool canRename = false;
    bool canInfo = false;
    if (context.source == CatalogSource::Database)
    {
        const QStandardItem* item = context.databaseItem;
        if (item)
        {
            const QString layerType = item->data(DatabaseLayerTypeRole).toString();
            const bool hasDatabaseIdentity = item->data(DatabaseNameRole).isValid();
            const bool hasSchema = !item->data(DatabaseSchemaRole).toString().isEmpty();
            const bool hasTable = !item->data(DatabaseTableRole).toString().isEmpty();
            canInfo = hasDatabaseIdentity || hasSchema || hasTable;
            canRename = (hasDatabaseIdentity || hasSchema || hasTable) &&
                        layerType != QStringLiteral("table");
        }
    }
    else if (context.localItem)
    {
        canInfo = context.localItem->type() != Qgis::BrowserItemType::Error;
        if (isCatalogLayer(context))
        {
            canRename = !localRemovalKey(context.localItem).isEmpty();
        }
        else
        {
            const QModelIndex favoritesIndex = favoritesRootIndex();
            QgsDataItem* favoritesItem = m_browserModel
                ? m_browserModel->dataItem(favoritesIndex)
                : nullptr;
            canRename = favoritesItem &&
                        context.localItem->parent() == favoritesItem &&
                        qobject_cast<QgsDirectoryItem*>(context.localItem);
        }
    }

    addToMap->setEnabled(hasLayerTarget && static_cast<bool>(m_loadCallback));
    refresh->setEnabled(hasRefreshTarget);
    const bool databaseImportTarget = context.source == CatalogSource::Database &&
        context.databaseItem &&
        context.databaseItem->data(DatabaseNameRole).isValid();
    importData->setEnabled(
        (databaseImportTarget && static_cast<bool>(m_dataImportCallback)) ||
        canImportLocalDirectory(context));
    dataClip->setEnabled(layer && static_cast<bool>(m_dataClipCallback));
    info->setEnabled(canInfo);
    exportData->setEnabled(layer);
    batchExport->setEnabled(collection || hasLayerTarget);
    rename->setEnabled(canRename);

    connect(addToMap, &QAction::triggered, this,
        [this, operationContexts]() { addContextItemsToMap(operationContexts); });
    connect(refresh, &QAction::triggered, this,
        [this, operationContexts]() { refreshContextItems(operationContexts); });
    connect(importData, &QAction::triggered, this,
        [this, context]() { importContextItem(context); });
    connect(dataClip, &QAction::triggered, this,
        [this]() {
            if (m_dataClipCallback)
            {
                m_dataClipCallback();
            }
        });
    connect(info, &QAction::triggered, this,
        [this, context]() { showContextItemInfo(context); });
    connect(exportData, &QAction::triggered, this,
        [this, context]() { exportContextItem(context); });
    connect(batchExport, &QAction::triggered, this,
        [this, context, operationContexts]()
        {
            if (isCatalogCollection(context))
            {
                batchExportContextItem(context);
            }
            else
            {
                batchExportContextItems(operationContexts);
            }
        });
    connect(rename, &QAction::triggered, this,
        [this, context]() { renameContextItem(context); });
    if (deleteData)
    {
        connect(deleteData, &QAction::triggered, this,
            [this, context]() { removeData(context); });
    }
}

bool DataCatalogWidget::isCatalogLayer(const CatalogContext& context) const
{
    if (context.source == CatalogSource::Database)
    {
        if (isProductCatalogDirectory(context.databaseItem))
        {
            return false;
        }
        return isSupportedCatalogUri(databaseUri(context.databaseItem));
    }

    return context.localItem &&
           qobject_cast<QgsLayerItem*>(context.localItem) &&
           !localUris(context.localItem).isEmpty();
}

bool DataCatalogWidget::isCatalogCollection(const CatalogContext& context) const
{
    if (context.source == CatalogSource::Database)
    {
        const QStandardItem* item = context.databaseItem;
        if (!item || !item->data(DatabaseNameRole).isValid())
        {
            return false;
        }
        if (isProductCatalogDirectory(item))
        {
            return true;
        }
        return item->data(DatabaseTableRole).toString().isEmpty();
    }

    if (!context.localItem)
    {
        return false;
    }
    const Qgis::BrowserItemType type = context.localItem->type();
    return type == Qgis::BrowserItemType::Collection ||
           type == Qgis::BrowserItemType::Directory ||
           type == Qgis::BrowserItemType::Favorites;
}

void DataCatalogWidget::addContextItemToMap(const CatalogContext& context)
{
    if (!m_loadCallback || !isCatalogLayer(context))
    {
        return;
    }

    if (context.source == CatalogSource::Database)
    {
        const QgsMimeDataUtils::Uri uri = databaseUri(context.databaseItem);
        if (isSupportedCatalogUri(uri))
        {
            m_loadCallback(uri);
        }
        return;
    }

    for (const QgsMimeDataUtils::Uri& uri : localUris(context.localItem))
    {
        if (isSupportedCatalogUri(uri))
        {
            m_loadCallback(uri);
        }
    }
}

void DataCatalogWidget::addContextItemsToMap(
    const QVector<CatalogContext>& contexts)
{
    if (!m_loadCallback)
    {
        return;
    }

    QgsMimeDataUtils::UriList uris;
    for (const CatalogContext& context : contexts)
    {
        if (context.source == CatalogSource::Database)
        {
            appendUniqueUri(&uris, databaseUri(context.databaseItem));
            continue;
        }

        for (const QgsMimeDataUtils::Uri& uri : localUris(context.localItem))
        {
            appendUniqueUri(&uris, uri);
        }
    }

    for (const QgsMimeDataUtils::Uri& uri : uris)
    {
        m_loadCallback(uri);
    }
}

void DataCatalogWidget::refreshContextItem(const CatalogContext& context)
{
    if (context.source == CatalogSource::Database)
    {
        if (isCatalogLayer(context) || isCatalogCollection(context))
        {
            refreshDatabaseData();
        }
        return;
    }

    if (!m_browserModel || !context.sourceIndex.isValid() ||
        (!isCatalogLayer(context) && !isCatalogCollection(context)))
    {
        return;
    }

    if (m_localStatusLabel)
    {
        m_localStatusLabel->setText(QStringLiteral("正在刷新本地数据目录…"));
    }
    m_browserModel->refresh(context.sourceIndex);
    QTimer::singleShot(0, this, &DataCatalogWidget::showFavoritesInTree);
}

void DataCatalogWidget::refreshContextItems(
    const QVector<CatalogContext>& contexts)
{
    if (contexts.isEmpty())
    {
        return;
    }

    if (contexts.first().source == CatalogSource::Database)
    {
        for (const CatalogContext& context : contexts)
        {
            if (isCatalogLayer(context) || isCatalogCollection(context))
            {
                refreshDatabaseData();
                return;
            }
        }
        return;
    }

    if (!m_browserModel)
    {
        return;
    }

    bool refreshed = false;
    for (const CatalogContext& context : contexts)
    {
        if (!context.sourceIndex.isValid() ||
            (!isCatalogLayer(context) && !isCatalogCollection(context)))
        {
            continue;
        }

        if (m_localStatusLabel)
        {
            m_localStatusLabel->setText(QStringLiteral("正在刷新本地数据目录…"));
        }
        m_browserModel->refresh(context.sourceIndex);
        refreshed = true;
    }

    if (refreshed)
    {
        QTimer::singleShot(0, this, &DataCatalogWidget::showFavoritesInTree);
    }
}

QString DataCatalogWidget::displayNameForItem(
    const CatalogContext& context) const
{
    if (context.source == CatalogSource::Database)
    {
        return context.databaseItem ? context.databaseItem->text() : QString();
    }
    if (!context.localItem)
    {
        return QString();
    }
    const QString key = localRemovalKey(context.localItem);
    if (!key.isEmpty())
    {
        const auto alias = m_localDisplayAliases.constFind(key);
        if (alias != m_localDisplayAliases.constEnd())
        {
            return alias.value();
        }
    }
    return context.localItem->name();
}

void DataCatalogWidget::showContextItemInfo(const CatalogContext& context)
{
    QStringList lines;
    lines.append(QStringLiteral("显示名称：%1").arg(displayNameForItem(context)));

    if (context.source == CatalogSource::Database)
    {
        const QStandardItem* item = context.databaseItem;
        if (!item)
        {
            return;
        }

        if (isProductCatalogProduct(item))
        {
            lines.append(QStringLiteral("来源：数据库成果目录"));
            lines.append(QStringLiteral("成果 ID：%1").arg(productId(item)));
            lines.append(QStringLiteral("成果类型：%1")
                .arg(item->data(ProductTypeRole).toString()));
            lines.append(QStringLiteral("文件格式：%1")
                .arg(item->data(ProductFileFormatRole).toString().toUpper()));
            lines.append(QStringLiteral("图层表：%1")
                .arg(item->data(ProductLayerTableRole).toString()));
            lines.append(QStringLiteral("几何类型：%1")
                .arg(item->data(ProductGeometryTypeRole).toString()));
            lines.append(QStringLiteral("密级：%1")
                .arg(item->data(ProductSecurityLevelRole).toString()));
            QMessageBox::information(
                this,
                QStringLiteral("基本信息"),
                lines.join(QChar('\n')));
            return;
        }

        lines.append(QStringLiteral("来源：数据库"));
        const QSqlDatabase database = catalogDatabase(m_databaseConnectionName);
        lines.append(QStringLiteral("数据库：%1").arg(
            item->data(DatabaseNameRole).toString().isEmpty()
                ? database.databaseName()
                : item->data(DatabaseNameRole).toString()));
        lines.append(QStringLiteral("主机：%1:%2")
            .arg(database.hostName())
            .arg(database.port()));

        const QString schemaName = item->data(DatabaseSchemaRole).toString();
        const QString tableName = item->data(DatabaseTableRole).toString();
        if (!schemaName.isEmpty())
        {
            lines.append(QStringLiteral("模式：%1").arg(schemaName));
        }
        if (!tableName.isEmpty())
        {
            lines.append(QStringLiteral("表：%1").arg(tableName));
        }
        const QString geometryColumn = item->data(DatabaseGeometryColumnRole).toString();
        if (!geometryColumn.isEmpty())
        {
            lines.append(QStringLiteral("空间列：%1").arg(geometryColumn));
        }
        lines.append(QStringLiteral("类型：%1").arg(
            item->data(DatabaseLayerTypeRole).toString().isEmpty()
                ? QStringLiteral("目录")
                : item->data(DatabaseLayerTypeRole).toString()));

        const QgsMimeDataUtils::Uri uri = databaseUri(item);
        if (!uri.uri.isEmpty())
        {
            lines.append(QStringLiteral("数据源 URI：%1").arg(uri.uri));
        }
    }
    else
    {
        QgsDataItem* item = context.localItem;
        if (!item)
        {
            return;
        }

        lines.append(QStringLiteral("来源：本地数据"));
        lines.append(QStringLiteral("提供者：%1")
            .arg(item->providerKey().isEmpty()
                ? QStringLiteral("未知")
                : item->providerKey()));
        lines.append(QStringLiteral("路径：%1").arg(item->path()));

        if (QgsDirectoryItem* directory = qobject_cast<QgsDirectoryItem*>(item))
        {
            const QFileInfo fileInfo(directory->dirPath());
            lines.append(QStringLiteral("文件夹：%1").arg(directory->dirPath()));
            if (fileInfo.exists())
            {
                lines.append(QStringLiteral("最后修改：%1")
                    .arg(fileInfo.lastModified().toString(Qt::ISODate)));
            }
        }

        const QgsMimeDataUtils::UriList uris = localUris(item);
        if (!uris.isEmpty())
        {
            lines.append(QStringLiteral("图层 URI："));
            for (const QgsMimeDataUtils::Uri& uri : uris)
            {
                lines.append(QStringLiteral("  %1 / %2：%3")
                    .arg(uri.layerType, uri.providerKey, uri.filePath.isEmpty()
                        ? uri.uri
                        : uri.filePath));
            }
        }
    }

    QMessageBox::information(
        this,
        QStringLiteral("基本信息"),
        lines.join(QChar('\n')));
}

bool DataCatalogWidget::canImportLocalDirectory(
    const CatalogContext& context) const
{
    if (context.source != CatalogSource::Local || !context.localItem)
    {
        return false;
    }

    QgsDirectoryItem* targetDirectory = qobject_cast<QgsDirectoryItem*>(
        context.localItem);
    return targetDirectory && QFileInfo(targetDirectory->dirPath()).isDir();
}

QString DataCatalogWidget::localImportTargetPath(
    const CatalogContext& context) const
{
    if (!canImportLocalDirectory(context))
    {
        return QString();
    }

    QgsDirectoryItem* targetDirectory = qobject_cast<QgsDirectoryItem*>(
        context.localItem);
    return targetDirectory
        ? QFileInfo(targetDirectory->dirPath()).absoluteFilePath()
        : QString();
}

QString DataCatalogWidget::uniqueLocalImportPath(
    const QString& targetDirectory,
    const QString& fileName) const
{
    const QFileInfo sourceInfo(fileName);
    const QString safeFileName = sourceInfo.fileName();
    if (safeFileName.isEmpty())
    {
        return QString();
    }

    const QDir destinationDirectory(targetDirectory);
    QString candidate = destinationDirectory.filePath(safeFileName);
    if (!QFileInfo::exists(candidate))
    {
        return candidate;
    }

    QString stem = sourceInfo.completeBaseName();
    if (stem.isEmpty())
    {
        stem = safeFileName;
    }
    const QString suffix = sourceInfo.completeSuffix();
    int sequence = 2;
    while (QFileInfo::exists(candidate))
    {
        candidate = destinationDirectory.filePath(
            stem + QStringLiteral("_") + QString::number(sequence++));
        if (!suffix.isEmpty())
        {
            candidate += QStringLiteral(".") + suffix;
        }
    }
    return candidate;
}

QStringList DataCatalogWidget::localImportCompanionFiles(
    const QString& sourcePath) const
{
    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile())
    {
        return QStringList();
    }

    QStringList files;
    files.append(sourceInfo.absoluteFilePath());

    const QString suffix = sourceInfo.suffix().toLower();
    QStringList companionSuffixes;
    if (suffix == QStringLiteral("shp"))
    {
        companionSuffixes << QStringLiteral("shx")
                          << QStringLiteral("dbf")
                          << QStringLiteral("prj")
                          << QStringLiteral("qpj")
                          << QStringLiteral("cpg")
                          << QStringLiteral("sbn")
                          << QStringLiteral("sbx")
                          << QStringLiteral("qix")
                          << QStringLiteral("fix")
                          << QStringLiteral("shp.xml");
    }
    else if (suffix == QStringLiteral("tif") ||
             suffix == QStringLiteral("tiff") ||
             suffix == QStringLiteral("img") ||
             suffix == QStringLiteral("vrt"))
    {
        companionSuffixes << QStringLiteral("tfw")
                          << QStringLiteral("wld")
                          << QStringLiteral("ovr");
    }

    const QString sourceBasePath = QDir(sourceInfo.absolutePath()).filePath(
        sourceInfo.completeBaseName());
    const auto appendIfExisting = [&files](const QString& path)
    {
        if (QFileInfo::exists(path) && !files.contains(path))
        {
            files.append(path);
        }
    };
    for (const QString& companionSuffix : companionSuffixes)
    {
        appendIfExisting(sourceBasePath + QStringLiteral(".") + companionSuffix);
    }

    if (suffix == QStringLiteral("tif") ||
        suffix == QStringLiteral("tiff") ||
        suffix == QStringLiteral("img") ||
        suffix == QStringLiteral("vrt"))
    {
        appendIfExisting(sourceInfo.absoluteFilePath() + QStringLiteral(".aux.xml"));
        appendIfExisting(sourceBasePath + QStringLiteral(".aux.xml"));
    }

    return files;
}

bool DataCatalogWidget::copyLocalImportDataset(
    const QString& sourcePath,
    const QString& targetDirectory,
    QString* errorMessage) const
{
    if (errorMessage)
    {
        errorMessage->clear();
    }

    const QFileInfo sourceInfo(sourcePath);
    const QDir destinationDirectory(targetDirectory);
    if (!sourceInfo.exists() || !sourceInfo.isFile())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("源文件不存在：%1").arg(sourcePath);
        }
        return false;
    }
    if (!destinationDirectory.exists())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("目标文件夹不存在：%1")
                .arg(targetDirectory);
        }
        return false;
    }
    if (QDir::cleanPath(sourceInfo.absolutePath()).compare(
            QDir::cleanPath(destinationDirectory.absolutePath()),
            Qt::CaseInsensitive) == 0)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("源文件已经位于目标文件夹：%1")
                .arg(sourceInfo.absoluteFilePath());
        }
        return false;
    }

    const QStringList sourceFiles = localImportCompanionFiles(sourcePath);
    if (sourceFiles.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("无法识别源数据文件：%1")
                .arg(sourcePath);
        }
        return false;
    }

    const QString primaryTargetPath = uniqueLocalImportPath(
        targetDirectory,
        sourceInfo.fileName());
    if (primaryTargetPath.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("无法生成目标文件名：%1")
                .arg(sourceInfo.fileName());
        }
        return false;
    }

    const QString sourcePrimaryName = sourceInfo.fileName();
    const QString sourceStem = sourceInfo.completeBaseName();
    const QString targetPrimaryName = QFileInfo(primaryTargetPath).fileName();
    const QString targetStem = QFileInfo(primaryTargetPath).completeBaseName();
    QStringList targetFiles;
    QSet<QString> plannedTargets;
    for (const QString& sourceFile : sourceFiles)
    {
        const QFileInfo companionInfo(sourceFile);
        const QString companionName = companionInfo.fileName();
        QString targetName;
        if (companionName.compare(sourcePrimaryName, Qt::CaseInsensitive) == 0)
        {
            targetName = targetPrimaryName;
        }
        else if (companionName.startsWith(sourcePrimaryName, Qt::CaseInsensitive))
        {
            targetName = targetPrimaryName + companionName.mid(sourcePrimaryName.size());
        }
        else if (companionName.startsWith(sourceStem, Qt::CaseInsensitive))
        {
            targetName = targetStem + companionName.mid(sourceStem.size());
        }
        else
        {
            targetName = companionName;
        }

        const QString targetPath = destinationDirectory.filePath(targetName);
        const QString sourceAbsolutePath = companionInfo.absoluteFilePath();
        const QString targetAbsolutePath = QFileInfo(targetPath).absoluteFilePath();
        if (QDir::cleanPath(sourceAbsolutePath).compare(
                QDir::cleanPath(targetAbsolutePath), Qt::CaseInsensitive) == 0)
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("源文件和目标文件夹相同：%1")
                    .arg(sourceAbsolutePath);
            }
            return false;
        }
        if (QFileInfo::exists(targetPath) || plannedTargets.contains(targetPath))
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("目标文件已存在：%1")
                    .arg(targetPath);
            }
            return false;
        }

        plannedTargets.insert(targetPath);
        targetFiles.append(targetPath);
    }

    QStringList copiedFiles;
    for (int index = 0; index < sourceFiles.size(); ++index)
    {
        if (!QFile::copy(sourceFiles.at(index), targetFiles.at(index)))
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("复制文件失败：%1 -> %2")
                    .arg(sourceFiles.at(index), targetFiles.at(index));
            }
            for (const QString& copiedFile : copiedFiles)
            {
                QFile::remove(copiedFile);
            }
            return false;
        }
        copiedFiles.append(targetFiles.at(index));
    }
    return true;
}

void DataCatalogWidget::refreshLocalImportTarget(
    const CatalogContext& context,
    const QString& targetDirectory)
{
    if (!m_browserModel || targetDirectory.isEmpty())
    {
        return;
    }

    QModelIndex targetIndex = context.sourceIndex;
    QgsDataItem* targetItem = m_browserModel->dataItem(targetIndex);
    QgsDirectoryItem* targetDirectoryItem = qobject_cast<QgsDirectoryItem*>(
        targetItem);
    if (!targetDirectoryItem ||
        QDir::cleanPath(QFileInfo(targetDirectoryItem->dirPath()).absoluteFilePath())
            .compare(QDir::cleanPath(targetDirectory), Qt::CaseInsensitive) != 0)
    {
        targetIndex = m_browserModel->findPath(targetDirectory);
    }

    if (targetIndex.isValid())
    {
        m_browserModel->refresh(targetIndex);
    }
    else
    {
        m_browserModel->reload();
    }

    if (m_localStatusLabel)
    {
        m_localStatusLabel->setText(
            QStringLiteral("正在刷新本地导入目录：%1")
                .arg(QDir::toNativeSeparators(targetDirectory)));
    }
    QTimer::singleShot(0, this, [this, targetDirectory]()
    {
        showFavoritesInTree();
        if (m_localTree)
        {
            m_localTree->expandPath(targetDirectory);
        }
    });
}

void DataCatalogWidget::importProductData(int targetDirectoryId)
{
    if (!m_dataImportCallback)
    {
        return;
    }
    if (targetDirectoryId <= 0)
    {
        m_dataImportCallback(0);
        return;
    }

    const QSqlDatabase database = catalogDatabase(m_databaseConnectionName);
    DataCatalogProductCatalog catalog(database);
    ProductImportSnapshot before;
    QString errorMessage;
    if (!catalog.captureImportSnapshot(&before, &errorMessage) ||
        !before.directoryIds.contains(targetDirectoryId))
    {
        QMessageBox::warning(
            this,
            QStringLiteral("导入数据"),
            QStringLiteral("无法确认目标目录，已取消导入：%1")
                .arg(errorMessage.isEmpty()
                    ? QStringLiteral("目录不存在。")
                    : errorMessage));
        return;
    }

    m_dataImportCallback(targetDirectoryId);
    if (!catalog.relocateImportedProducts(
            targetDirectoryId, before, &errorMessage))
    {
        QMessageBox::warning(
            this,
            QStringLiteral("导入数据"),
            QStringLiteral("数据已导入，但移入所选目录失败：%1")
                .arg(errorMessage));
    }
}

void DataCatalogWidget::importContextItem(const CatalogContext& context)
{
    if (context.source == CatalogSource::Database)
    {
        if (m_dataImportCallback && context.databaseItem &&
            (isProductCatalogDirectory(context.databaseItem) ||
             context.databaseItem->data(DatabaseNameRole).isValid()))
        {
            const int targetDirectoryId = isProductCatalogDirectory(context.databaseItem)
                ? productDirectoryId(context.databaseItem)
                : 0;
            importProductData(qMax(targetDirectoryId, 0));
        }
        return;
    }

    const QString targetDirectory = localImportTargetPath(context);
    if (targetDirectory.isEmpty())
    {
        return;
    }

    const QStringList sourcePaths = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("选择要导入的本地数据"),
        targetDirectory,
        QStringLiteral(
            "GIS数据 (*.shp *.geojson *.json *.gpkg *.kml *.gml *.dxf *.tif *.tiff *.img *.vrt);;所有文件 (*.*)"));
    if (sourcePaths.isEmpty())
    {
        return;
    }

    int successCount = 0;
    int failureCount = 0;
    QStringList failures;
    QSet<QString> processedSources;
    for (const QString& sourcePath : sourcePaths)
    {
        const QFileInfo sourceInfo(sourcePath);
        const QString sourceKey = QDir::cleanPath(
            sourceInfo.absoluteFilePath()).toLower();
        if (processedSources.contains(sourceKey))
        {
            continue;
        }
        processedSources.insert(sourceKey);

        QString errorMessage;
        if (copyLocalImportDataset(sourcePath, targetDirectory, &errorMessage))
        {
            ++successCount;
        }
        else
        {
            ++failureCount;
            failures.append(QStringLiteral("%1：%2")
                .arg(sourceInfo.fileName(), errorMessage));
        }
    }

    if (successCount > 0)
    {
        refreshLocalImportTarget(context, targetDirectory);
    }

    QString summary = QStringLiteral(
        "本地数据导入完成。\n成功：%1\n失败：%2")
        .arg(successCount)
        .arg(failureCount);
    if (!failures.isEmpty())
    {
        summary += QStringLiteral("\n\n失败详情：\n") + failures.join(QChar('\n'));
    }

    if (successCount > 0)
    {
        QMessageBox::information(this, QStringLiteral("导入数据"), summary);
    }
    else
    {
        QMessageBox::warning(this, QStringLiteral("导入数据"), summary);
    }
}

QString DataCatalogWidget::databaseDisplayAliasKey(
    const QStandardItem* item) const
{
    if (!item)
    {
        return QString();
    }

    const QString databaseName = item->data(DatabaseNameRole).toString();
    if (databaseName.isEmpty())
    {
        return QString();
    }

    const int productIdValue = item->data(ProductIdRole).toInt();
    if (productIdValue > 0)
    {
        return QStringLiteral("product:%1\x1f%2")
            .arg(databaseName)
            .arg(productIdValue);
    }

    const QString schemaName = item->data(DatabaseSchemaRole).toString();
    const QString tableName = item->data(DatabaseTableRole).toString();
    if (!tableName.isEmpty())
    {
        return QStringLiteral("table:%1\x1f%2\x1f%3\x1f%4\x1f%5")
            .arg(databaseName)
            .arg(schemaName)
            .arg(tableName)
            .arg(item->data(DatabaseLayerTypeRole).toString())
            .arg(item->data(DatabaseGeometryColumnRole).toString());
    }
    if (!schemaName.isEmpty())
    {
        return QStringLiteral("schema:%1\x1f%2")
            .arg(databaseName)
            .arg(schemaName);
    }
    return QStringLiteral("database:%1").arg(databaseName);
}

QString DataCatalogWidget::databaseRemovalKey(
    const QString& databaseName,
    const QString& schemaName,
    const QString& tableName,
    const QString& layerType,
    const QString& geometryColumn) const
{
    if (databaseName.trimmed().isEmpty())
    {
        return QString();
    }

    const QSqlDatabase database = catalogDatabase(m_databaseConnectionName);
    const QString identity = databaseIdentityKey(database, databaseName);
    return QStringLiteral("database-data:%1\x1f%2\x1f%3\x1f%4\x1f%5\x1f%6")
        .arg(identity)
        .arg(schemaName.trimmed())
        .arg(tableName.trimmed())
        .arg(layerType.trimmed())
        .arg(geometryColumn.trimmed());
}

QString DataCatalogWidget::productRemovalKey(
    const QString& databaseName,
    int productIdValue) const
{
    if (databaseName.trimmed().isEmpty() || productIdValue <= 0)
    {
        return QString();
    }

    const QSqlDatabase database = catalogDatabase(m_databaseConnectionName);
    return QStringLiteral("database-product:%1\x1f%2")
        .arg(databaseIdentityKey(database, databaseName))
        .arg(productIdValue);
}

QString DataCatalogWidget::databaseRemovalKey(const QStandardItem* item) const
{
    if (!item)
    {
        return QString();
    }

    const int productIdValue = item->data(ProductIdRole).toInt();
    const QString databaseName = item->data(DatabaseNameRole).toString();
    if (productIdValue > 0)
    {
        return productRemovalKey(databaseName, productIdValue);
    }

    return databaseRemovalKey(
        databaseName,
        item->data(DatabaseSchemaRole).toString(),
        item->data(DatabaseTableRole).toString(),
        item->data(DatabaseLayerTypeRole).toString(),
        item->data(DatabaseGeometryColumnRole).toString());
}

QString DataCatalogWidget::localRemovalKey(const QgsDataItem* item) const
{
    return stableLocalItemKey(item);
}

void DataCatalogWidget::renameContextItem(const CatalogContext& context)
{
    if (context.source == CatalogSource::Database)
    {
        QStandardItem* item = context.databaseItem;
        if (!item)
        {
            return;
        }

        const QString layerType = item->data(DatabaseLayerTypeRole).toString();
        const bool canRename = item->data(DatabaseNameRole).isValid() &&
            layerType != QStringLiteral("table") &&
            !databaseDisplayAliasKey(item).isEmpty();
        if (!canRename)
        {
            return;
        }

        bool accepted = false;
        const QString newName = QInputDialog::getText(
            this,
            QStringLiteral("重命名"),
            QStringLiteral("新的显示名称："),
            QLineEdit::Normal,
            item->text(),
            &accepted).trimmed();
        if (!accepted)
        {
            return;
        }
        if (newName.isEmpty() || newName.contains(QChar('\n')) ||
            newName.contains(QChar('\r')))
        {
            QMessageBox::warning(
                this,
                QStringLiteral("重命名"),
                QStringLiteral("显示名称不能为空，也不能包含换行。"));
            return;
        }
        if (newName == item->text())
        {
            return;
        }

        m_databaseDisplayAliases.insert(databaseDisplayAliasKey(item), newName);
        item->setText(newName);
        return;
    }

    QgsDataItem* item = context.localItem;
    if (!item || !m_browserModel)
    {
        return;
    }

    if (isCatalogLayer(context))
    {
        const QString key = localRemovalKey(item);
        if (key.isEmpty())
        {
            return;
        }

        bool accepted = false;
        const QString currentName = displayNameForItem(context);
        const QString newName = QInputDialog::getText(
            this,
            QStringLiteral("重命名"),
            QStringLiteral("新的显示名称："),
            QLineEdit::Normal,
            currentName,
            &accepted).trimmed();
        if (!accepted)
        {
            return;
        }
        if (newName.isEmpty() || newName.contains(QChar('\n')) ||
            newName.contains(QChar('\r')))
        {
            QMessageBox::warning(
                this,
                QStringLiteral("重命名"),
                QStringLiteral("显示名称不能为空，也不能包含换行。"));
            return;
        }
        if (newName == currentName)
        {
            return;
        }

        m_localDisplayAliases.insert(key, newName);
        if (m_localProxyModel)
        {
            static_cast<CatalogBrowserProxyModel*>(m_localProxyModel)
                ->refreshCatalogState();
        }
        return;
    }

    const QModelIndex favoritesIndex = favoritesRootIndex();
    QgsFavoritesItem* favoritesItem = qobject_cast<QgsFavoritesItem*>(
        m_browserModel->dataItem(favoritesIndex));
    QgsDirectoryItem* directory = qobject_cast<QgsDirectoryItem*>(item);
    if (!favoritesItem || !directory || item->parent() != favoritesItem)
    {
        return;
    }

    bool accepted = false;
    const QString newName = QInputDialog::getText(
        this,
        QStringLiteral("重命名"),
        QStringLiteral("新的文件夹显示名称："),
        QLineEdit::Normal,
        item->name(),
        &accepted).trimmed();
    if (!accepted)
    {
        return;
    }
    if (newName.isEmpty() || newName.contains(QChar('\n')) ||
        newName.contains(QChar('\r')))
    {
        QMessageBox::warning(
            this,
            QStringLiteral("重命名"),
            QStringLiteral("显示名称不能为空，也不能包含换行。"));
        return;
    }
    if (newName == item->name())
    {
        return;
    }

    favoritesItem->renameFavorite(directory->dirPath(), newName);
    showFavoritesInTree();
}

QStringList DataCatalogWidget::localDataPathsForContext(
    const CatalogContext& context) const
{
    QStringList result;
    if (context.source != CatalogSource::Local || !context.localItem)
    {
        return result;
    }

    for (const QgsMimeDataUtils::Uri& uri : localUris(context.localItem))
    {
        const QString sourcePath = uri.filePath.trimmed();
        if (sourcePath.isEmpty())
        {
            continue;
        }

        const QString normalizedPath = normalizedLocalPath(sourcePath);
        bool duplicate = false;
        for (const QString& existingPath : result)
        {
            if (existingPath.compare(normalizedPath, Qt::CaseInsensitive) == 0)
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
        {
            result.append(normalizedPath);
        }
    }
    return result;
}

QStringList DataCatalogWidget::localDeleteTargetsForPath(
    const QString& sourcePath) const
{
    const QFileInfo sourceInfo(sourcePath);
    if (sourceInfo.isDir())
    {
        return QStringList() << sourceInfo.absoluteFilePath();
    }

    const QStringList companionFiles = localImportCompanionFiles(sourcePath);
    return companionFiles.isEmpty()
        ? (QStringList() << sourceInfo.absoluteFilePath())
        : companionFiles;
}

bool DataCatalogWidget::chooseDataRemovalMode(
    const CatalogContext& context,
    const QStringList& permanentTargets,
    DataRemovalMode* mode)
{
    if (!mode)
    {
        return false;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("删除数据"));
    dialog.setModal(true);

    auto* layout = new QVBoxLayout(&dialog);
    const QString sourceLabel = context.source == CatalogSource::Database
        ? QStringLiteral("数据库")
        : QStringLiteral("本地数据");
    layout->addWidget(new QLabel(
        QStringLiteral("数据：%1\n来源：%2")
            .arg(displayNameForItem(context), sourceLabel),
        &dialog));

    auto* platformOnly = new QRadioButton(
        QStringLiteral("仅从平台列表移除（保留源数据）"),
        &dialog);
    auto* permanent = new QRadioButton(
        QStringLiteral("永久删除源数据"),
        &dialog);
    platformOnly->setChecked(true);
    layout->addWidget(platformOnly);
    layout->addWidget(permanent);

    auto* warning = new QLabel(&dialog);
    warning->setWordWrap(true);
    warning->setStyleSheet(QStringLiteral("color: #b00020;"));
    warning->setVisible(false);
    layout->addWidget(warning);

    QString targetSummary = permanentTargets.join(QChar('\n'));
    if (targetSummary.isEmpty())
    {
        targetSummary = QStringLiteral("当前源数据");
    }
    const auto updateWarning = [warning, targetSummary]()
    {
        warning->setText(
            QStringLiteral("将永久删除以下源数据，此操作不可恢复：\n%1")
                .arg(targetSummary));
    };
    connect(permanent, &QRadioButton::toggled, &dialog,
        [warning, updateWarning](bool checked)
        {
            warning->setVisible(checked);
            if (checked)
            {
                updateWarning();
            }
        });

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确认"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted,
        &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected,
        &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
    {
        return false;
    }

    *mode = permanent->isChecked()
        ? DataRemovalMode::Permanent
        : DataRemovalMode::PlatformOnly;
    return true;
}

bool DataCatalogWidget::permanentlyDeleteDatabaseData(
    const CatalogContext& context,
    QString* errorMessage)
{
    if (errorMessage)
    {
        errorMessage->clear();
    }
    if (context.source != CatalogSource::Database || !context.databaseItem)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("未选择数据库数据节点。");
        }
        return false;
    }

    if (isProductCatalogProduct(context.databaseItem) ||
        (isProductCatalogDirectory(context.databaseItem) &&
         context.databaseItem->data(ProductDirectoryNodeTypeRole).toInt() == 1))
    {
        const QList<int> productIds = isProductCatalogProduct(context.databaseItem)
            ? (QList<int>() << productId(context.databaseItem))
            : allProductIdsUnderItem(context.databaseItem);
        if (productIds.isEmpty())
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("当前节点没有可删除的成果数据。");
            }
            return false;
        }

        const QSqlDatabase database = catalogDatabase(m_databaseConnectionName);
        DataCatalogProductCatalog catalog(database);
        return catalog.deleteProducts(productIds, errorMessage);
    }

    const QString layerType = context.databaseItem->data(
        DatabaseLayerTypeRole).toString();
    const QString schemaName = context.databaseItem->data(
        DatabaseSchemaRole).toString().trimmed();
    const QString tableName = context.databaseItem->data(
        DatabaseTableRole).toString().trimmed();
    if ((layerType != QStringLiteral("vector") &&
         layerType != QStringLiteral("raster")) ||
        schemaName.isEmpty() || tableName.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("当前节点不是可删除的数据库空间图层。");
        }
        return false;
    }

    const QSqlDatabase database = catalogDatabase(m_databaseConnectionName);
    if (!database.isValid() || !database.isOpen())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("数据库连接未打开。");
        }
        return false;
    }

    QSqlQuery query(database);
    const QString qualifiedTable = quotedSqlIdentifier(schemaName) +
        QChar('.') + quotedSqlIdentifier(tableName);
    query.prepare(QStringLiteral("DROP TABLE IF EXISTS %1 CASCADE")
        .arg(qualifiedTable));
    if (!query.exec())
    {
        if (errorMessage)
        {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

bool DataCatalogWidget::permanentlyDeleteLocalData(
    const CatalogContext& context,
    QStringList* deletedPaths,
    QStringList* failedPaths)
{
    if (deletedPaths)
    {
        deletedPaths->clear();
    }
    if (failedPaths)
    {
        failedPaths->clear();
    }

    const QStringList sourcePaths = localDataPathsForContext(context);
    if (sourcePaths.isEmpty())
    {
        if (failedPaths)
        {
            failedPaths->append(QStringLiteral("未找到本地源文件路径。"));
        }
        return false;
    }

    QStringList targets;
    QStringList targetSources;
    for (const QString& sourcePath : sourcePaths)
    {
        for (const QString& targetPath : localDeleteTargetsForPath(sourcePath))
        {
            bool duplicate = false;
            for (const QString& existingPath : targets)
            {
                if (existingPath.compare(targetPath, Qt::CaseInsensitive) == 0)
                {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate)
            {
                targets.append(targetPath);
                targetSources.append(sourcePath);
            }
        }
    }

    for (int i = 0; i < targets.size(); ++i)
    {
        if (!isSafeLocalDeleteTarget(targets.at(i), targetSources.at(i)))
        {
            if (failedPaths)
            {
                failedPaths->append(targets.at(i));
            }
            return false;
        }
    }

    bool allSucceeded = true;
    for (const QString& targetPath : targets)
    {
        const QFileInfo targetInfo(targetPath);
        if (!targetInfo.exists())
        {
            continue;
        }

        const bool removed = targetInfo.isDir()
            ? QDir(targetInfo.absoluteFilePath()).removeRecursively()
            : QFile::remove(targetInfo.absoluteFilePath());
        if (removed || !QFileInfo::exists(targetPath))
        {
            if (deletedPaths)
            {
                deletedPaths->append(targetPath);
            }
        }
        else
        {
            allSucceeded = false;
            if (failedPaths)
            {
                failedPaths->append(targetPath);
            }
        }
    }
    return allSucceeded;
}

void DataCatalogWidget::removeData(const CatalogContext& context)
{
    if (!isDeletableDataContext(context))
    {
        return;
    }

    const auto warnPlatformStateSaveFailure = [this]()
    {
        QMessageBox::warning(
            this,
            QStringLiteral("删除数据"),
            QStringLiteral(
                "平台列表状态保存失败，本次会话的显示已更新，但重启后可能恢复显示。"));
    };

    QStringList permanentTargets;
    if (context.source == CatalogSource::Database)
    {
        if (isProductCatalogProduct(context.databaseItem))
        {
            permanentTargets.append(
                QStringLiteral("成果 ID：%1").arg(productId(context.databaseItem)));
        }
        else if (isProductCatalogDirectory(context.databaseItem))
        {
            for (const int id : allProductIdsUnderItem(context.databaseItem))
            {
                permanentTargets.append(QStringLiteral("成果 ID：%1").arg(id));
            }
        }
        else
        {
            permanentTargets.append(
                QStringLiteral("%1.%2")
                    .arg(context.databaseItem->data(DatabaseSchemaRole).toString())
                    .arg(context.databaseItem->data(DatabaseTableRole).toString()));
        }
    }
    else
    {
        permanentTargets = localDataPathsForContext(context);
        QStringList companionTargets;
        for (const QString& sourcePath : permanentTargets)
        {
            companionTargets.append(localDeleteTargetsForPath(sourcePath));
        }
        permanentTargets = companionTargets;
    }

    DataRemovalMode mode = DataRemovalMode::PlatformOnly;
    if (!chooseDataRemovalMode(context, permanentTargets, &mode))
    {
        return;
    }

    if (mode == DataRemovalMode::PlatformOnly)
    {
        if (context.source == CatalogSource::Database)
        {
            if (isProductCatalogProduct(context.databaseItem) ||
                (isProductCatalogDirectory(context.databaseItem) &&
                 context.databaseItem->data(ProductDirectoryNodeTypeRole).toInt() == 1))
            {
                const QString databaseName = context.databaseItem->data(
                    DatabaseNameRole).toString();
                for (const int id : allProductIdsUnderItem(context.databaseItem))
                {
                    const QString key = productRemovalKey(databaseName, id);
                    if (!key.isEmpty())
                    {
                        m_removedDatabaseKeys.insert(key);
                    }
                }
                if (isProductCatalogProduct(context.databaseItem))
                {
                    const QString key = productRemovalKey(
                        databaseName,
                        productId(context.databaseItem));
                    if (!key.isEmpty())
                    {
                        m_removedDatabaseKeys.insert(key);
                    }
                }
                for (const int id : allProductIdsUnderItem(context.databaseItem))
                {
                    m_checkedDatabaseKeys.remove(
                        QStringLiteral("product-check:%1\x1f%2")
                            .arg(databaseName)
                            .arg(id));
                }
                if (isProductCatalogDirectory(context.databaseItem))
                {
                    m_checkedDatabaseKeys.remove(
                        QStringLiteral("product-directory-check:%1\x1f%2")
                            .arg(databaseName)
                            .arg(productDirectoryId(context.databaseItem)));
                }
            }
            else
            {
                const QString key = databaseRemovalKey(context.databaseItem);
                if (!key.isEmpty())
                {
                    m_removedDatabaseKeys.insert(key);
                }
                m_checkedDatabaseKeys.remove(databaseCheckKey(context.databaseItem));
            }
            if (!savePlatformRemovalState())
            {
                warnPlatformStateSaveFailure();
            }
            loadDatabaseData();
        }
        else
        {
            const QString key = localRemovalKey(context.localItem);
            if (!key.isEmpty())
            {
                m_removedLocalPaths.insert(key);
                m_checkedLocalPaths.remove(key);
                if (!savePlatformRemovalState())
                {
                    warnPlatformStateSaveFailure();
                }
            }
            if (m_localProxyModel)
            {
                static_cast<CatalogBrowserProxyModel*>(m_localProxyModel)
                    ->refreshCatalogState();
            }
            showFavoritesInTree();
        }
        return;
    }

    if (context.source == CatalogSource::Database)
    {
        QString errorMessage;
        if (!permanentlyDeleteDatabaseData(context, &errorMessage))
        {
            QMessageBox::warning(
                this,
                QStringLiteral("删除数据"),
                QStringLiteral("数据库数据删除失败：%1")
                    .arg(errorMessage));
            return;
        }

        if (isProductCatalogProduct(context.databaseItem) ||
            (isProductCatalogDirectory(context.databaseItem) &&
             context.databaseItem->data(ProductDirectoryNodeTypeRole).toInt() == 1))
        {
            const QString databaseName = context.databaseItem->data(
                DatabaseNameRole).toString();
            for (const int id : allProductIdsUnderItem(context.databaseItem))
            {
                m_removedDatabaseKeys.remove(productRemovalKey(databaseName, id));
                m_checkedDatabaseKeys.remove(
                    QStringLiteral("product-check:%1\x1f%2")
                        .arg(databaseName)
                        .arg(id));
                m_databaseDisplayAliases.remove(
                    QStringLiteral("product:%1\x1f%2")
                        .arg(databaseName)
                        .arg(id));
            }
            if (isProductCatalogProduct(context.databaseItem))
            {
                const int id = productId(context.databaseItem);
                m_removedDatabaseKeys.remove(productRemovalKey(databaseName, id));
                m_checkedDatabaseKeys.remove(
                    QStringLiteral("product-check:%1\x1f%2")
                        .arg(databaseName)
                        .arg(id));
                m_databaseDisplayAliases.remove(
                    QStringLiteral("product:%1\x1f%2")
                        .arg(databaseName)
                        .arg(id));
            }
            if (isProductCatalogDirectory(context.databaseItem))
            {
                m_checkedDatabaseKeys.remove(
                    QStringLiteral("product-directory-check:%1\x1f%2")
                        .arg(databaseName)
                        .arg(productDirectoryId(context.databaseItem)));
            }
        }
        else
        {
            m_removedDatabaseKeys.remove(databaseRemovalKey(context.databaseItem));
            m_checkedDatabaseKeys.remove(databaseCheckKey(context.databaseItem));
            m_databaseDisplayAliases.remove(databaseDisplayAliasKey(context.databaseItem));
        }
        if (!savePlatformRemovalState())
        {
            warnPlatformStateSaveFailure();
        }
        refreshDatabaseData();
        return;
    }

    QStringList deletedPaths;
    QStringList failedPaths;
    const bool success = permanentlyDeleteLocalData(
        context,
        &deletedPaths,
        &failedPaths);
    const QString key = localRemovalKey(context.localItem);
    if (success && !key.isEmpty())
    {
        m_removedLocalPaths.remove(key);
        m_localDisplayAliases.remove(key);
        m_checkedLocalPaths.remove(key);
        if (!savePlatformRemovalState())
        {
            warnPlatformStateSaveFailure();
        }
    }

    if (m_browserModel && context.sourceIndex.isValid())
    {
        m_browserModel->refresh(context.sourceIndex.parent());
    }
    QTimer::singleShot(0, this, &DataCatalogWidget::showFavoritesInTree);

    if (!failedPaths.isEmpty())
    {
        QMessageBox::warning(
            this,
            QStringLiteral("删除数据"),
            QStringLiteral("部分本地数据删除失败：\n%1")
                .arg(failedPaths.join(QChar('\n'))));
    }
    else if (success)
    {
        QMessageBox::information(
            this,
            QStringLiteral("删除数据"),
            QStringLiteral("本地源数据已删除。"));
    }
}

bool DataCatalogWidget::exportVectorUri(
    const QgsMimeDataUtils::Uri& uri,
    const QString& outputPath,
    QString* errorMessage) const
{
    bool owner = false;
    QString resolveError;
    QgsVectorLayer* layer = uri.vectorLayer(owner, resolveError);
    if (!layer || !layer->isValid())
    {
        if (errorMessage)
        {
            *errorMessage = resolveError.isEmpty()
                ? QStringLiteral("无法打开矢量源数据。")
                : resolveError;
        }
        if (owner)
        {
            delete layer;
        }
        return false;
    }

    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = QgsVectorFileWriter::driverForExtension(
        QFileInfo(outputPath).suffix());
    if (options.driverName.isEmpty())
    {
        options.driverName = QStringLiteral("GPKG");
    }
    options.layerName = sanitizedExportBaseName(
        QFileInfo(outputPath).completeBaseName());
    options.fileEncoding = QStringLiteral("UTF-8");

    QString writerError;
    const QgsVectorFileWriter::WriterError result =
        QgsVectorFileWriter::writeAsVectorFormatV3(
            layer,
            outputPath,
            QgsProject::instance()->transformContext(),
            options,
            &writerError,
            nullptr,
            nullptr);
    if (owner)
    {
        delete layer;
    }

    if (result != QgsVectorFileWriter::NoError)
    {
        if (errorMessage)
        {
            *errorMessage = writerError.isEmpty()
                ? QStringLiteral("矢量 writer 返回错误代码 %1。")
                    .arg(static_cast<int>(result))
                : writerError;
        }
        return false;
    }
    return true;
}

bool DataCatalogWidget::exportRasterUri(
    const QgsMimeDataUtils::Uri& uri,
    const QString& outputPath,
    QString* errorMessage) const
{
    bool owner = false;
    QString resolveError;
    QgsRasterLayer* layer = uri.rasterLayer(owner, resolveError);
    if (!layer || !layer->isValid() || !layer->pipe())
    {
        if (errorMessage)
        {
            *errorMessage = resolveError.isEmpty()
                ? QStringLiteral("无法打开栅格源数据。")
                : resolveError;
        }
        if (owner)
        {
            delete layer;
        }
        return false;
    }

    QgsRasterFileWriter writer(outputPath);
    const QString driver = QgsRasterFileWriter::driverForExtension(
        QFileInfo(outputPath).suffix());
    if (!driver.isEmpty())
    {
        writer.setOutputFormat(driver);
    }
    const QgsRasterFileWriter::WriterError result = writer.writeRaster(
        layer->pipe(),
        layer->width(),
        layer->height(),
        layer->extent(),
        layer->crs(),
        QgsProject::instance()->transformContext());
    if (owner)
    {
        delete layer;
    }

    if (result != QgsRasterFileWriter::NoError)
    {
        if (errorMessage)
        {
            *errorMessage = rasterWriterErrorText(result);
        }
        return false;
    }
    return true;
}

void DataCatalogWidget::exportContextItem(const CatalogContext& context)
{
    if (!isCatalogLayer(context))
    {
        return;
    }

    QgsMimeDataUtils::Uri uri;
    if (context.source == CatalogSource::Database)
    {
        uri = databaseUri(context.databaseItem);
    }
    else
    {
        const QgsMimeDataUtils::UriList uris = localUris(context.localItem);
        if (!uris.isEmpty())
        {
            uri = uris.first();
        }
    }
    if (!isSupportedCatalogUri(uri))
    {
        return;
    }

    exportUri(uri);
}

void DataCatalogWidget::exportUri(const QgsMimeDataUtils::Uri& uri)
{
    if (!isSupportedCatalogUri(uri))
    {
        return;
    }

    const QString baseName = sanitizedExportBaseName(
        uri.name.isEmpty()
            ? QFileInfo(uri.filePath).completeBaseName()
            : uri.name);
    QString outputPath;
    if (uri.layerType == QStringLiteral("vector"))
    {
        outputPath = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("导出矢量数据"),
            QDir::home().filePath(baseName + QStringLiteral(".gpkg")),
            QgsVectorFileWriter::fileFilterString());
        if (!outputPath.isEmpty() && QFileInfo(outputPath).suffix().isEmpty())
        {
            outputPath += QStringLiteral(".gpkg");
        }
    }
    else
    {
        QStringList filters;
        const QList<QgsRasterFileWriter::FilterFormatDetails> formats =
            QgsRasterFileWriter::supportedFiltersAndFormats();
        for (const QgsRasterFileWriter::FilterFormatDetails& format : formats)
        {
            filters.append(format.filterString);
        }
        outputPath = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("导出栅格数据"),
            QDir::home().filePath(baseName + QStringLiteral(".tif")),
            filters.isEmpty()
                ? QStringLiteral("GeoTIFF (*.tif *.tiff);;所有文件 (*.*)")
                : filters.join(QStringLiteral(";;")));
        if (!outputPath.isEmpty() && QFileInfo(outputPath).suffix().isEmpty())
        {
            outputPath += QStringLiteral(".tif");
        }
    }

    if (outputPath.isEmpty())
    {
        return;
    }

    if (!uri.filePath.isEmpty() &&
        QDir::cleanPath(QFileInfo(uri.filePath).absoluteFilePath()).compare(
            QDir::cleanPath(QFileInfo(outputPath).absoluteFilePath()),
            Qt::CaseInsensitive) == 0)
    {
        QMessageBox::warning(
            this,
            QStringLiteral("导出数据"),
            QStringLiteral("输出文件不能覆盖当前源文件。"));
        return;
    }

    QString errorMessage;
    const bool success = uri.layerType == QStringLiteral("vector")
        ? exportVectorUri(uri, outputPath, &errorMessage)
        : exportRasterUri(uri, outputPath, &errorMessage);
    if (success)
    {
        QMessageBox::information(
            this,
            QStringLiteral("导出数据"),
            QStringLiteral("数据已导出到：\n%1").arg(outputPath));
    }
    else
    {
        QMessageBox::warning(
            this,
            QStringLiteral("导出数据"),
            QStringLiteral("导出失败：\n%1").arg(errorMessage));
    }
}

QgsMimeDataUtils::UriList DataCatalogWidget::collectDatabaseUris(
    const QStandardItem* item) const
{
    QgsMimeDataUtils::UriList result;
    if (!item)
    {
        return result;
    }

    appendUniqueUri(&result, databaseUri(item));
    for (int row = 0; row < item->rowCount(); ++row)
    {
        appendUniqueUri(&result, databaseUri(item->child(row)));
        const QgsMimeDataUtils::UriList childUris = collectDatabaseUris(
            item->child(row));
        for (const QgsMimeDataUtils::Uri& uri : childUris)
        {
            appendUniqueUri(&result, uri);
        }
    }
    return result;
}

QgsMimeDataUtils::UriList DataCatalogWidget::collectLocalUris(
    QgsDataItem* item,
    int* unloadedCount) const
{
    QgsMimeDataUtils::UriList result;
    if (!item)
    {
        return result;
    }

    const QgsMimeDataUtils::UriList ownUris = localUris(item);
    for (const QgsMimeDataUtils::Uri& uri : ownUris)
    {
        appendUniqueUri(&result, uri);
    }

    if (m_browserModel && item->state() != Qgis::BrowserItemState::Populated)
    {
        const QModelIndex itemIndex = m_browserModel->findPath(item->path());
        if (itemIndex.isValid() && m_browserModel->canFetchMore(itemIndex))
        {
            m_browserModel->fetchMore(itemIndex);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 150);
        }
    }

    const QVector<QgsDataItem*> children = item->children();
    if (ownUris.isEmpty() && children.isEmpty() &&
        item->state() != Qgis::BrowserItemState::Populated &&
        unloadedCount)
    {
        ++(*unloadedCount);
    }
    for (QgsDataItem* child : children)
    {
        const QgsMimeDataUtils::UriList childUris = collectLocalUris(
            child,
            unloadedCount);
        for (const QgsMimeDataUtils::Uri& uri : childUris)
        {
            appendUniqueUri(&result, uri);
        }
    }
    return result;
}

QgsMimeDataUtils::UriList DataCatalogWidget::exportUrisForContexts(
    const QVector<CatalogContext>& contexts,
    int* unloadedCount) const
{
    QgsMimeDataUtils::UriList result;
    if (unloadedCount)
    {
        *unloadedCount = 0;
    }

    for (const CatalogContext& context : contexts)
    {
        if (context.source == CatalogSource::Database)
        {
            const QgsMimeDataUtils::UriList contextUris =
                isCatalogCollection(context)
                    ? collectDatabaseUris(context.databaseItem)
                    : QgsMimeDataUtils::UriList()
                        << databaseUri(context.databaseItem);
            for (const QgsMimeDataUtils::Uri& uri : contextUris)
            {
                appendUniqueUri(&result, uri);
            }
            continue;
        }

        if (isCatalogCollection(context))
        {
            const QgsMimeDataUtils::UriList contextUris = collectLocalUris(
                context.localItem,
                unloadedCount);
            for (const QgsMimeDataUtils::Uri& uri : contextUris)
            {
                appendUniqueUri(&result, uri);
            }
            continue;
        }

        for (const QgsMimeDataUtils::Uri& uri : localUris(context.localItem))
        {
            appendUniqueUri(&result, uri);
        }
    }
    return result;
}

void DataCatalogWidget::batchExportContextItem(const CatalogContext& context)
{
    if (!isCatalogCollection(context) && !isCatalogLayer(context))
    {
        return;
    }

    batchExportContextItems(QVector<CatalogContext>() << context);
}

void DataCatalogWidget::batchExportContextItems(
    const QVector<CatalogContext>& contexts)
{
    if (contexts.isEmpty())
    {
        return;
    }

    int unloadedCount = 0;
    const QgsMimeDataUtils::UriList uris = exportUrisForContexts(
        contexts,
        &unloadedCount);
    batchExportUris(uris, unloadedCount);
}

void DataCatalogWidget::batchExportUris(
    const QgsMimeDataUtils::UriList& uris,
    int unloadedCount,
    const QString& title)
{
    if (uris.isEmpty())
    {
        QMessageBox::information(
            this,
            title,
            unloadedCount > 0
                ? QStringLiteral("当前目录没有已加载的可导出图层。请先展开或刷新目录。")
                : QStringLiteral("当前目录没有可导出的矢量或栅格图层。"));
        return;
    }

    const QString outputDirectory = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择批量导出目录"),
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (outputDirectory.isEmpty())
    {
        return;
    }

    int successCount = 0;
    int failureCount = 0;
    QStringList failures;
    for (const QgsMimeDataUtils::Uri& uri : uris)
    {
        const QString baseName = sanitizedExportBaseName(
            uri.name.isEmpty()
                ? QFileInfo(uri.filePath).completeBaseName()
                : uri.name);
        const QString extension = uri.layerType == QStringLiteral("raster")
            ? QStringLiteral("tif")
            : QStringLiteral("gpkg");
        const QString outputPath = uniqueExportPath(
            outputDirectory,
            baseName,
            extension);
        QString errorMessage;
        const bool success = uri.layerType == QStringLiteral("vector")
            ? exportVectorUri(uri, outputPath, &errorMessage)
            : exportRasterUri(uri, outputPath, &errorMessage);
        if (success)
        {
            ++successCount;
        }
        else
        {
            ++failureCount;
            failures.append(QStringLiteral("%1：%2")
                .arg(baseName, errorMessage));
        }
    }

    QString summary = QStringLiteral(
        "批量导出完成。\n成功：%1\n失败：%2\n未加载/不支持：%3")
        .arg(successCount)
        .arg(failureCount)
        .arg(unloadedCount);
    if (!failures.isEmpty())
    {
        summary += QStringLiteral("\n\n失败详情：\n") + failures.join(QChar('\n'));
    }
    QMessageBox::information(this, title, summary);
}

void DataCatalogWidget::showDatabaseMessage(
    const QString& message,
    bool isError)
{
    if (!m_databaseModel)
    {
        return;
    }

    m_databaseModel->removeRows(0, m_databaseModel->rowCount());
    const QString displayedMessage = gCurrentUserSession.isGuest
        ? QStringLiteral("\u533F\u540D\u6A21\u5F0F\u65E0\u6CD5\u8FDE\u63A5\u6570\u636E\u5E93\uFF0C\u8BF7\u767B\u5F55\u8D26\u6237\u540E\u4F7F\u7528\u6B64\u529F\u80FD\u3002")
        : message;
    QStandardItem* item = new QStandardItem(displayedMessage);
    item->setEditable(false);
    item->setEnabled(false);
    if (isError && !gCurrentUserSession.isGuest)
    {
        item->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
    }
    m_databaseModel->appendRow(item);
}
