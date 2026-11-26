#ifndef THEME_DAO_H
#define THEME_DAO_H

#include <QObject>
#include <QList>
#include <QStringList>
#include "theme_record.h"

/**
 * @brief 主题管理数据访问对象
 *
 * 封装 themes / theme_contents 两张表的 CRUD 操作。
 */
class ThemeDAO : public QObject
{
	Q_OBJECT

public:
	explicit ThemeDAO(QObject* parent = nullptr);

	// ===== 主题管理 =====
	// 添加主题，成功返回新主题 ID，失败返回 -1
	int insertTheme(const ThemeRecord& theme);
	// 删除主题（同时删除其下所有主题内容），返回是否成功
	bool deleteTheme(int themeId);
	// 获取全部主题
	QList<ThemeRecord> getAllThemes();
	// 获取全部主题类别名称（用于"主题类别"下拉）
	QStringList getAllThemeNames();
	// 获取指定主题
	ThemeRecord getTheme(int themeId);
	// 获取全部数据类别（去重，用于添加主题时下拉）
	QStringList getDataCategories();
	// 判断主题名称是否已存在
	bool isThemeNameExists(const QString& name, int excludeId = -1);

	// ===== 主题内容管理 =====
	// 添加主题内容，成功返回新内容 ID，失败返回 -1
	int insertContent(const ThemeContentRecord& content);
	// 更新主题内容（按 content.id 更新名称与关联数据），返回是否成功
	bool updateContent(const ThemeContentRecord& content);
	// 删除主题内容
	bool deleteContent(int contentId);
	// 获取某主题下的全部内容
	QList<ThemeContentRecord> getContentsByTheme(int themeId);

signals:
	void operationCompleted(const QString& message);
	void operationFailed(const QString& operation, const QString& error);

private:
	QString m_lastError;
};

#endif // THEME_DAO_H
