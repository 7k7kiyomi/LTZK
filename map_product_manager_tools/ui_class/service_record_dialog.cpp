#include "service_record_dialog.h"
#include "database/service_dao.h"
#include "database/product_dao.h"
#include "database/product_metadata.h"
#include "database/schema_manager.h"
#include "database/postgis_connector.h"
#include "core/directory_helper.h"
#include "core/layer_icon_helper.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QUuid>
#include <QDateTime>
#include <QHeaderView>
#include <QInputDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QDialog>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <qgsapplication.h>
#include <qgsmessagelog.h>
#include "core/metadata_extractor.h"
#include "core/data_importer.h"
#include "gdb_layer_selector_dialog.h"
#include <QSpinBox>
#include <QRegularExpression>
#include <QDebug>
#include <QSet>
#include <functional>
#include "qgsgui.h"

// 用于 mapKindTable 的角色常量
static const int KindRole = Qt::UserRole + 1;      // 图件种类字符串
static const int QtyRole  = Qt::UserRole + 2;      // 数量
static const int NameRole = Qt::UserRole + 3;      // 自定义名称

namespace {

// 递归填充目录树（与"成果存储与管理 → 按地图成果分类"一致）
void populateDirTreeChildren(QTreeWidgetItem* parentItem, int parentDirId, ProductDAO& dao,
                             const QSet<int>& productLeafDirIds)
{
	QStyle* style = QApplication::style();
	const auto children = dao.getChildDirectories(parentDirId);
	for (const auto& child : children)
	{
		auto* ci = new QTreeWidgetItem(parentItem);
		ci->setText(0, child.name);
		ci->setData(0, Qt::UserRole, child.id);
		ci->setData(0, Qt::UserRole + 1, 0); // 0=目录节点
		// 图层节点（nodeType==1）：根据其下产品类型显示矢量点/线/面或栅格图标
		if (child.nodeType == 1)
		{
			const auto layerProducts = dao.getProductsByDirectory(child.id);
			int kind = -1;
			for (const auto& p : layerProducts)
			{
				kind = LayerIconHelper::layerIconKindForProduct(p);
				if (kind >= 0) break;
			}
			if (kind >= 0)
				ci->setIcon(0, LayerIconHelper::drawLayerIcon(kind));
			else
				ci->setIcon(0, style->standardIcon(QStyle::SP_FileIcon));
		}
		// 其余目录节点（固定分类节点、导入的各级普通目录）统一用文件夹图标
		else
		{
			ci->setIcon(0, style->standardIcon(QStyle::SP_DirIcon));
		}

		// 固定叶子节点（AI/PDF/其它、影像/晕渲、文档资料/表格资料/其它）下展示产品数据子节点
		if (productLeafDirIds.contains(child.id))
		{
			const auto products = dao.getProductsByDirectory(child.id);
			for (const auto& p : products)
			{
				auto* pi = new QTreeWidgetItem(ci);
				pi->setText(0, p.productName);
				pi->setData(0, Qt::UserRole, p.id);
				pi->setData(0, Qt::UserRole + 1, 1); // 1=产品数据节点
				// 产品数据节点按产品类型显示对应图层图标
				int kind = LayerIconHelper::layerIconKindForProduct(p);
				pi->setIcon(0, kind >= 0 ? LayerIconHelper::drawLayerIcon(kind) : style->standardIcon(QStyle::SP_FileIcon));
				pi->setToolTip(0, QStringLiteral("数据名称: %1\n密级: %2")
					.arg(p.productName, securityLevelToString(p.securityLevel)));
			}
		}

		populateDirTreeChildren(ci, child.id, dao, productLeafDirIds);
	}
}

} // namespace

ServiceRecordDialog::ServiceRecordDialog(QWidget* parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	QgsGui::enableAutoGeometryRestore(this);

	// 新增/编辑服务单仅保留审批单上传；保存后的服务单列表仍提供查看入口。
	ui.viewApprovalBtn->setVisible(false);

	// 覆盖区域由各类地图服务形式的"制图区域"自动汇总，不允许手工输入
	ui.coverageAreaEdit->setReadOnly(true);
	ui.coverageAreaEdit->setToolTip(QStringLiteral(
		"自动汇总各类地图服务形式的「制图区域」，去重后以英文分号\";\"分隔；\n"
		"所有服务形式均未填写制图区域时，保留原有值。"));

	ui.scrollArea->setFrameShape(QFrame::NoFrame);

	initConnections();
	populateCombos();

	// 各服务形式 Tab 的图件（数据）种类表格：不同服务形式可提供的种类不同
	setupKindTable(ui.mapKindTable,      QStringLiteral("地图编制"), QStringLiteral("图件种类"));
	setupKindTable(ui.printKindTable,    QStringLiteral("地图打印"), QStringLiteral("图件种类"));
	setupKindTable(ui.mountingKindTable, QStringLiteral("地图装裱"), QStringLiteral("图件种类"));
	setupKindTable(ui.productKindTable,  QStringLiteral("地图成品"), QStringLiteral("图件种类"));
	setupKindTable(ui.dataKindTable,     QStringLiteral("数据发送"), QStringLiteral("数据种类"));  // 数据发送Tab用"数据种类"
	setupKindTable(ui.otherKindTable,    QStringLiteral("其他"),     QStringLiteral("图件种类"));

	// 设置默认日期为今天
	ui.recordDateEdit->setDateTime(QDateTime::currentDateTime());
}

ServiceRecordDialog::~ServiceRecordDialog()
{
}

void ServiceRecordDialog::setTitle(const QString& title)
{
	setWindowTitle(title);
}

// ============================================================================
// 初始化
// ============================================================================

void ServiceRecordDialog::initConnections()
{
	connect(ui.saveBtn,          &QPushButton::clicked, this, &ServiceRecordDialog::onSave);
	connect(ui.cancelBtn,        &QPushButton::clicked, this, &ServiceRecordDialog::onCancel);
	connect(ui.uploadApprovalBtn,&QPushButton::clicked, this, &ServiceRecordDialog::onUploadApprovalFile);
	connect(ui.viewApprovalBtn,  &QPushButton::clicked, this, &ServiceRecordDialog::onViewApprovalFile);
	connect(ui.clearApprovalBtn, &QPushButton::clicked, this, &ServiceRecordDialog::onClearApprovalFile);
	connect(ui.addUnitBtn,       &QPushButton::clicked, this, &ServiceRecordDialog::onAddUnit);
	connect(ui.delUnitBtn,       &QPushButton::clicked, this, &ServiceRecordDialog::onDeleteUnit);
	connect(ui.addScenarioBtn,   &QPushButton::clicked, this, &ServiceRecordDialog::onAddScenario);
	connect(ui.addLevelBtn,      &QPushButton::clicked, this, &ServiceRecordDialog::onAddLevel);
	connect(ui.addCategoryBtn,   &QPushButton::clicked, this, &ServiceRecordDialog::onAddCategory);
	connect(ui.addPrintMaterialBtn,    &QPushButton::clicked, this, &ServiceRecordDialog::onAddMaterial);
	connect(ui.addMountingMaterialBtn, &QPushButton::clicked, this, &ServiceRecordDialog::onAddMaterial);
	connect(ui.addOtherMaterialBtn,    &QPushButton::clicked, this, &ServiceRecordDialog::onAddMaterial);
	connect(ui.addMountingSizeBtn,     &QPushButton::clicked, this, &ServiceRecordDialog::onAddMountingSize);
	connect(ui.delScenarioBtn,      &QPushButton::clicked, this, &ServiceRecordDialog::onDeleteScenario);
	connect(ui.delLevelBtn,         &QPushButton::clicked, this, &ServiceRecordDialog::onDeleteLevel);
	connect(ui.delCategoryBtn,      &QPushButton::clicked, this, &ServiceRecordDialog::onDeleteCategory);
	connect(ui.delPrintMaterialBtn,    &QPushButton::clicked, this, &ServiceRecordDialog::onDeleteMaterial);
	connect(ui.delMountingMaterialBtn, &QPushButton::clicked, this, &ServiceRecordDialog::onDeleteMaterial);
	connect(ui.delOtherMaterialBtn,    &QPushButton::clicked, this, &ServiceRecordDialog::onDeleteMaterial);
	connect(ui.delMountingSizeBtn,     &QPushButton::clicked, this, &ServiceRecordDialog::onDeleteMountingSize);
	connect(ui.linkProductBtn,   &QPushButton::clicked, this, &ServiceRecordDialog::onLinkProduct);
	connect(ui.clearLinkBtn,     &QPushButton::clicked, this, &ServiceRecordDialog::onClearLink);
	connect(ui.uploadProductBtn, &QPushButton::clicked, this, &ServiceRecordDialog::onUploadProduct);
	connect(ui.serviceFormTab,   &QTabWidget::currentChanged, this, &ServiceRecordDialog::onServiceFormTabChanged);

	// 各服务形式Tab记录列表：新增/删除按钮
	connect(ui.addCompileRecordBtn,   &QPushButton::clicked, this, &ServiceRecordDialog::onAddCompileRecord);
	connect(ui.addPrintRecordBtn,     &QPushButton::clicked, this, &ServiceRecordDialog::onAddPrintRecord);
	connect(ui.addMountingRecordBtn,  &QPushButton::clicked, this, &ServiceRecordDialog::onAddMountingRecord);
	connect(ui.addProductRecordBtn,   &QPushButton::clicked, this, &ServiceRecordDialog::onAddProductRecord);
	connect(ui.addDataRecordBtn,      &QPushButton::clicked, this, &ServiceRecordDialog::onAddDataRecord);
	connect(ui.addOtherRecordBtn,     &QPushButton::clicked, this, &ServiceRecordDialog::onAddOtherRecord);
	connect(ui.delCompileRecordBtn,   &QPushButton::clicked, this, &ServiceRecordDialog::onDelCompileRecord);
	connect(ui.delPrintRecordBtn,     &QPushButton::clicked, this, &ServiceRecordDialog::onDelPrintRecord);
	connect(ui.delMountingRecordBtn,  &QPushButton::clicked, this, &ServiceRecordDialog::onDelMountingRecord);
	connect(ui.delProductRecordBtn,   &QPushButton::clicked, this, &ServiceRecordDialog::onDelProductRecord);
	connect(ui.delDataRecordBtn,      &QPushButton::clicked, this, &ServiceRecordDialog::onDelDataRecord);
	connect(ui.delOtherRecordBtn,     &QPushButton::clicked, this, &ServiceRecordDialog::onDelOtherRecord);

	// 各服务形式Tab记录列表：选中联动回填（currentItemChanged 以便同步旧项）
	connect(ui.compileListWidget,   &QListWidget::currentItemChanged, this, &ServiceRecordDialog::onCompileListCurrentChanged);
	connect(ui.printListWidget,     &QListWidget::currentItemChanged, this, &ServiceRecordDialog::onPrintListCurrentChanged);
	connect(ui.mountingListWidget,  &QListWidget::currentItemChanged, this, &ServiceRecordDialog::onMountingListCurrentChanged);
	connect(ui.productListWidget,   &QListWidget::currentItemChanged, this, &ServiceRecordDialog::onProductListCurrentChanged);
	connect(ui.dataListWidget,      &QListWidget::currentItemChanged, this, &ServiceRecordDialog::onDataListCurrentChanged);
	connect(ui.otherListWidget,     &QListWidget::currentItemChanged, this, &ServiceRecordDialog::onOtherListCurrentChanged);

	// 覆盖区域：自动汇总各类地图服务形式的"制图区域"（去重，英文分号分隔）
	// 注意：以下连接晚于上面的回填槽，确保刷新时读到的是同步后的数据
	connect(ui.serviceFormTab, &QTabWidget::currentChanged, this, &ServiceRecordDialog::refreshCoverageArea);
	connect(ui.mappingRegionEdit,         &QLineEdit::textChanged, this, &ServiceRecordDialog::refreshCoverageArea);
	connect(ui.printMappingRegionEdit,    &QLineEdit::textChanged, this, &ServiceRecordDialog::refreshCoverageArea);
	connect(ui.mountingMappingRegionEdit, &QLineEdit::textChanged, this, &ServiceRecordDialog::refreshCoverageArea);
	connect(ui.dataMappingRegionEdit,     &QLineEdit::textChanged, this, &ServiceRecordDialog::refreshCoverageArea);
	connect(ui.otherMappingRegionEdit,    &QLineEdit::textChanged, this, &ServiceRecordDialog::refreshCoverageArea);
	connect(ui.compileListWidget,  &QListWidget::currentItemChanged, this, &ServiceRecordDialog::refreshCoverageArea);
	connect(ui.printListWidget,    &QListWidget::currentItemChanged, this, &ServiceRecordDialog::refreshCoverageArea);
	connect(ui.mountingListWidget, &QListWidget::currentItemChanged, this, &ServiceRecordDialog::refreshCoverageArea);
	connect(ui.productListWidget,  &QListWidget::currentItemChanged, this, &ServiceRecordDialog::refreshCoverageArea);
	connect(ui.dataListWidget,     &QListWidget::currentItemChanged, this, &ServiceRecordDialog::refreshCoverageArea);
	connect(ui.otherListWidget,    &QListWidget::currentItemChanged, this, &ServiceRecordDialog::refreshCoverageArea);
}

void ServiceRecordDialog::populateCombos()
{
	ServiceDAO dao;

	// 应用场景：固定项 + 自定义项
	ui.scenarioCombo->clear();
	for (const QString& opt : applicationScenarioOptions())
		ui.scenarioCombo->addItem(opt);
	for (const QString& opt : dao.getDictOptions("scenario"))
		ui.scenarioCombo->addItem(opt);

	// 服务等级：固定项 + 自定义项
	ui.serviceLevelCombo->clear();
	for (const QString& opt : serviceLevelOptions())
		ui.serviceLevelCombo->addItem(opt);
	for (const QString& opt : dao.getDictOptions("level"))
		ui.serviceLevelCombo->addItem(opt);

	// 服务类别：固定项 + 自定义项
	ui.categoryCombo->clear();
	for (const QString& opt : categoryOptions())
		ui.categoryCombo->addItem(opt);
	for (const QString& opt : dao.getDictOptions("category"))
		ui.categoryCombo->addItem(opt);

	// 材质：固定项 + 自定义项
	populateMaterialCombo(ui.printMaterialEdit);
	populateMaterialCombo(ui.mountingMaterialEdit);
	populateMaterialCombo(ui.otherMaterialEdit);

	// 装裱尺寸：固定项 + 自定义项
	ui.mountingSizeEdit->clear();
	for (const QString& opt : mountingSizeOptions())
		ui.mountingSizeEdit->addItem(opt);
	for (const QString& opt : dao.getDictOptions("mounting_size"))
		ui.mountingSizeEdit->addItem(opt);

	// 服务单位（从数据库加载）
	ui.unitCombo->clear();
	QList<QVariantMap> units = dao.getUnitList();
	for (const auto& u : units)
	{
		ui.unitCombo->addItem(u.value("name").toString(), u.value("id").toInt());
	}
}

// 填充材质下拉框：固定项 + 数据库自定义项
void ServiceRecordDialog::populateMaterialCombo(QComboBox* combo)
{
	if (!combo) return;
	combo->clear();
	for (const QString& opt : materialOptions())
		combo->addItem(opt);
	ServiceDAO dao;
	for (const QString& opt : dao.getDictOptions("material"))
		combo->addItem(opt);
}

// 在种类表格末尾追加一行：列0=种类（只读）、列1=名称、列2=数量
static int appendKindRow(QTableWidget* tw, const QString& kind)
{
	const int row = tw->rowCount();
	tw->insertRow(row);

	// 列0：图件/数据种类（只读）
	QTableWidgetItem* kindItem = new QTableWidgetItem(kind);
	kindItem->setFlags(kindItem->flags() & ~Qt::ItemIsEditable);
	kindItem->setData(KindRole, kind);
	tw->setItem(row, 0, kindItem);

	// 列1：名称（仅"图集图册""创意地图""其他"可编辑，其余灰掉）
	QTableWidgetItem* nameItem = new QTableWidgetItem();
	nameItem->setData(NameRole, QString());
	if (ServiceFormMapKind::allowCustomName(kind))
	{
		nameItem->setFlags(nameItem->flags() | Qt::ItemIsEditable);
		const QString hint = ServiceFormMapKind::nameHint(kind);
		if (!hint.isEmpty())
			nameItem->setToolTip(hint);
	}
	else
	{
		nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
	}
	tw->setItem(row, 1, nameItem);

	// 列2：数量（QSpinBox）
	QSpinBox* spin = new QSpinBox();
	spin->setRange(0, 99999);
	spin->setValue(0);
	tw->setCellWidget(row, 2, spin);

	return row;
}

void ServiceRecordDialog::setupKindTable(QTableWidget* tw, const QString& serviceForm, const QString& kindHeader)
{
	if (!tw) return;

	// 按服务形式取该页可填写的种类列表
	const QStringList kinds = ServiceFormMapKind::forServiceForm(serviceForm);

	tw->clearContents();
	tw->setRowCount(0);
	tw->setColumnCount(3);
	tw->setHorizontalHeaderLabels({
		kindHeader,
		ServiceFormMapKind::nameColumnHeader(kinds),
		QStringLiteral("数量") });

	{
		QFont headerFont = tw->horizontalHeader()->font();
		headerFont.setFamily(QStringLiteral("微软雅黑"));
		headerFont.setPointSize(9);
		tw->horizontalHeader()->setFont(headerFont);
	}

	tw->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	tw->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	tw->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

	tw->verticalHeader()->setVisible(false);
	tw->setSelectionMode(QAbstractItemView::NoSelection);
	tw->setEditTriggers(QAbstractItemView::AllEditTriggers);

	for (const QString& kind : kinds)
		appendKindRow(tw, kind);

	// 记录基础行数：编辑历史服务单时，超出该行数的行属于保留的旧种类行
	tw->setProperty("baseKindRows", kinds.size());

	tw->resizeColumnsToContents();
}

// ============================================================================
// Slot: 服务形式 Tab 切换
// ============================================================================

void ServiceRecordDialog::onServiceFormTabChanged(int index)
{
	Q_UNUSED(index);
	// 切换 Tab 时，自动将 service_form 设置为对应标签页标题
	// 地图编制 Tab(index=0) 显示编译详情表，其他隐藏
}

// ============================================================================
// 收集界面数据
// ============================================================================

void ServiceRecordDialog::collectBasicInfo(ServiceRecord& rec) const
{
	rec.recordDate  = ui.recordDateEdit->dateTime().toString("yyyy-MM-dd hh:mm:ss");
	rec.submitTime  = ui.submitTimeEdit->dateTime().toString("yyyy-MM-dd hh:mm:ss");

	rec.applicationScenario = ui.scenarioCombo->currentText();
	rec.serviceLevel        = ui.serviceLevelCombo->currentText();
	rec.category            = ui.categoryCombo->currentText();
	rec.arrangedOffice      = ui.arrangedOfficeEdit->text().trimmed();
	rec.coverageArea        = ui.coverageAreaEdit->text().trimmed();
	rec.receiver            = ui.receiverEdit->text().trimmed();

	// 服务单位
	rec.unitName = ui.unitCombo->currentText().trimmed();
	QVariant unitIdVar = ui.unitCombo->currentData();
	rec.unitId = unitIdVar.isValid() ? unitIdVar.toInt() : 0;

	// 服务形式 - 从当前 Tab 标题获取
	rec.serviceForm = currentServiceForm();

	// 其他自定义服务形式
	rec.otherServiceForm = ui.otherServiceFormEdit->text().trimmed();

	// 归档
	rec.remarks       = ui.remarksEdit->text().trimmed();
	rec.approvalFilePath = ui.approvalFilePathEdit->text().trimmed();
	rec.approvalFileOid = mApprovalFileOid;
}

void ServiceRecordDialog::collectCompileDetails(ServiceRecord& rec) const
{
	// 先把当前编辑内容同步回列表选中项
	syncCurrentFormToList(ui.compileListWidget, ui.mapNameEdit, ui.mappingRegionEdit,
		ui.sizeEdit, nullptr, ui.mapKindTable);

	// 从记录列表收集全部 ServiceFormDetail
	QList<ServiceFormDetail> details = collectFormList(ui.compileListWidget,
		ui.mapNameEdit, ui.mappingRegionEdit, ui.sizeEdit, nullptr, ui.mapKindTable);

	// 存为 ServiceFormDetail 数组
	rec.compilationDetails = details.isEmpty() ? QString() : serviceFormDetailsToJson(details);

	// 同步顶层字段（兼容旧统计/任务单：取第一条）
	if (!details.isEmpty())
	{
		rec.mapName       = details.first().mapName;
		rec.mappingRegion = details.first().mappingRegion;
		rec.size          = details.first().size;
	}

	// 同步 sheetCount = 总量（用于旧统计兼容）
	int total = 0;
	for (const auto& d : details)
		for (const auto& item : d.items)
			total += item.quantity;
	if (rec.sheetCount <= 0 && total > 0)
		rec.sheetCount = total;
}

void ServiceRecordDialog::collectLinkedProducts(ServiceRecord& rec) const
{
	QListWidget* lw = ui.linkedProductList;
	rec.productIds.clear();
	rec.productNames.clear();
	for (int i = 0; i < lw->count(); ++i)
	{
		QListWidgetItem* item = lw->item(i);
		rec.productIds.append(item->data(Qt::UserRole).toString());
		rec.productNames.append(item->text());
	}
}

void ServiceRecordDialog::collectPrintDetails(ServiceRecord& rec) const
{
	QList<ServiceFormDetail> details = collectFormList(ui.printListWidget,
		ui.printMapNameEdit, ui.printMappingRegionEdit,
		ui.printSizeEdit, ui.printMaterialEdit, ui.printKindTable);
	rec.printDetails = details.isEmpty() ? QString() : serviceFormDetailsToJson(details);
}

void ServiceRecordDialog::collectMountingDetails(ServiceRecord& rec) const
{
	QList<ServiceFormDetail> details = collectFormList(ui.mountingListWidget,
		ui.mountingMapNameEdit, ui.mountingMappingRegionEdit,
		ui.mountingSizeEdit, ui.mountingMaterialEdit, ui.mountingKindTable);
	rec.mountingDetails = details.isEmpty() ? QString() : serviceFormDetailsToJson(details);
}

void ServiceRecordDialog::collectProductDetails(ServiceRecord& rec) const
{
	// 地图成品：没有 制图区域、材质
	QList<ServiceFormDetail> details = collectFormList(ui.productListWidget,
		ui.productMapNameEdit, nullptr,
		ui.productSizeEdit, nullptr, ui.productKindTable);
	rec.productDetails = details.isEmpty() ? QString() : serviceFormDetailsToJson(details);
}

void ServiceRecordDialog::collectDataDetails(ServiceRecord& rec) const
{
	// 数据发送：没有 尺寸、材质
	QList<ServiceFormDetail> details = collectFormList(ui.dataListWidget,
		ui.dataMapNameEdit, ui.dataMappingRegionEdit,
		nullptr, nullptr, ui.dataKindTable);
	rec.dataDetails = details.isEmpty() ? QString() : serviceFormDetailsToJson(details);
}

void ServiceRecordDialog::collectOtherDetails(ServiceRecord& rec) const
{
	QList<ServiceFormDetail> details = collectFormList(ui.otherListWidget,
		ui.otherMapNameEdit, ui.otherMappingRegionEdit,
		ui.otherSizeEdit, ui.otherMaterialEdit, ui.otherKindTable);
	rec.otherDetails = details.isEmpty() ? QString() : serviceFormDetailsToJson(details);
}

// ============================================================================
// 填充界面数据
// ============================================================================

void ServiceRecordDialog::populateBasicInfo(const ServiceRecord& rec)
{
	// 日期
	if (!rec.recordDate.isEmpty())
		ui.recordDateEdit->setDateTime(QDateTime::fromString(rec.recordDate, "yyyy-MM-dd hh:mm:ss"));
	if (!rec.submitTime.isEmpty())
		ui.submitTimeEdit->setDateTime(QDateTime::fromString(rec.submitTime, "yyyy-MM-dd hh:mm:ss"));

	// 下拉框
	int idx = ui.scenarioCombo->findText(rec.applicationScenario);
	if (idx >= 0) ui.scenarioCombo->setCurrentIndex(idx);

	idx = ui.serviceLevelCombo->findText(rec.serviceLevel);
	if (idx >= 0) ui.serviceLevelCombo->setCurrentIndex(idx);

	idx = ui.categoryCombo->findText(rec.category);
	if (idx >= 0) ui.categoryCombo->setCurrentIndex(idx);

	// 服务单位
	idx = ui.unitCombo->findText(rec.unitName);
	if (idx >= 0) ui.unitCombo->setCurrentIndex(idx);
	else if (!rec.unitName.isEmpty())
		ui.unitCombo->setEditText(rec.unitName);

	// 安排处室
	ui.arrangedOfficeEdit->setText(rec.arrangedOffice);

	// 覆盖区域
	ui.coverageAreaEdit->setText(rec.coverageArea);

	// 接收人
	ui.receiverEdit->setText(rec.receiver);

	// 服务形式：定位到对应 Tab
	int tabIdx = 0;
	if (rec.serviceForm == QStringLiteral("地图打印"))       tabIdx = 1;
	else if (rec.serviceForm == QStringLiteral("地图装裱")) tabIdx = 2;
	else if (rec.serviceForm == QStringLiteral("地图成品")) tabIdx = 3;
	else if (rec.serviceForm == QStringLiteral("数据发送")) tabIdx = 4;
	else if (rec.serviceForm == QStringLiteral("其他"))     tabIdx = 5;
	ui.serviceFormTab->setCurrentIndex(tabIdx);

	// 其他自定义
	ui.otherServiceFormEdit->setText(rec.otherServiceForm);

	// 归档
	ui.remarksEdit->setText(rec.remarks);
	ui.approvalFilePathEdit->setText(rec.approvalFilePath);
	mApprovalFileOid = rec.approvalFileOid;
}

void ServiceRecordDialog::populateCompileDetails(const ServiceRecord& rec)
{
	// 兼容解析地图编制的多记录（新ServiceFormDetail数组 / 旧CompilationItem数组）
	QList<ServiceFormDetail> details = compilationDetailsFromJson(rec.compilationDetails);
	// 旧数据若只有顶层字段，也作为一条记录展示
	if (details.isEmpty() && (!rec.mapName.isEmpty() || !rec.size.isEmpty()))
	{
		ServiceFormDetail d;
		d.mapName = rec.mapName;
		d.mappingRegion = rec.mappingRegion;
		d.size = rec.size;
		// 顶层没有 items，仅基本信息
		details.append(d);
	}

	populateFormList(ui.compileListWidget, ui.mapNameEdit, ui.mappingRegionEdit,
		ui.sizeEdit, nullptr, ui.mapKindTable, details);
}

void ServiceRecordDialog::populateLinkedProducts(const ServiceRecord& rec)
{
	QListWidget* lw = ui.linkedProductList;
	lw->clear();
	for (int i = 0; i < rec.productIds.size() && i < rec.productNames.size(); ++i)
	{
		QListWidgetItem* item = new QListWidgetItem(rec.productNames[i]);
		item->setData(Qt::UserRole, rec.productIds[i]);
		lw->addItem(item);
	}
}

void ServiceRecordDialog::populatePrintDetails(const ServiceRecord& rec)
{
	populateFormList(ui.printListWidget, ui.printMapNameEdit, ui.printMappingRegionEdit,
		ui.printSizeEdit, ui.printMaterialEdit, ui.printKindTable,
		serviceFormDetailsFromJson(rec.printDetails));
}

void ServiceRecordDialog::populateMountingDetails(const ServiceRecord& rec)
{
	populateFormList(ui.mountingListWidget, ui.mountingMapNameEdit, ui.mountingMappingRegionEdit,
		ui.mountingSizeEdit, ui.mountingMaterialEdit, ui.mountingKindTable,
		serviceFormDetailsFromJson(rec.mountingDetails));
}

void ServiceRecordDialog::populateProductDetails(const ServiceRecord& rec)
{
	populateFormList(ui.productListWidget, ui.productMapNameEdit, nullptr,
		ui.productSizeEdit, nullptr, ui.productKindTable,
		serviceFormDetailsFromJson(rec.productDetails));
}

void ServiceRecordDialog::populateDataDetails(const ServiceRecord& rec)
{
	populateFormList(ui.dataListWidget, ui.dataMapNameEdit, ui.dataMappingRegionEdit,
		nullptr, nullptr, ui.dataKindTable,
		serviceFormDetailsFromJson(rec.dataDetails));
}

void ServiceRecordDialog::populateOtherDetails(const ServiceRecord& rec)
{
	populateFormList(ui.otherListWidget, ui.otherMapNameEdit, ui.otherMappingRegionEdit,
		ui.otherSizeEdit, ui.otherMaterialEdit, ui.otherKindTable,
		serviceFormDetailsFromJson(rec.otherDetails));
}

// ============================================================================
// 通用：收集/填充 ServiceFormDetail
// ============================================================================

// 读取"尺寸"控件文本：兼容 QLineEdit 与可编辑 QComboBox
static QString sizeWidgetText(QWidget* w)
{
	if (auto* cb = qobject_cast<QComboBox*>(w)) return cb->currentText();
	if (auto* le = qobject_cast<QLineEdit*>(w)) return le->text();
	return QString();
}

// 写入"尺寸"控件文本：兼容 QLineEdit 与可编辑 QComboBox
static void setSizeWidgetText(QWidget* w, const QString& text)
{
	if (auto* cb = qobject_cast<QComboBox*>(w)) { cb->setCurrentText(text); return; }
	if (auto* le = qobject_cast<QLineEdit*>(w)) { le->setText(text); return; }
}

ServiceFormDetail ServiceRecordDialog::collectFormDetail(
	QLineEdit* mapNameEdit, QLineEdit* mappingRegionEdit,
	QWidget* sizeEdit, QComboBox* materialEdit,
	QTableWidget* kindTable) const
{
	ServiceFormDetail d;
	if (mapNameEdit)    d.mapName       = mapNameEdit->text().trimmed();
	if (mappingRegionEdit) d.mappingRegion = mappingRegionEdit->text().trimmed();
	if (sizeEdit)       d.size          = sizeWidgetText(sizeEdit).trimmed();
	if (materialEdit)   d.material      = materialEdit->currentText().trimmed();

	if (kindTable)
	{
		for (int i = 0; i < kindTable->rowCount(); ++i)
		{
			QTableWidgetItem* kindItem = kindTable->item(i, 0);
			QSpinBox* spin = qobject_cast<QSpinBox*>(kindTable->cellWidget(i, 2));
			if (!kindItem || !spin || spin->value() <= 0) continue;

			CompilationItem item;
			item.kind = kindItem->data(KindRole).toString();
			item.quantity = spin->value();
			QTableWidgetItem* nameItem = kindTable->item(i, 1);
			if (nameItem)
				item.name = nameItem->text().trimmed();
			d.items.append(item);
		}
	}
	return d;
}

void ServiceRecordDialog::populateFormDetail(
	QLineEdit* mapNameEdit, QLineEdit* mappingRegionEdit,
	QWidget* sizeEdit, QComboBox* materialEdit,
	QTableWidget* kindTable, const ServiceFormDetail& detail)
{
	if (mapNameEdit)    mapNameEdit->setText(detail.mapName);
	if (mappingRegionEdit) mappingRegionEdit->setText(detail.mappingRegion);
	if (sizeEdit)       setSizeWidgetText(sizeEdit, detail.size);
	if (materialEdit)   materialEdit->setCurrentText(detail.material);

	if (kindTable)
	{
		// 先移除上一次编辑历史服务单时追加的旧种类行（仅保留基础种类行）
		const int baseRows = kindTable->property("baseKindRows").toInt();
		if (baseRows > 0)
		{
			for (int r = kindTable->rowCount() - 1; r >= baseRows; --r)
				kindTable->removeRow(r);
		}

		for (int i = 0; i < kindTable->rowCount(); ++i)
		{
			QTableWidgetItem* kindItem = kindTable->item(i, 0);
			QSpinBox* spin = qobject_cast<QSpinBox*>(kindTable->cellWidget(i, 2));
			if (!kindItem || !spin) continue;

			spin->setValue(0);
			if (kindTable->item(i, 1))
				kindTable->item(i, 1)->setText(QString());

			QString kind = kindItem->data(KindRole).toString();
			for (const auto& item : detail.items)
			{
				if (item.kind == kind)
				{
					spin->setValue(item.quantity);
					if (kindTable->item(i, 1))
						kindTable->item(i, 1)->setText(item.name);
					break;
				}
			}
		}

		// 历史数据兼容：记录中出现、但当前服务形式未列出的种类，追加保留行，
		// 避免打开旧服务单后保存时丢失这些图件数量
		for (const auto& item : detail.items)
		{
			if (item.kind.trimmed().isEmpty())
				continue;

			bool exists = false;
			for (int i = 0; i < kindTable->rowCount(); ++i)
			{
				QTableWidgetItem* kindItem = kindTable->item(i, 0);
				if (kindItem && kindItem->data(KindRole).toString() == item.kind)
				{
					exists = true;
					break;
				}
			}
			if (exists) continue;

			const int row = appendKindRow(kindTable, item.kind);
			if (QSpinBox* spin = qobject_cast<QSpinBox*>(kindTable->cellWidget(row, 2)))
				spin->setValue(item.quantity);
			if (QTableWidgetItem* nameItem = kindTable->item(row, 1))
				nameItem->setText(item.name);
		}
	}
}

// ============================================================================
// 记录列表辅助：多记录服务形式
// ============================================================================

QString ServiceRecordDialog::formItemSummary(const ServiceFormDetail& d)
{
	// 摘要：图名为主，其次制图区域/尺寸
	if (!d.mapName.isEmpty())
		return QStringLiteral("%1（%2）").arg(d.mapName, d.mappingRegion.isEmpty() ? d.size : d.mappingRegion);
	if (!d.mappingRegion.isEmpty())
		return d.mappingRegion;
	if (!d.size.isEmpty())
		return QStringLiteral("尺寸：%1").arg(d.size);
	// 无基本信息时，显示图件种类合计
	int total = 0;
	for (const auto& item : d.items)
		total += item.quantity;
	return QStringLiteral("图件数量：%1").arg(total);
}

// 把表单当前内容写回列表选中项（保存前同步，避免丢失）
void ServiceRecordDialog::syncCurrentFormToList(QListWidget* list,
	QLineEdit* mapNameEdit, QLineEdit* mappingRegionEdit,
	QWidget* sizeEdit, QComboBox* materialEdit, QTableWidget* kindTable) const
{
	if (!list) return;
	int row = list->currentRow();
	if (row < 0 || row >= list->count()) return;
	QListWidgetItem* item = list->item(row);
	if (!item) return;

	ServiceFormDetail d = collectFormDetail(mapNameEdit, mappingRegionEdit, sizeEdit, materialEdit, kindTable);
	item->setData(Qt::UserRole, serviceFormDetailsToJson({ d }));
	item->setText(formItemSummary(d));
}

// 填充记录列表（摘要），并回填第一条到编辑表单
void ServiceRecordDialog::populateFormList(QListWidget* list,
	QLineEdit* mapNameEdit, QLineEdit* mappingRegionEdit,
	QWidget* sizeEdit, QComboBox* materialEdit, QTableWidget* kindTable,
	const QList<ServiceFormDetail>& details)
{
	if (!list) return;

	// 阻断信号，避免逐条回填触发 currentRowChanged
	QSignalBlocker blocker(list);
	list->clear();

	for (const auto& d : details)
	{
		QListWidgetItem* item = new QListWidgetItem(formItemSummary(d));
		item->setData(Qt::UserRole, serviceFormDetailsToJson({ d }));
		list->addItem(item);
	}

	// 默认折叠：仅保留列表，回填第一条到编辑表单
	if (list->count() > 0)
	{
		list->setCurrentRow(0);
		ServiceFormDetail first = details.first();
		populateFormDetail(mapNameEdit, mappingRegionEdit, sizeEdit, materialEdit, kindTable, first);
	}
	else
	{
		// 无记录：清空编辑表单
		populateFormDetail(mapNameEdit, mappingRegionEdit, sizeEdit, materialEdit, kindTable, ServiceFormDetail());
	}
}

// 从列表收集全部记录（先把当前表单内容同步到选中项）
QList<ServiceFormDetail> ServiceRecordDialog::collectFormList(QListWidget* list,
	QLineEdit* mapNameEdit, QLineEdit* mappingRegionEdit,
	QWidget* sizeEdit, QComboBox* materialEdit, QTableWidget* kindTable) const
{
	QList<ServiceFormDetail> details;
	if (!list) return details;

	// 同步当前正在编辑的项
	syncCurrentFormToList(list, mapNameEdit, mappingRegionEdit, sizeEdit, materialEdit, kindTable);

	for (int i = 0; i < list->count(); ++i)
	{
		QListWidgetItem* item = list->item(i);
		if (!item) continue;
		QString json = item->data(Qt::UserRole).toString();
		auto ds = serviceFormDetailsFromJson(json);
		if (!ds.isEmpty())
			details.append(ds.first());
	}
	return details;
}

// 新增一条记录：加入列表并选中新记录。
// 通过 setCurrentRow 触发 currentItemChanged，槽函数会自动把当前表单内容同步回原选中项，
// 并将新记录（空）回填到编辑表单。
void ServiceRecordDialog::addFormRecord(QListWidget* list)
{
	if (!list) return;
	QListWidgetItem* item = new QListWidgetItem(QStringLiteral("（新记录）"));
	item->setData(Qt::UserRole, serviceFormDetailsToJson({ ServiceFormDetail() }));
	list->addItem(item);
	list->setCurrentRow(list->count() - 1);
}

// 删除当前选中记录（用信号阻断避免把被删项内容写回相邻项）
void ServiceRecordDialog::deleteFormRecord(QListWidget* list)
{
	if (!list) return;
	int row = list->currentRow();
	if (row < 0) return;
	delete list->takeItem(row);

	{
		QSignalBlocker blocker(list);
		if (list->count() > 0)
		{
			int newRow = qMin(row, list->count() - 1);
			list->setCurrentRow(newRow);
		}
	}

	// 手动回填新选中项（或清空表单）
	if (list->count() > 0)
	{
		QListWidgetItem* cur = list->currentItem();
		if (cur)
		{
			auto ds = serviceFormDetailsFromJson(cur->data(Qt::UserRole).toString());
			// 依赖具体 Tab 槽的回填逻辑，此处由各 onDelXXXRecord 负责
		}
	}
}

// ============================================================================
// 各服务形式Tab记录列表：新增/删除/选中联动
// ============================================================================

// 地图编制
void ServiceRecordDialog::onAddCompileRecord()
{
	addFormRecord(ui.compileListWidget);
}
void ServiceRecordDialog::onDelCompileRecord()
{
	deleteFormRecord(ui.compileListWidget);
	// 手动回填新选中项或清空
	QListWidgetItem* cur = ui.compileListWidget->currentItem();
	if (cur)
	{
		auto ds = serviceFormDetailsFromJson(cur->data(Qt::UserRole).toString());
		populateFormDetail(ui.mapNameEdit, ui.mappingRegionEdit, ui.sizeEdit, nullptr, ui.mapKindTable,
			ds.isEmpty() ? ServiceFormDetail() : ds.first());
	}
	else
	{
		populateFormDetail(ui.mapNameEdit, ui.mappingRegionEdit, ui.sizeEdit, nullptr, ui.mapKindTable, ServiceFormDetail());
	}
}
void ServiceRecordDialog::onCompileListCurrentChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
	if (previous)
	{
		ServiceFormDetail d = collectFormDetail(ui.mapNameEdit, ui.mappingRegionEdit,
			ui.sizeEdit, nullptr, ui.mapKindTable);
		previous->setData(Qt::UserRole, serviceFormDetailsToJson({ d }));
		previous->setText(formItemSummary(d));
	}
	if (current)
	{
		auto ds = serviceFormDetailsFromJson(current->data(Qt::UserRole).toString());
		populateFormDetail(ui.mapNameEdit, ui.mappingRegionEdit, ui.sizeEdit, nullptr, ui.mapKindTable,
			ds.isEmpty() ? ServiceFormDetail() : ds.first());
	}
}

// 地图打印
void ServiceRecordDialog::onAddPrintRecord()
{
	addFormRecord(ui.printListWidget);
}
void ServiceRecordDialog::onDelPrintRecord()
{
	deleteFormRecord(ui.printListWidget);
	QListWidgetItem* cur = ui.printListWidget->currentItem();
	if (cur)
	{
		auto ds = serviceFormDetailsFromJson(cur->data(Qt::UserRole).toString());
		populateFormDetail(ui.printMapNameEdit, ui.printMappingRegionEdit,
			ui.printSizeEdit, ui.printMaterialEdit, ui.printKindTable,
			ds.isEmpty() ? ServiceFormDetail() : ds.first());
	}
	else
	{
		populateFormDetail(ui.printMapNameEdit, ui.printMappingRegionEdit,
			ui.printSizeEdit, ui.printMaterialEdit, ui.printKindTable, ServiceFormDetail());
	}
}
void ServiceRecordDialog::onPrintListCurrentChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
	if (previous)
	{
		ServiceFormDetail d = collectFormDetail(ui.printMapNameEdit, ui.printMappingRegionEdit,
			ui.printSizeEdit, ui.printMaterialEdit, ui.printKindTable);
		previous->setData(Qt::UserRole, serviceFormDetailsToJson({ d }));
		previous->setText(formItemSummary(d));
	}
	if (current)
	{
		auto ds = serviceFormDetailsFromJson(current->data(Qt::UserRole).toString());
		populateFormDetail(ui.printMapNameEdit, ui.printMappingRegionEdit,
			ui.printSizeEdit, ui.printMaterialEdit, ui.printKindTable,
			ds.isEmpty() ? ServiceFormDetail() : ds.first());
	}
}

// 地图装裱
void ServiceRecordDialog::onAddMountingRecord()
{
	addFormRecord(ui.mountingListWidget);
}
void ServiceRecordDialog::onDelMountingRecord()
{
	deleteFormRecord(ui.mountingListWidget);
	QListWidgetItem* cur = ui.mountingListWidget->currentItem();
	if (cur)
	{
		auto ds = serviceFormDetailsFromJson(cur->data(Qt::UserRole).toString());
		populateFormDetail(ui.mountingMapNameEdit, ui.mountingMappingRegionEdit,
			ui.mountingSizeEdit, ui.mountingMaterialEdit, ui.mountingKindTable,
			ds.isEmpty() ? ServiceFormDetail() : ds.first());
	}
	else
	{
		populateFormDetail(ui.mountingMapNameEdit, ui.mountingMappingRegionEdit,
			ui.mountingSizeEdit, ui.mountingMaterialEdit, ui.mountingKindTable, ServiceFormDetail());
	}
}
void ServiceRecordDialog::onMountingListCurrentChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
	if (previous)
	{
		ServiceFormDetail d = collectFormDetail(ui.mountingMapNameEdit, ui.mountingMappingRegionEdit,
			ui.mountingSizeEdit, ui.mountingMaterialEdit, ui.mountingKindTable);
		previous->setData(Qt::UserRole, serviceFormDetailsToJson({ d }));
		previous->setText(formItemSummary(d));
	}
	if (current)
	{
		auto ds = serviceFormDetailsFromJson(current->data(Qt::UserRole).toString());
		populateFormDetail(ui.mountingMapNameEdit, ui.mountingMappingRegionEdit,
			ui.mountingSizeEdit, ui.mountingMaterialEdit, ui.mountingKindTable,
			ds.isEmpty() ? ServiceFormDetail() : ds.first());
	}
}

// 地图成品（无制图区域、材质）
void ServiceRecordDialog::onAddProductRecord()
{
	addFormRecord(ui.productListWidget);
}
void ServiceRecordDialog::onDelProductRecord()
{
	deleteFormRecord(ui.productListWidget);
	QListWidgetItem* cur = ui.productListWidget->currentItem();
	if (cur)
	{
		auto ds = serviceFormDetailsFromJson(cur->data(Qt::UserRole).toString());
		populateFormDetail(ui.productMapNameEdit, nullptr,
			ui.productSizeEdit, nullptr, ui.productKindTable,
			ds.isEmpty() ? ServiceFormDetail() : ds.first());
	}
	else
	{
		populateFormDetail(ui.productMapNameEdit, nullptr,
			ui.productSizeEdit, nullptr, ui.productKindTable, ServiceFormDetail());
	}
}
void ServiceRecordDialog::onProductListCurrentChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
	if (previous)
	{
		ServiceFormDetail d = collectFormDetail(ui.productMapNameEdit, nullptr,
			ui.productSizeEdit, nullptr, ui.productKindTable);
		previous->setData(Qt::UserRole, serviceFormDetailsToJson({ d }));
		previous->setText(formItemSummary(d));
	}
	if (current)
	{
		auto ds = serviceFormDetailsFromJson(current->data(Qt::UserRole).toString());
		populateFormDetail(ui.productMapNameEdit, nullptr,
			ui.productSizeEdit, nullptr, ui.productKindTable,
			ds.isEmpty() ? ServiceFormDetail() : ds.first());
	}
}

// 数据发送（无尺寸、材质）
void ServiceRecordDialog::onAddDataRecord()
{
	addFormRecord(ui.dataListWidget);
}
void ServiceRecordDialog::onDelDataRecord()
{
	deleteFormRecord(ui.dataListWidget);
	QListWidgetItem* cur = ui.dataListWidget->currentItem();
	if (cur)
	{
		auto ds = serviceFormDetailsFromJson(cur->data(Qt::UserRole).toString());
		populateFormDetail(ui.dataMapNameEdit, ui.dataMappingRegionEdit,
			nullptr, nullptr, ui.dataKindTable,
			ds.isEmpty() ? ServiceFormDetail() : ds.first());
	}
	else
	{
		populateFormDetail(ui.dataMapNameEdit, ui.dataMappingRegionEdit,
			nullptr, nullptr, ui.dataKindTable, ServiceFormDetail());
	}
}
void ServiceRecordDialog::onDataListCurrentChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
	if (previous)
	{
		ServiceFormDetail d = collectFormDetail(ui.dataMapNameEdit, ui.dataMappingRegionEdit,
			nullptr, nullptr, ui.dataKindTable);
		previous->setData(Qt::UserRole, serviceFormDetailsToJson({ d }));
		previous->setText(formItemSummary(d));
	}
	if (current)
	{
		auto ds = serviceFormDetailsFromJson(current->data(Qt::UserRole).toString());
		populateFormDetail(ui.dataMapNameEdit, ui.dataMappingRegionEdit,
			nullptr, nullptr, ui.dataKindTable,
			ds.isEmpty() ? ServiceFormDetail() : ds.first());
	}
}

// 其他（自定义服务形式）
void ServiceRecordDialog::onAddOtherRecord()
{
	addFormRecord(ui.otherListWidget);
}
void ServiceRecordDialog::onDelOtherRecord()
{
	deleteFormRecord(ui.otherListWidget);
	QListWidgetItem* cur = ui.otherListWidget->currentItem();
	if (cur)
	{
		auto ds = serviceFormDetailsFromJson(cur->data(Qt::UserRole).toString());
		populateFormDetail(ui.otherMapNameEdit, ui.otherMappingRegionEdit,
			ui.otherSizeEdit, ui.otherMaterialEdit, ui.otherKindTable,
			ds.isEmpty() ? ServiceFormDetail() : ds.first());
	}
	else
	{
		populateFormDetail(ui.otherMapNameEdit, ui.otherMappingRegionEdit,
			ui.otherSizeEdit, ui.otherMaterialEdit, ui.otherKindTable, ServiceFormDetail());
	}
}
void ServiceRecordDialog::onOtherListCurrentChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
	if (previous)
	{
		ServiceFormDetail d = collectFormDetail(ui.otherMapNameEdit, ui.otherMappingRegionEdit,
			ui.otherSizeEdit, ui.otherMaterialEdit, ui.otherKindTable);
		previous->setData(Qt::UserRole, serviceFormDetailsToJson({ d }));
		previous->setText(formItemSummary(d));
	}
	if (current)
	{
		auto ds = serviceFormDetailsFromJson(current->data(Qt::UserRole).toString());
		populateFormDetail(ui.otherMapNameEdit, ui.otherMappingRegionEdit,
			ui.otherSizeEdit, ui.otherMaterialEdit, ui.otherKindTable,
			ds.isEmpty() ? ServiceFormDetail() : ds.first());
	}
}

// ============================================================================
// 组装/拆解
// ============================================================================

ServiceRecord ServiceRecordDialog::buildRecord() const
{
	ServiceRecord rec;
	rec.id = mEditId;
	collectBasicInfo(rec);
	collectCompileDetails(rec);
	collectPrintDetails(rec);
	collectMountingDetails(rec);
	collectProductDetails(rec);
	collectDataDetails(rec);
	collectOtherDetails(rec);
	collectLinkedProducts(rec);

	// 覆盖区域：自动汇总各类地图服务形式的"制图区域"（去重，多个以英文分号";"分隔）；
	// 若均未填写制图区域，则保留界面上已有的覆盖区域（兼容历史手工录入数据）
	const QString autoCoverageArea = collectMappingRegions().join(QStringLiteral(";"));
	if (!autoCoverageArea.isEmpty())
		rec.coverageArea = autoCoverageArea;

	rec.status = ServiceStatus::Pending;

	// 审计信息
	rec.createdBy = gCurrentUserSession.userName;
	rec.updatedBy = gCurrentUserSession.userName;

	return rec;
}

void ServiceRecordDialog::loadRecord(const ServiceRecord& record)
{
	populateBasicInfo(record);
	populateCompileDetails(record);
	populatePrintDetails(record);
	populateMountingDetails(record);
	populateProductDetails(record);
	populateDataDetails(record);
	populateOtherDetails(record);
	populateLinkedProducts(record);

	// 覆盖区域：以各类地图服务形式的"制图区域"为准（去重，英文分号分隔）；
	// 若均未填写制图区域，则保留记录中已保存的覆盖区域
	const QString autoCoverageArea = collectMappingRegions().join(QStringLiteral(";"));
	ui.coverageAreaEdit->setText(autoCoverageArea.isEmpty()
		? record.coverageArea.trimmed() : autoCoverageArea);
}

// ============================================================================
// 服务单号生成
// ============================================================================

QString ServiceRecordDialog::generateServiceNumber() const
{
	ServiceDAO dao;
	return dao.generateServiceNumber();
}

// ============================================================================
// 辅助
// ============================================================================

QString ServiceRecordDialog::currentServiceForm() const
{
	int idx = ui.serviceFormTab->currentIndex();
	switch (idx)
	{
	case 0: return QStringLiteral("地图编制");
	case 1: return QStringLiteral("地图打印");
	case 2: return QStringLiteral("地图装裱");
	case 3: return QStringLiteral("地图成品");
	case 4: return QStringLiteral("数据发送");
	case 5: return QStringLiteral("其他");
	default: return QStringLiteral("地图编制");
	}
}

// ============================================================================
// 覆盖区域：汇总各类地图服务形式的"制图区域"
// ============================================================================

// 汇总各服务形式Tab的"制图区域"（去重，保持出现顺序）
QStringList ServiceRecordDialog::collectMappingRegions() const
{
	QStringList regions;
	auto addRegion = [&regions](const QString& region)
	{
		const QString text = region.trimmed();
		if (text.isEmpty()) return;
		if (!regions.contains(text, Qt::CaseInsensitive))
			regions.append(text);
	};

	// 各Tab记录列表：跳过当前编辑项（其内容以编辑表单为准，列表内可能仍是旧值）
	auto addFromList = [&addRegion](QListWidget* list)
	{
		if (!list) return;
		const int currentRow = list->currentRow();
		for (int i = 0; i < list->count(); ++i)
		{
			if (i == currentRow) continue;
			const auto details = serviceFormDetailsFromJson(list->item(i)->data(Qt::UserRole).toString());
			for (const auto& d : details)
				addRegion(d.mappingRegion);
		}
	};

	addFromList(ui.compileListWidget);   // 地图编制
	addFromList(ui.printListWidget);     // 地图打印
	addFromList(ui.mountingListWidget);  // 地图装裱
	addFromList(ui.productListWidget);   // 地图成品（无制图区域字段）
	addFromList(ui.dataListWidget);      // 数据发送
	addFromList(ui.otherListWidget);     // 其他

	// 当前编辑项：以编辑表单内容为准（仅当该Tab存在选中记录时）
	if (ui.compileListWidget->currentItem())  addRegion(ui.mappingRegionEdit->text());
	if (ui.printListWidget->currentItem())    addRegion(ui.printMappingRegionEdit->text());
	if (ui.mountingListWidget->currentItem()) addRegion(ui.mountingMappingRegionEdit->text());
	if (ui.dataListWidget->currentItem())     addRegion(ui.dataMappingRegionEdit->text());
	if (ui.otherListWidget->currentItem())    addRegion(ui.otherMappingRegionEdit->text());

	return regions;
}

// 用制图区域刷新"覆盖区域"输入框：去重后以英文分号";"分隔
void ServiceRecordDialog::refreshCoverageArea()
{
	const QString derived = collectMappingRegions().join(QStringLiteral(";"));
	// 各类服务形式均未填写制图区域时，保留已填写的覆盖区域（兼容历史手工录入数据）
	if (derived.isEmpty() && !ui.coverageAreaEdit->text().trimmed().isEmpty())
		return;
	if (ui.coverageAreaEdit->text() != derived)
		ui.coverageAreaEdit->setText(derived);
}

// ============================================================================
// Slots — 保存/取消
// ============================================================================

void ServiceRecordDialog::onSave()
{
	QgsMessageLog::logMessage(
		QStringLiteral("[保存服务单] 开始保存，mEditId=%1").arg(mEditId),
		QStringLiteral("MapProductTools"), Qgis::Info);

	// 基本校验
	if (ui.unitCombo->currentText().trimmed().isEmpty())
	{
		QgsMessageLog::logMessage(
			QStringLiteral("[保存服务单] 校验失败：服务单位为空"),
			QStringLiteral("MapProductTools"), Qgis::Warning);
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请填写服务单位"));
		return;
	}

	ServiceRecord rec = buildRecord();

	QgsMessageLog::logMessage(
		QStringLiteral("[保存服务单] 数据组装完成，服务单位=%1, 服务形式=%2, 状态=%3")
			.arg(rec.unitName).arg(rec.serviceForm).arg(static_cast<int>(rec.status)),
		QStringLiteral("MapProductTools"), Qgis::Info);

	ServiceDAO dao;
	bool ok = false;

	if (mEditId > 0)
	{
		// 更新
		QgsMessageLog::logMessage(
			QStringLiteral("[保存服务单] 执行更新操作，id=%1").arg(mEditId),
			QStringLiteral("MapProductTools"), Qgis::Info);
		ok = dao.updateRecord(rec);
	}
	else
	{
		// 新增
		rec.serviceNumber = generateServiceNumber();
		// 确保 service_number 不重复
		rec.createdBy = gCurrentUserSession.userName;
		QgsMessageLog::logMessage(
			QStringLiteral("[保存服务单] 执行新增操作，服务单号=%1, 创建人=%2")
				.arg(rec.serviceNumber).arg(rec.createdBy),
			QStringLiteral("MapProductTools"), Qgis::Info);
		ok = dao.insertRecord(rec);
	}

	if (ok)
	{
		QgsMessageLog::logMessage(
			QStringLiteral("[保存服务单] 保存成功，id=%1, 服务单号=%2")
				.arg(rec.id).arg(rec.serviceNumber),
			QStringLiteral("MapProductTools"), Qgis::Info);
		emit recordSaved(rec);
		accept();
	}
	else
	{
		QgsMessageLog::logMessage(
			QStringLiteral("[保存服务单] 保存失败！"),
			QStringLiteral("MapProductTools"), Qgis::Critical);
		QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("保存服务单失败，请检查数据库连接。"));
	}
}

void ServiceRecordDialog::onCancel()
{
	reject();
}

// ============================================================================
// Slots — 浏览
// ============================================================================

void ServiceRecordDialog::onUploadApprovalFile()
{
	QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择电子审批单"),
		QString(),
		QStringLiteral("图片及PDF文件 (*.jpg *.jpeg *.png *.bmp *.gif *.pdf);;所有文件 (*.*)"));
	if (path.isEmpty()) return;

	// 上传到 PostgreSQL BLOB（支持跨机器访问，替代本地 product_storage 双写）
	int oid = PostgisConnector::instance()->loImport(path);
	if (oid > 0)
	{
		mApprovalFileOid = oid;
	}
	else
	{
		QString err = PostgisConnector::instance()->lastError();
		QMessageBox::warning(this, QStringLiteral("上传失败"),
			QStringLiteral("审批单 BLOB 上传失败: %1").arg(err));
		return;
	}

	// 文件路径仍保留原始值供展示
	ui.approvalFilePathEdit->setText(path);
	QgsMessageLog::logMessage(
		QStringLiteral("[上传审批单] 源文件=%1, OID=%2").arg(path).arg(oid),
		QStringLiteral("MapProductTools"), Qgis::Info);
}

void ServiceRecordDialog::onViewApprovalFile()
{
	QString path = ui.approvalFilePathEdit->text().trimmed();

	// 优先从 BLOB 导出（跨机器可用）
	if (mApprovalFileOid > 0)
	{
		QString tempPath = QDir::tempPath() + "/pmt_approval_view/";
		QDir().mkpath(tempPath);
		QFileInfo fi(path.isEmpty() ? QStringLiteral("approval_file") : path);
		QString ext = fi.suffix().isEmpty() ? QStringLiteral("pdf") : fi.suffix();
		QString exportPath = tempPath + QString("approval_%1.%2").arg(mApprovalFileOid).arg(ext);

		if (PostgisConnector::instance()->loExport(mApprovalFileOid, exportPath))
		{
			QDesktopServices::openUrl(QUrl::fromLocalFile(exportPath));
			return;
		}
		// BLOB 导出失败，继续尝试本地路径（回退）
		QgsMessageLog::logMessage(
			QStringLiteral("[查看审批单] BLOB 导出失败 OID=%1, 尝试本地路径").arg(mApprovalFileOid),
			QStringLiteral("MapProductTools"), Qgis::Warning);
	}

	// 回退：本地文件路径
	if (path.isEmpty())
	{
		QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("暂无审批单文件"));
		return;
	}
	if (!QFileInfo::exists(path))
	{
		QMessageBox::warning(this, QStringLiteral("无法查看"),
			QStringLiteral("文件不存在：%1").arg(path));
		return;
	}
	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void ServiceRecordDialog::onClearApprovalFile()
{
	ui.approvalFilePathEdit->clear();
	mApprovalFileOid = 0;
}

// ============================================================================
// Slots — 新增单位
// ============================================================================

void ServiceRecordDialog::onAddUnit()
{
	QString name = ui.unitCombo->currentText().trimmed();
	if (name.isEmpty())
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入服务单位名称"));
		return;
	}

	ServiceDAO dao;
	int newId = dao.getOrCreateUnitId(name);
	if (newId > 0)
	{
		// 刷新单位列表
		ui.unitCombo->clear();
		QList<QVariantMap> units = dao.getUnitList();
		for (const auto& u : units)
			ui.unitCombo->addItem(u.value("name").toString(), u.value("id").toInt());

		int idx = ui.unitCombo->findText(name);
		if (idx >= 0)
			ui.unitCombo->setCurrentIndex(idx);

		QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("服务单位已添加"));
	}
}

void ServiceRecordDialog::onDeleteUnit()
{
	QString name = ui.unitCombo->currentText().trimmed();
	if (name.isEmpty())
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择或输入要删除的服务单位"));
		return;
	}

	int idx = ui.unitCombo->findText(name);
	if (idx < 0)
	{
		// 当前输入内容不在列表中，属于新增未保存项，直接清空输入
		ui.unitCombo->setCurrentText(QString());
		return;
	}

	int unitId = ui.unitCombo->itemData(idx).toInt();

	QMessageBox::StandardButton ret = QMessageBox::question(this, QStringLiteral("确认删除"),
		QStringLiteral("确定要删除服务单位「%1」吗？").arg(name),
		QMessageBox::Yes | QMessageBox::No);
	if (ret != QMessageBox::Yes)
		return;

	ServiceDAO dao;
	if (dao.deleteUnit(unitId))
	{
		ui.unitCombo->removeItem(idx);
		QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("服务单位「%1」已删除").arg(name));
	}
	else
	{
		QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("删除服务单位失败，请检查数据库连接"));
	}
}

// ============================================================================
// 通用：向指定下拉框新增自定义选项并持久化到字典表
// ============================================================================

void ServiceRecordDialog::addDictOptionToCombo(QComboBox* combo, const QString& dictType, const QString& title)
{
	if (!combo) return;

	QString name = combo->currentText().trimmed();
	if (name.isEmpty())
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入%1名称").arg(title));
		return;
	}

	ServiceDAO dao;
	if (dao.addDictOption(dictType, name))
	{
		// 已存在则直接选中
		int idx = combo->findText(name);
		if (idx < 0)
		{
			combo->addItem(name);
			idx = combo->findText(name);
		}
		combo->setCurrentIndex(idx);
		QMessageBox::information(this, QStringLiteral("成功"),
			QStringLiteral("%1「%2」已添加").arg(title, name));
	}
	else
	{
		QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("添加%1失败，请检查数据库连接").arg(title));
	}
}

void ServiceRecordDialog::onAddScenario()
{
	addDictOptionToCombo(ui.scenarioCombo, "scenario", QStringLiteral("应用场景"));
}

void ServiceRecordDialog::onAddLevel()
{
	addDictOptionToCombo(ui.serviceLevelCombo, "level", QStringLiteral("服务等级"));
}

void ServiceRecordDialog::onAddCategory()
{
	addDictOptionToCombo(ui.categoryCombo, "category", QStringLiteral("服务类别"));
}

void ServiceRecordDialog::onAddMaterial()
{
	// 根据点击的新增按钮确定对应的材质下拉框
	QComboBox* combo = nullptr;
	if (sender() == ui.addPrintMaterialBtn)
		combo = ui.printMaterialEdit;
	else if (sender() == ui.addMountingMaterialBtn)
		combo = ui.mountingMaterialEdit;
	else if (sender() == ui.addOtherMaterialBtn)
		combo = ui.otherMaterialEdit;

	addDictOptionToCombo(combo, "material", QStringLiteral("材质"));
}

void ServiceRecordDialog::onAddMountingSize()
{
	addDictOptionToCombo(ui.mountingSizeEdit, "mounting_size", QStringLiteral("装裱尺寸"));
}

// ============================================================================
// 通用：从下拉框中删除当前选中的自定义选项并持久化到字典表
// ============================================================================

void ServiceRecordDialog::deleteDictOptionFromCombo(QComboBox* combo, const QString& dictType, const QString& title)
{
	if (!combo) return;

	QString name = combo->currentText().trimmed();
	if (name.isEmpty())
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择或输入要删除的%1").arg(title));
		return;
	}

	int idx = combo->findText(name);
	if (idx < 0)
	{
		// 当前输入内容不在下拉列表中，属于新增未保存项，直接清空输入即可
		combo->setCurrentText(QString());
		return;
	}

	QMessageBox::StandardButton ret = QMessageBox::question(this, QStringLiteral("确认删除"),
		QStringLiteral("确定要删除%1「%2」吗？").arg(title, name),
		QMessageBox::Yes | QMessageBox::No);
	if (ret != QMessageBox::Yes)
		return;

	ServiceDAO dao;
	if (dao.deleteDictOption(dictType, name))
	{
		combo->removeItem(idx);
		QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("%1「%2」已删除").arg(title, name));
	}
	else
	{
		QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("删除%1失败，请检查数据库连接").arg(title));
	}
}

void ServiceRecordDialog::onDeleteScenario()
{
	deleteDictOptionFromCombo(ui.scenarioCombo, "scenario", QStringLiteral("应用场景"));
}

void ServiceRecordDialog::onDeleteLevel()
{
	deleteDictOptionFromCombo(ui.serviceLevelCombo, "level", QStringLiteral("服务等级"));
}

void ServiceRecordDialog::onDeleteCategory()
{
	deleteDictOptionFromCombo(ui.categoryCombo, "category", QStringLiteral("服务类别"));
}

void ServiceRecordDialog::onDeleteMaterial()
{
	// 根据点击的删除按钮确定对应的材质下拉框
	QComboBox* combo = nullptr;
	if (sender() == ui.delPrintMaterialBtn)
		combo = ui.printMaterialEdit;
	else if (sender() == ui.delMountingMaterialBtn)
		combo = ui.mountingMaterialEdit;
	else if (sender() == ui.delOtherMaterialBtn)
		combo = ui.otherMaterialEdit;

	deleteDictOptionFromCombo(combo, "material", QStringLiteral("材质"));
}

void ServiceRecordDialog::onDeleteMountingSize()
{
	deleteDictOptionFromCombo(ui.mountingSizeEdit, "mounting_size", QStringLiteral("装裱尺寸"));
}

// ============================================================================
// Slots — 关联地图成果
// ============================================================================

void ServiceRecordDialog::onLinkProduct()
{
	// 打开目录树选择弹窗（与"成果存储与管理 → 按地图成果分类"一致的树控件）
	ProductDAO dao;

	QDialog dlg(this);
	dlg.setWindowTitle(QStringLiteral("关联成果"));
	dlg.resize(640, 720);

	auto* mainLayout = new QVBoxLayout(&dlg);

	auto* tip = new QLabel(QStringLiteral("请在左侧目录树中勾选要关联的成果节点（可多选），"
		"选中节点下的地图成果将被关联到服务单。"), &dlg);
	tip->setWordWrap(true);
	mainLayout->addWidget(tip);

	auto* tree = new QTreeWidget(&dlg);
	tree->setHeaderLabels({ QStringLiteral("地图成果分类") });
	tree->setSelectionMode(QAbstractItemView::SingleSelection);
	tree->setExpandsOnDoubleClick(true);
	{
		QFont tf = tree->font();
		tf.setFamily(QStringLiteral("微软雅黑"));
		tf.setPointSize(9);
		tree->setFont(tf);
		QFont hf = tree->header()->font();
		hf.setFamily(QStringLiteral("微软雅黑"));
		hf.setPointSize(9);
		tree->header()->setFont(hf);
	}
	mainLayout->addWidget(tree, 1);

	auto* btnRow = new QHBoxLayout();
	auto* selectAllBtn = new QPushButton(QStringLiteral("全选"), &dlg);
	auto* deselectAllBtn = new QPushButton(QStringLiteral("清空"), &dlg);
	btnRow->addWidget(selectAllBtn);
	btnRow->addWidget(deselectAllBtn);
	btnRow->addStretch();
	auto* okBtn = new QPushButton(QStringLiteral("确定"), &dlg);
	auto* cancelBtn = new QPushButton(QStringLiteral("取消"), &dlg);
	btnRow->addWidget(okBtn);
	btnRow->addWidget(cancelBtn);
	mainLayout->addLayout(btnRow);

	// 构建与成果存储一致的目录树
	QSettings settings(QStringLiteral("GarMap"), QStringLiteral("MapProductManager"));
	QString dbName = settings.value(QStringLiteral("db/database"), QStringLiteral("数据库")).toString();
	if (dbName.isEmpty()) dbName = QStringLiteral("数据库");

	auto* root = new QTreeWidgetItem(tree);
	root->setText(0, dbName);
	root->setData(0, Qt::UserRole, 0);
	root->setIcon(0, dlg.style()->standardIcon(QStyle::SP_DirHomeIcon));
	root->setFlags(root->flags() | Qt::ItemIsUserCheckable);
	root->setCheckState(0, Qt::Unchecked);

	// 幂等创建固定目录节点，确保树结构与成果存储一致
	auto ids = DirectoryHelper::ensureFixedDirectories(dao);

	// 需要在节点下展示产品数据子节点的固定叶子节点
	QSet<int> productLeafDirIds;
	productLeafDirIds << ids.resultAI << ids.resultPDF << ids.resultOther
	                  << ids.elementImage << ids.elementShading
	                  << ids.materialDoc << ids.materialTable << ids.materialOther;
	populateDirTreeChildren(root, 0, dao, productLeafDirIds);

	// 为所有目录节点开启复选框
	std::function<void(QTreeWidgetItem*)> enableCheck = [&](QTreeWidgetItem* item) {
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(0, Qt::Unchecked);
		for (int i = 0; i < item->childCount(); ++i)
			enableCheck(item->child(i));
	};
	enableCheck(root);
	tree->expandAll();

	// 级联勾选：勾选/取消父节点时，同步其所有子节点
	bool updatingCheck = false;
	QObject::connect(tree, &QTreeWidget::itemChanged, tree, [&updatingCheck](QTreeWidgetItem* item, int column) {
		if (updatingCheck || column != 0) return;
		updatingCheck = true;
		Qt::CheckState state = item->checkState(0);
		std::function<void(QTreeWidgetItem*)> apply = [&](QTreeWidgetItem* it) {
			for (int i = 0; i < it->childCount(); ++i)
			{
				it->child(i)->setCheckState(0, state);
				apply(it->child(i));
			}
		};
		apply(item);
		updatingCheck = false;
	});

	// 全选 / 清空
	auto setAllCheck = [tree, &updatingCheck](Qt::CheckState state) {
		updatingCheck = true;
		std::function<void(QTreeWidgetItem*)> apply = [&](QTreeWidgetItem* item) {
			item->setCheckState(0, state);
			for (int i = 0; i < item->childCount(); ++i)
				apply(item->child(i));
		};
		for (int i = 0; i < tree->topLevelItemCount(); ++i)
			apply(tree->topLevelItem(i));
		updatingCheck = false;
	};
	QObject::connect(selectAllBtn, &QPushButton::clicked, [tree, setAllCheck]() {
		Q_UNUSED(tree);
		setAllCheck(Qt::Checked);
	});
	QObject::connect(deselectAllBtn, &QPushButton::clicked, [tree, setAllCheck]() {
		Q_UNUSED(tree);
		setAllCheck(Qt::Unchecked);
	});
	QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

	// 确定：收集勾选节点下的产品
	QObject::connect(okBtn, &QPushButton::clicked, &dlg, [&dlg, tree, &dao]() {
		bool any = false;
		std::function<void(QTreeWidgetItem*)> collect = [&](QTreeWidgetItem* item) {
			if (item->checkState(0) == Qt::Checked)
			{
				// 产品数据节点：直接视为有成果
				if (item->data(0, Qt::UserRole + 1).toInt() == 1)
				{
					any = true;
				}
				else
				{
					int dirId = item->data(0, Qt::UserRole).toInt();
					if (dirId > 0 && !dao.getProductsByDirectory(dirId).isEmpty())
						any = true;
				}
			}
			for (int i = 0; i < item->childCount(); ++i)
				collect(item->child(i));
		};
		for (int i = 0; i < tree->topLevelItemCount(); ++i)
			collect(tree->topLevelItem(i));
		if (!any)
		{
			QMessageBox::warning(&dlg, QStringLiteral("提示"), QStringLiteral("请勾选包含地图成果的目录节点。"));
			return;
		}
		dlg.accept();
	});

	if (dlg.exec() != QDialog::Accepted) return;

	// 收集勾选节点下的产品（去重）
	QListWidget* lw = ui.linkedProductList;
	QSet<QString> existingIds;
	for (int i = 0; i < lw->count(); ++i)
		existingIds.insert(lw->item(i)->data(Qt::UserRole).toString());

	int addedCount = 0;
	std::function<void(QTreeWidgetItem*)> collectProducts = [&](QTreeWidgetItem* item) {
		if (item->checkState(0) == Qt::Checked)
		{
			// 产品数据节点：直接关联该产品
			if (item->data(0, Qt::UserRole + 1).toInt() == 1)
			{
				int pid = item->data(0, Qt::UserRole).toInt();
				auto meta = dao.getProduct(pid);
				if (meta.id > 0)
				{
					QString idStr = QString::number(meta.id);
					if (!existingIds.contains(idStr))
					{
						existingIds.insert(idStr);
						QString displayName = meta.productName.isEmpty() ? QStringLiteral("未命名成果") : meta.productName;
						QListWidgetItem* item2 = new QListWidgetItem(displayName);
						item2->setData(Qt::UserRole, idStr);
						lw->addItem(item2);
						++addedCount;
					}
				}
			}
			else
			{
				int dirId = item->data(0, Qt::UserRole).toInt();
				if (dirId > 0)
				{
					const auto prods = dao.getProductsByDirectory(dirId);
					for (const auto& p : prods)
					{
						QString idStr = QString::number(p.id);
						if (existingIds.contains(idStr)) continue;
						existingIds.insert(idStr);
						QString displayName = p.productName.isEmpty() ? QStringLiteral("未命名成果") : p.productName;
						QListWidgetItem* item2 = new QListWidgetItem(displayName);
						item2->setData(Qt::UserRole, idStr);
						lw->addItem(item2);
						++addedCount;
					}
				}
			}
		}
		for (int i = 0; i < item->childCount(); ++i)
			collectProducts(item->child(i));
	};
	for (int i = 0; i < tree->topLevelItemCount(); ++i)
		collectProducts(tree->topLevelItem(i));

	if (addedCount == 0)
		QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("所选节点下没有可新增关联的成果。"));
}

void ServiceRecordDialog::onClearLink()
{
	ui.linkedProductList->clear();
}

void ServiceRecordDialog::onUploadProduct()
{
	// 1. 打开文件选择对话框
	QString filePath = QFileDialog::getOpenFileName(this,
		QStringLiteral("选择地图成果文件"), QString(),
		QStringLiteral("所有支持格式 (*.shp *.geojson *.kml *.gdb *.mdb *.tif *.tiff *.img *.jpg *.jp2 *.png *.pdf *.ai *.cdr *.dwg *.dxf);;"
			"矢量文件 (*.shp *.geojson *.kml *.gdb *.mdb);;"
			"栅格文件 (*.tif *.tiff *.img *.jpg *.jp2);;"
			"制图文件 (*.pdf *.ai *.cdr *.dwg *.dxf);;"
			"其它类型文件 (*.*)"));
	if (filePath.isEmpty()) return;

	QFileInfo fi(filePath);

	// 2. 准备上传组件
	MetadataExtractor extractor;
	ProductDAO dao;
	DataImporter importer;
	SchemaManager schemaMgr;

	int successCount = 0;
	int failCount = 0;

	// 3. 构建待上传列表：(源路径, 产品名称, GDB图层信息)
	struct UploadEntry {
		QString srcPath;
		QString productName;
		bool    isGdbLayer = false;
		QString gdbPath;
		QString layerName;
	};
	QList<UploadEntry> entries;

	if (fi.suffix().compare(QStringLiteral("gdb"), Qt::CaseInsensitive) == 0 ||
		fi.suffix().compare(QStringLiteral("mdb"), Qt::CaseInsensitive) == 0)
	{
		GDBLayerSelectorDialog layerDlg(filePath, this);
		if (layerDlg.exec() != QDialog::Accepted)
			return;
		QStringList layers = layerDlg.selectedLayers();
		if (layers.isEmpty())
		{
			QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("未选择任何图层"));
			return;
		}
		QString baseName = fi.baseName();
		for (const QString& layer : layers)
		{
			UploadEntry e;
			e.srcPath     = filePath;
			e.productName = baseName + QStringLiteral("_") + layer;
			e.isGdbLayer  = true;
			e.gdbPath     = filePath;
			e.layerName   = layer;
			entries << e;
		}
	}
	else
	{
		UploadEntry e;
		e.srcPath     = filePath;
		e.productName = fi.baseName();
		entries << e;
	}

	// 4. 逐个上传
	qDebug() << "[ServiceRecordDialog] 开始上传" << entries.size() << "个地图成果";
	QgsMessageLog::logMessage(QStringLiteral("开始上传 %1 个地图成果").arg(entries.size()), "MapProductTools", Qgis::Info);
	for (const UploadEntry& entry : entries)
	{
		ProductMetadata meta;

		qDebug() << "[ServiceRecordDialog] 处理:" << entry.productName
			<< "gdbLayer:" << entry.isGdbLayer << "src:" << entry.srcPath;
		QgsMessageLog::logMessage(QStringLiteral("处理: %1 (GDB=%2)").arg(entry.productName).arg(entry.isGdbLayer), "MapProductTools", Qgis::Info);

		// 4a. 提取元数据
		if (entry.isGdbLayer)
		{
			meta = extractor.extractLayerMetadata(entry.gdbPath, entry.layerName);
			meta.fileFormat = QFileInfo(entry.gdbPath).suffix().toLower();
			meta.filePath   = entry.gdbPath;
			meta.fileSize   = QFileInfo(entry.gdbPath).size();
			// GDB 图层元数据不含 productType，需手动设置
			meta.productType = ProductType::Vector;
		}
		else
		{
			meta = extractor.extractMetadata(entry.srcPath);
		}

		meta.productName    = entry.productName;
		meta.dataId           = QUuid::createUuid().toString(QUuid::WithoutBraces);
		meta.currentVersion = 1;
		meta.securityLevel  = SecurityLevel::Unclassified;
		meta.createdBy      = QStringLiteral("YOUR_PASSWORD");
		meta.updatedBy      = QStringLiteral("YOUR_PASSWORD");

		qDebug() << "[ServiceRecordDialog] 元数据提取完成: type:" << static_cast<int>(meta.productType)
			<< "crs:" << meta.crs << "geomType:" << meta.geometryType
			<< "format:" << meta.fileFormat << "size:" << meta.fileSize;
		QgsMessageLog::logMessage(QStringLiteral("元数据: %1 type=%2 crs=%3 fmt=%4")
			.arg(entry.productName).arg(static_cast<int>(meta.productType)).arg(meta.crs).arg(meta.fileFormat),
			"MapProductTools", Qgis::Info);

		// 4b. 查找/创建产品目录
		QString typeDirName = productTypeToString(meta.productType);
		int dirId = dao.findOrCreateDirectory(typeDirName, 0);
		if (dirId > 0)
		{
			meta.parentDirId   = dirId;
			meta.directoryPath = typeDirName;
		}

		// 4c. 文件数据由 BLOB 存储，本地只保留原始路径引用
		meta.filePath = entry.srcPath;

		// 4d. 生成缩略图
		QString thumbPath = QDir::tempPath() + "/pmt_thumbnails/" + meta.dataId + ".png";
		QDir().mkpath(QFileInfo(thumbPath).absolutePath());
		if (MetadataExtractor::generateThumbnail(entry.srcPath, thumbPath))
			meta.thumbnailPath = thumbPath;

		// 4e. PostGIS 入库（矢量/栅格）
		int featureCount = 0;
		if (meta.productType == ProductType::Vector || meta.productType == ProductType::Raster)
		{
			// 生成安全表名
			QString safeName = entry.productName;
			safeName.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9_\\x{4e00}-\\x{9fff}]")), QStringLiteral("_"));
			safeName = safeName.toLower();
			if (!safeName.isEmpty() && !safeName.at(0).isLetter())
				safeName = QStringLiteral("l_") + safeName;
			if (safeName.isEmpty())
				safeName = QStringLiteral("layer_data");
			meta.layerTableName = safeName + "_v" + QString::number(meta.currentVersion);

			if (meta.productType == ProductType::Vector)
			{
				int srcSrid = 0;
				if (meta.crs.startsWith(QStringLiteral("EPSG:")))
					srcSrid = meta.crs.mid(5).toInt();

				if (entry.isGdbLayer)
					featureCount = importer.importVectorLayerToPostGIS(entry.gdbPath, entry.layerName, safeName, srcSrid, 4490);
				else
					featureCount = importer.importVectorToPostGIS(entry.srcPath, safeName, srcSrid, 4490);

				if (featureCount <= 0)
				{
					const VectorImportStats& stats = importer.lastVectorStats();
					QString errDetail = importer.lastError();
					if (stats.totalRead > 0)
					{
						errDetail += QStringLiteral("\n[统计] 读取=%1 无几何=%2 INSERT失败=%3")
							.arg(stats.totalRead).arg(stats.skipNoGeom).arg(stats.skipInsertFail);
					}

					qDebug() << "[ServiceRecordDialog] PostGIS 矢量导入失败:" << entry.srcPath
						<< "表名:" << safeName << "错误:" << errDetail;
					QgsMessageLog::logMessage(QStringLiteral("PostGIS 矢量导入失败: %1 表=%2 错误=%3")
						.arg(entry.productName).arg(safeName).arg(errDetail), "MapProductTools", Qgis::Critical);
					QMessageBox::warning(this, QStringLiteral("上传失败"),
						QStringLiteral("PostGIS 矢量导入失败: %1\n%2").arg(entry.productName).arg(errDetail));
					schemaMgr.dropDataLayerTable(safeName);
					failCount++;
					continue;
				}
			}
			else  // Raster
			{
				if (!importer.importRasterToPostGIS(entry.srcPath, safeName, 4490))
				{
					qDebug() << "[ServiceRecordDialog] PostGIS 栅格导入失败:" << entry.srcPath
						<< "表名:" << safeName << "错误:" << importer.lastError();
					QgsMessageLog::logMessage(QStringLiteral("PostGIS 栅格导入失败: %1 表=%2 错误=%3")
						.arg(entry.productName).arg(safeName).arg(importer.lastError()), "MapProductTools", Qgis::Critical);
					QMessageBox::warning(this, QStringLiteral("上传失败"),
						QStringLiteral("PostGIS 栅格导入失败: %1\n%2").arg(entry.productName).arg(importer.lastError()));
					schemaMgr.dropDataLayerTable(safeName);
					failCount++;
					continue;
				}
				featureCount = 1;
			}
		}

		// 4f. 写入产品元数据
		int productId = dao.insertProduct(meta);
		if (productId <= 0)
		{
			qDebug() << "[ServiceRecordDialog] 产品元数据写入失败:" << entry.productName
				<< "type:" << static_cast<int>(meta.productType) << "uuid:" << meta.dataId;
			QgsMessageLog::logMessage(QStringLiteral("产品元数据写入失败: %1 uuid=%2")
				.arg(entry.productName).arg(meta.dataId), "MapProductTools", Qgis::Critical);
			QMessageBox::warning(this, QStringLiteral("上传失败"),
				QStringLiteral("产品元数据写入失败: %1\n请检查数据库连接是否正常。").arg(entry.productName));
			if (!meta.layerTableName.isEmpty())
				schemaMgr.dropDataLayerTable(meta.layerTableName);
			failCount++;
			continue;
		}

		dao.enrichWithTypeMeta(productId);

		// 4g. 创建版本记录
		VersionRecord verRec;
		verRec.productId     = productId;
		verRec.versionNumber = 1;
		verRec.filePath      = meta.filePath;
		verRec.fileHash      = meta.fileHash;
		verRec.fileSize      = meta.fileSize;
		verRec.fileFormat    = meta.fileFormat;
		verRec.layerTableName = meta.layerTableName;
		verRec.changedBy     = QStringLiteral("YOUR_PASSWORD");
		verRec.changeNote    = QString::fromUtf8("初次入库（服务单关联上传）");
		verRec.parentVersion = 0;  // 根版本，无父版本
		dao.insertVersionRecord(verRec);

		// 4h. 注册图层
		if (!meta.layerTableName.isEmpty())
		{
			dao.registerLayer(productId, meta.layerTableName,
				meta.productType == ProductType::Vector ? QStringLiteral("vector") : QStringLiteral("raster"),
				4490, featureCount);
		}

		// 4i. 添加到关联地图成果列表
		QListWidgetItem* item = new QListWidgetItem(entry.productName);
		item->setData(Qt::UserRole, QString::number(productId));
		item->setData(Qt::UserRole + 1, meta.dataId);
		ui.linkedProductList->addItem(item);

		QgsMessageLog::logMessage(QStringLiteral("上传成功: %1 (id=%2)").arg(entry.productName).arg(productId),
			"MapProductTools", Qgis::Info);
		successCount++;
	}

	// 5. 结果提示
	qDebug() << "[ServiceRecordDialog] 上传完成: success=" << successCount << "fail=" << failCount;
	QgsMessageLog::logMessage(QStringLiteral("上传完成: 成功=%1 失败=%2").arg(successCount).arg(failCount),
		"MapProductTools", successCount > 0 ? Qgis::Info : Qgis::Critical);
	if (successCount > 0 && failCount == 0)
	{
		QMessageBox::information(this, QStringLiteral("上传成功"),
			QStringLiteral("成功上传 %1 个地图成果并已关联到当前服务单。").arg(successCount));
	}
	else if (successCount > 0)
	{
		QMessageBox::warning(this, QStringLiteral("上传结果"),
			QStringLiteral("成功 %1 个，失败 %2 个。\n请查看 QGIS 日志面板获取详细错误信息。").arg(successCount).arg(failCount));
	}
	else
	{
		QMessageBox::critical(this, QStringLiteral("上传失败"),
			QStringLiteral("所有文件上传失败，请检查：\n"
				"1. 文件格式是否正确（.shp/.geojson/.gdb 等）\n"
				"2. PostGIS 数据库连接是否正常\n"
				"3. 工作目录是否有写入权限\n\n"
				"详细信息请查看 QGIS 日志面板（Ctrl+Alt+L）。"));
	}
}
