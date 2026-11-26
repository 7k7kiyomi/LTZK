#ifndef EXPORT_DATA_HELPER_H
#define EXPORT_DATA_HELPER_H

// 数据文件导出通用辅助函数
// 从 PostGIS 数据库将地图成果导出为本地文件：
// - 栅格 / 制图 / 文档等：从 BLOB（Large Object）导出
// - 矢量：从 PostGIS 图层表导出为 ESRI Shapefile
// 供成果检索与预览、主题管理等多个对话框复用，保证各处导出行为一致。

#include "database/product_metadata.h"
#include "database/postgis_connector.h"

#include <QString>
#include <QStringList>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QVariant>
#include <QMessageBox>
#include <QWidget>
#include <QObject>

#include "qgsvectorlayer.h"
#include "qgsvectorfilewriter.h"
#include "qgsdatasourceuri.h"
#include "qgscoordinatetransform.h"
#include "qgscoordinatereferencesystem.h"
#include "qgsmessagelog.h"
#include "qgis.h"

#include <libpq-fe.h>

namespace ExportDataHelper
{

// 覆盖确认弹窗。
// 返回 QMessageBox::StandardButton：Yes=覆盖，YesToAll=覆盖全部，No=跳过，NoToAll=跳过全部。
inline int confirmOverwrite(QWidget* parent, const QString& filePath)
{
	QMessageBox box(QMessageBox::Question, QObject::tr("文件已存在"),
		QObject::tr("目标文件已存在：\n%1\n\n是否覆盖？").arg(filePath),
		QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll,
		parent);
	box.setButtonText(QMessageBox::Yes, QObject::tr("覆盖"));
	box.setButtonText(QMessageBox::YesToAll, QObject::tr("全部覆盖"));
	box.setButtonText(QMessageBox::No, QObject::tr("跳过"));
	box.setButtonText(QMessageBox::NoToAll, QObject::tr("全部跳过"));
	return box.exec();
}

// 检查目标文件是否需要覆盖确认。
// overwriteMode 初始应为 QMessageBox::NoButton（每次导出会话开始时重置）。
// 返回 true 表示可以继续导出（覆盖或不存在），false 表示用户选择跳过。
inline bool checkOverwrite(QWidget* parent, const QString& filePath, int& overwriteMode)
{
	if (!QFile::exists(filePath))
		return true;
	if (overwriteMode == QMessageBox::NoToAll)
		return false;          // 已选择"全部跳过"
	if (overwriteMode == QMessageBox::YesToAll)
		return true;           // 已选择"全部覆盖"
	overwriteMode = confirmOverwrite(parent, filePath);
	return (overwriteMode == QMessageBox::Yes || overwriteMode == QMessageBox::YesToAll);
}

// 导出单个成果到指定目录。
// 成功返回 true；文件已存在且用户选择跳过时返回 false（不视为失败）。
inline bool exportProduct(QWidget* parent, const ProductMetadata& meta, const QString& outputDir, int& overwriteMode)
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected())
		return false;

	QDir dir;
	if (!dir.mkpath(outputDir))
		return false;

	QString outputFilePath;

	if (meta.productType == ProductType::Raster)
	{
		// 栅格数据：优先从 BLOB（Large Object）导出
		int rasterOid = meta.fileOid;
		// 老数据 fileOid 可能为 0，从栅格元数据表回退查询
		if (rasterOid <= 0 && !meta.layerTableName.isEmpty())
		{
			QVariantMap row = db->executeQueryOne(
				QString("SELECT file_oid FROM \"%1\" LIMIT 1").arg(meta.layerTableName));
			if (!row.isEmpty())
				rasterOid = row.value("file_oid").toInt();
		}

		if (rasterOid <= 0)
		{
			QgsMessageLog::logMessage(
				QString::fromUtf8("栅格数据导出失败: BLOB 不可用 (%1)").arg(meta.productName),
				QString::fromUtf8("成果仓"), Qgis::Warning);
			return false;
		}

		outputFilePath = outputDir + "/" + meta.productName + "." + meta.fileFormat.toLower();
		if (!checkOverwrite(parent, outputFilePath, overwriteMode))
			return false;

		if (!PostgisConnector::instance()->loExport(rasterOid, outputFilePath))
		{
			QgsMessageLog::logMessage(
				QString::fromUtf8("栅格 BLOB 导出失败: OID=%1, 错误=%2")
					.arg(rasterOid).arg(PostgisConnector::instance()->lastError()),
				QString::fromUtf8("成果仓"), Qgis::Warning);
			return false;
		}
	}
	else if (meta.productType == ProductType::Vector)
	{
		// 矢量数据：从 PostGIS 图层表导出
		if (meta.layerTableName.isEmpty())
		{
			QgsMessageLog::logMessage(
				QString::fromUtf8("矢量数据导出失败: 未入库 PostGIS (%1)").arg(meta.productName),
				QString::fromUtf8("成果仓"), Qgis::Warning);
			return false;
		}

		PGconn* conn = db->nativeConnection();
		QString host = QString::fromUtf8(PQhost(conn));
		QString port = QString::fromUtf8(PQport(conn));
		QString dbName = QString::fromUtf8(PQdb(conn));
		QString user = QString::fromUtf8(PQuser(conn));
		QString password = QString::fromUtf8(PQpass(conn));

		// 查询表实际所在的 schema（PostgreSQL 不加双引号建表时标识符自动转小写，用 LOWER() 匹配）
		QString actualSchema = db->searchPath();
		if (actualSchema.isEmpty()) actualSchema = "public";

		QString findSchemaSql = QString(
			"SELECT table_schema, table_name FROM information_schema.tables "
			"WHERE LOWER(table_name) = LOWER('%1') LIMIT 1")
			.arg(meta.layerTableName);
		auto schemaResult = db->executeQueryOne(findSchemaSql);
		QString foundSchema = schemaResult.value("table_schema").toString();
		QString realTableName = schemaResult.value("table_name").toString();
		if (!foundSchema.isEmpty())
			actualSchema = foundSchema;
		QString effectiveTableName = realTableName.isEmpty() ? meta.layerTableName.toLower() : realTableName;

		QgsDataSourceUri dsUri;
		dsUri.setConnection(host, port, dbName, user, password);
		dsUri.setDataSource(actualSchema, effectiveTableName, "geom", QString(), QString());
		dsUri.setUseEstimatedMetadata(true);

		QgsVectorLayer::LayerOptions layerOptions;
		QgsVectorLayer* vectorLayer = new QgsVectorLayer(
			dsUri.uri(), meta.layerTableName, "YOUR_PASSWORD", layerOptions);
		if (!vectorLayer->isValid())
		{
			delete vectorLayer;
			return false;
		}

		// 导出文件路径（不手动追加 .shp，由 QgsVectorFileWriter 根据驱动自动补全）
		QString vectorBasePath = outputDir + "/" + meta.productName;
		QString shpPath = vectorBasePath + ".shp";
		if (!checkOverwrite(parent, shpPath, overwriteMode))
		{
			delete vectorLayer;
			return false;
		}

		// 先删除旧文件
		QStringList exts = { ".shp", ".shx", ".dbf", ".prj", ".cpg", ".qpj" };
		for (const auto& ext : exts)
			QFile::remove(outputDir + "/" + meta.productName + ext);

		QgsVectorFileWriter::SaveVectorOptions options;
		options.driverName = "ESRI Shapefile";
		options.layerName = meta.layerTableName;
		options.fileEncoding = "UTF-8";
		options.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteFile;

		QgsCoordinateReferenceSystem crs = vectorLayer->crs();
		QgsCoordinateTransform ct;
		if (crs.isValid())
			options.ct = ct;  // 无坐标转换

		QString errorMsg;
		QString newFilename;
		QgsVectorFileWriter::WriterError writerError = QgsVectorFileWriter::writeAsVectorFormatV3(
			vectorLayer, vectorBasePath, vectorLayer->transformContext(), options, &errorMsg, &newFilename);
		delete vectorLayer;

		if (writerError != QgsVectorFileWriter::NoError)
			return false;

		outputFilePath = newFilename.isEmpty() ? vectorBasePath + ".shp" : newFilename;
	}
	else
	{
		// 其他类型（CAD, AI, CDR, PDF, Document, Archive, Other）：从 BLOB 导出
		if (meta.fileOid <= 0)
		{
			QgsMessageLog::logMessage(
				QString::fromUtf8("导出失败: BLOB 不可用 (%1)").arg(meta.productName),
				QString::fromUtf8("成果仓"), Qgis::Critical);
			return false;
		}

		QString fileName;
		QFileInfo fi(meta.filePath);
		fileName = fi.fileName().isEmpty()
			? meta.productName + "." + meta.fileFormat.toLower()
			: fi.fileName();
		outputFilePath = outputDir + "/" + fileName;

		if (!checkOverwrite(parent, outputFilePath, overwriteMode))
			return false;

		if (!PostgisConnector::instance()->loExport(meta.fileOid, outputFilePath))
		{
			QgsMessageLog::logMessage(
				QString::fromUtf8("BLOB lo_export 失败: %1, OID=%2, 错误=%3")
					.arg(fileName).arg(meta.fileOid)
					.arg(PostgisConnector::instance()->lastError()),
				QString::fromUtf8("成果仓"), Qgis::Warning);
			return false;
		}
	}

	return true;
}

// 批量导出成果，返回成功数量（跳过覆盖的文件不算失败）。
inline int exportProducts(QWidget* parent, const QList<ProductMetadata>& products, const QString& outputDir)
{
	int overwriteMode = QMessageBox::NoButton;
	int success = 0;
	for (const auto& meta : products)
	{
		if (exportProduct(parent, meta, outputDir, overwriteMode))
			++success;
	}
	return success;
}

} // namespace ExportDataHelper

#endif // EXPORT_DATA_HELPER_H
