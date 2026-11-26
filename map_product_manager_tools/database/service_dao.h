#ifndef SERVICE_DAO_H
#define SERVICE_DAO_H

#include <QObject>
#include <QList>
#include <QMap>
#include "service_record.h"

/**
 * @brief 地图服务管理数据访问对象
 */
class ServiceDAO : public QObject
{
	Q_OBJECT

public:
	explicit ServiceDAO(QObject* parent = nullptr);

	// ========== 服务记录管理 ==========
	bool insertRecord(ServiceRecord& record);
	bool updateRecord(const ServiceRecord& record);
	bool deleteRecord(int recordId);
	ServiceRecord getRecord(int recordId);
	QList<ServiceRecord> getAllRecords();

	// 多条件检索
	QList<ServiceRecord> searchRecords(const SearchCondition& cond);
	int countSearchResults(const SearchCondition& cond);

	// ========== 归档管理 ==========
	bool updateArchiveInfo(int recordId, const QString& archivePath, const QString& archiveFormat);

	// ========== 统计 ==========
	ServiceStatistics getStatistics(const QString& dateFrom, const QString& dateTo, const QString& freqGranularity = QStringLiteral("month"));

	/**
	 * @brief 动态统计 - 按指定时间粒度聚合，灵活选择统计列
	 * @param config 统计配置（粒度、启用的列、日期范围、图件种类）
	 * @return 动态统计行列表
	 */
	QList<DynamicStatsRow> getDynamicStatistics(const DynamicStatisticsConfig& config);

	// ========== 服务单号生成 ==========
	QString generateServiceNumber();

	// ========== 单位管理 ==========
	QList<QVariantMap> getUnitList();
	int getOrCreateUnitId(const QString& unitName);
	bool deleteUnit(int unitId);

	// ========== 通用字典管理（应用场景/服务等级/服务类别/材质等自定义选项） ==========
	QStringList getDictOptions(const QString& dictType);
	bool addDictOption(const QString& dictType, const QString& name);
	bool deleteDictOption(const QString& dictType, const QString& name);

	// ========== 获取某列的去重值（用于筛选下拉） ==========
	QStringList getDistinctValues(const QString& column, const QString& dateFrom = QString(), const QString& dateTo = QString());

	// ========== 地图装裱统计报表 ==========
	QList<MountingStatsRow> getMountingStatistics(const QString& dateFrom, const QString& dateTo);

	// ========== 装裱清单导出（图件个数>0 才记录） ==========
	QList<MountingStatsRow> getMountingLedger();

signals:
	void operationCompleted(const QString& message);
	void operationFailed(const QString& operation, const QString& error);

private:
	QString buildWhereClause(const SearchCondition& cond, QVariantList& outParams);
};

#endif // SERVICE_DAO_H
