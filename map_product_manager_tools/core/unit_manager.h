#pragma once
#include <QStringList>

/**
 * @brief 服务单位管理器（单例）
 * 提供单位名称列表，供 service_record_dialog 等界面使用。
 */
class UnitManager
{
public:
	static UnitManager* instance();

	/// 获取所有单位名称
	QStringList unitNames();

	/// 刷新缓存（当前每次查询数据库）
	void refresh();

private:
	UnitManager() = default;
	UnitManager(const UnitManager&) = delete;
	UnitManager& operator=(const UnitManager&) = delete;
};
