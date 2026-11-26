#ifndef SERVICE_RECORD_DIALOG_H
#define SERVICE_RECORD_DIALOG_H

#include <QDialog>
#include <QDateTimeEdit>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QListWidget>
#include <QTableWidget>
#include <QTabWidget>
#include "ui_service_record_dialog.h"
#include "database/service_record.h"

/**
 * @brief 新增/编辑地图服务单对话框（UNV2）
 *
 * 变更摘要：
 * - 去掉状态、经费栏
 * - 安排领导 → 安排处室（处室+安排领导）
 * - 应用场景、服务等级、服务类别按新规范
 * - 服务形式使用 QTabWidget 分页：地图编制、地图打印、地图装裱、地图成品、数据发送、其他
 * - 地图编制页支持图件种类+数量表格
 * - 去掉旧版图件种类/用图统计指标 GroupBox
 * - 关联成果区域新增"上传新的地图成果"按钮
 */
class ServiceRecordDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ServiceRecordDialog(QWidget* parent = nullptr);
	~ServiceRecordDialog();

	/// 设置对话框标题
	void setTitle(const QString& title);

	// ---- 数据存取 ----
	void loadRecord(const ServiceRecord& record);
	ServiceRecord buildRecord() const;

	/// 自动生成服务单号
	QString generateServiceNumber() const;

	/// 获取/设置编辑模式（ID > 0 表示编辑已有记录）
	void setEditId(int id) { mEditId = id; }
	int editId() const { return mEditId; }

signals:
	void recordSaved(const ServiceRecord& record);
	/// 点击"上传新的地图成果"时发出，供外部打开产品入库流程
	void uploadNewProductRequested();

private slots:
	void onSave();
	void onCancel();
	void onUploadApprovalFile();
	void onViewApprovalFile();
	void onClearApprovalFile();
	void onAddUnit();
	void onDeleteUnit();
	void onAddScenario();
	void onAddLevel();
	void onAddCategory();
	void onAddMaterial();
	void onAddMountingSize();
	void onDeleteMountingSize();
	void onDeleteScenario();
	void onDeleteLevel();
	void onDeleteCategory();
	void onDeleteMaterial();
	void onLinkProduct();
	void onClearLink();
	void onUploadProduct();
	void onServiceFormTabChanged(int index);

	// 各服务形式Tab记录列表：新增/删除/选中联动
	void onAddCompileRecord();
	void onAddPrintRecord();
	void onAddMountingRecord();
	void onAddProductRecord();
	void onAddDataRecord();
	void onAddOtherRecord();
	void onDelCompileRecord();
	void onDelPrintRecord();
	void onDelMountingRecord();
	void onDelProductRecord();
	void onDelDataRecord();
	void onDelOtherRecord();
	void onCompileListCurrentChanged(QListWidgetItem* current, QListWidgetItem* previous);
	void onPrintListCurrentChanged(QListWidgetItem* current, QListWidgetItem* previous);
	void onMountingListCurrentChanged(QListWidgetItem* current, QListWidgetItem* previous);
	void onProductListCurrentChanged(QListWidgetItem* current, QListWidgetItem* previous);
	void onDataListCurrentChanged(QListWidgetItem* current, QListWidgetItem* previous);
	void onOtherListCurrentChanged(QListWidgetItem* current, QListWidgetItem* previous);

private:
	// 初始化相关
	void initConnections();
	void populateCombos();
	void populateMaterialCombo(QComboBox* combo);
	void addDictOptionToCombo(QComboBox* combo, const QString& dictType, const QString& title);
	void deleteDictOptionFromCombo(QComboBox* combo, const QString& dictType, const QString& title);

	// 从界面控件收集数据
	void collectBasicInfo(ServiceRecord& rec) const;
	void collectCompileDetails(ServiceRecord& rec) const;
	void collectLinkedProducts(ServiceRecord& rec) const;
	void collectPrintDetails(ServiceRecord& rec) const;
	void collectMountingDetails(ServiceRecord& rec) const;
	void collectProductDetails(ServiceRecord& rec) const;
	void collectDataDetails(ServiceRecord& rec) const;
	void collectOtherDetails(ServiceRecord& rec) const;

	// 向界面控件填充数据
	void populateBasicInfo(const ServiceRecord& rec);
	void populateCompileDetails(const ServiceRecord& rec);
	void populateLinkedProducts(const ServiceRecord& rec);
	void populatePrintDetails(const ServiceRecord& rec);
	void populateMountingDetails(const ServiceRecord& rec);
	void populateProductDetails(const ServiceRecord& rec);
	void populateDataDetails(const ServiceRecord& rec);
	void populateOtherDetails(const ServiceRecord& rec);

	// 记录列表辅助：把表单当前内容写回列表选中项
	void syncCurrentFormToList(QListWidget* list,
		QLineEdit* mapNameEdit, QLineEdit* mappingRegionEdit,
		QWidget* sizeEdit, QComboBox* materialEdit, QTableWidget* kindTable) const;
	// 记录列表辅助：填充记录列表（摘要），并回填第一条
	void populateFormList(QListWidget* list,
		QLineEdit* mapNameEdit, QLineEdit* mappingRegionEdit,
		QWidget* sizeEdit, QComboBox* materialEdit, QTableWidget* kindTable,
		const QList<ServiceFormDetail>& details);
	// 记录列表辅助：从列表收集全部记录
	QList<ServiceFormDetail> collectFormList(QListWidget* list,
		QLineEdit* mapNameEdit, QLineEdit* mappingRegionEdit,
		QWidget* sizeEdit, QComboBox* materialEdit, QTableWidget* kindTable) const;
	// 记录列表辅助：新增一条记录
	void addFormRecord(QListWidget* list);
	// 记录列表辅助：删除当前选中记录
	void deleteFormRecord(QListWidget* list);
	// 生成列表项摘要文本
	static QString formItemSummary(const ServiceFormDetail& d);

	// 通用：按服务形式初始化图件/数据种类QTableWidget
	void setupKindTable(QTableWidget* table, const QString& serviceForm, const QString& kindHeader);

	// 通用：从表单控件收集 ServiceFormDetail
	ServiceFormDetail collectFormDetail(
		QLineEdit* mapNameEdit, QLineEdit* mappingRegionEdit,
		QWidget* sizeEdit, QComboBox* materialEdit,
		QTableWidget* kindTable) const;

	// 通用：将 ServiceFormDetail 填充到表单控件
	void populateFormDetail(
		QLineEdit* mapNameEdit, QLineEdit* mappingRegionEdit,
		QWidget* sizeEdit, QComboBox* materialEdit,
		QTableWidget* kindTable, const ServiceFormDetail& detail);

	/// 获取当前选中的服务形式文本
	QString currentServiceForm() const;

	// ---- 覆盖区域自动汇总 ----
	/// 汇总各类地图服务形式（地图编制/打印/装裱/数据发送/其他）的"制图区域"，去重并保持出现顺序
	QStringList collectMappingRegions() const;
	/// 用制图区域刷新"覆盖区域"输入框：去重后以英文分号";"分隔
	void refreshCoverageArea();

	Ui::ServiceRecordDialog ui;
	int mEditId = 0;
	int mApprovalFileOid = 0;  // 当前审批单 BLOB OID
};

#endif // SERVICE_RECORD_DIALOG_H
