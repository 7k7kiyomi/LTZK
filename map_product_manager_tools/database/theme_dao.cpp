#include "theme_dao.h"
#include "postgis_connector.h"
#include <qgis.h>
#include <qgsmessagelog.h>

namespace {
QString themeLogPrefix()
{
	return QStringLiteral("[ThemeDAO]");
}
} // namespace

ThemeDAO::ThemeDAO(QObject* parent)
	: QObject(parent)
{
}

// ==================== 主题管理 ====================

int ThemeDAO::insertTheme(const ThemeRecord& theme)
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected())
	{
		QgsMessageLog::logMessage(
			themeLogPrefix() + QStringLiteral(" 数据库未连接，无法添加主题"),
			QStringLiteral("MapProductTools"), Qgis::Warning);
		emit operationFailed(QStringLiteral("添加主题"), QStringLiteral("数据库未连接"));
		return -1;
	}

	QString sql = R"(
		INSERT INTO themes (name, data_category, created_by)
		VALUES ($1, $2, $3)
		RETURNING id
	)";
	QVariantList params = {
		theme.name.trimmed(),
		theme.dataCategory.trimmed(),
		theme.createdBy.isEmpty() ? QStringLiteral("system") : theme.createdBy
	};
	auto rows = db->executeQuery(sql, params);
	if (rows.isEmpty())
	{
		QgsMessageLog::logMessage(
			themeLogPrefix() + QStringLiteral(" 插入主题失败: %1").arg(db->lastError()),
			QStringLiteral("MapProductTools"), Qgis::Critical);
		emit operationFailed(QStringLiteral("添加主题"), db->lastError());
		return -1;
	}
	const int newId = rows.first().toMap().value("id").toInt();
	emit operationCompleted(QStringLiteral("主题已添加：%1").arg(theme.name));
	return newId;
}

bool ThemeDAO::deleteTheme(int themeId)
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return false;

	// 先删除该主题下的所有内容
	db->executeNonQuery(QStringLiteral("DELETE FROM theme_contents WHERE theme_id = %1").arg(themeId));
	// 再删除主题本身
	if (!db->executeNonQuery(QStringLiteral("DELETE FROM themes WHERE id = %1").arg(themeId)))
	{
		emit operationFailed(QStringLiteral("删除主题"), db->lastError());
		return false;
	}
	emit operationCompleted(QStringLiteral("主题已删除"));
	return true;
}

QList<ThemeRecord> ThemeDAO::getAllThemes()
{
	QList<ThemeRecord> list;
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return list;

	auto rows = db->executeQuery(
		QStringLiteral("SELECT id, name, data_category, created_by, created_at FROM themes ORDER BY id"));
	for (const auto& row : rows)
	{
		auto map = row.toMap();
		ThemeRecord t;
		t.id = map.value("id").toInt();
		t.name = map.value("name").toString();
		t.dataCategory = map.value("data_category").toString();
		t.createdBy = map.value("created_by").toString();
		t.createdAt = map.value("created_at").toDateTime();
		list.append(t);
	}
	return list;
}

QStringList ThemeDAO::getAllThemeNames()
{
	QStringList names;
	const auto themes = getAllThemes();
	for (const auto& t : themes)
		names << t.name;
	names.removeDuplicates();
	return names;
}

ThemeRecord ThemeDAO::getTheme(int themeId)
{
	ThemeRecord t;
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected() || themeId <= 0) return t;

	auto row = db->executeQueryOne(
		QStringLiteral("SELECT id, name, data_category, created_by, created_at FROM themes WHERE id = %1").arg(themeId));
	if (row.isEmpty()) return t;
	t.id = row.value("id").toInt();
	t.name = row.value("name").toString();
	t.dataCategory = row.value("data_category").toString();
	t.createdBy = row.value("created_by").toString();
	t.createdAt = row.value("created_at").toDateTime();
	return t;
}

QStringList ThemeDAO::getDataCategories()
{
	QStringList cats;
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return cats;

	auto rows = db->executeQuery(
		QStringLiteral("SELECT DISTINCT data_category FROM themes WHERE data_category <> '' ORDER BY data_category"));
	for (const auto& row : rows)
		cats << row.toMap().value("data_category").toString();
	return cats;
}

bool ThemeDAO::isThemeNameExists(const QString& name, int excludeId)
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return false;
	QString sql = QStringLiteral("SELECT COUNT(*) AS c FROM themes WHERE name = '%1'")
		.arg(name.trimmed().replace(QStringLiteral("'"), QStringLiteral("''")));
	if (excludeId > 0)
		sql += QStringLiteral(" AND id <> %1").arg(excludeId);
	auto row = db->executeQueryOne(sql);
	return !row.isEmpty() && row.value("c").toInt() > 0;
}

// ==================== 主题内容管理 ====================

int ThemeDAO::insertContent(const ThemeContentRecord& content)
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected())
	{
		emit operationFailed(QStringLiteral("添加主题内容"), QStringLiteral("数据库未连接"));
		return -1;
	}

	QString sql = R"(
		INSERT INTO theme_contents (theme_id, content_name, product_ids, product_names, created_by)
		VALUES ($1, $2, $3, $4, $5)
		RETURNING id
	)";
	QVariantList params = {
		content.themeId,
		content.contentName.trimmed(),
		content.productIdsStr,
		content.productNamesStr,
		content.createdBy.isEmpty() ? QStringLiteral("system") : content.createdBy
	};
	auto rows = db->executeQuery(sql, params);
	if (rows.isEmpty())
	{
		emit operationFailed(QStringLiteral("添加主题内容"), db->lastError());
		return -1;
	}
	const int newId = rows.first().toMap().value("id").toInt();
	emit operationCompleted(QStringLiteral("主题内容已添加：%1").arg(content.contentName));
	return newId;
}

bool ThemeDAO::updateContent(const ThemeContentRecord& content)
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected() || content.id <= 0)
	{
		emit operationFailed(QStringLiteral("更新主题内容"), QStringLiteral("数据库未连接"));
		return false;
	}

	QString sql = QStringLiteral(
		"UPDATE theme_contents SET content_name = $1, product_ids = $2, product_names = $3 WHERE id = $4");
	QVariantList params = {
		content.contentName.trimmed(),
		content.productIdsStr,
		content.productNamesStr,
		content.id
	};
	if (!db->executeNonQuery(sql, params))
	{
		QgsMessageLog::logMessage(
			themeLogPrefix() + QStringLiteral(" 更新主题内容失败: %1").arg(db->lastError()),
			QStringLiteral("MapProductTools"), Qgis::Critical);
		emit operationFailed(QStringLiteral("更新主题内容"), db->lastError());
		return false;
	}
	emit operationCompleted(QStringLiteral("主题内容已更新：%1").arg(content.contentName));
	return true;
}

bool ThemeDAO::deleteContent(int contentId)
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected()) return false;
	if (!db->executeNonQuery(QStringLiteral("DELETE FROM theme_contents WHERE id = %1").arg(contentId)))
	{
		emit operationFailed(QStringLiteral("删除主题内容"), db->lastError());
		return false;
	}
	emit operationCompleted(QStringLiteral("主题内容已删除"));
	return true;
}

QList<ThemeContentRecord> ThemeDAO::getContentsByTheme(int themeId)
{
	QList<ThemeContentRecord> list;
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected() || themeId <= 0) return list;

	auto rows = db->executeQuery(QStringLiteral(
		"SELECT id, theme_id, content_name, product_ids, product_names, created_by, created_at "
		"FROM theme_contents WHERE theme_id = %1 ORDER BY id").arg(themeId));
	for (const auto& row : rows)
	{
		auto map = row.toMap();
		ThemeContentRecord c;
		c.id = map.value("id").toInt();
		c.themeId = map.value("theme_id").toInt();
		c.contentName = map.value("content_name").toString();
		c.productIdsStr = map.value("product_ids").toString();
		c.productNamesStr = map.value("product_names").toString();
		c.createdBy = map.value("created_by").toString();
		c.createdAt = map.value("created_at").toDateTime();

		// 解析逗号分隔串
		const QStringList ids = c.productIdsStr.split(',', Qt::SkipEmptyParts);
		for (const auto& idStr : ids)
			c.productIds << idStr.trimmed();
		const QStringList names = c.productNamesStr.split(',', Qt::SkipEmptyParts);
		for (const auto& nm : names)
			c.productNames << nm.trimmed();
		list.append(c);
	}
	return list;
}
