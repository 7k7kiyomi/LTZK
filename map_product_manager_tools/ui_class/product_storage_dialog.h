#ifndef PRODUCT_STORAGE_DIALOG_H
#define PRODUCT_STORAGE_DIALOG_H

#include <QDialog>
#include <QList>
#include <QThread>
#include <QTreeWidgetItem>
#include <QTabWidget>
#include <QFormLayout>
#include <QLabel>
#include <QScrollArea>
#include "database/product_metadata.h"
#include "core/map_product_clip_engine.h"
#include "ui_product_storage_dialog.h"

class MetadataExtractor;
class FileStorageManager;
class ProductDAO;
class DataImporter;
class QgisInterface;

/**
 * @brief 成果存储与管理对话框
 * 
 * 实现成果文件的上传、自动元数据提取、版本管理功能。
 */
class ProductStorageDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ProductStorageDialog(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~ProductStorageDialog() override;

	void setQgisInterface(QgisInterface* iface) { mQGisIface = iface; }

	/// 快速上传：预填文件信息后自动触发入库（用于服务单中"上传成果"按钮）
	void quickUpload(const QString& filePath);

	/// 由主程序数据目录触发，复用本对话框已有的目录树操作。
	/// operation 支持 export、range、main-area、theme、map。
	void executeExternalDirectoryOperation(
		const QString& operation,
		int directoryId,
		int productId,
		const QList<int>& productIds);

private slots:
	void onUploadFile(const QString& filePath);
	void onDeleteProduct();
	void onAddToMap();
	void onRefreshList();
	void onProductSelected(int row, int col);
	void onExtractionProgress(int percent, const QString& message);
	void onTypeTreeItemClicked(QTreeWidgetItem* item, int column);
	void onDirectoryTreeItemClicked(QTreeWidgetItem* item, int column);
	void onDirectoryTreeItemChanged(QTreeWidgetItem* item, int column);
	void onDirectoryTreeContextMenu(const QPoint& pos);
	void onAddDirSelectionToMap();
	// 将目录树当前节点（及其勾选范围）的成果加入指定主题
	void onAddSelectionToTheme();
	void onCreateDirectory();
	void onDeleteDirectory();
	void onRenameDirectory();
	void onImportData();
	void onExportData();
	// 目录树范围裁剪导出：按范围导出 / 按主区裁切导出
	void onExportByRange();
	void onExportByMainAreaClip();

private:
	void setupConnections();
	void setupTableColumns();
	void loadProductList();
	void loadProductTypeTree();
	void loadDirectoryTree();
	void clearForm();
	void fillForm(const ProductMetadata& meta);
	void updateRasterFieldsVisibility();
	void processGdbLayers(const QString& gdbPath, const QStringList& layerNames,
		const QString& productType, const QString& versionNote);
	void updateProgressText(const QString& text, int value = -1);
	bool loadLayerToMap(const ProductMetadata& meta);
	QString resolveTableName(const QString& schema, const QString& candidateName);

	// 目录树管理：导入/导出
	void importPathRecursive(const QString& path, int targetDirId, int& imported, int& failed);
	bool importFileToDir(const QString& filePath, int targetDirId);
	bool exportProductFile(const ProductMetadata& meta, const QString& outputDir);

	// 范围/主区裁剪导出共用辅助
	// 汇总当前右键节点（及其子节点中"勾选"范围）待处理的产品，语义与 onExportData 完全一致
	QList<ProductMetadata> collectCurrentNodeProducts() const;
	// 汇总当前右键节点下的全部产品（忽略勾选状态，根节点为全部产品），用于"添加到主题"
	QList<ProductMetadata> collectAllProductsUnderCurrentNode() const;
	// 执行裁剪导出引擎并汇总结果（含成功/失败/被跳过提示）
	bool runClipExportAndReport(const QList<ProductMetadata>& clippables,
		const MapProductClipEngine::ClipOptions& opt, const QString& title);
	// 当前 QGIS 地图画布坐标系（无画布/工程时返回无效 CRS）
	QgsCoordinateReferenceSystem currentMapCrs() const;

	// Tab 详情页相关
	void initDetailTabs();
	void fillBasicMetaTab(const ProductMetadata& meta);
	void fillSpecializedMetaTab(const ProductMetadata& meta);
	void clearBasicMetaTab();
	void clearSpecializedMetaTab();

	// 核心组件
	MetadataExtractor* mExtractor = nullptr;
	ProductDAO* mDAO = nullptr;
	DataImporter* mImporter = nullptr;

	// QGIS 接口
	QgisInterface* mQGisIface = nullptr;

	QList<ProductMetadata> mProducts;
	ProductMetadata mCurrentMeta;

	// GDB 图层选择相关
	QString mSelectedGdbPath;
	QStringList mSelectedGdbLayers;
	bool mIsGdbMultiLayer = false;

	// 类型过滤
	QString mCurrentTypeFilter;

	// 目录过滤（地图成果树）
	int mCurrentDirId = -1;         // -1 表示按类型过滤，0 表示全部，>0 表示按目录 id 过滤
	QString mDatabaseName;          // 数据库名称（目录树根节点）

	// 目录树节点勾选（控制"导出数据"范围）
	bool mDirectoryCheckSyncing = false;  // 防止级联勾选时递归触发
	void setDirectorySubtreeCheck(QTreeWidgetItem* node, Qt::CheckState state);

	// 导入控制：-1 表示按产品类型自动分配目录，>0 表示导入到指定目录节点下
	int mImportTargetDirId = -1;
	bool mImportSilent = false;   // 批量导入时抑制成功弹窗
	bool mLastImportOk = false;   // 最近一次导入是否成功（批量导入统计用）

	// Tab 控件
	QTabWidget* mDetailTabWidget = nullptr;

	// 基本元数据 Tab 字段
	QLabel* mLabelDataId = nullptr;
	QLabel* mLabelProductName = nullptr;
	QLabel* mLabelDescription = nullptr;
	QLabel* mLabelSource = nullptr;
	QLabel* mLabelVersionNote = nullptr;
	QLabel* mLabelFileFormat = nullptr;
	QLabel* mLabelFileSize = nullptr;
	QLabel* mLabelIsCompressed = nullptr;
	QLabel* mLabelBounds = nullptr;
	QLabel* mLabelCenterLon = nullptr;
	QLabel* mLabelCenterLat = nullptr;
	QLabel* mLabelStartDatetime = nullptr;
	QLabel* mLabelEndDatetime = nullptr;
	QLabel* mLabelSecurityLevel = nullptr;
	QLabel* mLabelCreatedBy = nullptr;
	QLabel* mLabelCreatedAt = nullptr;
	QLabel* mLabelUpdatedAt = nullptr;
	QLabel* mLabelTags = nullptr;
	QLabel* mLabelCity = nullptr;
	QLabel* mLabelScale = nullptr;
	QLabel* mLabelProjectName = nullptr;
	QLabel* mLabelProducer = nullptr;
	QLabel* mLabelProductionDate = nullptr;
	QLabel* mLabelCrs = nullptr;
	QLabel* mLabelCompilationInfo = nullptr;
	QLabel* mLabelDeliveryStatus = nullptr;
	QLabel* mLabelDeliveryTime = nullptr;

	// 专有元数据 Tab
	QWidget* mSpecializedMetaPage = nullptr;
	QVBoxLayout* mSpecializedLayout = nullptr;
	QScrollArea* mSpecializedScroll = nullptr;

	Ui::ProductStorageDialog ui;
};

#endif // PRODUCT_STORAGE_DIALOG_H
