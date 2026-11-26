#include "unit_manager.h"
#include "database/service_dao.h"

// ============================================================================
// UnitManager 实现
// ============================================================================

UnitManager* UnitManager::instance()
{
	static UnitManager inst;
	return &inst;
}

QStringList UnitManager::unitNames()
{
	ServiceDAO dao;
	QStringList names;
	QList<QVariantMap> unitList = dao.getUnitList();
	for (const auto& u : unitList)
	{
		QString name = u.value("name").toString();
		if (!name.isEmpty())
			names.append(name);
	}
	return names;
}

void UnitManager::refresh()
{
	// 暂无缓存机制，每次直接从数据库查询
	(void)unitNames();
}
