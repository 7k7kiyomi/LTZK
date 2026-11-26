#ifndef MAP_SERVICE_DIALOG_H
#define MAP_SERVICE_DIALOG_H

#include <QDialog>
#include <QList>
#include <QTreeWidget>
#include <QTableWidget>

#include <QComboBox>
#include <QDateEdit>
#include <QPushButton>
#include <QSplitter>
#include <QGroupBox>
#include <QTabWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include "ui_map_service_dialog.h"
#include "database/service_record.h"
#include "chart_widget.h"

class ServiceDAO;
class QgisInterface;

/**
 * @brief 地图服务管理主对话框
 * 
 * 包含三大功能模块：
 * 1. 服务台账：电子化台账，按类别/服务对象/服务单位分类归档
 * 2. 统计分析：动态统计图幅数、图件种类、覆盖区域、服务频次等指标
 * 3. 报表导出：一键导出 Excel/PDF/PNG 三种格式
 */
class MapServiceDialog : public QDialog
{
	Q_OBJECT

public:
	explicit MapServiceDialog(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~MapServiceDialog() override;
	void setQgisInterface(QgisInterface* iface) { mQGisIface = iface; }

private slots:
	// 台账 Tab
	void onSearch();
	void onReset();
	void onAddRecord();
	void onEditRecord();
	void onDeleteRecord();
	void onArchiveRecord();
	void onManageUnits();
	void onRecordTableClicked(int row, int col);
	void onRecordTableContextMenu(const QPoint& pos);   // 关联成果列右键预览
	void onMountingLedgerExport();   // 装裱清单导出
	void onServiceLedgerExport();    // 服务台账导出

	// 统计分析 Tab
	void onRefreshStatistics();
	void onFrequencyGranularityChanged(int index);
	void onExportReportFromStats();

	// 动态统计 Tab
	void onDynamicStatRefresh();
	void onDynamicStatExportExcel();
	void onDynamicStatExportPdf();
	void onDynamicStatExportPng();
	void onDynamicMetricItemChanged(QTreeWidgetItem* item, int column);
	void onMountingStatExport();               // 地图装裱统计导出


	// 报表导出 Tab
	void onExportReport();
	void onViewApproval(int recordId);

private:
	void setupTableColumns();
	void setupConnections();
	void setupFilterCombos();
	void performSearch();
	void refreshRecordTable();
	void previewProduct(int productId);   // 预览单个关联成果（矢量/栅格地图显示，其它系统程序打开）
	void refreshStatistics(const ServiceStatistics& stats);
	void populateChartTable(QTableWidget* table, const QMap<QString, int>& data,
		const QString& col1Name, const QString& col2Name);

	// 动态统计 Tab——构建 UI 与数据刷新
	void setupDynamicStatsTab();
	void refreshDynamicStatsTable(const QList<DynamicStatsRow>& rows);
	void refreshDynamicChart();
	DynamicStatisticsConfig buildDynamicStatsConfig();

	// 导出实现
	void doExportReport(const QString& dateFrom, const QString& dateTo,
		bool exportExcel, bool exportPdf, bool exportPng, int chartType = 0);
	bool exportToExcel(const QString& filePath, const ServiceStatistics& stats);
	bool exportToPdf(const QString& filePath, const ServiceStatistics& stats, int chartType = 1);
	bool exportToPng(const QString& filePath, const ServiceStatistics& stats, int chartType = 0);
	int renderStatisticsToPainter(QPainter* painter, const QRect& rect, const ServiceStatistics& stats, int chartType = 0);
	void addExportHistory(const QString& fileName, const QString& format, const QString& filePath);
	void showApprovalDialog(const QString& filePath);

	ServiceDAO* mDAO = nullptr;
	QgisInterface* mQGisIface = nullptr;
	QList<ServiceRecord> mSearchResults;
	int mCurrentPage = 1;
	int mTotalPages = 1;
	int mTotalResults = 0;
	int mPageSize = 20;

	// 统计分析——频次粒度选择
	QComboBox* mFreqGranularityCombo = nullptr;

	// 动态统计 Tab 控件
	QComboBox*    mDynamicGranularityCombo = nullptr;
	QDateEdit*    mDynamicDateFrom = nullptr;
	QDateEdit*    mDynamicDateTo = nullptr;
	QPushButton*  mDynamicRefreshBtn = nullptr;
	QPushButton*  mDynamicExportExcelBtn = nullptr;
	QPushButton*  mDynamicExportPdfBtn = nullptr;
	QPushButton*  mDynamicExportPngBtn = nullptr;
	QPushButton*  mMountingExportBtn = nullptr;        // 地图装裱统计导出按钮
	QComboBox*    mDynamicChartTypeCombo = nullptr;
	QTreeWidget*  mDynamicMetricTree = nullptr;

	// 图表可视化 Tab 控件
	QTabWidget*   mDynamicResultTabs = nullptr;    // 右侧 TabWidget（表格 | 图表）
	QTableWidget* mDynamicStatsTable = nullptr;
	QStackedWidget* mDynamicChartStack = nullptr;  // 图表页面栈（bar tabs | pie widget）
	QTabWidget*   mDynamicBarTabs = nullptr;        // 柱状图：按指标分 Tab
	ChartWidget*  mPieChartWidget = nullptr;         // 饼状图：单页显示所有指标
	QComboBox*    mDynamicChartStyleCombo = nullptr; // 图表样式选择器
	QList<DynamicStatsRow> mDynamicStatsRows;          // 缓存原始数据供导出使用

	Ui::MapServiceDialog ui;
};

#endif // MAP_SERVICE_DIALOG_H
