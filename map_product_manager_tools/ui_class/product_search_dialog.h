#ifndef PRODUCT_SEARCH_DIALOG_H
#define PRODUCT_SEARCH_DIALOG_H

#include <QDialog>
#include <QGraphicsItem>
#include <QPen>
#include <QFont>
#include <QMetaObject>
#include <QTimer>
#include <QToolButton>
#include <QTabWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include "database/product_metadata.h"
#include "ui_product_search_dialog.h"
#include "qgsmapcanvasitem.h"

class ProductDAO;
class QgsMapCanvas;
class QgsMapToolPan;
class QgsVectorLayer;
class QgsRasterLayer;
class QgisInterface;

/**
 * @brief 经纬网装饰项，在 QgsMapCanvas 上绘制经纬线和标注
 * 
 * 自动根据画布 extent 计算经纬线位置，固定 4 条经线、3 条纬线。
 */
class GraticuleItem : public QgsMapCanvasItem
{
public:
	GraticuleItem(QgsMapCanvas* canvas);
	void paint(QPainter* painter) override;

private:
	QPen mLinePen;
	QPen mLabelPen;
	QFont mLabelFont;
};

/**
 * @brief 成果检索与预览对话框
 * 
 * 实现多条件组合检索、缩略图在线预览、查询结果导出功能。
 */
class ProductSearchDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ProductSearchDialog(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~ProductSearchDialog() override;

	void setQgisInterface(QgisInterface* iface) { mQGisIface = iface; }

private slots:
	void onSearch();
	void scheduleSearch();
	void onReset();
	void onExportResults();
	void onAddToMap();
	void onPreviewSelected();
	void onResultSelected(int row, int col);
	void onResultTableContextMenu(const QPoint& pos);
	void onPageChanged();

private:
	void setupConnections();
	void setupTableColumns();
	void performSearch();
	void updateProductDetail(const ProductMetadata& meta);
	void createDetailPanel();
	void toggleDetailPanel();
	void fillBasicMetadata(const ProductMetadata& meta);
	void fillSpecializedMetadata(const ProductMetadata& meta);
	bool exportDataFile(const ProductMetadata& meta, const QString& outputDir);
	enum OverwriteChoice { OverwriteYes, OverwriteAll, OverwriteNo, OverwriteCancel };
	OverwriteChoice confirmOverwrite(const QString& filePath);
	void clearPreview();
	void addGraticule();
	void ensureGeographicCRS();
	void autoPreview(const ProductMetadata& meta);

	// 收集结果表格当前选中行对应的成果（按行号升序、去重）
	QList<ProductMetadata> selectedProducts() const;

	// 数据
	QList<ProductMetadata> mSearchResults;
	int mCurrentPage = 1;
	int mTotalPages = 1;
	int mTotalResults = 0;
	int mPageSize = 20;
	
	ProductDAO* mDAO = nullptr;

	// QGIS 接口
	QgisInterface* mQGisIface = nullptr;

	// 地图预览画布
	QgsMapCanvas* mMapCanvas = nullptr;

	// 漫游工具
	QgsMapToolPan* mPanTool = nullptr;

	// 经纬网装饰
	GraticuleItem* mGraticule = nullptr;

	// 画布 extentsChanged 信号连接
	QMetaObject::Connection mExtentConnection;

	// 自动搜索防抖定时器
	QTimer* mDebounceTimer = nullptr;

	// 可折叠详细信息面板
	QToolButton* mDetailToggleBtn = nullptr;
	QWidget* mDetailContainer = nullptr;
	QTabWidget* mDetailTabWidget = nullptr;

	// 基础元数据 Tab：ScrollArea + 动态表单
	QScrollArea* mBasicScroll = nullptr;
	QWidget* mBasicPage = nullptr;
	QVBoxLayout* mBasicLayout = nullptr;

	// 专有元数据 Tab：ScrollArea + 动态表单
	QScrollArea* mSpeScroll = nullptr;
	QWidget* mSpePage = nullptr;
	QVBoxLayout* mSpeLayout = nullptr;

	// 预览不支持时的红色警告标签（位于用户信息栏左侧）
	QLabel* mPreviewWarningLabel = nullptr;

	// 导出时文件覆盖策略
	bool mOverwriteAll = false;
	bool mSkipAll = false;
	bool mExportCancelled = false;

	Ui::ProductSearchDialog ui;
};

#endif // PRODUCT_SEARCH_DIALOG_H
