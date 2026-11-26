#ifndef DYNAMIC_STATISTICS_DIALOG_H
#define DYNAMIC_STATISTICS_DIALOG_H

#include <QDialog>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QListWidget>
#include <QTableWidget>
#include "database/service_record.h"

namespace Ui {
	class DynamicStatisticsDialog;
}

class ServiceDAO;

/**
 * @brief 动态统计对话框
 *
 * 功能：
 * - 按年/月/季度灵活统计
 * - 统计列可灵活增删（用图数量、图件种类、覆盖区域、服务数量等）
 * - 图件种类可灵活修改（添加/删除/重命名）
 * - 导出 Excel / PDF / PNG
 */
class DynamicStatisticsDialog : public QDialog
{
	Q_OBJECT

public:
	explicit DynamicStatisticsDialog(QWidget* parent = nullptr);
	~DynamicStatisticsDialog();

private slots:
	// ---- 查询 ----
	void onRefreshClicked();

	// ---- 指标列选择 ----
	void onMetricItemChanged(QTreeWidgetItem* item, int column);
	void onSelectAllMetrics();
	void onDeselectAllMetrics();

	// ---- 图件种类管理 ----
	void onAddCategory();
	void onEditCategory();
	void onRemoveCategory();
	void onResetCategory();

	// ---- 导出 ----
	void onExportExcel();
	void onExportPdf();
	void onExportPng();

private:
	// ---- 初始化 ----
	void initUI();
	void initMetricTree();
	void initMapCategoryList();
	void loadMapCategories();

	// ---- 统计逻辑 ----
	DynamicStatisticsConfig buildConfig() const;
	QStringList getEnabledMetricKeys() const;
	void populateStatsTable(const QList<DynamicStatsRow>& rows);

	// ---- 导出辅助 ----
	void exportToFile(const QString& format);
	void setupTableForExport(QTableWidget& table) const;
	QString buildExportTitle() const;

	// ---- 工具 ----
	static QString getTimePeriodLabel(const QString& tp);

	Ui::DynamicStatisticsDialog* ui;
	ServiceDAO* m_dao;
	QList<MapCategoryItem> m_mapCategories;  // 当前图件种类列表
};

#endif // DYNAMIC_STATISTICS_DIALOG_H
