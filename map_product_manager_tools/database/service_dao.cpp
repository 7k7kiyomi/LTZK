#include "service_dao.h"
#include "postgis_connector.h"
#include <qgis.h>
#include <qgsmessagelog.h>
#include <QUuid>
#include <QDateTime>

// ServiceDAO 实现

// 生成"把服务形式 JSON 列规范化为 items 数组"的 SQL 表达式。
// 兼容三种存储格式：
//  - 新格式 ServiceFormDetail 数组：[{"mapName":...,"items":[...]}, ...] → 聚合所有 items
//  - 旧格式单对象：{ "items":[...] } → 取 items
//  - 旧格式 CompilationItem 数组：[{"kind":...}, ...]（地图编制旧数据）→ 直接用
static QString svcFormItemsExpr(const QString& col)
{
	return QString(
		"CASE "
		" WHEN NULLIF(%1, '') IS NULL OR NULLIF(%1, '')::jsonb = '[]'::jsonb OR NULLIF(%1, '')::jsonb = '{}'::jsonb "
		"   THEN '[]'::jsonb "
		" WHEN jsonb_typeof(NULLIF(%1, '')::jsonb) = 'object' "
		"   THEN COALESCE(NULLIF(%1, '')::jsonb->'items', '[]'::jsonb) "
		" WHEN NULLIF(%1, '')::jsonb->0 ? 'items' "
		"   THEN COALESCE((SELECT jsonb_agg(elem) FROM ("
		"       SELECT jsonb_array_elements(f->'items') AS elem"
		"       FROM jsonb_array_elements(NULLIF(%1, '')::jsonb) AS f"
		"     ) t), '[]'::jsonb) "
		" ELSE NULLIF(%1, '')::jsonb "
		"END"
	).arg(col);
}

ServiceDAO::ServiceDAO(QObject* parent)
	: QObject(parent)
{
}

bool ServiceDAO::insertRecord(ServiceRecord& record)
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected())
	{
		QgsMessageLog::logMessage(
			QStringLiteral("[ServiceDAO::insertRecord] 数据库未连接！db=%1")
				.arg(db ? QStringLiteral("非空") : QStringLiteral("空指针")),
			QStringLiteral("MapProductTools"), Qgis::Critical);
		return false;
	}

	QgsMessageLog::logMessage(
		QStringLiteral("[ServiceDAO::insertRecord] 数据库已连接，开始插入服务单号=%1")
			.arg(record.serviceNumber),
		QStringLiteral("MapProductTools"), Qgis::Info);

	if (record.serviceNumber.isEmpty())
		record.serviceNumber = generateServiceNumber();

	QString sql = R"(
		INSERT INTO service_records (
			service_number, record_date, application_scenario, service_content, coverage_area,
			service_level, arranged_office, category, service_form, other_service_form,
			map_name, mapping_region, size, material, compilation_details,
			print_details, mounting_details, product_details, data_details, other_details,
			sheet_count, unit_id, unit_name, submit_time, receiver,
			status, remarks, created_by, updated_by,
			product_ids, product_names, approval_file_path, approval_file_oid
			) VALUES (
			$1, $2, $3, $4, $5,
			$6, $7, $8, $9, $10,
			$11, $12, $13, $14, $15,
			$16, $17, $18, $19, $20,
			$21, $22, $23, $24, $25,
			$26, $27, $28, $29,
			$30, $31, $32, $33
			) RETURNING id
	)";

	QVariantList params;
	params << record.serviceNumber
		<< record.recordDate
		<< record.applicationScenario
		<< record.serviceContent
		<< record.coverageArea
		<< record.serviceLevel
		<< record.arrangedOffice
		<< record.category
		<< record.serviceForm
		<< record.otherServiceForm
		<< record.mapName
		<< record.mappingRegion
		<< record.size
		<< record.material
		<< record.compilationDetails
		<< record.printDetails
		<< record.mountingDetails
		<< record.productDetails
		<< record.dataDetails
		<< record.otherDetails
		<< record.sheetCount
		<< record.unitId
		<< record.unitName
		<< record.submitTime
		<< record.receiver
		<< static_cast<int>(record.status)
		<< record.remarks
		<< (record.createdBy.isEmpty() ? QStringLiteral("system") : record.createdBy)
		<< (record.updatedBy.isEmpty() ? QStringLiteral("system") : record.updatedBy)
		<< record.productIds.join(QStringLiteral(","))
		<< record.productNames.join(QStringLiteral(","))
		<< record.approvalFilePath
	<< record.approvalFileOid;

	auto result = db->executeQueryOne(sql, params);
	if (!result.isEmpty())
	{
		record.id = result.value("id").toInt();
		QgsMessageLog::logMessage(
			QStringLiteral("[ServiceDAO::insertRecord] 插入成功，返回id=%1").arg(record.id),
			QStringLiteral("MapProductTools"), Qgis::Info);
		emit operationCompleted(QString::fromUtf8("服务单 %1 创建成功").arg(record.serviceNumber));
		return true;
	}

	QString err = db->lastError();
	QgsMessageLog::logMessage(
		QStringLiteral("[ServiceDAO::insertRecord] 插入失败！错误=%1").arg(err),
		QStringLiteral("MapProductTools"), Qgis::Critical);
	emit operationFailed(QString::fromUtf8("创建服务单"), err);
	return false;
}

bool ServiceDAO::updateRecord(const ServiceRecord& record)
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected())
	{
		QgsMessageLog::logMessage(
			QStringLiteral("[ServiceDAO::updateRecord] 数据库未连接！db=%1")
				.arg(db ? QStringLiteral("非空") : QStringLiteral("空指针")),
			QStringLiteral("MapProductTools"), Qgis::Critical);
		return false;
	}

	QgsMessageLog::logMessage(
		QStringLiteral("[ServiceDAO::updateRecord] 数据库已连接，开始更新id=%1").arg(record.id),
		QStringLiteral("MapProductTools"), Qgis::Info);

	QString sql = R"(
		UPDATE service_records SET
			record_date = $1,
			application_scenario = $2,
			service_content = $3,
			coverage_area = $4,
			service_level = $5,
			arranged_office = $6,
			category = $7,
			service_form = $8,
			other_service_form = $9,
			map_name = $10,
			mapping_region = $11,
			size = $12,
			material = $13,
			compilation_details = $14,
			print_details = $15,
			mounting_details = $16,
			product_details = $17,
			data_details = $18,
			other_details = $19,
			sheet_count = $20,
			unit_id = $21,
			unit_name = $22,
			submit_time = $23,
			receiver = $24,
			status = $25,
			remarks = $26,
			updated_by = $27,
			updated_at = CURRENT_TIMESTAMP,
			product_ids = $28,
			product_names = $29,
			approval_file_path = $30,
			approval_file_oid = $31
			WHERE id = $32
	)";

	QVariantList params;
	params << record.recordDate
		<< record.applicationScenario
		<< record.serviceContent
		<< record.coverageArea
		<< record.serviceLevel
		<< record.arrangedOffice
		<< record.category
		<< record.serviceForm
		<< record.otherServiceForm
		<< record.mapName
		<< record.mappingRegion
		<< record.size
		<< record.material
		<< record.compilationDetails
		<< record.printDetails
		<< record.mountingDetails
		<< record.productDetails
		<< record.dataDetails
		<< record.otherDetails
		<< record.sheetCount
		<< record.unitId
		<< record.unitName
		<< record.submitTime
		<< record.receiver
		<< static_cast<int>(record.status)
		<< record.remarks
		<< (record.updatedBy.isEmpty() ? QStringLiteral("system") : record.updatedBy)
		<< record.productIds.join(QStringLiteral(","))
		<< record.productNames.join(QStringLiteral(","))
		<< record.approvalFilePath
		<< record.approvalFileOid
		<< record.id; 

	bool ok = db->executeNonQuery(sql, params);
	if (!ok)
	{
		QString err = db->lastError();
		QgsMessageLog::logMessage(
			QStringLiteral("[ServiceDAO::updateRecord] 更新失败！id=%1, 错误=%2")
				.arg(record.id).arg(err),
			QStringLiteral("MapProductTools"), Qgis::Critical);
		emit operationFailed(QString::fromUtf8("更新服务单"), err);
	}
	else
	{
		QgsMessageLog::logMessage(
			QStringLiteral("[ServiceDAO::updateRecord] 更新成功，id=%1").arg(record.id),
			QStringLiteral("MapProductTools"), Qgis::Info);
	}
	return ok;
}

bool ServiceDAO::deleteRecord(int id)
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return false;

	QString sql = "DELETE FROM service_records WHERE id = $1";
	bool ok = db->executeNonQuery(sql, { id });
	return ok;
}

QString ServiceDAO::generateServiceNumber()
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected())
		return QStringLiteral("FW-%1").arg(QUuid::createUuid().toString(QUuid::Id128).left(10));

	QString today = QDateTime::currentDateTime().toString("yyyyMMdd");
	QString sql = QString(
		"SELECT COUNT(*) as cnt FROM service_records "
		"WHERE service_number LIKE 'FW-%1-%%'").arg(today);

	auto rows = db->executeQuery(sql);
	int cnt = 1;
	if (!rows.isEmpty() && !rows[0].toMap().isEmpty())
	{
		cnt = rows[0].toMap().value("cnt").toInt() + 1;
	}
	return QString("FW-%1-%2").arg(today).arg(cnt, 4, 10, QChar('0'));
}

QList<QVariantMap> ServiceDAO::getUnitList()
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return {};

	QString sql = "SELECT id, name FROM service_units ORDER BY name";
	auto rows = db->executeQuery(sql);
	QList<QVariantMap> result;
	for (const auto& row : rows)
		result.append(row.toMap());
	return result;
}

int ServiceDAO::getOrCreateUnitId(const QString& unitName)
{
	if (unitName.trimmed().isEmpty()) return 0;

	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return 0;

	QString findSql = "SELECT id FROM service_units WHERE name = $1";
	auto rows = db->executeQuery(findSql, { unitName.trimmed() });
	if (!rows.isEmpty() && !rows[0].toMap().isEmpty())
	{
		return rows[0].toMap().value("id").toInt();
	}

	QString insertSql = "INSERT INTO service_units (name) VALUES ($1) RETURNING id";
	auto result = db->executeQueryOne(insertSql, { unitName.trimmed() });
	if (!result.isEmpty())
	{
		return result.value("id").toInt();
	}
	return 0;
}

bool ServiceDAO::deleteUnit(int unitId)
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return false;

	QString sql = "DELETE FROM service_units WHERE id = $1";
	return db->executeNonQuery(sql, { unitId });
}

QStringList ServiceDAO::getDictOptions(const QString& dictType)
{
	QStringList result;
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return result;

	QString sql = "SELECT name FROM service_dict WHERE dict_type = $1 ORDER BY id";
	auto rows = db->executeQuery(sql, { dictType });
	for (const auto& row : rows)
	{
		QString name = row.toMap().value("name").toString();
		if (!name.isEmpty())
			result.append(name);
	}
	return result;
}

bool ServiceDAO::addDictOption(const QString& dictType, const QString& name)
{
	QString trimmed = name.trimmed();
	if (dictType.isEmpty() || trimmed.isEmpty()) return false;

	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return false;

	// 已存在则不重复插入
	QString findSql = "SELECT id FROM service_dict WHERE dict_type = $1 AND name = $2";
	auto rows = db->executeQuery(findSql, { dictType, trimmed });
	if (!rows.isEmpty() && !rows[0].toMap().isEmpty())
		return true;

	QString insertSql = "INSERT INTO service_dict (dict_type, name) VALUES ($1, $2)";
	return db->executeNonQuery(insertSql, { dictType, trimmed });
}

bool ServiceDAO::deleteDictOption(const QString& dictType, const QString& name)
{
	QString trimmed = name.trimmed();
	if (dictType.isEmpty() || trimmed.isEmpty()) return false;

	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return false;

	QString sql = "DELETE FROM service_dict WHERE dict_type = $1 AND name = $2";
	return db->executeNonQuery(sql, { dictType, trimmed });
}

bool ServiceDAO::updateArchiveInfo(int id, const QString& path, const QString& format)
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return false;

	QString sql = "UPDATE service_records SET archive_path = $1, archive_format = $2, updated_at = CURRENT_TIMESTAMP WHERE id = $3";
	bool ok = db->executeNonQuery(sql, { path, format, id });
	if (ok)
	{
		emit operationCompleted(QString::fromUtf8("归档信息更新成功"));
	}
	return ok;
}

// ============================================================================
// 辅助: 将 QSqlRow 映射到 ServiceRecord
// ============================================================================
static ServiceRecord mapToServiceRecord(const QVariantMap& m)
{
	ServiceRecord rec;
	rec.id = m.value("id").toInt();
	rec.serviceNumber = m.value("service_number").toString();
	rec.recordDate = m.value("record_date").toString();
	rec.applicationScenario = m.value("application_scenario").toString();
	rec.serviceContent = m.value("service_content").toString();
	rec.coverageArea = m.value("coverage_area").toString();
	rec.serviceLevel = m.value("service_level").toString();
	rec.arrangedOffice = m.value("arranged_office").toString();
	rec.category = m.value("category").toString();
	rec.serviceForm = m.value("service_form").toString();
	rec.otherServiceForm = m.value("other_service_form").toString();
	rec.mapName = m.value("map_name").toString();
	rec.mappingRegion = m.value("mapping_region").toString();
	rec.size = m.value("size").toString();
	rec.material = m.value("material").toString();
	rec.compilationDetails = m.value("compilation_details").toString();
	rec.printDetails = m.value("print_details").toString();
	rec.mountingDetails = m.value("mounting_details").toString();
	rec.productDetails = m.value("product_details").toString();
	rec.dataDetails = m.value("data_details").toString();
	rec.otherDetails = m.value("other_details").toString();
	rec.sheetCount = m.value("sheet_count").toInt();
	rec.unitId = m.value("unit_id").toInt();
	rec.unitName = m.value("unit_name").toString();
	rec.submitTime = m.value("submit_time").toString();
	rec.receiver = m.value("receiver").toString();
	rec.status = static_cast<ServiceStatus>(m.value("status").toInt());
	rec.remarks = m.value("remarks").toString();
	rec.funding = m.value("funding").toDouble();
	rec.archivePath = m.value("archive_path").toString();
	rec.archiveFormat = m.value("archive_format").toString();
	rec.productIdsStr = m.value("product_ids").toString();
	rec.productNamesStr = m.value("product_names").toString();
	if (!rec.productIdsStr.isEmpty())
		rec.productIds = rec.productIdsStr.split(QStringLiteral(","), Qt::SkipEmptyParts);
	if (!rec.productNamesStr.isEmpty())
		rec.productNames = rec.productNamesStr.split(QStringLiteral(","), Qt::SkipEmptyParts);
	rec.approvalFilePath = m.value("approval_file_path").toString();
	rec.approvalFileOid = m.value("approval_file_oid", "0").toString().toInt();
	rec.createdAt = m.value("created_at").toDateTime();
	rec.createdBy = m.value("created_by").toString();
	rec.updatedBy = m.value("updated_by").toString();
	rec.updatedAt = m.value("updated_at").toDateTime();
	return rec;
}

// 默认 SELECT 列（不依赖表结构变化）
static QString defaultSelectColumns()
{
	return QString(
		"id, service_number, record_date, application_scenario, service_content, coverage_area, "
		"service_level, arranged_office, category, service_form, other_service_form, "
		"map_name, mapping_region, size, material, compilation_details, "
		"print_details, mounting_details, product_details, data_details, other_details, "
		"sheet_count, unit_id, unit_name, submit_time, receiver, "
		"status, remarks, funding, archive_path, archive_format, "
		"product_ids, product_names, approval_file_path, approval_file_oid, "
		"created_by, created_at, updated_by, updated_at");
}

// ============================================================================
// 搜索与统计
// ============================================================================

QList<ServiceRecord> ServiceDAO::searchRecords(const SearchCondition& cond)
{
	QList<ServiceRecord> results;
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return results;

	QVariantList params;
	QString where = buildWhereClause(cond, params);

	int paramIdx = params.size() + 1;

	QString sql = QString(
		"SELECT %1 "
		"FROM service_records %2 "
		"ORDER BY id DESC LIMIT $%3 OFFSET $%4")
		.arg(defaultSelectColumns(), where)
		.arg(paramIdx).arg(paramIdx + 1);

	params << cond.limit << cond.offset;

	auto rows = db->executeQuery(sql, params);
	for (const auto& row : rows)
	{
		results.append(mapToServiceRecord(row.toMap()));
	}
	return results;
}

int ServiceDAO::countSearchResults(const SearchCondition& cond)
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return 0;

	QVariantList params;
	QString where = buildWhereClause(cond, params);
	QString sql = QString("SELECT COUNT(*) as cnt FROM service_records %1").arg(where);

	auto rows = db->executeQuery(sql, params);
	if (!rows.isEmpty() && !rows[0].toMap().isEmpty())
		return rows[0].toMap().value("cnt").toInt();
	return 0;
}

ServiceStatistics ServiceDAO::getStatistics(const QString& dateFrom, const QString& dateTo, const QString& freqGranularity)
{
	ServiceStatistics stats;
	stats.dateFrom = dateFrom;
	stats.dateTo = dateTo;
	stats.frequencyGranularity = freqGranularity;

	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return stats;

	QString dateClause;
	QVariantList dateParams;
	int dpIdx = 1;
	if (!dateFrom.isEmpty() && !dateTo.isEmpty())
	{
		dateClause = QString(" WHERE record_date >= $%1 AND record_date <= $%2").arg(dpIdx).arg(dpIdx + 1);
		dateParams << dateFrom << dateTo;
	}
	else if (!dateFrom.isEmpty())
	{
		dateClause = QString(" WHERE record_date >= $%1").arg(dpIdx);
		dateParams << dateFrom;
	}
	else if (!dateTo.isEmpty())
	{
		dateClause = QString(" WHERE record_date <= $%1").arg(dpIdx);
		dateParams << dateTo;
	}

	// ---- 1. 服务单总数 ----
	{
		QString sql = "SELECT COUNT(*) as cnt FROM service_records" + dateClause;
		auto rows = db->executeQuery(sql, dateParams);
		if (!rows.isEmpty() && !rows[0].toMap().isEmpty())
			stats.totalRecords = rows[0].toMap().value("cnt").toInt();
	}

	// ---- 2. 图幅总数（跨6个服务形式Tab汇总所有 quantity 总和，与动态统计保持一致） ----
	// 6 个字段均为 ServiceFormDetail 数组（兼容旧单对象/CompilationItem数组）
	{
		auto quantitySubquery = [](const QString& colName) -> QString {
			return QString(
				"(SELECT COALESCE(SUM((item->>'quantity')::int), 0) "
				" FROM jsonb_array_elements(%1) AS item)"
			).arg(svcFormItemsExpr(colName));
		};

		const QStringList allSvcFormCols = {
			QStringLiteral("compilation_details"),
			QStringLiteral("print_details"),
			QStringLiteral("mounting_details"),
			QStringLiteral("product_details"),
			QStringLiteral("data_details"),
			QStringLiteral("other_details")
		};

		QStringList sumExprs;
		for (const auto& col : allSvcFormCols)
			sumExprs << quantitySubquery(col);

		QString sql = QString("SELECT COALESCE(SUM(%1), 0) as cnt FROM service_records%2")
			.arg(sumExprs.join(QStringLiteral(" + ")),
				 dateClause.isEmpty() ? QString() : (" " + dateClause));

		auto rows = db->executeQuery(sql, dateParams);
		if (!rows.isEmpty() && !rows[0].toMap().isEmpty())
			stats.totalSheetCount = rows[0].toMap().value("cnt").toInt();
	}

	// ---- 3. 服务单位数（去重，仅统计有实际服务数据的单位） ----
	{
		QString dataCheck = QStringLiteral(
			"("
			" (compilation_details IS NOT NULL AND compilation_details != '' AND compilation_details != '[]' AND compilation_details != '{}')"
			" OR (print_details IS NOT NULL AND print_details != '' AND print_details != '[]' AND print_details != '{}')"
			" OR (mounting_details IS NOT NULL AND mounting_details != '' AND mounting_details != '[]' AND mounting_details != '{}')"
			" OR (product_details IS NOT NULL AND product_details != '' AND product_details != '[]' AND product_details != '{}')"
			" OR (data_details IS NOT NULL AND data_details != '' AND data_details != '[]' AND data_details != '{}')"
			" OR (other_details IS NOT NULL AND other_details != '' AND other_details != '[]' AND other_details != '{}')"
			")"
		);

		QString dataFilter = QStringLiteral(" unit_name IS NOT NULL AND unit_name != '' AND ") + dataCheck;
		QString filter;
		if (dateClause.isEmpty())
			filter = QStringLiteral(" WHERE ") + dataFilter;
		else
			filter = dateClause + QStringLiteral(" AND ") + dataFilter;
		QString sql = "SELECT COUNT(DISTINCT unit_name) as cnt FROM service_records" + filter;
		auto rows = db->executeQuery(sql, dateParams);
		if (!rows.isEmpty() && !rows[0].toMap().isEmpty())
			stats.totalUnitCount = rows[0].toMap().value("cnt").toInt();
	}

	// ---- 4. 按图件种类统计（跨6个服务形式Tab对 quantity 求和，与动态统计保持一致） ----
	// 6 个字段均为 ServiceFormDetail 数组（兼容旧单对象/CompilationItem数组）
	{
		const QStringList allKindCols = {
			QStringLiteral("compilation_details"),
			QStringLiteral("print_details"),
			QStringLiteral("mounting_details"),
			QStringLiteral("product_details"),
			QStringLiteral("data_details"),
			QStringLiteral("other_details")
		};

		// 指标键 → 中文图件种类名
		QMap<QString, QString> kindNameMap = {
			{ QStringLiteral("单张图"),   QStringLiteral("单张图") },
			{ QStringLiteral("图集图册"), QStringLiteral("图集图册") },
			{ QStringLiteral("政务图包"), QStringLiteral("政务图包") },
			{ QStringLiteral("制图数据"), QStringLiteral("制图数据") },
			{ QStringLiteral("矢量数据"), QStringLiteral("矢量数据") },
			{ QStringLiteral("创意地图"), QStringLiteral("创意地图") },
			{ QStringLiteral("其他"),     QStringLiteral("其他") }
		};

		QString dateFilter = dateClause.isEmpty()
			? QStringLiteral(" WHERE record_date IS NOT NULL AND record_date != ''")
			: dateClause + QStringLiteral(" AND record_date IS NOT NULL AND record_date != ''");

		for (auto kit = kindNameMap.constBegin(); kit != kindNameMap.constEnd(); ++kit)
		{
			const QString& kindName = kit.key();
			QStringList sumExprs;

			for (const auto& col : allKindCols)
			{
				sumExprs << QString(
					"(SELECT COALESCE(SUM((item->>'quantity')::int), 0)"
					" FROM jsonb_array_elements(%1) AS item"
					" WHERE item->>'kind' = '%2')"
				).arg(svcFormItemsExpr(col), kindName);
			}

			// 不分组（无 GROUP BY），直接对全表 SUM，避免 GROUP BY 非分组列问题
			QString sql = QString("SELECT COALESCE(SUM(%1), 0) as cnt FROM service_records%2")
				.arg(sumExprs.join(QStringLiteral(" + ")), dateFilter);

			auto rows = db->executeQuery(sql, dateParams);
			int cnt = 0;
			if (!rows.isEmpty() && !rows[0].toMap().isEmpty())
				cnt = rows[0].toMap().value("cnt").toInt();
			stats.byMapType[kindName] = cnt;
		}
	}

	// ---- 5. 按服务单位统计 ----
	{
		QString filter = dateClause.isEmpty()
			? QStringLiteral(" WHERE unit_name IS NOT NULL AND unit_name != ''")
			: dateClause + QStringLiteral(" AND unit_name IS NOT NULL AND unit_name != ''");
		QString sql = "SELECT unit_name, COUNT(*) as cnt FROM service_records" + filter
			+ " GROUP BY unit_name ORDER BY cnt DESC";
		auto rows = db->executeQuery(sql, dateParams);
		for (const auto& row : rows)
		{
			auto m = row.toMap();
			stats.byUnit[m.value("unit_name").toString()] = m.value("cnt").toInt();
		}
	}

	// ---- 6. 按服务等级统计 ----
	{
		QString filter = dateClause.isEmpty()
			? QStringLiteral(" WHERE service_level IS NOT NULL AND service_level != ''")
			: dateClause + QStringLiteral(" AND service_level IS NOT NULL AND service_level != ''");
		QString sql = "SELECT service_level, COUNT(*) as cnt FROM service_records" + filter
			+ " GROUP BY service_level ORDER BY cnt DESC";
		auto rows = db->executeQuery(sql, dateParams);
		for (const auto& row : rows)
		{
			auto m = row.toMap();
			stats.byLevel[m.value("service_level").toString()] = m.value("cnt").toInt();
		}
	}

	// ---- 7. 按覆盖区域统计 ----
	{
		QString filter = dateClause.isEmpty()
			? QStringLiteral(" WHERE coverage_area IS NOT NULL AND coverage_area != ''")
			: dateClause + QStringLiteral(" AND coverage_area IS NOT NULL AND coverage_area != ''");
		QString sql = "SELECT coverage_area, COUNT(*) as cnt FROM service_records" + filter
			+ " GROUP BY coverage_area ORDER BY cnt DESC";
		auto rows = db->executeQuery(sql, dateParams);
		for (const auto& row : rows)
		{
			auto m = row.toMap();
			stats.byArea[m.value("coverage_area").toString()] = m.value("cnt").toInt();
		}
	}

	// ---- 8. 按类别统计 ----
	{
		QString filter = dateClause.isEmpty()
			? QStringLiteral(" WHERE category IS NOT NULL AND category != ''")
			: dateClause + QStringLiteral(" AND category IS NOT NULL AND category != ''");
		QString sql = "SELECT category, COUNT(*) as cnt FROM service_records" + filter
			+ " GROUP BY category ORDER BY cnt DESC";
		auto rows = db->executeQuery(sql, dateParams);
		for (const auto& row : rows)
		{
			auto m = row.toMap();
			stats.byCategory[m.value("category").toString()] = m.value("cnt").toInt();
		}
	}

	// ---- 9. 按服务形式统计（汇总各服务形式Tab中所有图件种类的 quantity 总和，与动态统计保持一致） ----
	{
		// 服务形式 → 对应列名映射
		struct SvcFormDef {
			QString label;
			QString colName;
		};
		const QList<SvcFormDef> svcForms = {
			{ QStringLiteral("地图编制"), QStringLiteral("compilation_details") },
			{ QStringLiteral("地图打印"), QStringLiteral("print_details") },
			{ QStringLiteral("地图装裱"), QStringLiteral("mounting_details") },
			{ QStringLiteral("地图成品"), QStringLiteral("product_details") },
			{ QStringLiteral("数据发送"), QStringLiteral("data_details") },
			{ QStringLiteral("其他"),     QStringLiteral("other_details") }
		};

		// 构建 SELECT 子句：每个服务形式一个 SUM(quantity) 聚合列
		QStringList selectParts;
		for (const auto& sf : svcForms)
		{
			// 规范化 items 数组（兼容单对象/CompilationItem数组/ServiceFormDetail数组）
			QString itemsExpr = svcFormItemsExpr(sf.colName);
			selectParts << QString(
				"COALESCE(SUM("
				"  (SELECT COALESCE(SUM((item->>'quantity')::int), 0) "
				"   FROM jsonb_array_elements(%1) AS item)"
				"), 0) AS \"%2\""
			).arg(itemsExpr, sf.colName);
		}

		QString sql = QString("SELECT %1 FROM service_records%2")
			.arg(selectParts.join(QStringLiteral(", ")),
				 dateClause.isEmpty() ? QString() : (" " + dateClause));

		auto rows = db->executeQuery(sql, dateParams);
		if (!rows.isEmpty())
		{
			auto m = rows[0].toMap();
			for (const auto& sf : svcForms)
			{
				stats.bySvcForm[sf.label] = m.value(sf.colName).toInt();
			}
		}
	}

	// ---- 10. 服务频次（支持 year / quarter / month） ----
	{
		QString timeExpr;
		if (freqGranularity == QStringLiteral("year"))
		{
			timeExpr = QStringLiteral("TO_CHAR(record_date::DATE, 'YYYY')");
		}
		else if (freqGranularity == QStringLiteral("quarter"))
		{
			timeExpr = QStringLiteral("TO_CHAR(record_date::DATE, 'YYYY') || '-Q' || TO_CHAR(record_date::DATE, 'Q')");
		}
		else // month
		{
			timeExpr = QStringLiteral("TO_CHAR(record_date::DATE, 'YYYY-MM')");
		}

		QString filter = dateClause.isEmpty()
			? QStringLiteral(" WHERE record_date IS NOT NULL AND record_date != ''")
			: dateClause + QStringLiteral(" AND record_date IS NOT NULL AND record_date != ''");
		QString sql = QString(
			"SELECT %1 as period, COUNT(*) as cnt "
			"FROM service_records%2 GROUP BY period ORDER BY period")
			.arg(timeExpr, filter);
		auto rows = db->executeQuery(sql, dateParams);
		for (const auto& row : rows)
		{
			auto m = row.toMap();
			stats.monthlyFrequency[m.value("period").toString()] = m.value("cnt").toInt();
		}
	}

	return stats;
}

// ============================================================================
// 动态统计
// ============================================================================

QList<DynamicStatsRow> ServiceDAO::getDynamicStatistics(const DynamicStatisticsConfig& config)
{
	QgsMessageLog::logMessage(
		QStringLiteral("[动态统计] 开始查询: 粒度=%1 启用的指标=%2 日期范围=%3~%4")
			.arg(config.granularity)
			.arg(config.enabledColumns.join(QStringLiteral(", ")))
			.arg(config.dateFrom.isEmpty() ? QStringLiteral("*") : config.dateFrom)
			.arg(config.dateTo.isEmpty() ? QStringLiteral("*") : config.dateTo),
		QStringLiteral("MapProductTools"), Qgis::Info);

	QList<DynamicStatsRow> results;
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected())
	{
		QgsMessageLog::logMessage(
			QStringLiteral("[动态统计] 数据库未连接，返回空结果"),
			QStringLiteral("MapProductTools"), Qgis::Warning);
		return results;
	}

	QString timeExpr;
	if (config.granularity == QStringLiteral("month"))
	{
		timeExpr = QStringLiteral("TO_CHAR(record_date::DATE, 'YYYY-MM')");
	}
	else if (config.granularity == QStringLiteral("quarter"))
	{
		timeExpr = QStringLiteral("TO_CHAR(record_date::DATE, 'YYYY') || '-Q' || TO_CHAR(record_date::DATE, 'Q')");
	}
	else
	{
		timeExpr = QStringLiteral("TO_CHAR(record_date::DATE, 'YYYY')");
	}

	QStringList whereParts;
	whereParts << QStringLiteral("record_date IS NOT NULL AND record_date != ''");

	QVariantList params;
	int paramIdx = 1;

	if (!config.dateFrom.isEmpty())
	{
		whereParts << QString("record_date >= $%1").arg(paramIdx++);
		params << config.dateFrom;
	}
	if (!config.dateTo.isEmpty())
	{
		whereParts << QString("record_date <= $%1").arg(paramIdx++);
		params << config.dateTo;
	}

	QString whereClause = QStringLiteral(" WHERE ") + whereParts.join(QStringLiteral(" AND "));

	QStringList selectParts;
	selectParts << timeExpr + QStringLiteral(" AS time_period");

	for (const QString& colKey : config.enabledColumns)
	{
		bool found = false;

		if (StatMetrics::isSpecialMetric(colKey))
		{
			// 图件种类: 汇总所有服务形式Tab中对应 kind 的 quantity 总和
			// 对每个服务单，从 compilation_details 数组 + 其余5个Tab的 items 数组中
			// 逐一提取 kind 匹配的条目，对其 quantity 求和
			if (colKey.startsWith(QStringLiteral("mapkind_")))
			{
				// 指标键 → 中文图件种类名
				QMap<QString, QString> kindNameMap = {
					{ StatMetrics::KEY_MAPKIND_SHEET,  QStringLiteral("单张图") },
					{ StatMetrics::KEY_MAPKIND_ATLAS,  QStringLiteral("图集图册") },
					{ StatMetrics::KEY_MAPKIND_GOV,    QStringLiteral("政务图包") },
					{ StatMetrics::KEY_MAPKIND_CARTO,  QStringLiteral("制图数据") },
					{ StatMetrics::KEY_MAPKIND_VECTOR, QStringLiteral("矢量数据") },
					{ StatMetrics::KEY_MAPKIND_CREATIVE, QStringLiteral("创意地图") },
					{ StatMetrics::KEY_MAPKIND_OTHER,  QStringLiteral("其他") }
				};
				QString kindName = kindNameMap.value(colKey);

				if (!kindName.isEmpty())
				{
					QStringList sumExprs;

					// 6 个服务形式字段均为 ServiceFormDetail 数组（兼容旧格式）
					QStringList allKindCols = {
						QStringLiteral("compilation_details"),
						QStringLiteral("print_details"),
						QStringLiteral("mounting_details"),
						QStringLiteral("product_details"),
						QStringLiteral("data_details"),
						QStringLiteral("other_details")
					};
					for (const auto& col : allKindCols)
					{
						sumExprs << QString(
							"(SELECT COALESCE(SUM((item->>'quantity')::int), 0)"
							" FROM jsonb_array_elements(%1) AS item"
							" WHERE item->>'kind' = '%2')"
						).arg(svcFormItemsExpr(col), kindName);
					}

					selectParts << QString("COALESCE(SUM(%1), 0) AS \"%2\"")
						.arg(sumExprs.join(QStringLiteral(" + ")), colKey);
					found = true;
				}
			}
			// 服务形式: 汇总对应 JSON 列中所有图件种类的 quantity 总和（6类图种合计）
			// 不对 service_form 做过滤——一个服务单可能同时在多个 Tab 中有数据
			// 每个 svcform_* 指标直接汇总自己对应 JSON 列中全部记录的 quantity
			else if (colKey.startsWith(QStringLiteral("svcform_")))
			{
				QString jsonCol = StatMetrics::svcformJsonColumn(colKey);
				if (!jsonCol.isEmpty())
				{
					// 6 个服务形式字段均为 ServiceFormDetail 数组，统一规范化 items 数组
					selectParts << QString(
						"COALESCE(SUM("
						"  (SELECT COALESCE(SUM((item->>'quantity')::int), 0) "
						"   FROM jsonb_array_elements(%1) AS item)"
						"), 0) AS \"%2\""
					).arg(svcFormItemsExpr(jsonCol), colKey);
					found = true;
				}
			}
			else  // covarea_count
			{
				auto pattern = StatMetrics::specialMetricPattern(colKey);
				if (!pattern.first.isEmpty())
				{
					selectParts << QString("COUNT(*) FILTER (WHERE %1 IS NOT NULL AND %1 != '') AS \"%2\"")
						.arg(pattern.first, colKey);
					found = true;
				}
			}
		}

		if (!found)
		{
			if (colKey == StatMetrics::KEY_RECORD_COUNT)
			{
				selectParts << QString("COUNT(*) AS %1").arg(colKey);
			}
			else if (colKey == StatMetrics::KEY_FUNDING)
			{
				selectParts << QString("COALESCE(SUM(funding), 0) AS %1").arg(colKey);
			}
			else
			{
				selectParts << QString("COALESCE(SUM(%1), 0) AS \"%2\"").arg(colKey, colKey);
			}
		}
	}

	QString sql = QString("SELECT %1 FROM service_records%2 GROUP BY time_period ORDER BY time_period")
		.arg(selectParts.join(QStringLiteral(", ")), whereClause);

	// 输出完整 SQL（动态统计 SQL 通常很长，截取前 2000 字符用于日志）
	QString sqlLog = sql.length() > 2000
		? sql.left(2000) + QStringLiteral("...[截断, 总长=%1]").arg(sql.length())
		: sql;
	QgsMessageLog::logMessage(
		QStringLiteral("[动态统计] 执行 SQL:\n%1").arg(sqlLog),
		QStringLiteral("MapProductTools"), Qgis::Info);

	auto rows = db->executeQuery(sql, params);

	// 检查是否有 SQL 错误
	QString sqlError = db->lastError();
	if (!sqlError.isEmpty())
	{
		QgsMessageLog::logMessage(
			QStringLiteral("[动态统计] 查询失败: %1").arg(sqlError.trimmed()),
			QStringLiteral("MapProductTools"), Qgis::Critical);
	}

	QgsMessageLog::logMessage(
		QStringLiteral("[动态统计] 查询返回 %1 行").arg(rows.size()),
		QStringLiteral("MapProductTools"), Qgis::Info);

	for (const auto& row : rows)
	{
		auto m = row.toMap();
		DynamicStatsRow dsr;
		dsr.timePeriod = m.value("time_period").toString();

		for (const QString& colKey : config.enabledColumns)
		{
			dsr.metrics[colKey] = m.value(colKey);
		}
		results.append(dsr);
	}

	return results;
}

QStringList ServiceDAO::getDistinctValues(const QString& column, const QString& dateFrom, const QString& dateTo)
{
	QStringList values;
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return values;

	QString colKey = column.split('.').last();

	QVariantList params;
	QString dateClause;
	int dpIdx = 1;
	if (!dateFrom.isEmpty() && !dateTo.isEmpty())
	{
		dateClause = QString(" AND record_date >= $%1 AND record_date <= $%2").arg(dpIdx).arg(dpIdx + 1);
		params << dateFrom << dateTo;
	}
	else if (!dateFrom.isEmpty())
	{
		dateClause = QString(" AND record_date >= $%1").arg(dpIdx);
		params << dateFrom;
	}
	else if (!dateTo.isEmpty())
	{
		dateClause = QString(" AND record_date <= $%1").arg(dpIdx);
		params << dateTo;
	}

	QString sql = QString("SELECT DISTINCT %1 FROM service_records WHERE %1 IS NOT NULL AND %1 != ''%2 ORDER BY %1")
		.arg(colKey, dateClause);

	auto rows = db->executeQuery(sql, params);
	for (const auto& row : rows)
	{
		auto m = row.toMap();
		QString val = m.value(colKey).toString().trimmed();
		if (!val.isEmpty())
			values.append(val);
	}
	return values;
}

ServiceRecord ServiceDAO::getRecord(int recordId)
{
	ServiceRecord rec;
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected())
	{
		QgsMessageLog::logMessage(
			QStringLiteral("[ServiceDAO::getRecord] 数据库未连接！recordId=%1").arg(recordId),
			QStringLiteral("MapProductTools"), Qgis::Critical);
		return rec;
	}

	QString sql = QString("SELECT %1 FROM service_records WHERE id = $1")
		.arg(defaultSelectColumns());

	auto result = db->executeQueryOne(sql, { recordId });
	if (!result.isEmpty())
	{
		rec = mapToServiceRecord(result);
		QgsMessageLog::logMessage(
			QStringLiteral("[ServiceDAO::getRecord] 查询成功 id=%1, 服务单号=%2")
				.arg(rec.id).arg(rec.serviceNumber),
			QStringLiteral("MapProductTools"), Qgis::Info);
	}
	else
	{
		QgsMessageLog::logMessage(
			QStringLiteral("[ServiceDAO::getRecord] 未查到记录 recordId=%1").arg(recordId),
			QStringLiteral("MapProductTools"), Qgis::Warning);
	}
	return rec;
}

QList<ServiceRecord> ServiceDAO::getAllRecords()
{
	SearchCondition cond;
	cond.limit = 10000;
	cond.offset = 0;
	return searchRecords(cond);
}

// ============================================================================
// 地图装裱统计 - 导出报表数据
// ============================================================================

QList<MountingStatsRow> ServiceDAO::getMountingStatistics(const QString& dateFrom, const QString& dateTo)
{
	QList<MountingStatsRow> results;
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return results;

	QString dateClause;
	QVariantList dateParams;
	int dpIdx = 1;
	if (!dateFrom.isEmpty() && !dateTo.isEmpty())
	{
		dateClause = QString(" AND record_date >= $%1 AND record_date <= $%2").arg(dpIdx).arg(dpIdx + 1);
		dateParams << dateFrom << dateTo;
	}
	else if (!dateFrom.isEmpty())
	{
		dateClause = QString(" AND record_date >= $%1").arg(dpIdx);
		dateParams << dateFrom;
	}
	else if (!dateTo.isEmpty())
	{
		dateClause = QString(" AND record_date <= $%1").arg(dpIdx);
		dateParams << dateTo;
	}

	// 筛选服务形式为"地图装裱"的记录
	QString sql = QString(
		"SELECT id, submit_time, unit_name, size, material, compilation_details "
		"FROM service_records "
		"WHERE service_form ILIKE '%%地图装裱%%'%1 "
		"ORDER BY submit_time, id").arg(dateClause);

	auto rows = db->executeQuery(sql, dateParams);
	int seq = 1;
	for (const auto& row : rows)
	{
		auto m = row.toMap();
		MountingStatsRow r;
		r.seq = seq++;
		r.submitTime = m.value("submit_time").toString();
		r.unitName = m.value("unit_name").toString();
		r.frameSpec = m.value("size").toString();
		r.material = m.value("material").toString();

		// 数量 = 各图件种类的 quantity 求和（兼容旧 CompilationItem 数组与新 ServiceFormDetail 数组）
		QString details = m.value("compilation_details").toString();
		QList<ServiceFormDetail> compileDs = compilationDetailsFromJson(details);
		int totalQty = 0;
		for (const auto& d : compileDs)
			for (const auto& item : d.items)
				totalQty += item.quantity;
		r.totalQuantity = totalQty;

		// 单价、金额、小计不预填，留空
		r.unitPrice = 0.0;
		r.amount = 0.0;
		r.subtotal = 0.0;

		results.append(r);
	}
	return results;
}

QList<MountingStatsRow> ServiceDAO::getMountingLedger()
{
	QList<MountingStatsRow> results;
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return results;

	// 筛选服务形式为"地图装裱"的记录，解析 mounting_details 获取尺寸/材质/图件数量
	QString sql = QString(
		"SELECT id, submit_time, unit_name, mounting_details "
		"FROM service_records "
		"WHERE service_form ILIKE '%%地图装裱%%' "
		"ORDER BY submit_time, id");

	auto rows = db->executeQuery(sql);
	int seq = 1;
	for (const auto& row : rows)
	{
		auto m = row.toMap();
		QString details = m.value("mounting_details").toString();
		// 兼容单对象旧数据与多记录数组
		QList<ServiceFormDetail> ds = serviceFormDetailsFromJson(details);

		// 图件个数 = 各图件种类 quantity 汇总，大于0才记录
		int totalQty = 0;
		for (const auto& d : ds)
			for (const auto& item : d.items)
				totalQty += item.quantity;
		if (totalQty <= 0)
			continue;

		// 图框规格/材质取第一条
		QString frameSpec, material;
		if (!ds.isEmpty())
		{
			frameSpec = ds.first().size;
			material = ds.first().material;
		}

		MountingStatsRow r;
		r.seq = seq++;
		r.submitTime = m.value("submit_time").toString();
		r.unitName = m.value("unit_name").toString();
		r.frameSpec = frameSpec;   // 图框规格（米）← 地图装裱的尺寸
		r.material = material;     // 材质 ← 地图装裱的材质
		r.totalQuantity = totalQty;
		results.append(r);
	}
	return results;
}

// ============================================================================
// 私有辅助函数
// ============================================================================

QString ServiceDAO::buildWhereClause(const SearchCondition& cond, QVariantList& outParams)
{
	outParams.clear();
	QStringList clauses;
	int idx = 1;

	if (!cond.keyword.isEmpty())
	{
		QString like = QStringLiteral("%%1%").arg(cond.keyword);
		clauses << QString("(service_number ILIKE $%1 OR unit_name ILIKE $%2 OR service_content ILIKE $%3 "
			"OR coverage_area ILIKE $%4 OR category ILIKE $%5 OR service_level ILIKE $%6 "
			"OR arranged_office ILIKE $%7 OR remarks ILIKE $%8)")
			.arg(idx).arg(idx+1).arg(idx+2).arg(idx+3).arg(idx+4).arg(idx+5).arg(idx+6).arg(idx+7);
		for (int i = 0; i < 8; ++i)
			outParams << like;
		idx += 8;
	}

	if (!cond.unitName.isEmpty())
	{
		clauses << QString("unit_name = $%1").arg(idx++);
		outParams << cond.unitName;
	}

	if (!cond.category.isEmpty())
	{
		clauses << QString("category = $%1").arg(idx++);
		outParams << cond.category;
	}

	if (!cond.serviceLevel.isEmpty())
	{
		clauses << QString("service_level = $%1").arg(idx++);
		outParams << cond.serviceLevel;
	}

	if (!cond.status.isEmpty())
	{
		clauses << QString("status = $%1").arg(idx++);
		outParams << cond.status.toInt();
	}

	if (!cond.dateFrom.isEmpty())
	{
		clauses << QString("record_date >= $%1").arg(idx++);
		outParams << cond.dateFrom;
	}

	if (!cond.dateTo.isEmpty())
	{
		clauses << QString("record_date <= $%1").arg(idx++);
		outParams << cond.dateTo;
	}

	if (clauses.isEmpty())
		return QString();

	return QStringLiteral(" WHERE ") + clauses.join(QStringLiteral(" AND "));
}
