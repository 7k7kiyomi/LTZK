/*--------------本模块---------------*/
#include "map_product_clip_engine.h"

/*--------------QT---------------*/
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <QVector>
#include <QSet>
#include <QDebug>

/*--------------QGIS---------------*/
#include <qgsvectorlayer.h>
#include <qgsdatasourceuri.h>
#include <qgsfields.h>
#include <qgsfield.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgscoordinatetransform.h>
#include <qgsproject.h>
#include <qgsexception.h>
#include <qgswkbtypes.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorfilewriter.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsmessagelog.h>

/*--------------PostgreSQL---------------*/
#include <libpq-fe.h>

/*--------------GDAL---------------*/
#include <gdal.h>
#include <gdal_priv.h>
#include <gdal_utils.h>
#include <cpl_string.h>
#include <cpl_conv.h>

/*--------------数据库---------------*/
#include "postgis_connector.h"

using namespace MapProductClipEngine;

namespace MapProductClipEngine
{
namespace
{
// 本文件的编译时间戳：写在日志与失败信息里，用于确认 QGIS 当前加载的
// 插件 DLL 到底是哪一次编译的产物（QGIS 不会卸载已加载的插件 DLL，
// 重新生成后必须完全退出并重启 QGIS 才会生效）。
const QString kEngineBuildStamp = QStringLiteral(__DATE__ " " __TIME__);

// ------------------------------------------------------------------
// 通用小工具
// ------------------------------------------------------------------
QString sanitizeBaseName(const QString& name)
{
    QString s = name;
    s.replace('\\', '_').replace('/', '_').replace(':', '_')
        .replace('*', '_').replace('?', '_').replace('"', '_')
        .replace('<', '_').replace('>', '_').replace('|', '_');
    if (s.trimmed().isEmpty())
        s = QStringLiteral("product");
    return s.trimmed();
}

QString vectorSuffixForDriver(const QString& driver)
{
    if (driver.compare(QStringLiteral("GPKG"), Qt::CaseInsensitive) == 0)
        return QStringLiteral(".gpkg");
    // 文件型地理数据库（GDAL 3.6+ 的 OpenFileGDB 支持写入）
    if (driver.compare(QStringLiteral("OpenFileGDB"), Qt::CaseInsensitive) == 0)
        return QStringLiteral(".gdb");
    return QStringLiteral(".shp");
}

// 目标矢量格式中 FID 列的名字。Shapefile / GeoJSON 等没有独立 FID 列的格式
// 返回空串。
//
// QGIS 的 OGR provider 在写要素时（qgsogrprovider.cpp 的 addFeaturePrivate，
// 其中唯一的 OGR_F_SetFID 调用），会把"与目标 FID 列同名"的那个属性值直接
// 当作 OGR FID 使用，该属性也不会再作为普通字段写出。FileGDB 的 FID 列名为
// OBJECTID、GPKG 为 fid，因此源数据里同名的 OBJECTID / fid 字段会被当成 FID。
QString targetFidColumnName(const QString& driver)
{
    if (driver.compare(QStringLiteral("OpenFileGDB"), Qt::CaseInsensitive) == 0
        || driver.compare(QStringLiteral("FileGDB"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("OBJECTID");
    if (driver.compare(QStringLiteral("GPKG"), Qt::CaseInsensitive) == 0
        || driver.compare(QStringLiteral("SQLite"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("fid");
    return QString();
}

struct TransformResult
{
    QgsGeometry geom;
    bool ok = true;
    QString err;
};

// 把几何从 fromCrs 换算到 toCrs；任一坐标系无效/相同时不换算直接返回
TransformResult transformGeom(const QgsGeometry& g,
                              const QgsCoordinateReferenceSystem& fromCrs,
                              const QgsCoordinateReferenceSystem& toCrs)
{
    TransformResult ret;
    if (g.isNull())
    {
        ret.ok = false;
        ret.err = QStringLiteral("裁剪几何无效");
        return ret;
    }
    if (!fromCrs.isValid() || !toCrs.isValid() || fromCrs == toCrs)
    {
        ret.geom = g;
        return ret;
    }
    QgsCoordinateTransform ct(fromCrs, toCrs, QgsProject::instance());
    if (!ct.isValid())
    {
        ret.ok = false;
        ret.err = QStringLiteral("坐标系换算不可用：%1 → %2")
                      .arg(fromCrs.authid(), toCrs.authid());
        return ret;
    }
    try
    {
        QgsGeometry t = g; // transform() 为就地修改，需先拷贝
        if (t.transform(ct) == Qgis::GeometryOperationResult::Success)
            ret.geom = t;
        else
        {
            ret.ok = false;
            ret.err = QStringLiteral("坐标系换算失败：%1 → %2")
                          .arg(fromCrs.authid(), toCrs.authid());
        }
    }
    catch (const QgsCsException&)
    {
        ret.ok = false;
        ret.err = QStringLiteral("坐标系换算失败：%1 → %2")
                      .arg(fromCrs.authid(), toCrs.authid());
    }
    return ret;
}

// 把矩形外包范围换算到目标坐标系，供栅格像素窗口计算
bool transformBBox(const QgsRectangle& rect,
                   const QgsCoordinateReferenceSystem& fromCrs,
                   const QgsCoordinateReferenceSystem& toCrs,
                   QgsRectangle& outRect)
{
    if (rect.isNull() || rect.isEmpty()) return false;
    if (!fromCrs.isValid() || !toCrs.isValid() || fromCrs == toCrs)
    {
        outRect = rect;
        return true;
    }
    QgsCoordinateTransform ct(fromCrs, toCrs, QgsProject::instance());
    if (!ct.isValid()) return false;
    try
    {
        outRect = ct.transformBoundingBox(rect);
        return !outRect.isNull() && !outRect.isEmpty();
    }
    catch (const QgsCsException&)
    {
        return false;
    }
}

QgsGeometry polygonFromRect(const QgsRectangle& rect)
{
    return QgsGeometry::fromWkt(rect.asWktPolygon());
}

// 目标文件路径：合并模式写单一容器文件；单产品用显式路径；多产品在输出目录下按产品名自动命名
bool composeOutPath(const ClipOptions& opt, const ProductMetadata& meta,
                    bool isRaster, QString& outPath, QString& err)
{
    // ① 合并写入单一矢量容器文件（GPKG/GDB）：所有矢量成果共用同一目标文件，
    //    图层名由写出阶段按产品名区分（栅格无法写入，仍走下面的目录模式）
    if (!isRaster && opt.mergeLayersIntoOneFile && !opt.explicitOutputFile.isEmpty())
    {
        QString p = opt.explicitOutputFile;
        const QString wantSuffix = vectorSuffixForDriver(opt.vectorDriver);
        if (!p.endsWith(wantSuffix, Qt::CaseInsensitive))
        {
            QFileInfo fi(p);
            p = fi.absolutePath() + "/" + fi.completeBaseName() + wantSuffix;
        }
        outPath = p;
        return true;
    }

    // ② 单产品显式输出文件（非合并模式）
    if (!opt.explicitOutputFile.isEmpty() && !opt.mergeLayersIntoOneFile)
    {
        QString p = opt.explicitOutputFile;
        if (isRaster)
        {
            // 栅格统一产出 GeoTIFF：非 .tif/.tiff 尾缀统一纠正为 .tif
            if (p.endsWith(QStringLiteral(".tiff"), Qt::CaseInsensitive))
            {
                p.chop(5);
                p += QStringLiteral(".tif");
            }
            else if (!p.endsWith(QStringLiteral(".tif"), Qt::CaseInsensitive))
            {
                p += QStringLiteral(".tif");
            }
        }
        else
        {
            // 矢量输出扩展名与驱动保持一致
            const QString wantSuffix = vectorSuffixForDriver(opt.vectorDriver);
            if (!p.endsWith(wantSuffix, Qt::CaseInsensitive))
            {
                QFileInfo fi(p);
                if (fi.suffix().isEmpty())
                    p += wantSuffix;
                else
                    p = fi.absolutePath() + "/" + fi.completeBaseName() + wantSuffix;
            }
        }
        outPath = p;
        return true;
    }

    if (opt.outputDir.isEmpty())
    {
        err = QStringLiteral("未指定输出目录");
        return false;
    }
    QDir dir(opt.outputDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
    {
        err = QStringLiteral("输出目录不可用：%1").arg(opt.outputDir);
        return false;
    }
    QString base = dir.filePath(sanitizeBaseName(meta.productName));
    if (isRaster)
        outPath = base + QStringLiteral(".tif");
    else
        outPath = base + vectorSuffixForDriver(opt.vectorDriver);
    return true;
}

bool removeIfExists(const QString& path)
{
    return QFile::remove(path) || !QFile::exists(path);
}

// 删除已存在的数据集：普通文件直接删；目录型数据集（如 .gdb）递归删除
bool removeDatasetIfExists(const QString& path)
{
    if (!QFileInfo::exists(path)) return true;
    if (QFileInfo(path).isDir())
        return QDir(path).removeRecursively();
    return QFile::remove(path);
}

// ------------------------------------------------------------------
// 数据源读取
// ------------------------------------------------------------------
// 栅格：优先 fileOid，否则从图层表查询
bool resolveRasterOid(const ProductMetadata& meta, PostgisConnector* db, int& oid)
{
    oid = meta.fileOid;
    if (oid <= 0 && !meta.layerTableName.isEmpty())
    {
        QVariantMap row = db->executeQueryOne(
            QString("SELECT file_oid FROM \"%1\" LIMIT 1").arg(meta.layerTableName));
        if (!row.isEmpty())
            oid = row.value("file_oid").toInt();
    }
    return oid > 0;
}

// 栅格：把 BLOB 大对象落盘为临时文件
bool exportRasterTemp(const ProductMetadata& meta, QString& tempPath)
{
    PostgisConnector* db = PostgisConnector::instance();
    if (!db || !db->isConnected()) return false;

    int oid = 0;
    if (!resolveRasterOid(meta, db, oid)) return false;

    QString ext = meta.fileFormat.trimmed().toLower();
    QString fname = QStringLiteral("mmp_clip_%1_%2")
                        .arg(meta.id > 0 ? meta.id : 0)
                        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    if (!ext.isEmpty())
        fname += QStringLiteral(".") + ext;

    QString path = QDir::temp().filePath(fname);
    if (QFile::exists(path)) QFile::remove(path);
    if (!db->loExport(oid, path) || !QFile::exists(path))
        return false;
    tempPath = path;
    return true;
}

// 泛型 GEOMETRY 列（未约束具体子类型）在无人值守下无法让 PostGIS provider
// 推断几何类型，直接打开会失败；而 PostGIS 对"视图"的 geometry_columns
// 元数据识别又不可靠（试过仍然打不开）。这里先统计要素实际几何类型，再
// 物化成一张"类型化临时表"（几何列 CAST 成 geometry(具体类型, srid)），
// 与其它已成功打开的类型化图层表注册方式完全一致，确保能正常打开。
// 成功返回已加引号的 "schema"."table"（调用方用完后须 DROP TABLE），
// outTablePlain 返回未限定的临时表名（供 setDataSource 使用）。
static QString tryBuildTypedGeomTable(const QString& schema, const QString& table,
                                      const QString& geomCol, int srid,
                                      QString& outTablePlain, QString& errMsg)
{
    PostgisConnector* db = PostgisConnector::instance();
    if (!db || !db->isConnected())
    {
        errMsg = QStringLiteral("数据库连接已断开");
        return QString();
    }
    // 取数量最多的实际几何类型作为临时表类型
    const QVariantMap census = db->executeQueryOne(QString(
        "SELECT GeometryType(\"%3\") AS t, count(*) AS c "
        "FROM \"%1\".\"%2\" WHERE \"%3\" IS NOT NULL "
        "GROUP BY 1 ORDER BY c DESC LIMIT 1")
        .arg(schema, table, geomCol));
    const QString rawType = census.value("t").toString().toUpper();
    // ST_CollectionExtract 用的维度：面 3 / 线 2 / 点 1
    int dim = 3;
    QString pgType = QStringLiteral("MultiPolygon");
    if (rawType.isEmpty())
    {
        // 空表：临时表内没有要素，声明任意合法类型即可
    }
    else if (rawType == QLatin1String("ST_POLYGON")
             || rawType == QLatin1String("ST_MULTIPOLYGON")
             || rawType == QLatin1String("ST_CURVEPOLYGON"))
    {
        dim = 3;
        pgType = QStringLiteral("MultiPolygon");
    }
    else if (rawType == QLatin1String("ST_LINESTRING")
             || rawType == QLatin1String("ST_MULTILINESTRING")
             || rawType == QLatin1String("ST_COMPOUNDCURVE")
             || rawType == QLatin1String("ST_MULTICURVE")
             || rawType == QLatin1String("ST_CIRCULARSTRING"))
    {
        dim = 2;
        pgType = QStringLiteral("MultiLineString");
    }
    else if (rawType == QLatin1String("ST_POINT")
             || rawType == QLatin1String("ST_MULTIPOINT"))
    {
        dim = 1;
        pgType = QStringLiteral("MultiPoint");
    }
    else
    {
        errMsg = QStringLiteral("无法从要素推断出可用的几何类型（实际为 %1）")
                     .arg(rawType.isEmpty() ? census.value("t").toString() : rawType);
        return QString();
    }

    const QVariantMap colsRow = db->executeQueryOne(QString(
        "SELECT array_to_string(array_agg(quote_ident(column_name) "
        "ORDER BY ordinal_position), ',') AS cs "
        "FROM information_schema.columns "
        "WHERE LOWER(table_schema) = LOWER('%1') "
        "AND LOWER(table_name) = LOWER('%2') "
        "AND LOWER(column_name) <> LOWER('%3')").arg(schema, table, geomCol));
    const QString colList = colsRow.value("cs").toString();
    const QString colPrefix = colList.isEmpty() ? QString() : (colList + QStringLiteral(", "));

    const QString outTablePlainTmp = QStringLiteral("mmp_tmp_%1").arg(table.left(40));
    outTablePlain = outTablePlainTmp;
    // 先清理可能残留的同名临时表（上次异常中断等场景）
    db->executeNonQuery(QStringLiteral("DROP TABLE IF EXISTS \"%1\".\"%2\"")
                            .arg(schema, outTablePlain));
    const QString createSql = QString(
        "CREATE TABLE \"%1\".\"%2\" AS SELECT %3"
        "ST_CollectionExtract(ST_Multi(ST_Force2D(ST_CurveToLine(\"%4\"))), %5)"
        "::geometry(%6, %7) AS \"%4\" FROM \"%1\".\"%8\"")
        .arg(schema, outTablePlain, colPrefix, geomCol)
        .arg(dim).arg(pgType).arg(srid).arg(table);
    if (!db->executeNonQuery(createSql))
    {
        errMsg = QStringLiteral("创建类型化临时表失败：%1").arg(db->lastError());
        outTablePlain.clear();
        return QString();
    }
    return QStringLiteral("\"%1\".\"%2\"").arg(schema, outTablePlain);
}

// 矢量：从 PostGIS 图层表建图层（与成果普通导出使用同一套连接/表解析逻辑）
// tmpTableToDrop：当原表为泛型 GEOMETRY、经由物化类型化临时表打开时，返回
// 需 DROP 的临时表限定名（非空时调用方在数据读取完成后 DROP TABLE IF EXISTS）。
bool loadVectorProduct(const ProductMetadata& meta, QgsVectorLayer*& outLayer,
                       QString& tmpTableToDrop, QString& err)
{
    PostgisConnector* db = PostgisConnector::instance();
    if (!db || !db->isConnected())
    {
        err = QStringLiteral("数据库未连接");
        return false;
    }
    if (meta.layerTableName.isEmpty())
    {
        err = QStringLiteral("该产品缺少 PostGIS 图层表信息");
        return false;
    }
    PGconn* conn = db->nativeConnection();
    if (!conn)
    {
        err = QStringLiteral("获取数据库连接失败");
        return false;
    }
    QString host = QString::fromUtf8(PQhost(conn));
    QString port = QString::fromUtf8(PQport(conn));
    QString dbName = QString::fromUtf8(PQdb(conn));
    QString user = QString::fromUtf8(PQuser(conn));
    QString password = QString::fromUtf8(PQpass(conn));

    QString actualSchema = db->searchPath();
    if (actualSchema.isEmpty()) actualSchema = QStringLiteral("public");
    QString findSchemaSql = QString(
        "SELECT table_schema, table_name FROM information_schema.tables "
        "WHERE LOWER(table_name) = LOWER('%1') LIMIT 1").arg(meta.layerTableName);
    QVariantMap schemaResult = db->executeQueryOne(findSchemaSql);
    QString foundSchema = schemaResult.value("table_schema").toString();
    QString realTableName = schemaResult.value("table_name").toString();
    if (!foundSchema.isEmpty()) actualSchema = foundSchema;
    QString effectiveTableName = realTableName.isEmpty()
        ? meta.layerTableName.toLower() : realTableName;

    // 几何列不写死为 geom：先从元数据查出真实几何列名，
    // 避免部分图层表把几何列命名为 shape/the_geom 等导致按 geom 打开失败
    QString geometryCol;
    const QVariantMap geomRow = db->executeQueryOne(QString(
        "SELECT column_name AS c FROM information_schema.columns "
        "WHERE LOWER(table_schema) = LOWER('%1') "
        "AND LOWER(table_name) = LOWER('%2') "
        "AND udt_name IN ('geometry', 'geography') "
        "ORDER BY column_name LIMIT 1").arg(actualSchema, effectiveTableName));
    if (!geomRow.isEmpty())
        geometryCol = geomRow.value("c").toString();

    QgsDataSourceUri dsUri;
    dsUri.setConnection(host, port, dbName, user, password);
    dsUri.setDataSource(actualSchema, effectiveTableName,
                        geometryCol.isEmpty() ? QStringLiteral("geom") : geometryCol,
                        QString(), QString());
    dsUri.setUseEstimatedMetadata(true);

    QgsVectorLayer::LayerOptions layerOptions;
    QgsVectorLayer* layer = new QgsVectorLayer(dsUri.uri(), meta.layerTableName,
                                               QStringLiteral("YOUR_PASSWORD"), layerOptions);
    if (!layer || !layer->isValid())
    {
        // 打开失败时给出可定位的具体原因：优先取数据源自身的报错，
        // 取不到再用数据库信息做常见原因判断
        QString detail;
        QVariantMap gMeta;               // 几何列元数据（类型/SRID）
        int geomSrid = 0;
        QString geomMetaType;
        if (layer)
        {
            if (QgsVectorDataProvider* dp = layer->dataProvider())
            {
                const QString providerErr = dp->lastError().trimmed();
                if (!providerErr.isEmpty())
                    detail = QStringLiteral("数据源报错：%1").arg(providerErr);
            }
        }
        if (detail.isEmpty())
        {
            const QString tblExistsSql = QString(
                "SELECT 1 AS found FROM information_schema.tables "
                "WHERE LOWER(table_schema) = LOWER('%1') "
                "AND LOWER(table_name) = LOWER('%2') LIMIT 1")
                .arg(actualSchema, effectiveTableName);
            const bool tableExists = !db->executeQueryOne(tblExistsSql).isEmpty();
            if (!tableExists)
                detail = QStringLiteral("数据库中不存在数据表 %1.%2")
                             .arg(actualSchema, effectiveTableName);
            else if (geometryCol.isEmpty())
                detail = QStringLiteral("数据表 %1.%2 中没有几何列（缺 geometry/geography 字段）")
                             .arg(actualSchema, effectiveTableName);
            else
            {
                // 把几何列的元数据带出来，便于定位打开失败的真实原因
                gMeta = db->executeQueryOne(QString(
                    "SELECT srid, type, coord_dimension AS dim FROM geometry_columns "
                    "WHERE LOWER(f_table_schema) = LOWER('%1') "
                    "AND LOWER(f_table_name) = LOWER('%2') "
                    "AND LOWER(f_geometry_column) = LOWER('%3') LIMIT 1")
                    .arg(actualSchema, effectiveTableName, geometryCol));
                if (!gMeta.isEmpty())
                {
                    geomSrid = gMeta.value("srid").toInt();
                    geomMetaType = gMeta.value("type").toString().trimmed().toUpper();
                    detail = QStringLiteral("数据表 %1.%2 的几何列「%3」为 SRID=%4、"
                        "类型=%5、坐标维度=%6，但按 PostGIS 矢量图层打开失败，"
                        "请核对 SRID 是否已注册、字段是否为有效几何")
                        .arg(actualSchema, effectiveTableName, geometryCol,
                             gMeta.value("srid").toString(),
                             gMeta.value("type").toString(),
                             gMeta.value("dim").toString());
                }
                else
                    detail = QStringLiteral("数据表 %1.%2 的几何列「%3」未在 PostGIS "
                        "geometry_columns 中注册，无法推断几何类型")
                        .arg(actualSchema, effectiveTableName, geometryCol);
            }
        }
        // 其它失败路径下也补查一次几何列元数据，用于泛型 GEOMETRY 的判断
        if (geomMetaType.isEmpty() && !geometryCol.isEmpty())
        {
            gMeta = db->executeQueryOne(QString(
                "SELECT srid, type FROM geometry_columns "
                "WHERE LOWER(f_table_schema) = LOWER('%1') "
                "AND LOWER(f_table_name) = LOWER('%2') "
                "AND LOWER(f_geometry_column) = LOWER('%3') LIMIT 1")
                .arg(actualSchema, effectiveTableName, geometryCol));
            if (!gMeta.isEmpty())
            {
                geomSrid = gMeta.value("srid").toInt();
                geomMetaType = gMeta.value("type").toString().trimmed().toUpper();
            }
        }

        // 泛型 GEOMETRY 列没有具体子类型约束，PostGIS provider 无人值守时
        // 无法确定几何类型；按要素实际类型物化一张类型化临时表再打开一次
        if (geomMetaType == QLatin1String("GEOMETRY") && !geometryCol.isEmpty())
        {
            QString tablePlain;
            QString fbErr;
            const QString tableQual =
                tryBuildTypedGeomTable(actualSchema, effectiveTableName,
                                       geometryCol, geomSrid, tablePlain, fbErr);
            if (!tableQual.isEmpty())
            {
                QgsDataSourceUri tmpUri;
                tmpUri.setConnection(host, port, dbName, user, password);
                tmpUri.setDataSource(actualSchema, tablePlain, geometryCol,
                                     QString(), QString());
                tmpUri.setUseEstimatedMetadata(true);
                QgsVectorLayer* vLayer = new QgsVectorLayer(tmpUri.uri(),
                    meta.layerTableName, QStringLiteral("YOUR_PASSWORD"), layerOptions);
                if (vLayer && vLayer->isValid())
                {
                    delete layer;
                    tmpTableToDrop = tableQual;
                    outLayer = vLayer;
                    return true;
                }
                QString viewErr;
                if (vLayer)
                {
                    if (QgsVectorDataProvider* dp = vLayer->dataProvider())
                        viewErr = dp->lastError().trimmed();
                }
                delete vLayer;
                db->executeNonQuery(QString("DROP TABLE IF EXISTS %1").arg(tableQual));
                fbErr = QStringLiteral("重新打开类型化临时表失败%1")
                            .arg(viewErr.isEmpty() ? QString()
                                 : QStringLiteral("（%1）").arg(viewErr));
            }
            else if (!fbErr.isEmpty())
                fbErr = QStringLiteral("无法建立类型化临时表：%1").arg(fbErr);
            if (!fbErr.isEmpty())
                detail += QStringLiteral("；%1").arg(fbErr);
        }

        delete layer;
        err = QStringLiteral("打开图层失败：%1").arg(detail);
        return false;
    }
    outLayer = layer;
    return true;
}

// ------------------------------------------------------------------
// 矢量裁剪
// ------------------------------------------------------------------
bool writeVectorClip(QgsVectorLayer* src, const QgsGeometry& clipPoly,
                     const ClipOptions& opt, const QString& outPath,
                     bool appendLayer, const QString& layerNameOverride,
                     QString& realOutPath, QString& note, QString& err,
                     const std::function<bool(const QString&)>& report)
{
    if (!src || !src->isValid())
    {
        err = QStringLiteral("源图层无效");
        return false;
    }
    if (clipPoly.isNull() || clipPoly.isEmpty())
    {
        err = QStringLiteral("裁剪范围无效");
        return false;
    }

    // 输出文件（Shapefile 系列文件 / GPKG 由驱动追加扩展名）
    QString suffix = vectorSuffixForDriver(opt.vectorDriver);
    QString noExt = outPath;
    if (noExt.endsWith(suffix, Qt::CaseInsensitive))
        noExt.chop(suffix.size());
    else if (noExt.endsWith(QStringLiteral(".tif"), Qt::CaseInsensitive)
             || noExt.endsWith(QStringLiteral(".gdb"), Qt::CaseInsensitive))
        noExt = QFileInfo(noExt).path() + "/" + QFileInfo(noExt).completeBaseName();

    // 合并写入单一容器文件（GPKG/GDB）时直接使用完整目标路径，
    // 后续图层以 CreateOrOverwriteLayer 追加到同一文件
    const bool mergeIntoFile = opt.mergeLayersIntoOneFile;
    const QString datasetName = mergeIntoFile ? outPath : noExt;

    // 仅首个图层需要做"是否已存在/清理旧文件"处理，追加图层不能动容器文件
    if (!appendLayer)
    {
        if (!opt.overwriteExisting && QFile::exists(outPath))
        {
            err = QStringLiteral("输出文件已存在：%1").arg(outPath);
            return false;
        }
        if (mergeIntoFile)
        {
            // 容器文件（.gpkg 文件 / .gdb 目录）先整体清掉，保证从头写入
            removeDatasetIfExists(outPath);
        }
        // 清理可能残留的同名附属文件（shp 系列）
        else if (opt.vectorDriver.compare(QStringLiteral("GPKG"), Qt::CaseInsensitive) != 0)
        {
            const QStringList suffixes =
                { ".shp", ".shx", ".dbf", ".prj", ".cpg", ".qpj" };
            for (const QString& s : suffixes)
                removeIfExists(noExt + s);
        }
        else
        {
            removeIfExists(outPath);
        }
    }

    // 目标几何类型（统一提升为 Multi 以便容纳求交结果）
    const QgsWkbTypes::Type srcType = src->wkbType();
    const QgsWkbTypes::Type outType =
        QgsWkbTypes::multiType(QgsWkbTypes::flatType(srcType));
    const QString typeStr = QgsWkbTypes::displayString(outType);
    if (typeStr.isEmpty())
    {
        err = QStringLiteral("无法识别源图层几何类型");
        return false;
    }

    QString crsToken;
    if (src->crs().isValid())
    {
        crsToken = src->crs().authid();
        if (crsToken.isEmpty()) crsToken = src->crs().toWkt();
    }
    QString uri = typeStr;
    if (!crsToken.isEmpty())
        uri += QString("?crs=%1").arg(crsToken);

    QgsVectorLayer* mem = new QgsVectorLayer(uri,
        QFileInfo(noExt).completeBaseName(), QStringLiteral("memory"));
    if (!mem || !mem->isValid())
    {
        delete mem;
        err = QStringLiteral("创建输出图层失败");
        return false;
    }
    mem->dataProvider()->addAttributes(src->fields().toList());
    mem->updateFields();

    if (report && !report(QStringLiteral("正在读取源数据并与裁剪范围求交…")))
    {
        delete mem;
        err = QStringLiteral("已取消");
        return false;
    }

    // 内存图层声明为“目标基类 + Multi 部件”；求交结果须规整到同样式，
    // 否则提交时会被拒绝（几何图形类型与当前图层不相容）
    const QgsWkbTypes::GeometryType memGeomType = QgsWkbTypes::geometryType(outType);
    QgsFeatureIterator it = src->getFeatures();
    QgsFeature f;
    QgsFeatureList feats;
    qint64 clipped = 0;
    qint64 scanned = 0;
    qint64 skippedGeom = 0;
    // 输出要素重新编号（1 起自增）：源数据的 FID 可能超出目标格式的表示范围。
    // 例如 FileGDB 仅支持 32 位正整数 FID，而源为 GeoPackage / PostGIS 等
    // 64 位 FID 数据时，直接沿用会出现"Only 32 bit positive integers FID
    // supported by FileGDB"导致整个成果写出失败。裁剪成果为新数据集，
    // 重新编号不影响业务属性。
    QgsFeatureId outFid = 1;
    while (it.nextFeature(f))
    {
        ++scanned;
        // 周期上报：导出大图层时让界面及时刷新并可取消
        if (scanned % 2000 == 0)
        {
            if (report && !report(QStringLiteral("正在与裁剪范围求交，已处理 %1 个要素…")
                                      .arg(scanned)))
            {
                delete mem;
                err = QStringLiteral("已取消");
                return false;
            }
        }
        QgsGeometry g = f.geometry();
        if (g.isNull() || g.isEmpty()) continue;
        QgsGeometry inter = g.intersection(clipPoly);
        if (inter.isNull() || inter.isEmpty()) continue;
        // 统一转成目标基类的 Multi 部件：顺带丢弃曲线与 Z/M 维度，
        // 让单部件、带 Z/M、曲线等结果都能被图层接受
        const QgsGeometry norm = inter.convertToType(memGeomType, true);
        if (norm.isNull() || norm.isEmpty()
            || QgsWkbTypes::flatType(norm.wkbType()) != outType)
        {
            // 极端情况下仍无法规整的要素跳过，避免整个成果提交失败
            ++skippedGeom;
            continue;
        }
        f.setGeometry(norm);
        // 顺序编号仅用于排查：内存图层本身会重新分配 FID，最终写出的 FID
        // 取决于目标格式（见下方"与目标 FID 列同名的属性字段处理"）
        f.setId(outFid++);
        feats << f;
        ++clipped;
    }

    if (report && !report(QStringLiteral("求交完成，共裁剪出 %1 个要素，正在写出文件…")
                              .arg(clipped)))
    {
        delete mem;
        err = QStringLiteral("已取消");
        return false;
    }
    // ------------------------------------------------------------------
    // 与目标 FID 列同名的属性字段处理
    //
    // QGIS 写要素时，OGR 要素的 FID 取自"与目标 FID 列同名"的那个属性
    // （qgsogrprovider.cpp 的 addFeaturePrivate，其中唯一的 OGR_F_SetFID 调用），
    // 该属性也不会再作为普通字段写出。FileGDB 的 FID 列名是 OBJECTID、
    // GPKG 是 fid，因此源数据里同名的 OBJECTID / fid 字段会被当成 FID 使用。
    // 只要其中某个值不是 32 位正整数（0、负值、超出 2^31-1，或与其它要素重复），
    // OpenFileGDB 就会整层写出失败：
    //   Only 32 bit positive integers FID supported by FileGDB
    // 这里先收集全部合法值，再把非法值改成"本图层内未被占用的正整数"，
    // 既保证唯一也保证落在 32 位范围内；合法值保持原样不变。
    // ------------------------------------------------------------------
    const QString fidFieldName = targetFidColumnName(opt.vectorDriver);
    int fidAttrIdx = -1;
    if (!fidFieldName.isEmpty() && !feats.isEmpty())
    {
        const QgsFields srcFields = src->fields();
        for (int i = 0; i < srcFields.count(); ++i)
        {
            if (srcFields.at(i).name().compare(fidFieldName, Qt::CaseInsensitive) == 0)
            {
                fidAttrIdx = i;
                break;
            }
        }
    }
    if (fidAttrIdx >= 0)
    {
        // 第一遍：收集全部合法取值（用于保留原值与判重）
        QSet<qlonglong> usedFids;
        for (const QgsFeature& cf : feats)
        {
            bool ok = false;
            const qlonglong fidValue = cf.attribute(fidAttrIdx).toLongLong(&ok);
            if (ok && fidValue > 0 && fidValue <= 2147483647LL)
                usedFids.insert(fidValue);
        }
        // 第二遍：合法值只保留给第一个使用它的要素，其余（0/负值/超范围/重复）
        // 改成图层内尚未占用的正整数
        qlonglong nextFid = 1;
        qint64 invalidFids = 0;
        for (QgsFeature& cf : feats)
        {
            bool ok = false;
            const qlonglong fidValue = cf.attribute(fidAttrIdx).toLongLong(&ok);
            if (ok && fidValue > 0 && fidValue <= 2147483647LL && usedFids.remove(fidValue))
                continue;
            while (nextFid <= 2147483647LL && usedFids.contains(nextFid))
                ++nextFid;
            if (nextFid > 2147483647LL)
            {
                // 极端情况：32 位内已无可用值，置空交由驱动分配
                cf.setAttribute(fidAttrIdx, QVariant());
            }
            else
            {
                usedFids.insert(nextFid);
                cf.setAttribute(fidAttrIdx, QVariant(nextFid));
                ++nextFid;
            }
            ++invalidFids;
        }
        QgsMessageLog::logMessage(
            QStringLiteral("裁剪写出：%1 将使用字段 %2 作为 FID（该字段不作为普通字段写出）%3")
                .arg(opt.vectorDriver, fidFieldName)
                .arg(invalidFids > 0
                         ? QStringLiteral("；其中 %1 个值非法（0/负值/超 32 位/重复），"
                                          "已改为图层内未占用的正整数")
                               .arg(invalidFids)
                         : QString()),
            QStringLiteral("MapProduct"),
            invalidFids > 0 ? Qgis::Warning : Qgis::Info);
    }

    if (!feats.isEmpty())
    {
        mem->startEditing();
        if (!mem->addFeatures(feats))
        {
            mem->rollBack();
            delete mem;
            err = QStringLiteral("生成裁剪要素失败");
            return false;
        }
        if (!mem->commitChanges())
        {
            const QStringList commitErrs = mem->commitErrors();
            mem->rollBack();
            delete mem;
            err = QStringLiteral("提交裁剪要素失败");
            if (!commitErrs.isEmpty())
                err += QStringLiteral("：%1").arg(commitErrs.join(QStringLiteral("；")));
            return false;
        }
    }

    // 诊断日志：记录本次写出使用的引擎构建、驱动与要素数，
    // 便于确认运行的插件 DLL 版本（QGIS 需完全重启才会加载新 DLL）
    QgsMessageLog::logMessage(
        QStringLiteral("裁剪写出［引擎构建 %1］：目标=%2，驱动=%3，要素 %4 个")
            .arg(kEngineBuildStamp, datasetName, opt.vectorDriver).arg(clipped),
        QStringLiteral("MapProduct"), Qgis::Info);

    QgsVectorFileWriter::SaveVectorOptions saveOpt;
    saveOpt.driverName = opt.vectorDriver;
    // 合并模式下图层名取产品名（由调用方传入），否则沿用输出文件名
    saveOpt.layerName = layerNameOverride.isEmpty()
        ? QFileInfo(noExt).completeBaseName() : layerNameOverride;
    saveOpt.fileEncoding = opt.fileEncoding;
    // 追加图层：在已存在的容器文件中新建/覆盖同名图层；否则整体新建文件
    saveOpt.actionOnExistingFile = appendLayer
        ? QgsVectorFileWriter::CreateOrOverwriteLayer
        : QgsVectorFileWriter::CreateOrOverwriteFile;

    QString errMsg;
    QString newFilename;
    QgsVectorFileWriter::WriterError wErr = QgsVectorFileWriter::writeAsVectorFormatV3(
        mem, datasetName, mem->transformContext(), saveOpt, &errMsg, &newFilename);
    delete mem;

    if (wErr != QgsVectorFileWriter::NoError)
    {
        err = QStringLiteral("写出矢量文件失败：%1").arg(
            errMsg.isEmpty() ? QString::number(int(wErr)) : errMsg);
        // 附带引擎构建时间：若报错中看不到该标记，说明运行的仍是旧的插件 DLL
        err += QStringLiteral("［裁剪引擎构建：%1］").arg(kEngineBuildStamp);
        return false;
    }
    if (skippedGeom > 0)
        note = QStringLiteral("有 %1 个要素因几何类型无法规整而跳过").arg(skippedGeom);
    if (clipped == 0)
    {
        if (!note.isEmpty())
            note += QStringLiteral("；");
        note += QStringLiteral("裁剪范围内没有要素，已生成空文件");
    }
    realOutPath = newFilename.isEmpty() ? outPath : newFilename;
    if (report) report(QStringLiteral("写出完成"));
    return true;
}

// ------------------------------------------------------------------
// 栅格裁剪（GDAL，按范围外接矩形切出 GeoTIFF，不重采样）
// ------------------------------------------------------------------
bool rasterCropByRect(const QString& srcFile, const QString& outFile,
                      const QgsRectangle& rectInRasterCrs, QString& err)
{
    GDALDatasetH hSrc = GDALOpen(srcFile.toUtf8().constData(), GA_ReadOnly);
    if (!hSrc)
    {
        err = QStringLiteral("GDAL 无法打开源栅格：%1").arg(srcFile);
        return false;
    }
    if (GDALGetRasterCount(hSrc) < 1)
    {
        GDALClose(hSrc);
        err = QStringLiteral("源栅格没有波段");
        return false;
    }
    int nXSize = GDALGetRasterXSize(hSrc);
    int nYSize = GDALGetRasterYSize(hSrc);
    if (nXSize <= 0 || nYSize <= 0)
    {
        GDALClose(hSrc);
        err = QStringLiteral("源栅格尺寸无效");
        return false;
    }

    double gt[6] = { 0, 1, 0, 0, 0, -1 };
    if (GDALGetGeoTransform(hSrc, gt) != CE_None)
    {
        GDALClose(hSrc);
        err = QStringLiteral("源栅格没有地理变换信息，无法按范围裁剪");
        return false;
    }
    const double px = gt[1];
    const double py = gt[5];
    if (std::fabs(px) < 1e-30 || std::fabs(py) < 1e-30)
    {
        GDALClose(hSrc);
        err = QStringLiteral("源栅格像素尺寸异常");
        return false;
    }

    // 外包矩形 → 像素窗口（兼容负像素高度 gt[5]<0）
    auto xToCol = [&](double x) { return (x - gt[0]) / px; };
    auto yToRow = [&](double y) { return (y - gt[3]) / py; };
    double colA = xToCol(rectInRasterCrs.xMinimum());
    double colB = xToCol(rectInRasterCrs.xMaximum());
    double rowA = yToRow(rectInRasterCrs.yMaximum());
    double rowB = yToRow(rectInRasterCrs.yMinimum());

    int xOff = static_cast<int>(std::floor(std::min(colA, colB)));
    int yOff = static_cast<int>(std::floor(std::min(rowA, rowB)));
    int nCols = static_cast<int>(std::ceil(std::fabs(colB - colA)));
    int nRows = static_cast<int>(std::ceil(std::fabs(rowB - rowA)));
    if (nCols < 1) nCols = 1;
    if (nRows < 1) nRows = 1;

    if (xOff >= nXSize || yOff >= nYSize || xOff + nCols <= 0 || yOff + nRows <= 0)
    {
        GDALClose(hSrc);
        err = QStringLiteral("裁剪范围与源栅格没有交集");
        return false;
    }
    if (xOff < 0) { nCols += xOff; xOff = 0; }
    if (yOff < 0) { nRows += yOff; yOff = 0; }
    if (xOff + nCols > nXSize) nCols = nXSize - xOff;
    if (yOff + nRows > nYSize) nRows = nYSize - yOff;

    if (QFile::exists(outFile)) QFile::remove(outFile);

    QVector<QString> args;
    args << QStringLiteral("-of") << QStringLiteral("GTiff")
         << QStringLiteral("-srcwin")
         << QString::number(xOff) << QString::number(yOff)
         << QString::number(nCols) << QString::number(nRows);
    QVector<QByteArray> argUtf8;
    argUtf8.reserve(args.size());
    QVector<char*> argv;
    for (const QString& s : args)
    {
        argUtf8 << s.toUtf8();
        argv << argUtf8.last().data();
    }
    argv << nullptr;

    GDALTranslateOptions* trOpt = GDALTranslateOptionsNew(argv.data(), nullptr);
    GDALDatasetH hDst = GDALTranslate(outFile.toUtf8().constData(), hSrc, trOpt, nullptr);
    GDALTranslateOptionsFree(trOpt);
    GDALClose(hSrc);
    if (hDst)
    {
        GDALClose(hDst);
        return QFile::exists(outFile);
    }
    err = QStringLiteral("GDAL 按范围裁切失败");
    return false;
}

// ------------------------------------------------------------------
// 单个产品处理
// ------------------------------------------------------------------
ClipResult processProduct(const ProductMetadata& meta, const ClipOptions& opt,
                          bool appendToContainer,
                          const std::function<bool(const QString&)>& report)
{
    ClipResult r;
    r.productName = meta.productName;
    r.ok = false;

    // 阶段进度上报；返回 false 表示用户取消
    const auto reportStep = [&report](const QString& msg) -> bool
    {
        return !report || report(msg);
    };

    if (opt.clipGeom.isNull() || opt.clipGeom.isEmpty())
    {
        r.error = QStringLiteral("裁剪范围为空");
        return r;
    }

    // ---------- 栅格 ----------
    if (meta.productType == ProductType::Raster)
    {
        QString outPath;
        QString err;
        if (!composeOutPath(opt, meta, true, outPath, err))
        {
            r.error = err;
            return r;
        }
        if (!opt.overwriteExisting && QFile::exists(outPath))
        {
            r.error = QStringLiteral("输出文件已存在：%1").arg(outPath);
            return r;
        }

        QString temp;
        if (!reportStep(QStringLiteral("正在从数据库读取栅格数据…")))
        {
            r.error = QStringLiteral("已取消");
            return r;
        }
        if (!exportRasterTemp(meta, temp))
        {
            r.error = QStringLiteral("读取栅格数据失败");
            return r;
        }

        // 栅格按裁剪范围（多边形/矩形）的外接矩形裁切，保持源坐标系与分辨率
        QgsCoordinateReferenceSystem rasterCrs;
        GDALDatasetH hProbe = GDALOpen(temp.toUtf8().constData(), GA_ReadOnly);
        if (hProbe)
        {
            const char* proj = GDALGetProjectionRef(hProbe);
            if (proj && proj[0])
                rasterCrs = QgsCoordinateReferenceSystem::fromWkt(QString::fromUtf8(proj));
            GDALClose(hProbe);
        }

        QgsRectangle targetRect;
        bool hasRect = transformBBox(opt.clipGeom.boundingBox(), opt.clipCrs,
                                     rasterCrs, targetRect);
        if (!hasRect)
        {
            // 无坐标系信息时按同一坐标直接计算
            targetRect = opt.clipGeom.boundingBox();
        }
        if (targetRect.isNull() || targetRect.isEmpty())
        {
            if (!temp.isEmpty()) QFile::remove(temp);
            r.error = QStringLiteral("裁剪范围换算到栅格坐标系失败");
            return r;
        }

        if (!reportStep(QStringLiteral("正在按所选范围裁切栅格…")))
        {
            if (!temp.isEmpty()) QFile::remove(temp);
            r.error = QStringLiteral("已取消");
            return r;
        }
        bool okClip = rasterCropByRect(temp, outPath, targetRect, err);
        if (!temp.isEmpty()) QFile::remove(temp);
        if (!okClip)
        {
            r.error = err;
            return r;
        }
        r.filePath = outPath;
        r.note = QStringLiteral("栅格按所选范围的外接矩形裁切（保持原分辨率）");
        r.ok = true;

        if (opt.autoLoadToMap && opt.qgisIface)
            opt.qgisIface->addRasterLayer(outPath, meta.productName);
        return r;
    }

    // ---------- 矢量 ----------
    if (meta.productType == ProductType::Vector)
    {
        QString outPath;
        QString err;
        if (!composeOutPath(opt, meta, false, outPath, err))
        {
            r.error = err;
            return r;
        }

        if (!reportStep(QStringLiteral("正在从数据库加载矢量数据…")))
        {
            r.error = QStringLiteral("已取消");
            return r;
        }
        QgsVectorLayer* layer = nullptr;
        QString tmpTableToDrop;  // 泛型 GEOMETRY 表经类型化临时表打开时需清理
        if (!loadVectorProduct(meta, layer, tmpTableToDrop, err))
        {
            r.error = err;
            return r;
        }
        const auto dropTmpTable = [&tmpTableToDrop]()
        {
            if (!tmpTableToDrop.isEmpty())
                PostgisConnector::instance()->executeNonQuery(
                    QString("DROP TABLE IF EXISTS %1").arg(tmpTableToDrop));
            tmpTableToDrop.clear();
        };

        // 裁剪几何换算到源图层坐标系
        TransformResult tr = transformGeom(opt.clipGeom, opt.clipCrs, layer->crs());
        if (!tr.ok)
        {
            r.error = tr.err;
            delete layer;
            dropTmpTable();
            return r;
        }

        QString realOut;
        QString note;
        // 合并写入单一容器文件时：图层名取产品名，追加到已建好的容器中
        const QString layerName = opt.mergeLayersIntoOneFile
            ? sanitizeBaseName(meta.productName) : QString();
        bool okWrite = writeVectorClip(layer, tr.geom, opt, outPath,
                                       appendToContainer, layerName,
                                       realOut, note, err, report);
        delete layer;
        dropTmpTable();
        if (!okWrite)
        {
            r.error = err;
            return r;
        }
        r.filePath = realOut;
        r.note = note;
        r.ok = true;

        if (opt.autoLoadToMap && opt.qgisIface)
        {
            if (opt.mergeLayersIntoOneFile)
            {
                // 单一容器文件：按图层名精确加载当前成果图层，避免重复加载整库
                const QString uri = QStringLiteral("%1|layername=%2")
                    .arg(realOut, layerName);
                opt.qgisIface->addVectorLayer(uri, meta.productName, QStringLiteral("ogr"));
            }
            else
            {
                opt.qgisIface->addVectorLayer(realOut, meta.productName, QStringLiteral("ogr"));
            }
        }
        return r;
    }

    r.error = QStringLiteral("该类型产品不支持空间裁剪（仅支持矢量/栅格）");
    return r;
}
} // namespace
} // namespace

// ============================================================
// 对外入口
// ============================================================
QList<ClipResult> MapProductClipEngine::clipProducts(
    const QList<ProductMetadata>& products, const ClipOptions& opt)
{
    QList<ClipResult> results;
    const int total = products.size();
    // 合并写入单一容器文件时，标记容器是否已由前面的矢量成果建立：
    // 首个矢量成果整体新建文件，其余以"追加图层"方式写入同一文件
    bool containerWritten = false;
    for (int i = 0; i < total; ++i)
    {
        const ProductMetadata& meta = products.at(i);
        // 组装带"第 x/y 个成果：名称"前缀的阶段文字，转发给外部进度回调
        const std::function<bool(const QString&)> report =
            [&opt, &meta, i, total](const QString& stage) -> bool
            {
                if (!opt.progress) return true;
                const QString msg = QStringLiteral("正在导出第 %1/%2 个成果：%3\n%4")
                    .arg(i + 1).arg(total).arg(meta.productName).arg(stage);
                return opt.progress(i, total, msg);
            };

        ClipResult r = processProduct(meta, opt, containerWritten, report);
        results << r;
        // 合并模式下，矢量成果成功写出后容器文件即已建立
        if (r.ok && opt.mergeLayersIntoOneFile && meta.productType == ProductType::Vector)
            containerWritten = true;
        // 用户取消（中途或当前成果刚取消）：立即中止后续导出
        if (r.error == QStringLiteral("已取消"))
            break;
    }
    return results;
}
