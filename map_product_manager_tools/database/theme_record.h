#ifndef THEME_RECORD_H
#define THEME_RECORD_H

#include <QString>
#include <QDateTime>
#include <QStringList>

/**
 * @brief 主题记录结构
 *
 * 一个主题 = 数据类别（矢量/栅格/AI等）+ 主题类别名称。
 * 主题创建后可向其添加多组主题内容（每组内容关联若干产品数据）。
 */
struct ThemeRecord
{
	int id = -1;
	QString dataCategory;   // 数据类别（矢量数据/栅格数据/AI/CDR/PDF/CAD等）
	QString name;           // 主题类别名称
	QString createdBy;
	QDateTime createdAt;

	QString displayName() const
	{
		if (dataCategory.isEmpty())
			return name;
		return QStringLiteral("%1（%2）").arg(name, dataCategory);
	}
};

/**
 * @brief 主题内容记录结构
 *
 * 主题内容 = 用户为某一组数据输入的主题名称 + 勾选的若干产品数据。
 * 在主题管理视图中，选中某主题后，其目录下按"主题名称 → 数据"树形展示。
 */
struct ThemeContentRecord
{
	int id = -1;
	int themeId = -1;          // 所属主题 ID
	QString contentName;       // 主题名称（用户为这组数据输入的名称）
	QStringList productIds;    // 关联产品 ID 列表
	QStringList productNames;  // 关联产品名称列表
	QString productIdsStr;     // 逗号分隔 ID 串（数据库存储）
	QString productNamesStr;   // 逗号分隔名称串（数据库存储）
	QString createdBy;
	QDateTime createdAt;
};

#endif // THEME_RECORD_H
