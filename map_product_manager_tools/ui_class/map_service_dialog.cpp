#include "map_service_dialog.h"
#include "service_record_dialog.h"
#include "database/service_dao.h"
#include "database/product_dao.h"
#include "database/product_metadata.h"
#include "database/postgis_connector.h"
#include "core/xlsx_writer.h"
#include "core/xlsx_template_filler.h"
#include "core/price_config.h"
#include "core/calendar_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <qgis.h>
#include <qgisinterface.h>
#include <qgsmessagebar.h>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QPainter>
#include <QPainterPath>

#include <QListWidget>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPointer>
#include <QScrollArea>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QDate>
#include <QSet>
#include <QFont>
#include <QPrinter>
#include <QPageSize>
#include <QPageLayout>
#include <QTextDocument>
#include <QPixmap>
#include <qgsmessagelog.h>
#include <algorithm>
#include <QDesktopWidget>
#include <QUrl>
#include <QProcess>
#include <QCoreApplication>
#include <QCompleter>
#include <QApplication>
#include <QProgressBar>
#include <QPrinter>
#include <QTextDocument>
#include <QSet>
#include <QPair>
#include <algorithm>
#include <cmath>
#include "qgsgui.h"

// 关联成果预览（矢量/栅格地图显示）
#include <libpq-fe.h>
#include <QMenu>
#include <QColor>
#include <QVariantMap>
#include <qgsmapcanvas.h>
#include <qgsmaptoolpan.h>
#include <qgsmaplayer.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsdatasourceuri.h>
#include <qgsproject.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>

// 判断文件是否被其他进程独占占用（Windows 上 Qt 以互斥方式打开，
// open(ReadWrite) 失败通常意味着文件正被 Excel/WPS 等程序锁定）
static bool isFileLocked(const QString& filePath)
{
	QFile f(filePath);
	if (!f.open(QIODevice::ReadWrite))
		return true;
	f.close();
	return false;
}

// ===================== 构造与析构 =====================

MapServiceDialog::MapServiceDialog(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
	, mDAO(new ServiceDAO(this))
{
	ui.setupUi(this);

	ui.mStatsScrollArea->setFrameShape(QFrame::NoFrame);
	auto applyTableStyle = [](QTableWidget* table) {
		if (!table) return;
		table->setAlternatingRowColors(true);
	};
	applyTableStyle(ui.mRecordTable);
	applyTableStyle(ui.mScenarioChart);
	applyTableStyle(ui.mUnitChart);
	applyTableStyle(ui.mLevelChart);
	applyTableStyle(ui.mAreaChart);
	applyTableStyle(ui.mMonthlyChart);
	applyTableStyle(ui.mSvcFormChart);
	applyTableStyle(ui.mExportHistoryTable);

	QgsGui::enableAutoGeometryRestore(this);
	setupTableColumns();
	setupFilterCombos();
	setupConnections();

	// ===== 统计分析 Tab：在日期筛选区添加频次粒度下拉框 =====
	mFreqGranularityCombo = new QComboBox(this);
	mFreqGranularityCombo->addItem(QString::fromUtf8("按月"), QStringLiteral("month"));
	mFreqGranularityCombo->addItem(QString::fromUtf8("按季度"), QStringLiteral("quarter"));
	mFreqGranularityCombo->addItem(QString::fromUtf8("按年"), QStringLiteral("year"));
	mFreqGranularityCombo->setCurrentIndex(0);
	connect(mFreqGranularityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &MapServiceDialog::onFrequencyGranularityChanged);

	// 将粒度下拉框加入统计筛选区布局
	QLabel* freqLabel = new QLabel(QString::fromUtf8("频次粒度:"), this);

	// 在 mStatsScrollContent 的 layout (statsLayout) 中查找 statsFilterLayout
	QHBoxLayout* statsFilterLayout = nullptr;
	if (ui.mStatsDateFrom->parentWidget())
	{
		QLayout* contentLayout = ui.mStatsDateFrom->parentWidget()->layout();
		if (contentLayout)
		{
			for (int i = 0; i < contentLayout->count(); ++i)
			{
				QLayoutItem* item = contentLayout->itemAt(i);
				QHBoxLayout* hbox = qobject_cast<QHBoxLayout*>(item->layout());
				if (hbox)
				{
					statsFilterLayout = hbox;
					break;
				}
			}
		}
	}

	if (statsFilterLayout)
	{
		statsFilterLayout->addWidget(freqLabel);
		statsFilterLayout->addWidget(mFreqGranularityCombo);
	}

	// ===== 动态统计 Tab：构建完整 UI =====
	setupDynamicStatsTab();

	setWindowTitle(QString::fromUtf8("地图服务管理"));
	resize(1200, 800);

	// 默认加载所有服务单
	performSearch();
}

MapServiceDialog::~MapServiceDialog()
{
}

// ===================== UI 初始化 =====================

void MapServiceDialog::setupTableColumns()
{
	// 服务台账表格 — UNV2：去掉状态、经费、图件种类、安排领导、服务数量，增加安排处室
	ui.mRecordTable->setColumnCount(12);
	ui.mRecordTable->setHorizontalHeaderLabels({
		QString::fromUtf8("序号"), QString::fromUtf8("日期"), QString::fromUtf8("应用场景"),
		QString::fromUtf8("覆盖区域"), QString::fromUtf8("服务单位"),
		QString::fromUtf8("类别"), QString::fromUtf8("服务等级"),
		QString::fromUtf8("服务形式"), QString::fromUtf8("安排处室"),
		QString::fromUtf8("关联成果"), QString::fromUtf8("接收人"),
		QString::fromUtf8("电子审批单")
	});
	ui.mRecordTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	// 电子审批单列允许手动调整宽度
	ui.mRecordTable->horizontalHeader()->setSectionResizeMode(11, QHeaderView::Interactive);
	ui.mRecordTable->horizontalHeader()->resizeSection(11, 240);
	ui.mRecordTable->horizontalHeader()->setStretchLastSection(false);
	// 覆盖区域列自动扩展
	ui.mRecordTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

	// 导出历史表格
	ui.mExportHistoryTable->setColumnCount(4);
	ui.mExportHistoryTable->setHorizontalHeaderLabels({
		QString::fromUtf8("文件名"), QString::fromUtf8("格式"),
		QString::fromUtf8("导出时间"), QString::fromUtf8("文件路径")
	});
	ui.mExportHistoryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui.mExportHistoryTable->horizontalHeader()->setStretchLastSection(true);
}

void MapServiceDialog::setupFilterCombos()
{
	// 服务单位下拉
	ui.mUnitCombo->clear();
	ui.mUnitCombo->addItem(QString::fromUtf8("-- 服务单位 --"), QString());

	// 从 service_units 表加载已有单位（与新增服务单一致）
	QList<QVariantMap> unitList = mDAO->getUnitList();
	for (const auto& u : unitList)
	{
		QString name = u.value("name").toString();
		if (!name.isEmpty())
			ui.mUnitCombo->addItem(name, name);
	}

	// 类别下拉（固定选项）
	ui.mScenarioCombo->clear();
	ui.mScenarioCombo->addItem(QString::fromUtf8("-- 类别 --"), QString());
	for (const auto& c : categoryOptions())
		ui.mScenarioCombo->addItem(c, c);

	// 服务等级下拉（固定选项，替换原"服务对象"）
	ui.mLevelCombo->clear();
	ui.mLevelCombo->addItem(QString::fromUtf8("-- 服务等级 --"), QString());
	for (const auto& t : serviceLevelOptions())
		ui.mLevelCombo->addItem(t, t);

	// 状态下拉
	ui.mStatusCombo->addItem(QString::fromUtf8("-- 状态 --"), -1);
	ui.mStatusCombo->addItem(serviceStatusToString(ServiceStatus::Pending), 0);
	ui.mStatusCombo->addItem(serviceStatusToString(ServiceStatus::Completed), 1);
	ui.mStatusCombo->addItem(serviceStatusToString(ServiceStatus::Cancelled), 2);
	ui.mStatusCombo->addItem(serviceStatusToString(ServiceStatus::Archived), 3);
}

void MapServiceDialog::setupConnections()
{
	// 台账 Tab
	connect(ui.mSearchBtn, &QPushButton::clicked, this, &MapServiceDialog::onSearch);
	connect(ui.mResetBtn, &QPushButton::clicked, this, &MapServiceDialog::onReset);
	connect(ui.mAddRecordBtn, &QPushButton::clicked, this, &MapServiceDialog::onAddRecord);
	connect(ui.mEditRecordBtn, &QPushButton::clicked, this, &MapServiceDialog::onEditRecord);
	connect(ui.mDeleteRecordBtn, &QPushButton::clicked, this, &MapServiceDialog::onDeleteRecord);
	connect(ui.mArchiveBtn, &QPushButton::clicked, this, &MapServiceDialog::onArchiveRecord);
	connect(ui.mManageUnitsBtn, &QPushButton::clicked, this, &MapServiceDialog::onManageUnits);
	connect(ui.mMountingLedgerBtn, &QPushButton::clicked, this, &MapServiceDialog::onMountingLedgerExport);
	connect(ui.mServiceLedgerExportBtn, &QPushButton::clicked, this, &MapServiceDialog::onServiceLedgerExport);
	connect(ui.mRecordTable, &QTableWidget::cellClicked, this, &MapServiceDialog::onRecordTableClicked);
	// 关联成果列右键预览
	ui.mRecordTable->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui.mRecordTable, &QTableWidget::customContextMenuRequested,
		this, &MapServiceDialog::onRecordTableContextMenu);
	connect(ui.mPrevPageBtn, &QPushButton::clicked, this, [this]() {
		if (mCurrentPage > 1) { mCurrentPage--; performSearch(); }
	});
	connect(ui.mNextPageBtn, &QPushButton::clicked, this, [this]() {
		if (mCurrentPage < mTotalPages) { mCurrentPage++; performSearch(); }
	});
	connect(ui.mPageSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int page) {
		if (page != mCurrentPage && page >= 1 && page <= mTotalPages) {
			mCurrentPage = page;
			performSearch();
		}
	});

	// 统计分析 Tab
	connect(ui.mRefreshStatsBtn, &QPushButton::clicked, this, &MapServiceDialog::onRefreshStatistics);
	connect(ui.mExportReportBtn, &QPushButton::clicked, this, &MapServiceDialog::onExportReportFromStats);

	// 报表导出 Tab
	connect(ui.mExportBtn, &QPushButton::clicked, this, &MapServiceDialog::onExportReport);

	// 日期选择器（台账搜索、统计分析、报表导出三处共 6 个日期编辑框）
	setupDateEdit(ui.mDateFromEdit);
	setupDateEdit(ui.mDateToEdit);
	setupDateEdit(ui.mStatsDateFrom);
	setupDateEdit(ui.mStatsDateTo);
	setupDateEdit(ui.mExportDateFrom);
	setupDateEdit(ui.mExportDateTo);
}

// ===================== 台账 - 搜索 =====================

void MapServiceDialog::onSearch()
{
	mCurrentPage = 1;
	performSearch();
}

void MapServiceDialog::onReset()
{
	ui.mKeywordEdit->clear();
	ui.mUnitCombo->setCurrentIndex(0);
	ui.mScenarioCombo->setCurrentIndex(0);
	ui.mLevelCombo->setCurrentIndex(0);
	ui.mStatusCombo->setCurrentIndex(0);
	ui.mDateFromEdit->clear();
	ui.mDateToEdit->clear();
	ui.mRecordTable->setRowCount(0);
	ui.mResultCountLabel->setText(QString::fromUtf8("共找到 0 条记录"));
}

void MapServiceDialog::performSearch()
{
	SearchCondition cond;
	cond.keyword = ui.mKeywordEdit->text().trimmed();

	QString category = ui.mScenarioCombo->currentData().toString();
	if (!category.isEmpty())
		cond.category = category;

	QString unitName = ui.mUnitCombo->currentData().toString();
	if (!unitName.isEmpty())
		cond.unitName = unitName;

	QString serviceLevel = ui.mLevelCombo->currentText();
	if (ui.mLevelCombo->currentIndex() > 0 && !serviceLevel.isEmpty())
		cond.serviceLevel = serviceLevel;

	int statusVal = ui.mStatusCombo->currentData().toInt();
	if (statusVal >= 0)
		cond.status = QString::number(statusVal);

	cond.dateFrom = ui.mDateFromEdit->text().trimmed();
	cond.dateTo = ui.mDateToEdit->text().trimmed();
	cond.limit = mPageSize;
	cond.offset = (mCurrentPage - 1) * mPageSize;

	mTotalResults = mDAO->countSearchResults(cond);
	mTotalPages = qMax(1, (mTotalResults + mPageSize - 1) / mPageSize);
	mSearchResults = mDAO->searchRecords(cond);
	ui.mPageSpinBox->setMaximum(mTotalPages);
	ui.mPageSpinBox->setValue(mCurrentPage);
	ui.mTotalPageLabel->setText(QString("/ %1 %2").arg(mTotalPages).arg(QString::fromUtf8("页")));
	ui.mResultCountLabel->setText(QString::fromUtf8("共找到 %1 条记录").arg(mTotalResults));

	refreshRecordTable();
}

void MapServiceDialog::refreshRecordTable()
{
	ui.mRecordTable->setRowCount(0);
	for (int i = 0; i < mSearchResults.size(); ++i)
	{
		const auto& rec = mSearchResults[i];
		ui.mRecordTable->insertRow(i);
		ui.mRecordTable->setItem(i, 0, new QTableWidgetItem(QString::number(rec.id)));
		ui.mRecordTable->setItem(i, 1, new QTableWidgetItem(rec.recordDate));
		ui.mRecordTable->setItem(i, 2, new QTableWidgetItem(rec.applicationScenario));
		ui.mRecordTable->setItem(i, 3, new QTableWidgetItem(rec.coverageArea));
		ui.mRecordTable->setItem(i, 4, new QTableWidgetItem(rec.unitName));
		ui.mRecordTable->setItem(i, 5, new QTableWidgetItem(rec.category));
		ui.mRecordTable->setItem(i, 6, new QTableWidgetItem(rec.serviceLevel));
		ui.mRecordTable->setItem(i, 7, new QTableWidgetItem(rec.serviceForm));
		// 列 8：安排处室（原 col 9）
		ui.mRecordTable->setItem(i, 8, new QTableWidgetItem(rec.arrangedOffice));

		// 关联成果（原 col 10 → col 9）
		QString productInfo;
		if (!rec.productNames.isEmpty()) {
			productInfo = rec.productNames.join(QString::fromUtf8("; "));
		}
		ui.mRecordTable->setItem(i, 9, new QTableWidgetItem(productInfo));

		// 接收人（原 col 11 → col 10）
		ui.mRecordTable->setItem(i, 10, new QTableWidgetItem(rec.receiver));

		// 电子审批单查看按钮（原 col 12 → col 11）
		QgsMessageLog::logMessage(
			QStringLiteral("[刷新表格] row=%1 id=%2 服务单号=%3 approvalFilePath=[%4]")
				.arg(i).arg(rec.id).arg(rec.serviceNumber).arg(rec.approvalFilePath),
			QStringLiteral("MapProductTools"), Qgis::Info);
		QPushButton* viewBtn = new QPushButton(QString::fromUtf8("查看"));
		viewBtn->setToolTip(rec.approvalFilePath.isEmpty()
			? QString::fromUtf8("未上传审批单")
			: QString::fromUtf8("点击查看：%1").arg(rec.approvalFilePath));
		int recordId = rec.id;
		connect(viewBtn, &QPushButton::clicked, this, [this, recordId]() {
			onViewApproval(recordId);
		});
		ui.mRecordTable->setCellWidget(i, 11, viewBtn);
	}
}

void MapServiceDialog::onRecordTableClicked(int row, int /*col*/)
{
	if (row < 0 || row >= mSearchResults.size()) return;
	// 单击选中行，支持编辑操作
}

// ===================== 台账 - 关联成果右键预览 =====================

// 构造矢量成果的地图预览图层（连接 PostGIS 图层表）
static QgsMapLayer* buildVectorPreviewLayer(const ProductMetadata& meta, QString* errMsg)
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected())
	{
		if (errMsg) *errMsg = QString::fromUtf8("数据库未连接，无法在线预览矢量成果。");
		return nullptr;
	}
	if (meta.layerTableName.isEmpty())
	{
		if (errMsg) *errMsg = QString::fromUtf8("该矢量成果没有关联的 PostGIS 图层，无法在线预览。");
		return nullptr;
	}

	PGconn* pg = db->nativeConnection();
	if (!pg)
	{
		if (errMsg) *errMsg = QString::fromUtf8("无法获取数据库连接。");
		return nullptr;
	}

	const QString host     = QString::fromUtf8(PQhost(pg));
	const QString port     = QString::fromUtf8(PQport(pg));
	const QString dbName   = QString::fromUtf8(PQdb(pg));
	const QString user     = QString::fromUtf8(PQuser(pg));
	const QString password = QString::fromUtf8(PQpass(pg));

	QString actualSchema = db->searchPath();
	if (actualSchema.isEmpty()) actualSchema = QStringLiteral("public");

	// 查询表实际所在的 schema（表名大小写可能与记录不同）
	const QString findSchemaSql = QStringLiteral(
		"SELECT table_schema, table_name FROM information_schema.tables "
		"WHERE LOWER(table_name) = LOWER('%1') LIMIT 1").arg(meta.layerTableName);
	const QVariantMap schemaRow = db->executeQueryOne(findSchemaSql);
	const QString foundSchema   = schemaRow.value(QStringLiteral("table_schema")).toString();
	const QString realTableName = schemaRow.value(QStringLiteral("table_name")).toString();
	if (!foundSchema.isEmpty()) actualSchema = foundSchema;
	const QString effectiveTableName = realTableName.isEmpty() ? meta.layerTableName.toLower() : realTableName;

	QgsDataSourceUri dsUri;
	dsUri.setConnection(host, port, dbName, user, password);
	dsUri.setDataSource(actualSchema, effectiveTableName, QStringLiteral("geom"), QString(), QStringLiteral("id"));
	dsUri.setUseEstimatedMetadata(true);

	auto* layer = new QgsVectorLayer(dsUri.uri(), meta.productName, QStringLiteral("YOUR_PASSWORD"));
	if (!layer->isValid())
	{
		if (errMsg) *errMsg = QString::fromUtf8("无法加载矢量图层：%1").arg(meta.productName);
		delete layer;
		return nullptr;
	}
	return layer;
}

// 构造栅格成果的地图预览图层（优先从数据库 BLOB 导出，其次使用本地文件）
static QgsMapLayer* buildRasterPreviewLayer(const ProductMetadata& meta, QString* errMsg)
{
	const QString tempDir = QDir(QDir::tempPath()).absoluteFilePath(QStringLiteral("map_product_preview"));
	QDir().mkpath(tempDir);

	auto* db = PostgisConnector::instance();

	int rasterOid = meta.fileOid;
	if (rasterOid <= 0 && !meta.layerTableName.isEmpty() && db && db->isConnected())
	{
		const QVariantMap row = db->executeQueryOne(
			QStringLiteral("SELECT file_oid FROM \"%1\" LIMIT 1").arg(meta.layerTableName));
		if (!row.isEmpty())
			rasterOid = row.value(QStringLiteral("file_oid")).toInt();
	}

	QString localPath;
	if (rasterOid > 0)
	{
		localPath = QStringLiteral("%1/%2_oid%3.%4")
			.arg(tempDir, meta.productName).arg(rasterOid).arg(meta.fileFormat.toLower());
	}
	else if (!meta.filePath.isEmpty())
	{
		QString fileName = QFileInfo(meta.filePath).fileName();
		if (fileName.isEmpty())
			fileName = QStringLiteral("%1.%2").arg(meta.productName, meta.fileFormat.toLower());
		localPath = tempDir + QStringLiteral("/") + fileName;
	}
	else
	{
		if (errMsg) *errMsg = QString::fromUtf8("该栅格成果没有可用的文件或 BLOB 数据，无法预览。");
		return nullptr;
	}

	if (!QFile::exists(localPath))
	{
		if (rasterOid > 0)
		{
			if (!db || !db->loExport(rasterOid, localPath))
			{
				if (errMsg) *errMsg = QString::fromUtf8("栅格数据从数据库导出失败。");
				return nullptr;
			}
		}
		else
		{
			if (!QFile::exists(meta.filePath) || !QFile::copy(meta.filePath, localPath))
			{
				if (errMsg) *errMsg = QString::fromUtf8("栅格文件复制失败：%1").arg(meta.filePath);
				return nullptr;
			}
		}
	}

	auto* layer = new QgsRasterLayer(localPath, meta.productName);
	if (!layer->isValid())
	{
		if (errMsg) *errMsg = QString::fromUtf8("无法加载栅格文件：%1").arg(localPath);
		delete layer;
		return nullptr;
	}
	return layer;
}

// 弹出地图预览对话框（矢量/栅格成果）
static void showProductMapPreview(const ProductMetadata& meta, QWidget* parent)
{
	QDialog dlg(parent);
	dlg.setWindowTitle(QString::fromUtf8("成果预览 — %1").arg(meta.productName));
	dlg.resize(1000, 700);

	auto* layout = new QVBoxLayout(&dlg);

	auto* infoLabel = new QLabel(QString::fromUtf8("成果：%1    类型：%2    格式：%3")
		.arg(meta.productName, productTypeToDisplayName(meta.productType), meta.fileFormat), &dlg);
	layout->addWidget(infoLabel);

	auto* canvas = new QgsMapCanvas(&dlg);
	canvas->setCanvasColor(QColor(255, 255, 255));
	canvas->enableAntiAliasing(true);
	auto* panTool = new QgsMapToolPan(canvas);
	canvas->setMapTool(panTool);
	layout->addWidget(canvas, 1);

	auto* btnBox = new QDialogButtonBox(&dlg);
	QPushButton* zoomBtn = btnBox->addButton(QString::fromUtf8("缩放到全图"), QDialogButtonBox::ActionRole);
	btnBox->addButton(QString::fromUtf8("关闭"), QDialogButtonBox::RejectRole);
	QObject::connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	layout->addWidget(btnBox);

	QString errMsg;
	QgsMapLayer* layer = (meta.productType == ProductType::Vector)
		? buildVectorPreviewLayer(meta, &errMsg)
		: buildRasterPreviewLayer(meta, &errMsg);

	if (!layer)
	{
		QMessageBox::warning(parent, QString::fromUtf8("无法预览"),
			errMsg.isEmpty() ? QString::fromUtf8("无法加载该成果用于地图预览。") : errMsg);
		return;
	}

	canvas->setDestinationCrs(QgsCoordinateReferenceSystem::fromEpsgId(4490));
	canvas->setLayers(QList<QgsMapLayer*>() << layer);
	canvas->zoomToFullExtent();
	canvas->refresh();

	QObject::connect(zoomBtn, &QPushButton::clicked, &dlg, [canvas]() {
		canvas->zoomToFullExtent();
		canvas->refresh();
	});

	dlg.exec();

	// 先解除画布对图层的引用，再释放图层，避免悬空指针
	canvas->setLayers(QList<QgsMapLayer*>());
	delete layer;
}

// 非地图类成果（pdf/word/excel/txt 等）：提示后使用系统默认程序打开
static void openProductWithDefaultApp(const ProductMetadata& meta, QWidget* parent)
{
	QString path;
	if (!meta.filePath.isEmpty() && QFile::exists(meta.filePath))
	{
		path = meta.filePath;
	}
	else
	{
		// 本地文件不存在时，尝试从数据库 BLOB 导出到临时文件
		auto* db = PostgisConnector::instance();
		if (meta.fileOid > 0 && db && db->isConnected())
		{
			const QString tempDir = QDir(QDir::tempPath()).absoluteFilePath(QStringLiteral("map_product_preview"));
			QDir().mkpath(tempDir);
			QString ext = meta.fileFormat.isEmpty() ? QFileInfo(meta.filePath).suffix() : meta.fileFormat.toLower();
			const QString out = QStringLiteral("%1/%2_oid%3%4")
				.arg(tempDir, meta.productName).arg(meta.fileOid)
				.arg(ext.isEmpty() ? QString() : QStringLiteral(".") + ext);
			if (!QFile::exists(out))
				db->loExport(meta.fileOid, out);
			if (QFile::exists(out))
				path = out;
		}
	}

	if (path.isEmpty())
	{
		QMessageBox::warning(parent, QString::fromUtf8("无法预览"),
			QString::fromUtf8("找不到可用的文件，无法预览：\n%1")
				.arg(meta.filePath.isEmpty() ? meta.productName : meta.filePath));
		return;
	}

	const auto ret = QMessageBox::question(parent, QString::fromUtf8("预览"),
		QString::fromUtf8("该文件将使用系统默认程序打开：\n%1\n\n是否继续？").arg(path),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
	if (ret == QMessageBox::Yes)
		QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MapServiceDialog::previewProduct(int productId)
{
	ProductDAO dao;
	const ProductMetadata meta = dao.getProduct(productId);
	if (meta.id < 0)
	{
		QMessageBox::warning(this, QString::fromUtf8("无法预览"),
			QString::fromUtf8("未找到该关联成果（ID=%1），可能已被删除。").arg(productId));
		return;
	}

	if (meta.productType == ProductType::Vector || meta.productType == ProductType::Raster)
		showProductMapPreview(meta, this);
	else
		openProductWithDefaultApp(meta, this);
}

void MapServiceDialog::onRecordTableContextMenu(const QPoint& pos)
{
	QTableWidgetItem* item = ui.mRecordTable->itemAt(pos);
	if (!item) return;

	// 仅"关联成果"列支持右键预览（按表头文本定位，避免列序变化导致错位）
	int productCol = -1;
	for (int c = 0; c < ui.mRecordTable->columnCount(); ++c)
	{
		QTableWidgetItem* header = ui.mRecordTable->horizontalHeaderItem(c);
		if (header && header->text() == QString::fromUtf8("关联成果"))
		{
			productCol = c;
			break;
		}
	}
	if (productCol < 0 || item->column() != productCol) return;

	const int row = item->row();
	if (row < 0 || row >= mSearchResults.size()) return;

	const ServiceRecord& rec = mSearchResults[row];
	if (rec.productIds.isEmpty())
	{
		QMessageBox::information(this, QString::fromUtf8("预览"),
			QString::fromUtf8("该服务单未关联地图成果。"));
		return;
	}

	QMenu menu(this);
	QList<QAction*> actions;
	for (int i = 0; i < rec.productIds.size(); ++i)
	{
		bool ok = false;
		const int pid = rec.productIds[i].trimmed().toInt(&ok);
		if (!ok) continue;
		const QString name = (i < rec.productNames.size() && !rec.productNames[i].isEmpty())
			? rec.productNames[i] : QString::number(pid);
		QAction* act = menu.addAction(QString::fromUtf8("预览：%1").arg(name));
		act->setData(pid);
		actions << act;
	}

	if (actions.isEmpty())
	{
		QMessageBox::information(this, QString::fromUtf8("预览"),
			QString::fromUtf8("该服务单未关联有效的地图成果。"));
		return;
	}

	QAction* chosen = menu.exec(ui.mRecordTable->viewport()->mapToGlobal(pos));
	if (!chosen) return;
	previewProduct(chosen->data().toInt());
}

// ===================== 台账 - CRUD =====================

void MapServiceDialog::onAddRecord()
{
	ServiceRecordDialog dlg(this);
	dlg.setWindowTitle(QString::fromUtf8("新增地图服务单"));

	// 上传产品已改为在 ServiceRecordDialog::onUploadProduct() 内部处理

	if (dlg.exec() == QDialog::Accepted)
	{
		// ServiceRecordDialog::onSave() 已处理数据库插入
		if (mQGisIface)
			mQGisIface->messageBar()->pushMessage(QStringLiteral("MapProductTools"),
				QString::fromUtf8("服务单创建成功"),
				Qgis::Success, 3);
		performSearch();
	}
}

void MapServiceDialog::onEditRecord()
{
	int row = ui.mRecordTable->currentRow();
	if (row < 0 || row >= mSearchResults.size())
	{
		QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("请先选择一条服务记录"));
		return;
	}

	ServiceRecordDialog dlg(this);
	dlg.setWindowTitle(QString::fromUtf8("编辑地图服务单"));
	dlg.setEditId(mSearchResults[row].id);
	dlg.loadRecord(mSearchResults[row]);

	// 上传产品已改为在 ServiceRecordDialog::onUploadProduct() 内部处理

	if (dlg.exec() == QDialog::Accepted)
	{
		// ServiceRecordDialog::onSave() 已处理数据库更新
		performSearch();
	}
}

void MapServiceDialog::onDeleteRecord()
{
	int row = ui.mRecordTable->currentRow();
	if (row < 0 || row >= mSearchResults.size())
	{
		QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("请先选择一条服务记录"));
		return;
	}

	const auto& rec = mSearchResults[row];
	int ret = QMessageBox::question(this, QString::fromUtf8("确认删除"),
		QString::fromUtf8("确定要删除服务单「%1」吗？\n此操作不可恢复。").arg(rec.serviceNumber),
		QMessageBox::Yes | QMessageBox::No);
	if (ret != QMessageBox::Yes) return;

	if (mDAO->deleteRecord(rec.id))
	{
		performSearch();
	}
	else
	{
		QMessageBox::critical(this, QString::fromUtf8("错误"),
			QString::fromUtf8("删除失败：%1").arg(PostgisConnector::instance()->lastError()));
	}
}

// 生成服务内容文本（按服务形式逐项列举，格式与电子版存档一致）
static QString buildServiceContent(const ServiceRecord& rec)
{
	QString taskContent;
	QStringList summaryParts;  // 合计各部分
	QStringList cnNums = { QString::fromUtf8("一"), QString::fromUtf8("二"),
		QString::fromUtf8("三"), QString::fromUtf8("四"),
		QString::fromUtf8("五"), QString::fromUtf8("六") };

	int formIndex = 0;

	// ---- 地图编制（compilationDetails 现为 ServiceFormDetail 数组，兼容旧 CompilationItem 数组） ----
	QList<ServiceFormDetail> compileDetails = compilationDetailsFromJson(rec.compilationDetails);
	// 兼容旧数据：顶层 mapName/region/size 作为一条记录的兜底
	if (compileDetails.isEmpty() && (!rec.mapName.isEmpty() || !rec.size.isEmpty()))
	{
		ServiceFormDetail d;
		d.mapName = rec.mapName;
		d.mappingRegion = rec.mappingRegion;
		d.size = rec.size;
		compileDetails.append(d);
	}
	if (!compileDetails.isEmpty())
	{
		if (!taskContent.isEmpty())
			taskContent += QString::fromUtf8("\n\n");

		taskContent += cnNums[formIndex] + QString::fromUtf8("、地图编制：");

		int total = 0;
		int seq = 0;
		for (const auto& detail : compileDetails)
		{
			QString mapTitle = detail.mapName.trimmed();
			QString mat = detail.material.trimmed();
			QString sz = detail.size.trimmed();
			for (int j = 0; j < detail.items.size(); ++j)
			{
				const auto& item = detail.items[j];
				total += item.quantity;
				QString itemName = item.name.isEmpty() ? item.kind : item.name;
				++seq;

				if (!mat.isEmpty())
					taskContent += QString::fromUtf8("\n      %1.%2 %3%4%5编制%6幅；")
						.arg(seq).arg(mapTitle).arg(itemName).arg(sz).arg(mat).arg(item.quantity);
				else
					taskContent += QString::fromUtf8("\n      %1.%2 %3%4编制%5幅；")
						.arg(seq).arg(mapTitle).arg(itemName).arg(sz).arg(item.quantity);
			}
		}

		summaryParts.append(QString::fromUtf8("地图编制%1幅").arg(total));
		formIndex++;
	}

	// ---- 其他服务形式Tab（使用 ServiceFormDetail） ----
	struct FormDef { QString name; QString json; QString verb; };
	QList<FormDef> otherForms;
	otherForms << FormDef{ QString::fromUtf8("地图打印"), rec.printDetails, QString::fromUtf8("打印") };
	otherForms << FormDef{ QString::fromUtf8("地图装裱"), rec.mountingDetails, QString::fromUtf8("装裱") };
	otherForms << FormDef{ QString::fromUtf8("地图成品"), rec.productDetails, QString::fromUtf8("制作") };
	otherForms << FormDef{ QString::fromUtf8("数据发送"), rec.dataDetails, QString::fromUtf8("发送") };
	otherForms << FormDef{ QString::fromUtf8("其他"), rec.otherDetails, QString() };

	for (int i = 0; i < otherForms.size(); ++i)
	{
		QList<ServiceFormDetail> details = serviceFormDetailsFromJson(otherForms[i].json);
		if (details.isEmpty())
			continue;
		bool hasAny = false;
		for (const auto& d : details)
			if (!d.items.isEmpty() || !d.mapName.isEmpty()) { hasAny = true; break; }
		if (!hasAny)
			continue;

		if (!taskContent.isEmpty())
			taskContent += QString::fromUtf8("\n\n");

		taskContent += cnNums[formIndex] + QString::fromUtf8("、") + otherForms[i].name + QString::fromUtf8("：");

		int total = 0;
		int seq = 0;
		for (const auto& detail : details)
		{
			QString mapTitle = detail.mapName.trimmed();
			for (int j = 0; j < detail.items.size(); ++j)
			{
				const auto& item = detail.items[j];
				total += item.quantity;
				QString itemName = item.name.isEmpty() ? item.kind : item.name;
				++seq;

				if (!detail.material.isEmpty())
					taskContent += QString::fromUtf8("\n       %1.%2 %3%4%5%6%7幅；")
						.arg(seq).arg(mapTitle).arg(itemName).arg(detail.size)
						.arg(detail.material).arg(otherForms[i].verb).arg(item.quantity);
				else
					taskContent += QString::fromUtf8("\n       %1.%2 %3%4%5%6幅；")
						.arg(seq).arg(mapTitle).arg(itemName).arg(detail.size)
						.arg(otherForms[i].verb).arg(item.quantity);
			}
		}

		if (total > 0)
		{
			if (details.first().material.isEmpty())
				summaryParts.append(otherForms[i].name + QString::number(total) + QString::fromUtf8("幅"));
			else
				summaryParts.append(otherForms[i].name + details.first().material + otherForms[i].verb
					+ QString::number(total) + QString::fromUtf8("幅"));
		}

		formIndex++;
	}

	// 合计行
	if (!summaryParts.isEmpty())
	{
		taskContent += QString::fromUtf8("\n\n（合计：")
			+ summaryParts.join(QString::fromUtf8("，"))
			+ QString::fromUtf8("）");
	}

	return taskContent;
}

void MapServiceDialog::onArchiveRecord()
{
	int row = ui.mRecordTable->currentRow();
	if (row < 0 || row >= mSearchResults.size())
	{
		QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("请先选择一条服务记录"));
		return;
	}

	const auto& rec = mSearchResults[row];

	// 选择保存路径
	QString safeName = rec.serviceNumber;
	safeName.replace("/", "_").replace("\\", "_");
	QString suggestedFile = safeName + QStringLiteral(".docx");
	QString docxPath = QFileDialog::getSaveFileName(this,
		QString::fromUtf8("导出电子版存档"),
		suggestedFile,
		QString::fromUtf8("Word 文档 (*.docx)"));
	if (docxPath.isEmpty()) return;

	// 解析日期
	QDate recDate = QDate::fromString(rec.recordDate, QStringLiteral("yyyy-MM-dd"));
	if (!recDate.isValid())
		recDate = QDate::currentDate();

	// 查找 Python 解释器
	QString pythonExe;
	QStringList pythonCandidates;
	// 插件目录相对于 QGIS bin 目录的 Python
	QString appDir = QCoreApplication::applicationDirPath();
	pythonCandidates << (appDir + QStringLiteral("/python3.exe"));
	pythonCandidates << (appDir + QStringLiteral("/../bin/python3.exe"));
	pythonCandidates << (appDir + QStringLiteral("/../../apps/Python39/python.exe"));
	pythonCandidates << QStringLiteral("python3");
	pythonCandidates << QStringLiteral("python");
	for (const auto& p : pythonCandidates)
	{
		if (!QStandardPaths::findExecutable(p).isEmpty())
		{
			pythonExe = QStandardPaths::findExecutable(p);
			break;
		}
		// 也尝试绝对路径
		QFileInfo fi(p);
		if (fi.exists() && fi.isExecutable())
		{
			pythonExe = fi.absoluteFilePath();
			break;
		}
	}
	if (pythonExe.isEmpty())
	{
		QMessageBox::critical(this, QString::fromUtf8("错误"),
			QString::fromUtf8("找不到 Python 解释器，无法生成 DOCX 文件。"));
		return;
	}

	// 脚本与模板路径：exe 所在目录下的 scripts 目录
	QString scriptsDir = appDir + QStringLiteral("/scripts");
	QString fillScript = QDir(scriptsDir).absoluteFilePath(QStringLiteral("fill_archive.py"));

	if (!QFile::exists(fillScript))
	{
		QMessageBox::critical(this, QString::fromUtf8("错误"),
			QString::fromUtf8("找不到归档脚本：\n%1\n\n请确保 scripts 目录已部署到 exe 所在目录。").arg(fillScript));
		return;
	}

	// 任务内容：按服务形式逐项列举（格式参考用户示例，与台账导出保持一致）
	QString taskContent = buildServiceContent(rec);

	// 事由固定为：办公用图
	QString cause = QString::fromUtf8("办公用图");

	// 安排处室
	QString office = rec.arrangedOffice.trimmed().isEmpty()
		? QString::fromUtf8("（无）") : rec.arrangedOffice;

	// 调用 Python 脚本填充模板
	QStringList args;
	args << fillScript
		 << docxPath
		 << QString::number(recDate.year())
		 << QString::number(recDate.month())
		 << QString::number(recDate.day())
		 << rec.unitName
		 << cause
		 << taskContent
		 << office
		 << rec.remarks;

	QProcess proc;
	proc.start(pythonExe, args);
	if (!proc.waitForFinished(30000))
	{
		QMessageBox::critical(this, QString::fromUtf8("错误"),
			QString::fromUtf8("生成 DOCX 超时或失败。"));
		return;
	}

	QString stdoutStr = QString::fromUtf8(proc.readAllStandardOutput().trimmed());
	QString stderrStr = QString::fromUtf8(proc.readAllStandardError().trimmed());

	if (proc.exitCode() == 0 && stdoutStr.startsWith(QStringLiteral("OK:")))
	{
		// 更新数据库归档信息
		mDAO->updateArchiveInfo(rec.id, docxPath, QStringLiteral("DOCX"));
		QMessageBox::information(this, QString::fromUtf8("导出成功"),
			QString::fromUtf8("电子版存档已导出至：\n%1").arg(docxPath));

		// 自动更新状态为已归档
		ServiceRecord updatedRec = rec;
		updatedRec.status = ServiceStatus::Archived;
		updatedRec.updatedBy = QStringLiteral("system");
		updatedRec.archivePath = docxPath;
		updatedRec.archiveFormat = QStringLiteral("DOCX");
		mDAO->updateRecord(updatedRec);
		performSearch();
	}
	else
	{
		QString errMsg = stderrStr.isEmpty() ? stdoutStr : stderrStr;
		QMessageBox::critical(this, QString::fromUtf8("导出失败"),
			QString::fromUtf8("DOCX 生成失败：\n%1").arg(errMsg));
	}
}

void MapServiceDialog::onManageUnits()
{
	// 从 service_units 表获取所有单位
	QList<QVariantMap> unitList = mDAO->getUnitList();

	// 自定义对话框
	QDialog dlg(this);
	dlg.setWindowTitle(QString::fromUtf8("管理服务单位"));
	dlg.resize(800, 700);

	auto* layout = new QVBoxLayout(&dlg);

	// 提示标签
	auto* label = new QLabel(QString::fromUtf8("已有服务单位（选中后可删除）："), &dlg);
	layout->addWidget(label);

	// 单位列表
	auto* listWidget = new QListWidget(&dlg);
	listWidget->setAlternatingRowColors(true);
	{
		QFont lf = listWidget->font();
		lf.setFamily(QStringLiteral("微软雅黑"));
		lf.setPointSize(9);
		listWidget->setFont(lf);
	}
	for (const auto& u : unitList)
	{
		QString name = u.value("name").toString();
		int id = u.value("id").toInt();
		if (!name.isEmpty())
		{
			auto* item = new QListWidgetItem(name);
			item->setData(Qt::UserRole, id);
			listWidget->addItem(item);
		}
	}
	layout->addWidget(listWidget);

	// 删除与刷新
	auto* actionLayout = new QHBoxLayout();
	auto* deleteBtn = new QPushButton(QString::fromUtf8("删除选中单位"), &dlg);
	auto* refreshBtn = new QPushButton(QString::fromUtf8("刷新列表"), &dlg);
	actionLayout->addWidget(deleteBtn);
	actionLayout->addWidget(refreshBtn);
	actionLayout->addStretch();
	layout->addLayout(actionLayout);

	// 确定 / 取消
	auto* btnBox = new QDialogButtonBox(&dlg);
	btnBox->addButton(QString::fromUtf8("确定"), QDialogButtonBox::AcceptRole);
	btnBox->addButton(QString::fromUtf8("取消"), QDialogButtonBox::RejectRole);
	layout->addWidget(btnBox);
	connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

	// 删除逻辑
	QPointer<QListWidget> listPtr = listWidget;
	connect(deleteBtn, &QPushButton::clicked, &dlg, [this, listPtr, unitList]() {
		if (!listPtr) return;
		QListWidgetItem* cur = listPtr->currentItem();
		if (!cur)
		{
			QMessageBox::warning(listPtr->parentWidget(),
				QString::fromUtf8("提示"), QString::fromUtf8("请先选中一个单位"));
			return;
		}
		int id = cur->data(Qt::UserRole).toInt();
		QString name = cur->text();
		auto ret = QMessageBox::question(listPtr->parentWidget(),
			QString::fromUtf8("确认删除"),
			QString::fromUtf8("确定要删除单位 \"%1\" 吗？\n（已有服务记录不会受影响）").arg(name),
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (ret == QMessageBox::Yes)
		{
			if (mDAO->deleteUnit(id))
			{
				delete listPtr->takeItem(listPtr->row(cur));
			}
			else
			{
				QMessageBox::critical(listPtr->parentWidget(),
					QString::fromUtf8("删除失败"), QString::fromUtf8("删除单位失败，请重试。"));
			}
		}
	});

	// 刷新逻辑
	connect(refreshBtn, &QPushButton::clicked, &dlg, [this, listPtr]() {
		if (!listPtr) return;
		setupFilterCombos();
		listPtr->clear();
		QList<QVariantMap> updatedList = mDAO->getUnitList();
		for (const auto& u : updatedList)
		{
			QString name = u.value("name").toString();
			int id = u.value("id").toInt();
			if (!name.isEmpty())
			{
				auto* item = new QListWidgetItem(name);
				item->setData(Qt::UserRole, id);
				listPtr->addItem(item);
			}
		}
		QMessageBox::information(listPtr->parentWidget(),
			QString::fromUtf8("提示"), QString::fromUtf8("单位列表已刷新。"));
	});

	dlg.exec();

	// 关闭后刷新主界面的过滤下拉
	setupFilterCombos();
}

// ===================== 统计分析 =====================

// 统计项展示顺序：把“其他”统一排到最后（第6行），其余项保持原有顺序
static QList<QPair<QString, int>> orderedStatItems(const QMap<QString, int>& data)
{
	static const QString kOther = QString::fromUtf8("其他");

	QList<QPair<QString, int>> items;
	items.reserve(data.size());

	for (auto it = data.constBegin(); it != data.constEnd(); ++it)
	{
		if (it.key() == kOther)
			continue;
		items.append(qMakePair(it.key(), it.value()));
	}

	auto otherIt = data.constFind(kOther);
	if (otherIt != data.constEnd())
		items.append(qMakePair(otherIt.key(), otherIt.value()));

	return items;
}

void MapServiceDialog::onRefreshStatistics()
{
	QString dateFrom = ui.mStatsDateFrom->text().trimmed();
	QString dateTo = ui.mStatsDateTo->text().trimmed();

	QString granularity = QStringLiteral("month");
	if (mFreqGranularityCombo)
		granularity = mFreqGranularityCombo->currentData().toString();

	ServiceStatistics stats = mDAO->getStatistics(dateFrom, dateTo, granularity);
	refreshStatistics(stats);
}

void MapServiceDialog::onFrequencyGranularityChanged(int /*index*/)
{
	onRefreshStatistics();
}

void MapServiceDialog::refreshStatistics(const ServiceStatistics& stats)
{
	// 概览指标
	ui.mTotalRecordsValue->setText(QString::number(stats.totalRecords));
	ui.mTotalMapsValue->setText(QString::number(stats.totalSheetCount));
	ui.mTotalUnitsValue->setText(QString::number(stats.totalUnitCount));

	// 频次图表表头根据粒度动态变化
	QString freqTimeLabel;
	if (stats.frequencyGranularity == QStringLiteral("year"))
		freqTimeLabel = QString::fromUtf8("年份");
	else if (stats.frequencyGranularity == QStringLiteral("quarter"))
		freqTimeLabel = QString::fromUtf8("季度");
	else
		freqTimeLabel = QString::fromUtf8("月份");

	// 各维度统计图表
	populateChartTable(ui.mScenarioChart, stats.byMapType,
		QString::fromUtf8("图件种类"), QString::fromUtf8("数量"));
	populateChartTable(ui.mUnitChart, stats.byUnit,
		QString::fromUtf8("服务单位"), QString::fromUtf8("服务单数"));
	populateChartTable(ui.mLevelChart, stats.byLevel,
		QString::fromUtf8("服务等级"), QString::fromUtf8("服务单数"));
	populateChartTable(ui.mAreaChart, stats.byArea,
		QString::fromUtf8("覆盖区域"), QString::fromUtf8("服务单数"));
	populateChartTable(ui.mMonthlyChart, stats.monthlyFrequency,
		freqTimeLabel, QString::fromUtf8("服务频次"));
	populateChartTable(ui.mSvcFormChart, stats.bySvcForm,
		QString::fromUtf8("服务形式"), QString::fromUtf8("数量"));
}

void MapServiceDialog::populateChartTable(QTableWidget* table, const QMap<QString, int>& data,
	const QString& col1Name, const QString& col2Name)
{
	// 先断开信号避免 clear 触发多余更新
	table->blockSignals(true);

	table->clear();
	table->setRowCount(0);
	table->setColumnCount(3);
	table->setHorizontalHeaderLabels({ col1Name, col2Name, QString::fromUtf8("占比") });
	table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	table->horizontalHeader()->setStretchLastSection(true);

	// 确保水平表头可见
	table->horizontalHeader()->setVisible(true);
	// 设置最小列宽和行高，确保表头有足够空间显示
	table->horizontalHeader()->setMinimumSectionSize(60);
	// 设置表格最小高度（表头 + 至少一行数据）
	table->setMinimumHeight(qMax(80, table->horizontalHeader()->height() + (data.isEmpty() ? 0 : data.size() * 28) + 4));

	// “其他”统一排在最后一行
	const QList<QPair<QString, int>> items = orderedStatItems(data);

	int total = 0;
	for (const auto& kv : items)
		total += kv.second;

	table->setRowCount(items.size());
	int row = 0;
	for (const auto& kv : items)
	{
		QTableWidgetItem* nameItem = new QTableWidgetItem(kv.first);
		table->setItem(row, 0, nameItem);
		QTableWidgetItem* countItem = new QTableWidgetItem(QString::number(kv.second));
		table->setItem(row, 1, countItem);

		double pct = total > 0 ? (100.0 * kv.second / total) : 0;
		QString pctStr = QString("%1%").arg(pct, 0, 'f', 1);
		QTableWidgetItem* pctItem = new QTableWidgetItem(pctStr);
		pctItem->setTextAlignment(Qt::AlignCenter);

		table->setItem(row, 2, pctItem);
		++row;
	}

	// 添加合计行
	if (data.size() > 1)
	{
		int lastRow = table->rowCount();
		table->insertRow(lastRow);
		QTableWidgetItem* totalLabel = new QTableWidgetItem(QString::fromUtf8("合计"));
		QFont boldFont;
		boldFont.setBold(true);
		totalLabel->setFont(boldFont);
		table->setItem(lastRow, 0, totalLabel);

		QTableWidgetItem* totalVal = new QTableWidgetItem(QString::number(total));
		totalVal->setFont(boldFont);
		table->setItem(lastRow, 1, totalVal);

		QTableWidgetItem* totalPct = new QTableWidgetItem("100%");
		totalPct->setTextAlignment(Qt::AlignCenter);
		totalPct->setFont(boldFont);
		table->setItem(lastRow, 2, totalPct);
	}

	table->blockSignals(false);
}

// ===================== 报表导出 =====================

void MapServiceDialog::onExportReportFromStats()
{
	// 从统计分析 Tab 点击"导出报表"，默认导出全部三种格式
	QString dateFrom = ui.mStatsDateFrom->text().trimmed();
	QString dateTo = ui.mStatsDateTo->text().trimmed();
	int chartType = ui.mChartTypeCombo->currentIndex();
	doExportReport(dateFrom, dateTo, true, true, true, chartType);
}

void MapServiceDialog::doExportReport(const QString& dateFrom, const QString& dateTo,
	bool exportExcel, bool exportPdf, bool exportPng, int chartType)
{
	if (!exportExcel && !exportPdf && !exportPng) return;

	// 选择导出目录
	QString outputDir = QFileDialog::getExistingDirectory(this,
		QString::fromUtf8("选择报表导出目录"));
	if (outputDir.isEmpty()) return;

	// 获取统计数据
	ServiceStatistics stats = mDAO->getStatistics(dateFrom, dateTo);

	// 生成文件前缀
	QString prefix = QString::fromUtf8("地图服务报表_%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
	QString basePath = outputDir + "/" + prefix;

	// 进度条
	ui.mExportProgressBar->setVisible(true);
	ui.mExportProgressBar->setValue(0);
	ui.mExportBtn->setEnabled(false);
	QApplication::processEvents();

	int totalSteps = 0;
	if (exportExcel) totalSteps++;
	if (exportPdf) totalSteps++;
	if (exportPng) totalSteps++;

	int currentStep = 0;
	QStringList exportedFiles;

	// 导出 Excel
	if (exportExcel)
	{
		currentStep++;
		ui.mExportProgressBar->setValue(currentStep * 100 / totalSteps);
		ui.mExportProgressBar->setFormat(QString::fromUtf8("正在导出 Excel..."));
		QApplication::processEvents();

		QString excelPath = basePath + ".xlsx";
		if (exportToExcel(excelPath, stats))
		{
			exportedFiles << excelPath;
			addExportHistory(QFileInfo(excelPath).fileName(), "Excel", excelPath);
		}
	}

	// 导出 PDF
	if (exportPdf)
	{
		currentStep++;
		ui.mExportProgressBar->setValue(currentStep * 100 / totalSteps);
		ui.mExportProgressBar->setFormat(QString::fromUtf8("正在导出 PDF..."));
		QApplication::processEvents();

		QString pdfPath = basePath + ".pdf";
		if (exportToPdf(pdfPath, stats, chartType))
		{
			exportedFiles << pdfPath;
			addExportHistory(QFileInfo(pdfPath).fileName(), "PDF", pdfPath);
		}
	}

	// 导出 PNG
	if (exportPng)
	{
		currentStep++;
		ui.mExportProgressBar->setValue(currentStep * 100 / totalSteps);
		ui.mExportProgressBar->setFormat(QString::fromUtf8("正在导出 PNG 图表..."));
		QApplication::processEvents();

		QString pngPath = basePath + ".png";
		if (exportToPng(pngPath, stats, chartType))
		{
			exportedFiles << pngPath;
			addExportHistory(QFileInfo(pngPath).fileName(), "PNG", pngPath);
		}
	}

	ui.mExportProgressBar->setVisible(false);
	ui.mExportBtn->setEnabled(true);

	QString msg = QString::fromUtf8("报表导出完成。\n\n导出目录：%1\n\n导出文件：\n%2")
		.arg(outputDir, exportedFiles.join("\n"));
	QMessageBox::information(this, QString::fromUtf8("导出完成"), msg);
}

void MapServiceDialog::onExportReport()
{
	if (!ui.mExportExcelCheck->isChecked() && !ui.mExportPdfCheck->isChecked() && !ui.mExportPngCheck->isChecked())
	{
		QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("请至少选择一种导出格式"));
		return;
	}

	QString dateFrom = ui.mExportDateFrom->text().trimmed();
	QString dateTo = ui.mExportDateTo->text().trimmed();
	int chartType = ui.mChartTypeCombo->currentIndex();
	doExportReport(dateFrom, dateTo,
		ui.mExportExcelCheck->isChecked(),
		ui.mExportPdfCheck->isChecked(),
		ui.mExportPngCheck->isChecked(),
		chartType);
}

bool MapServiceDialog::exportToExcel(const QString& filePath, const ServiceStatistics& stats)
{
	XlsxWriter xlsx;

	// ==================== Sheet 1: 概览指标 ====================
	xlsx.addSheet(QString::fromUtf8("概览指标"));
	xlsx.setColumnWidths({25, 15});

	xlsx.writeHeader({
		QString::fromUtf8("指标"),
		QString::fromUtf8("数值")
	});

	xlsx.writeRow({QString::fromUtf8("服务单总数"), QString::number(stats.totalRecords)});
	xlsx.writeRow({QString::fromUtf8("图幅总数"), QString::number(stats.totalSheetCount)});
	xlsx.writeRow({QString::fromUtf8("服务单位数"), QString::number(stats.totalUnitCount)});

	// 报表信息
	xlsx.writeRow({"", ""});
	xlsx.writeRow({
		QString::fromUtf8("报表生成时间：") + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"),
		""
	});
	xlsx.writeRow({
		QString::fromUtf8("统计范围：") + stats.dateFrom + " ~ " + stats.dateTo,
		""
	});

	// 通用写入函数（带占比）
	auto writeSectionWithPercent = [&](const QString& sheetName, const QMap<QString, int>& data) {
		xlsx.addSheet(sheetName);
		xlsx.setColumnWidths({35, 15, 15});

		xlsx.writeHeader({
			QString::fromUtf8("类别"),
			QString::fromUtf8("数量"),
			QString::fromUtf8("占比")
		});

		// “其他”统一排在最后一行
		const QList<QPair<QString, int>> items = orderedStatItems(data);

		int total = 0;
		for (const auto& kv : items) total += kv.second;
		for (const auto& kv : items)
		{
			double pct = total > 0 ? (100.0 * kv.second / total) : 0;
			xlsx.writeRow({
				kv.first,
				QString::number(kv.second),
				QString("%1%").arg(pct, 0, 'f', 1)
			});
		}
		xlsx.writeRow({
			QString::fromUtf8("合计"),
			QString::number(total),
			"100%"
		});
	};

	// ==================== Sheet 2: 按图件种类统计 ====================
	writeSectionWithPercent(QString::fromUtf8("按图件种类统计"), stats.byMapType);

	// ==================== Sheet 3: 按服务单位统计(用图数量) ====================
	writeSectionWithPercent(QString::fromUtf8("按服务单位统计(用图数量)"), stats.byUnit);

	// ==================== Sheet 4: 按服务等级统计 ====================
	writeSectionWithPercent(QString::fromUtf8("按服务等级统计"), stats.byLevel);

	// ==================== Sheet 5: 按覆盖区域统计 ====================
	writeSectionWithPercent(QString::fromUtf8("按覆盖区域统计"), stats.byArea);

	// ==================== Sheet 6: 月度服务频次统计 ====================
	writeSectionWithPercent(QString::fromUtf8("月度服务频次统计"), stats.monthlyFrequency);

	// ==================== Sheet 7: 按服务形式统计 ====================
	writeSectionWithPercent(QString::fromUtf8("按服务形式统计"), stats.bySvcForm);

	return xlsx.save(filePath);
}

bool MapServiceDialog::exportToPdf(const QString& filePath, const ServiceStatistics& stats, int chartType)
{
	// 使用 QPrinter + QPainter 直接绘制 PDF
	// 注意：使用 HighResolution 时坐标系为物理像素，字体大小和间距需要相应放大
	QPrinter printer(QPrinter::HighResolution);
	printer.setOutputFormat(QPrinter::PdfFormat);
	printer.setOutputFileName(filePath);
	printer.setPageSize(QPrinter::A4);
	printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

	QPainter painter;
	if (!painter.begin(&printer))
		return false;

	painter.setRenderHint(QPainter::Antialiasing);

	// 获取可用绘制区域（物理像素）
	QRectF pageRect = printer.pageRect(QPrinter::DevicePixel);
	qreal pageW = pageRect.width();
	qreal pageH = pageRect.height();

	// 计算 DPI 缩放因子（A4 ≈ 190mm 可用宽，物理像素/毫米 ≈ 每毫米像素数）
	// 在 HighResolution 下通常约 14 px/mm（≈ 350 dpi）
	qreal dpiScale = pageW / 190.0;  // px per mm

	// ========== 字体定义（使用 pixelSize 确保高 DPI 下字体可见） ==========
	auto makeFont = [&](int ptSize, bool bold = false) {
		QFont f("Microsoft YaHei");
		f.setPixelSize(qRound(ptSize * dpiScale * 0.3528)); // pt → px（1pt ≈ 0.3528mm）
		f.setBold(bold);
		return f;
	};

	QFont titleFont    = makeFont(18, true);
	QFont subtitleFont = makeFont(9);
	QFont sectionFont  = makeFont(11, true);
	QFont headerFont   = makeFont(8, true);
	QFont cellFont     = makeFont(8);


	QFont cardLabelFont = makeFont(8);
	QFont totalFont    = makeFont(8, true);

	// 数值使用 Arial（数字字体）
	QFont valueFont("Arial");
	valueFont.setPixelSize(qRound(20 * dpiScale * 0.3528));
	valueFont.setBold(true);

	// 行高和间距（毫米 → 像素）
	qreal titleH    = 10 * dpiScale;
	qreal subtitleH = 7 * dpiScale;
	qreal sectionH  = 8 * dpiScale;
	qreal rowH      = 7 * dpiScale;
	qreal cardH     = 22 * dpiScale;
	qreal margin    = 5 * dpiScale;

	qreal y = 0;

	// ========== 标题 ==========
	painter.setFont(titleFont);
	painter.setPen(QColor(44, 62, 80));
	painter.drawText(QRectF(0, y, pageW, titleH), Qt::AlignCenter,
		QString::fromUtf8("地图服务管理报表"));
	y += titleH + 2 * dpiScale;

	// ========== 副标题 ==========
	painter.setFont(subtitleFont);
	painter.setPen(QColor(102, 102, 102));
	QString infoStr = QString::fromUtf8("生成时间：%1    统计范围：%2 ~ %3")
		.arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
		.arg(stats.dateFrom, stats.dateTo);
	painter.drawText(QRectF(0, y, pageW, subtitleH), Qt::AlignCenter, infoStr);
	y += subtitleH + 3 * dpiScale;

	// ========== 概览指标卡片 ==========
	qreal cardW = (pageW - 4 * margin) / 3;
	qreal cardY = y;

	auto drawCard = [&](qreal cx, const QString& value, const QString& label, const QColor& color) {
		QRectF cardRect(cx, cardY, cardW, cardH);
		painter.setPen(QPen(QColor(222, 226, 230), dpiScale * 0.3));
		painter.setBrush(QColor(248, 249, 250));
		painter.drawRoundedRect(cardRect, 2 * dpiScale, 2 * dpiScale);

		// 数值
		painter.setPen(color);
		painter.setFont(valueFont);
		qreal valueAreaH = cardH * 0.55;
		painter.drawText(QRectF(cx, cardY + 1 * dpiScale, cardW, valueAreaH),
			Qt::AlignCenter, value);

		// 标签
		painter.setPen(QColor(108, 117, 125));
		painter.setFont(cardLabelFont);
		qreal labelY = cardY + valueAreaH + 2 * dpiScale;
		qreal labelH = cardH - valueAreaH - 2 * dpiScale;
		painter.drawText(QRectF(cx, labelY, cardW, labelH), Qt::AlignCenter, label);
	};

	drawCard(margin,
		QString::number(stats.totalRecords),
		QString::fromUtf8("服务单总数"),
		QColor(74, 144, 217));
	drawCard(margin * 2 + cardW,
		QString::number(stats.totalSheetCount),
		QString::fromUtf8("图幅总数"),
		QColor(91, 143, 249));
	drawCard(margin * 3 + cardW * 2,
		QString::number(stats.totalUnitCount),
		QString::fromUtf8("服务单位数"),
		QColor(97, 221, 170));

	y = cardY + cardH + 5 * dpiScale;

	// ========== 各维度统计：左侧统计表 + 右侧统计图 ==========
	// 柱状图配色（与 PNG 导出保持一致）
	QList<QColor> chartColors = {
		QColor(74, 144, 217), QColor(91, 143, 249), QColor(97, 221, 170),
		QColor(246, 189, 22), QColor(255, 107, 107), QColor(93, 112, 146),
		QColor(154, 132, 230), QColor(255, 156, 87)
	};

	// 饼图配色：暖色调，与图表控件 ChartWidget::pieColors() 保持一致
	const QList<QColor> pieChartColors = ChartWidget::pieColors();

	qreal colGap = 6 * dpiScale;
	qreal tableW = (pageW - colGap) * 0.50;   // 左侧统计表宽度
	qreal chartX = tableW + colGap;           // 右侧统计图起始 X
	qreal chartW = pageW - chartX;            // 右侧统计图宽度

	auto drawSection = [&](const QString& title, const QMap<QString, int>& data) {
		// “其他”统一排在最后一行
		const QList<QPair<QString, int>> items = orderedStatItems(data);

		int total = 0;
		for (const auto& kv : items)
			total += kv.second;

		// 左侧统计表高度：表头 + 数据行 + 合计行
		qreal tableBodyH = rowH * (items.size() + 2);

		// 右侧统计图高度（饼图尺寸适当收小，避免顶面顶满右栏）
		qreal pieRadius  = chartW * 0.26;
		qreal pieRadiusY = pieRadius * pie3DVerticalScale();
		qreal pieDepth   = qBound(4.0 * dpiScale, pieRadius / 3.0, 12.0 * dpiScale);
		qreal legendItemH = rowH * 0.85;
		qreal chartBodyH = 0;
		if (total > 0)
		{
			chartBodyH = (chartType == 1)
				? (pieRadiusY * 2 + pieDepth + 2 * dpiScale + legendItemH * items.size()) // 饼图：饼 + 图例
				: (rowH * items.size());                                                  // 柱状图
		}

		qreal bodyH = qMax(tableBodyH, chartBodyH);
		qreal sectionTotalH = sectionH + bodyH + 6 * dpiScale;

		// 换页检查
		if (y + sectionTotalH > pageH - 10 * dpiScale) {
			printer.newPage();
			y = 0;
		}

		// Section 标题（横跨两列）
		painter.setFont(sectionFont);
		painter.setPen(QColor(74, 144, 217));
		painter.drawText(QRectF(0, y, pageW, sectionH), Qt::AlignLeft | Qt::AlignVCenter, title);
		y += sectionH + 1 * dpiScale;

		qreal bodyTop = y;

		// ==================== 左侧：统计表 ====================
		qreal col1W = tableW * 0.46;
		qreal col2W = tableW * 0.27;
		qreal col3W = tableW - col1W - col2W;
		qreal textPad = 1.5 * dpiScale;

		// 表头
		painter.setPen(Qt::NoPen);
		painter.setBrush(QColor(74, 144, 217));
		painter.drawRect(QRectF(0, y, tableW, rowH));

		painter.setFont(headerFont);
		painter.setPen(Qt::white);
		painter.drawText(QRectF(0, y, col1W, rowH), Qt::AlignCenter,
			QString::fromUtf8("类别"));
		painter.drawText(QRectF(col1W, y, col2W, rowH), Qt::AlignCenter,
			QString::fromUtf8("数量"));
		painter.drawText(QRectF(col1W + col2W, y, col3W, rowH), Qt::AlignCenter,
			QString::fromUtf8("占比"));
		y += rowH;

		// 数据行
		int idx = 0;
		for (const auto& kv : items)
		{
			QColor rowBg = (idx % 2 == 0) ? Qt::white : QColor(248, 249, 250);
			painter.setPen(Qt::NoPen);
			painter.setBrush(rowBg);
			painter.drawRect(QRectF(0, y, tableW, rowH));

			// 底部边框线
			painter.setPen(QPen(QColor(222, 226, 230), dpiScale * 0.15));
			painter.drawLine(QPointF(0, y + rowH), QPointF(tableW, y + rowH));

			double pct = total > 0 ? (100.0 * kv.second / total) : 0;

			painter.setFont(cellFont);
			painter.setPen(QColor(51, 51, 51));
			painter.drawText(QRectF(textPad, y, col1W - textPad, rowH),
				Qt::AlignLeft | Qt::AlignVCenter, kv.first);
			painter.drawText(QRectF(col1W, y, col2W, rowH), Qt::AlignCenter,
				QString::number(kv.second));
			painter.drawText(QRectF(col1W + col2W, y, col3W, rowH), Qt::AlignCenter,
				QString("%1%").arg(pct, 0, 'f', 1));
			y += rowH;
			++idx;
		}

		// 合计行
		painter.setPen(Qt::NoPen);
		painter.setBrush(QColor(233, 236, 239));
		painter.drawRect(QRectF(0, y, tableW, rowH));

		painter.setFont(totalFont);
		painter.setPen(QColor(51, 51, 51));
		painter.drawText(QRectF(textPad, y, col1W - textPad, rowH),
			Qt::AlignLeft | Qt::AlignVCenter, QString::fromUtf8("合计"));
		painter.drawText(QRectF(col1W, y, col2W, rowH), Qt::AlignCenter,
			QString::number(total));
		painter.drawText(QRectF(col1W + col2W, y, col3W, rowH), Qt::AlignCenter, "100%");
		y += rowH;

		// ==================== 右侧：统计图 ====================
		if (total > 0 && chartType == 1)
		{
			// ---- 立体饼图 ----
			QVector<QPair<QString, int>> pieItems;
			pieItems.reserve(items.size());
			for (const auto& kv : items)
				pieItems.append(qMakePair(kv.first, kv.second));

			QPoint pieTopCenter(qRound(chartX + chartW / 2.0), qRound(bodyTop + pieRadiusY));

			// 最后一个参数 drawGloss=false：QPdfEngine 会把带 alpha 的径向渐变高光
			// 渲染成不透明白色并盖住整个顶面（饼图“空白”的原因），PDF 中必须关闭
			ChartWidget::drawPie3D(painter, pieTopCenter, qRound(pieRadius), qRound(pieDepth),
				pieItems, pieChartColors, nullptr, -1.0, false);

			// ---- 图例（饼图下方） ----
			qreal legendY = bodyTop + pieRadiusY * 2 + pieDepth + 2 * dpiScale;
			qreal block = 3 * dpiScale;
			for (int i = 0; i < items.size(); ++i)
			{
				QColor color = pieChartColors[i % pieChartColors.size()];
				painter.setPen(Qt::NoPen);
				painter.setBrush(color);
				painter.drawRect(QRectF(chartX, legendY + (legendItemH - block) / 2, block, block));

				double pct = 100.0 * items[i].second / total;
				painter.setFont(cellFont);
				painter.setPen(QColor(60, 60, 60));
				painter.drawText(QRectF(chartX + block + 1.5 * dpiScale, legendY,
					chartW - block - 1.5 * dpiScale, legendItemH),
					Qt::AlignLeft | Qt::AlignVCenter,
					QString("%1: %2 (%3%)").arg(items[i].first)
						.arg(items[i].second).arg(pct, 0, 'f', 1));
				legendY += legendItemH;
			}
		}
		else if (total > 0)
		{
			// ---- 水平柱状图 ----
			qreal labelW   = chartW * 0.36;
			qreal valueW   = chartW * 0.28;
			qreal barMaxW  = qMax(2.0 * dpiScale, chartW - labelW - valueW - 2 * dpiScale);
			qreal barH     = rowH * 0.66;
			int   maxVal   = total;

			qreal by = bodyTop;
			for (int i = 0; i < items.size(); ++i)
			{
				qreal barW = qMax(2.0, barMaxW * items[i].second / maxVal);

				painter.setFont(cellFont);
				painter.setPen(QColor(60, 60, 60));
				painter.drawText(QRectF(chartX, by, labelW - dpiScale, rowH),
					Qt::AlignRight | Qt::AlignVCenter, items[i].first);

				painter.setPen(Qt::NoPen);
				painter.setBrush(chartColors[i % chartColors.size()]);
				painter.drawRoundedRect(
					QRectF(chartX + labelW, by + (rowH - barH) / 2, barW, barH),
					1 * dpiScale, 1 * dpiScale);

				double pct = 100.0 * items[i].second / total;
				painter.setPen(QColor(60, 60, 60));
				painter.drawText(QRectF(chartX + labelW + barW + dpiScale, by, valueW, rowH),
					Qt::AlignLeft | Qt::AlignVCenter,
					QString("%1 (%2%)").arg(items[i].second).arg(pct, 0, 'f', 1));
				by += rowH;
			}
		}

		// 以内容较高者推进纵向游标
		y = bodyTop + bodyH + 6 * dpiScale;
	};

	drawSection(QString::fromUtf8("按图件种类统计"), stats.byMapType);
	drawSection(QString::fromUtf8("按服务单位统计"), stats.byUnit);
	drawSection(QString::fromUtf8("按服务等级统计"), stats.byLevel);
	drawSection(QString::fromUtf8("按覆盖区域统计"), stats.byArea);
	drawSection(QString::fromUtf8("月度服务频次"), stats.monthlyFrequency);
	drawSection(QString::fromUtf8("按服务形式统计"), stats.bySvcForm);

	painter.end();
	return true;
}

bool MapServiceDialog::exportToPng(const QString& filePath, const ServiceStatistics& stats, int chartType)
{
	// 使用超大画布绘制，然后按实际内容裁切
	int imgWidth = 1600;
	int maxHeight = 8000; // 足够大的画布

	QImage image(imgWidth, maxHeight, QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::white);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	int actualHeight = renderStatisticsToPainter(&painter, QRect(0, 0, imgWidth, maxHeight), stats, chartType);
	painter.end();

	// 裁切到实际高度
	if (actualHeight > 0 && actualHeight < maxHeight)
	{
		image = image.copy(0, 0, imgWidth, actualHeight + 60);
	}

	return image.save(filePath, "PNG");
}

int MapServiceDialog::renderStatisticsToPainter(QPainter* painter, const QRect& rect,
	const ServiceStatistics& stats, int chartType)
{
	painter->setRenderHint(QPainter::Antialiasing);

	int y = 50;
	int margin = 25;

	// 标题
	QFont titleFont("SimHei");
	titleFont.setPixelSize(33);
	titleFont.setBold(true);
	painter->setFont(titleFont);
	painter->setPen(QColor(44, 62, 80));
	painter->drawText(rect.x(), rect.y() + y, rect.width(), 50, Qt::AlignCenter,
		QString::fromUtf8("地图服务管理报表"));
	y += 80;

	// 时间和范围
	QFont infoFont("SimHei");
	infoFont.setPixelSize(18);
	painter->setFont(infoFont);
	painter->setPen(QColor(100, 100, 100));
	QString infoStr = QString::fromUtf8("生成时间: %1    统计范围: %2 ~ %3")
		.arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
		.arg(stats.dateFrom, stats.dateTo);
	painter->drawText(rect.x(), rect.y() + y, rect.width(), 30, Qt::AlignCenter, infoStr);
	y += 45;

	// 概览指标
	QFont valueFont("Arial");
	valueFont.setPixelSize(33);
	valueFont.setBold(true);
	QFont labelFont("SimHei");
	labelFont.setPixelSize(18);

	int cardW = (rect.width() - 4 * margin) / 3;
	int cardH = 96;
	int cardValueH = cardH * 3 / 5;
	int cardLabelH = cardH - cardValueH;
	int cx = margin;

	auto drawCard = [&](const QString& value, const QString& label, const QColor& color) {
		QRect cardRect(rect.x() + cx, rect.y() + y, cardW, cardH);
		painter->setPen(Qt::NoPen);
		painter->setBrush(QColor(248, 249, 250));
		painter->drawRoundedRect(cardRect, 8, 8);

		painter->setPen(color);
		painter->setFont(valueFont);
		painter->drawText(cardRect.adjusted(4, 8, -4, -(cardLabelH)), Qt::AlignCenter, value);

		painter->setPen(QColor(108, 117, 125));
		painter->setFont(labelFont);
		painter->drawText(cardRect.adjusted(6, cardValueH, -6, -6), Qt::AlignCenter | Qt::TextWordWrap, label);

		cx += cardW + margin;
	};

	drawCard(QString::number(stats.totalRecords), QString::fromUtf8("服务单总数"), QColor(74, 144, 217));
	drawCard(QString::number(stats.totalSheetCount), QString::fromUtf8("图幅总数"), QColor(91, 143, 249));
	drawCard(QString::number(stats.totalUnitCount), QString::fromUtf8("服务单位数"), QColor(97, 221, 170));
	y += cardH + 30;

	// 颜色列表 — 饼状图专用高区分度配色
	QList<QColor> colors = {
		QColor(74, 144, 217), QColor(91, 143, 249), QColor(97, 221, 170),
		QColor(246, 189, 22), QColor(255, 107, 107), QColor(93, 112, 146),
		QColor(154, 132, 230), QColor(255, 156, 87)
	};

	// 饼状图专用配色 — 暖色调，与图表控件保持一致
	const QList<QColor> pieColors = ChartWidget::pieColors();

	// 根据图表类型选择绘制方式
	if (chartType == 1) // 饼状图
	{
	auto drawPieSection = [&](const QString& title, const QMap<QString, int>& data, int& yPos) {
		QFont sectionFont("SimHei");
		sectionFont.setPixelSize(20);
		sectionFont.setBold(true);
		painter->setFont(sectionFont);
		painter->setPen(QColor(74, 144, 217));
		painter->drawText(rect.x() + margin, rect.y() + yPos, rect.width() - 2 * margin, 30,
			Qt::AlignLeft | Qt::AlignVCenter, title);
		yPos += 38;

			// “其他”统一排在最后一行
			const QList<QPair<QString, int>> items = orderedStatItems(data);

			int total = 0;
			for (const auto& kv : items)
				total += kv.second;
			if (total == 0)
			{
				yPos += 20;
				return;
			}

			// 饼状图区域（顶面为椭圆：整体高度 = 2*纵向半径 + 侧壁厚度）
			int pieAreaX = rect.x() + margin + 20;
			int pieAreaY = rect.y() + yPos;
			int pieSize = qMin(240, rect.width() - 2 * margin - 400);   // 顶面椭圆水平直径
			int pieRadius  = pieSize / 2;
			int pieRadiusY = qRound(pieRadius * pie3DVerticalScale());
			int pieDepth   = qBound(8, pieSize / 12, 36);
			int pieCenterX = pieAreaX + pieRadius;
			int pieCenterY = pieAreaY + pieRadiusY;

			// 图例区域
			int legendX = pieAreaX + pieSize + 30;

			// ---- 立体饼图（与图表控件共用同一实现，保证风格统一）----
			QVector<QPair<QString, int>> pieItems;
			pieItems.reserve(items.size());
			for (const auto& kv : items)
				pieItems.append(qMakePair(kv.first, kv.second));

			ChartWidget::drawPie3D(*painter, QPoint(pieCenterX, pieCenterY), pieRadius, pieDepth,
				pieItems, pieColors);

		// 绘制图例
		int legendY = pieAreaY;
		int legendItemH = 32;   // 每行高度加大
		int legendGap = 4;       // 行间距
		int colorBlockSize = 16; // 颜色方块大小
		int colorIdx = 0;
		QFont legendFont("SimHei");
		legendFont.setPixelSize(18);
			for (const auto& kv : items)
			{
				double pct = 100.0 * kv.second / total;
				QColor color = pieColors[colorIdx % pieColors.size()];

				// 颜色方块
				painter->setPen(Qt::NoPen);
				painter->setBrush(color);
				int blockY = legendY + (legendItemH - colorBlockSize) / 2;
				painter->drawRect(legendX, blockY, colorBlockSize, colorBlockSize);

				// 标签文本
				painter->setFont(legendFont);
				painter->setPen(QColor(60, 60, 60));
				QString legendText = QString("%1: %2 (%3%)")
					.arg(kv.first).arg(kv.second).arg(pct, 0, 'f', 1);
				painter->drawText(legendX + colorBlockSize + 8, legendY,
					rect.width() - legendX - 50, legendItemH,
					Qt::AlignLeft | Qt::AlignVCenter, legendText);

				legendY += legendItemH + legendGap;
				++colorIdx;
			}

			int contentH = qMax(pieRadiusY * 2 + pieDepth, legendY - pieAreaY);
			yPos += contentH + 30;
		};

		drawPieSection(QString::fromUtf8("按图件种类统计"), stats.byMapType, y);
		drawPieSection(QString::fromUtf8("按服务单位统计"), stats.byUnit, y);
		drawPieSection(QString::fromUtf8("按服务等级统计"), stats.byLevel, y);
		drawPieSection(QString::fromUtf8("按覆盖区域统计"), stats.byArea, y);
		drawPieSection(QString::fromUtf8("月度服务频次"), stats.monthlyFrequency, y);
		drawPieSection(QString::fromUtf8("按服务形式统计"), stats.bySvcForm, y);
	}
	else // 柱状图（默认）
	{
	auto drawSection = [&](const QString& title, const QMap<QString, int>& data, int& yPos) {
		QFont sectionFont("SimHei");
		sectionFont.setPixelSize(20);
		sectionFont.setBold(true);
		painter->setFont(sectionFont);
		painter->setPen(QColor(74, 144, 217));
		painter->drawText(rect.x() + margin, rect.y() + yPos, rect.width() - 2 * margin, 30,
			Qt::AlignLeft | Qt::AlignVCenter, title);
		yPos += 38;

			// “其他”统一排在最后一行
			const QList<QPair<QString, int>> items = orderedStatItems(data);

			int total = 0;
			for (const auto& kv : items) total += kv.second;
			int maxVal = total > 0 ? total : 1;

		QFont barFont("SimHei");
		barFont.setPixelSize(16);
		painter->setFont(barFont);

		int barAreaX = rect.x() + margin;
		int labelW = 140;
			int barMaxW = rect.width() - 2 * margin - labelW - 120;
			int barH = 30;
			int barGap = 8;

		QFont barValFont("SimHei");
		barValFont.setPixelSize(15);

			int idx = 0;
			for (const auto& kv : items)
			{
				int barW = static_cast<int>(static_cast<double>(kv.second) / maxVal * barMaxW);
				if (barW < 2) barW = 2;

				painter->setPen(QColor(60, 60, 60));
				painter->drawText(barAreaX, rect.y() + yPos, labelW, barH,
					Qt::AlignRight | Qt::AlignVCenter, kv.first);

				QColor color = colors[idx % colors.size()];
				painter->setPen(Qt::NoPen);
				painter->setBrush(color);
				QRect barRect(barAreaX + labelW + 10, rect.y() + yPos + 4, barW, barH - 8);
				painter->drawRoundedRect(barRect, 4, 4);

				painter->setPen(QColor(60, 60, 60));
				painter->setFont(barValFont);
				double pct = total > 0 ? (100.0 * kv.second / total) : 0;
				QString valStr = QString("%1 (%2%)").arg(kv.second).arg(pct, 0, 'f', 1);
				painter->drawText(barAreaX + labelW + barW + 15, rect.y() + yPos, 130, barH,
					int(Qt::AlignLeft | Qt::AlignVCenter), valStr);

				yPos += barH + barGap;
				++idx;
			}
			yPos += 15;
		};

		drawSection(QString::fromUtf8("按图件种类统计"), stats.byMapType, y);
		drawSection(QString::fromUtf8("按服务单位统计"), stats.byUnit, y);
		drawSection(QString::fromUtf8("按服务等级统计"), stats.byLevel, y);
		drawSection(QString::fromUtf8("按覆盖区域统计"), stats.byArea, y);
		drawSection(QString::fromUtf8("月度服务频次"), stats.monthlyFrequency, y);
		drawSection(QString::fromUtf8("按服务形式统计"), stats.bySvcForm, y);
	}

	return y;
}

void MapServiceDialog::addExportHistory(const QString& fileName, const QString& format, const QString& filePath)
{
	int row = ui.mExportHistoryTable->rowCount();
	ui.mExportHistoryTable->insertRow(row);
	QTableWidgetItem* it0 = new QTableWidgetItem(fileName);
	ui.mExportHistoryTable->setItem(row, 0, it0);
	QTableWidgetItem* it1 = new QTableWidgetItem(format);
	ui.mExportHistoryTable->setItem(row, 1, it1);
	QTableWidgetItem* it2 = new QTableWidgetItem(
		QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
	ui.mExportHistoryTable->setItem(row, 2, it2);
	QTableWidgetItem* it3 = new QTableWidgetItem(filePath);
	ui.mExportHistoryTable->setItem(row, 3, it3);
}

// 电子审批单查看

void MapServiceDialog::onViewApproval(int recordId)
{
	QgsMessageLog::logMessage(
		QStringLiteral("[查看审批单] recordId=%1").arg(recordId),
		QStringLiteral("MapProductTools"), Qgis::Info);

	ServiceRecord rec = mDAO->getRecord(recordId);

	QgsMessageLog::logMessage(
		QStringLiteral("[查看审批单] id=%1, 服务单号=%2, approvalFilePath=%3, isEmpty=%4")
			.arg(rec.id)
			.arg(rec.serviceNumber)
			.arg(rec.approvalFilePath)
			.arg(rec.approvalFilePath.isEmpty() ? QStringLiteral("true") : QStringLiteral("false")),
		QStringLiteral("MapProductTools"), Qgis::Info);

	if (rec.approvalFilePath.isEmpty())
	{
		QgsMessageLog::logMessage(
			QStringLiteral("[查看审批单] 审批单路径为空，提示用户"),
			QStringLiteral("MapProductTools"), Qgis::Warning);
		QMessageBox::information(this, QString::fromUtf8("提示"),
			QString::fromUtf8("该服务单未上传电子审批单。"));
		return;
	}

	if (!QFileInfo::exists(rec.approvalFilePath))
	{
		QgsMessageLog::logMessage(
			QStringLiteral("[查看审批单] 文件不存在：%1").arg(rec.approvalFilePath),
			QStringLiteral("MapProductTools"), Qgis::Warning);
		QMessageBox::warning(this, QString::fromUtf8("无法查看"),
			QString::fromUtf8("审批单文件不存在：\n%1").arg(rec.approvalFilePath));
		return;
	}

	QgsMessageLog::logMessage(
		QStringLiteral("[查看审批单] 打开文件：%1").arg(rec.approvalFilePath),
		QStringLiteral("MapProductTools"), Qgis::Info);
	showApprovalDialog(rec.approvalFilePath);
}

void MapServiceDialog::showApprovalDialog(const QString& filePath)
{
	QFileInfo fi(filePath);
	QString suffix = fi.suffix().toLower();

	bool isPdf = (suffix == "pdf");
	bool isImage = (suffix == "jpg" || suffix == "jpeg" || suffix == "png"
		|| suffix == "bmp" || suffix == "gif");

	if (!QFile::exists(filePath))
	{
		QMessageBox::warning(this, QString::fromUtf8("文件不存在"),
			QString::fromUtf8("审批单文件不存在：\n%1").arg(filePath));
		return;
	}

	if (isPdf)
	{
		// PDF：提示用户是否用系统查看器打开
		auto ret = QMessageBox::question(this, QString::fromUtf8("电子审批单"),
			QString::fromUtf8("PDF 文件：%1\n\n是否使用系统默认程序打开？").arg(fi.fileName()),
			QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
		if (ret == QMessageBox::Yes)
			QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
		return;
	}

	if (!isImage)
	{
		QMessageBox::warning(this, QString::fromUtf8("格式不支持"),
			QString::fromUtf8("无法预览此格式的文件。"));
		return;
	}

	// 图片预览对话框
	QDialog dlg(this);
	dlg.setWindowTitle(QString::fromUtf8("电子审批单 — %1").arg(fi.fileName()));
	QRect screenRect = QApplication::desktop()->availableGeometry();
	dlg.resize(
		qMin(1200, screenRect.width() * 3 / 4),
		qMin(900, screenRect.height() * 3 / 4));

	auto* layout = new QVBoxLayout(&dlg);

	auto* scrollArea = new QScrollArea(&dlg);
	scrollArea->setWidgetResizable(true);

	auto* imageLabel = new QLabel(&dlg);
	imageLabel->setAlignment(Qt::AlignCenter);

	QPixmap pixmap(filePath);
	if (pixmap.isNull())
	{
		QMessageBox::warning(this, QString::fromUtf8("加载失败"),
			QString::fromUtf8("无法加载图片文件。"));
		return;
	}

	imageLabel->setPixmap(pixmap);
	scrollArea->setWidget(imageLabel);
	layout->addWidget(scrollArea);

	auto* btnBox = new QDialogButtonBox(&dlg);
	QPushButton* exportBtn = btnBox->addButton(QString::fromUtf8("导出"), QDialogButtonBox::ActionRole);
	btnBox->addButton(QString::fromUtf8("关闭"), QDialogButtonBox::RejectRole);
	connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

	// 导出：另存为图片
	QString srcPath = filePath;
	QString srcSuffix = suffix;
	connect(exportBtn, &QPushButton::clicked, &dlg, [&dlg, srcPath, srcSuffix]() {
		QString savePath = QFileDialog::getSaveFileName(&dlg,
			QString::fromUtf8("导出审批单"),
			QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + QStringLiteral("/审批单.%1").arg(srcSuffix),
			QString::fromUtf8("图片文件 (*.%1);;所有文件 (*.*)").arg(srcSuffix));
		if (savePath.isEmpty())
			return;
		if (QFile::copy(srcPath, savePath))
			QMessageBox::information(&dlg, QString::fromUtf8("导出成功"),
				QString::fromUtf8("审批单已导出到：\n%1").arg(savePath));
		else
			QMessageBox::critical(&dlg, QString::fromUtf8("导出失败"),
				QString::fromUtf8("无法导出文件，请检查目标路径权限。"));
	});
	layout->addWidget(btnBox);

	dlg.exec();
}

// ===================== 动态统计 Tab =====================

void MapServiceDialog::setupDynamicStatsTab()
{
	QWidget* page = ui.tabDynamicStats;
	if (!page) return;

	// 确保页面有 layout
	if (!page->layout())
		page->setLayout(new QVBoxLayout(page));

	QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(page->layout());
	if (!mainLayout) return;

	// ----- 顶部筛选栏 -----
	QGroupBox* filterGroup = new QGroupBox(QString::fromUtf8("统计筛选"), page);
	QHBoxLayout* filterLayout = new QHBoxLayout(filterGroup);

	filterLayout->addWidget(new QLabel(QString::fromUtf8("时间粒度:"), page));
	mDynamicGranularityCombo = new QComboBox(page);
	mDynamicGranularityCombo->addItem(QString::fromUtf8("按年"),   QStringLiteral("year"));
	mDynamicGranularityCombo->addItem(QString::fromUtf8("按季度"), QStringLiteral("quarter"));
	mDynamicGranularityCombo->addItem(QString::fromUtf8("按月"),   QStringLiteral("month"));
	filterLayout->addWidget(mDynamicGranularityCombo);

	// 时间粒度切换 → 自动刷新统计 + 图表
	connect(mDynamicGranularityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &MapServiceDialog::onDynamicStatRefresh);

	filterLayout->addWidget(new QLabel(QString::fromUtf8("  起始:"), page));
	mDynamicDateFrom = new QDateEdit(QDate::currentDate().addYears(-1), page);
	mDynamicDateFrom->setCalendarPopup(true);
	mDynamicDateFrom->setDisplayFormat("yyyy-MM-dd");
	filterLayout->addWidget(mDynamicDateFrom);

	filterLayout->addWidget(new QLabel(QString::fromUtf8("截止:"), page));
	mDynamicDateTo = new QDateEdit(QDate::currentDate(), page);
	mDynamicDateTo->setCalendarPopup(true);
	mDynamicDateTo->setDisplayFormat("yyyy-MM-dd");
	filterLayout->addWidget(mDynamicDateTo);

	mDynamicRefreshBtn = new QPushButton(QString::fromUtf8("刷新统计"), page);
	filterLayout->addWidget(mDynamicRefreshBtn);

	mDynamicExportExcelBtn = new QPushButton(QString::fromUtf8("导出Excel"), page);
	filterLayout->addWidget(mDynamicExportExcelBtn);
	mDynamicExportPdfBtn = new QPushButton(QString::fromUtf8("导出PDF"), page);
	filterLayout->addWidget(mDynamicExportPdfBtn);
	mDynamicExportPngBtn = new QPushButton(QString::fromUtf8("导出PNG"), page);
	filterLayout->addWidget(mDynamicExportPngBtn);

	mMountingExportBtn = new QPushButton(QString::fromUtf8("地图装裱统计导出"), page);
	QFont exportFont = mMountingExportBtn->font();
	exportFont.setBold(true);
	mMountingExportBtn->setFont(exportFont);
	filterLayout->addWidget(mMountingExportBtn);

	// PNG 导出专用图表类型
	filterLayout->addWidget(new QLabel(QString::fromUtf8("导出图表类型:"), page));
	mDynamicChartTypeCombo = new QComboBox(page);
	mDynamicChartTypeCombo->addItem(QString::fromUtf8("柱状图"), 0);
	mDynamicChartTypeCombo->addItem(QString::fromUtf8("饼状图"), 1);
	filterLayout->addWidget(mDynamicChartTypeCombo);

	filterLayout->addStretch();
	mainLayout->addWidget(filterGroup);

	// ----- 主体区域：QSplitter（左侧指标树 | 右侧 TabWidget） -----
	QSplitter* splitter = new QSplitter(Qt::Horizontal, page);

	// ---- 左侧：指标选择树 ----
	QWidget* leftPanel = new QWidget(page);
	QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);

	QLabel* metricLabel = new QLabel(QString::fromUtf8("统计指标（可多选）:"), leftPanel);
	QFont boldFont;
	boldFont.setBold(true);
	metricLabel->setFont(boldFont);
	leftLayout->addWidget(metricLabel);

	mDynamicMetricTree = new QTreeWidget(leftPanel);
	mDynamicMetricTree->setHeaderHidden(true);
	mDynamicMetricTree->setMinimumWidth(200);
	{
		QFont tf = mDynamicMetricTree->font();
		tf.setFamily(QStringLiteral("微软雅黑"));
		tf.setPointSize(9);
		mDynamicMetricTree->setFont(tf);
	}
	// 载入指标树
	QMap<QString, QTreeWidgetItem*> categoryItems;
	const auto& allMetrics = StatMetrics::allMetrics();
	for (const auto& m : allMetrics)
	{
		if (!categoryItems.contains(m.category))
		{
			QTreeWidgetItem* catItem = new QTreeWidgetItem(QStringList() << m.category);
			catItem->setFlags(catItem->flags() | Qt::ItemIsUserCheckable);
			catItem->setCheckState(0, Qt::Unchecked);
			catItem->setExpanded(false);
			mDynamicMetricTree->addTopLevelItem(catItem);
			categoryItems[m.category] = catItem;
		}
		QTreeWidgetItem* leafItem = new QTreeWidgetItem(QStringList() << m.displayName);
		leafItem->setFlags(leafItem->flags() | Qt::ItemIsUserCheckable);
		leafItem->setCheckState(0, Qt::Unchecked);
		leafItem->setData(0, Qt::UserRole, m.key);
		categoryItems[m.category]->addChild(leafItem);
	}
	// 默认选中全部
	for (int i = 0; i < mDynamicMetricTree->topLevelItemCount(); ++i)
	{
		QTreeWidgetItem* cat = mDynamicMetricTree->topLevelItem(i);
		cat->setCheckState(0, Qt::Checked);
		for (int j = 0; j < cat->childCount(); ++j)
			cat->child(j)->setCheckState(0, Qt::Checked);
	}
	connect(mDynamicMetricTree, &QTreeWidget::itemChanged,
		this, &MapServiceDialog::onDynamicMetricItemChanged);
	leftLayout->addWidget(mDynamicMetricTree);

	splitter->addWidget(leftPanel);

	// ---- 右侧：TabWidget（数据表格 | 图表可视化） ----
	mDynamicResultTabs = new QTabWidget(page);
	mDynamicResultTabs->setMinimumWidth(500);

	// -- Tab 1: 数据表格 --
	mDynamicStatsTable = new QTableWidget(page);
	mDynamicResultTabs->addTab(mDynamicStatsTable, QString::fromUtf8("数据表格"));

	// -- Tab 2: 图表可视化 --
	QWidget* chartPage = new QWidget(page);
	QVBoxLayout* chartLayout = new QVBoxLayout(chartPage);
	chartLayout->setContentsMargins(4, 4, 4, 4);

	// 图表工具栏
	QHBoxLayout* chartToolLayout = new QHBoxLayout();
	chartToolLayout->addWidget(new QLabel(QString::fromUtf8("图表样式:"), chartPage));

	mDynamicChartStyleCombo = new QComboBox(chartPage);
	mDynamicChartStyleCombo->addItem(QString::fromUtf8("2D 柱状图"), ChartWidget::Bar2D);
	mDynamicChartStyleCombo->addItem(QString::fromUtf8("2D 饼状图"), ChartWidget::Pie2D);
	mDynamicChartStyleCombo->addItem(QString::fromUtf8("3D 柱状图"), ChartWidget::Bar3D);
	mDynamicChartStyleCombo->addItem(QString::fromUtf8("3D 饼状图"), ChartWidget::Pie3D);
	mDynamicChartStyleCombo->setMinimumWidth(160);
	chartToolLayout->addWidget(mDynamicChartStyleCombo);
	chartToolLayout->addStretch();

	chartLayout->addLayout(chartToolLayout);

	// 图表页面栈：柱状图分Tab | 饼状图单页
	mDynamicChartStack = new QStackedWidget(chartPage);

	// 页面 0：柱状图（按指标分 Tab）
	mDynamicBarTabs = new QTabWidget();
	mDynamicChartStack->addWidget(mDynamicBarTabs);

	// 页面 1：饼状图（单页显示所有指标）
	mPieChartWidget = new ChartWidget();
	mDynamicChartStack->addWidget(mPieChartWidget);

	chartLayout->addWidget(mDynamicChartStack, 1);

	mDynamicResultTabs->addTab(chartPage, QString::fromUtf8("图表可视化"));

	splitter->addWidget(mDynamicResultTabs);
	splitter->setStretchFactor(0, 1);
	splitter->setStretchFactor(1, 3);

	mainLayout->addWidget(splitter, 1);

	// ----- 信号连接 -----
	connect(mDynamicRefreshBtn, &QPushButton::clicked, this, &MapServiceDialog::onDynamicStatRefresh);
	connect(mDynamicExportExcelBtn, &QPushButton::clicked, this, &MapServiceDialog::onDynamicStatExportExcel);
	connect(mDynamicExportPdfBtn, &QPushButton::clicked, this, &MapServiceDialog::onDynamicStatExportPdf);
	connect(mDynamicExportPngBtn, &QPushButton::clicked, this, &MapServiceDialog::onDynamicStatExportPng);
	connect(mMountingExportBtn, &QPushButton::clicked, this, &MapServiceDialog::onMountingStatExport);

	// 图表样式切换 → 自动重建图表
	connect(mDynamicChartStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, [this](int /*idx*/) {
			refreshDynamicChart();
		});
}

DynamicStatisticsConfig MapServiceDialog::buildDynamicStatsConfig()
{
	DynamicStatisticsConfig config;

	// 粒度
	if (mDynamicGranularityCombo)
		config.granularity = mDynamicGranularityCombo->currentData().toString();

	// 日期范围
	if (mDynamicDateFrom)
		config.dateFrom = mDynamicDateFrom->date().toString("yyyy-MM-dd");
	if (mDynamicDateTo)
		config.dateTo = mDynamicDateTo->date().toString("yyyy-MM-dd");

	// 启用的指标列
	if (mDynamicMetricTree)
	{
		for (int i = 0; i < mDynamicMetricTree->topLevelItemCount(); ++i)
		{
			QTreeWidgetItem* cat = mDynamicMetricTree->topLevelItem(i);
			for (int j = 0; j < cat->childCount(); ++j)
			{
				QTreeWidgetItem* leaf = cat->child(j);
				if (leaf->checkState(0) == Qt::Checked)
				{
					QString key = leaf->data(0, Qt::UserRole).toString();
					if (!key.isEmpty())
						config.enabledColumns << key;
				}
			}
		}
	}

	return config;
}

void MapServiceDialog::onDynamicStatRefresh()
{
	if (!mDAO || !mDynamicStatsTable) return;

	DynamicStatisticsConfig config = buildDynamicStatsConfig();
	mDynamicStatsRows = mDAO->getDynamicStatistics(config);  // 缓存原始数据
	refreshDynamicStatsTable(mDynamicStatsRows);
	refreshDynamicChart();

	QgsMessageLog::logMessage(
		QStringLiteral("动态统计刷新完成，粒=%1, 行数=%2").arg(config.granularity).arg(mDynamicStatsRows.size()),
		QStringLiteral("MapProductTools"), Qgis::Info);
}

void MapServiceDialog::refreshDynamicStatsTable(const QList<DynamicStatsRow>& rows)
{
	if (!mDynamicStatsTable) return;

	mDynamicStatsTable->blockSignals(true);
	mDynamicStatsTable->clear();
	mDynamicStatsTable->setRowCount(0);

	if (rows.isEmpty())
	{
		mDynamicStatsTable->setColumnCount(1);
		mDynamicStatsTable->setHorizontalHeaderLabels({ QString::fromUtf8("时间周期") });
		mDynamicStatsTable->blockSignals(false);
		return;
	}

	// 收集所有出现的列名
	QSet<QString> colSet;
	for (const auto& row : rows)
	{
		for (auto it = row.metrics.constBegin(); it != row.metrics.constEnd(); ++it)
			colSet.insert(it.key());
	}
	QStringList cols;
	cols << QStringLiteral("time_period");
	// 排序保证确定性
	QStringList sortedMetricCols = colSet.values();
	std::sort(sortedMetricCols.begin(), sortedMetricCols.end());
	cols << sortedMetricCols;

	mDynamicStatsTable->setColumnCount(cols.size());

	// 表头用指标名称替换键
	QStringList headerLabels;
	headerLabels << QString::fromUtf8("时间周期");
	const auto& metrics = StatMetrics::allMetrics();
	QMap<QString, QString> keyToName;
	for (const auto& m : metrics)
		keyToName[m.key] = m.displayName;

	for (const auto& col : sortedMetricCols)
	{
		headerLabels << keyToName.value(col, col);
	}
	mDynamicStatsTable->setHorizontalHeaderLabels(headerLabels);

	mDynamicStatsTable->setRowCount(rows.size());
	for (int r = 0; r < rows.size(); ++r)
	{
		const auto& row = rows[r];
		mDynamicStatsTable->setItem(r, 0, new QTableWidgetItem(row.timePeriod));

		for (int c = 1; c < cols.size(); ++c)
		{
			QString key = cols[c];
			QVariant val = row.metrics.value(key);
			QString text = val.isNull() ? "0" : val.toString();
			mDynamicStatsTable->setItem(r, c, new QTableWidgetItem(text));
		}
	}

	mDynamicStatsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	mDynamicStatsTable->blockSignals(false);
}

// 前向声明
static void collectDynamicMetricCols(const QList<DynamicStatsRow>& rows,
	QStringList& metricCols,
	QMap<QString, QString>& keyToName);

void MapServiceDialog::refreshDynamicChart()
{
	if (!mDynamicChartStack) return;

	// 收集指标列
	QStringList metricCols;
	QMap<QString, QString> keyToName;
	collectDynamicMetricCols(mDynamicStatsRows, metricCols, keyToName);

	int chartType = ChartWidget::Bar2D;
	if (mDynamicChartStyleCombo)
		chartType = mDynamicChartStyleCombo->currentData().toInt();
	bool isPie = (chartType == ChartWidget::Pie2D || chartType == ChartWidget::Pie3D);

	// ===== 柱状图：按指标分 Tab =====
	{
		QString currentTabText;
		int curIdx = mDynamicBarTabs->currentIndex();
		if (curIdx >= 0 && curIdx < mDynamicBarTabs->count())
			currentTabText = mDynamicBarTabs->tabText(curIdx);

		mDynamicBarTabs->blockSignals(true);
		while (mDynamicBarTabs->count() > 0)
		{
			QWidget* w = mDynamicBarTabs->widget(0);
			mDynamicBarTabs->removeTab(0);
			delete w;
		}

		if (!metricCols.isEmpty())
		{
			int restoreIdx = -1;
			for (int i = 0; i < metricCols.size(); ++i)
			{
				const QString& col = metricCols[i];
				QString displayName = keyToName.value(col, col);

				auto* cw = new ChartWidget(mDynamicBarTabs);
				cw->setChartType(static_cast<ChartWidget::ChartType>(isPie ? ChartWidget::Pie2D : chartType));

				QStringList singleCol;
				singleCol << col;
				cw->setData(mDynamicStatsRows, singleCol, keyToName);

				mDynamicBarTabs->addTab(cw, displayName);

				if (displayName == currentTabText)
					restoreIdx = i;
			}
			mDynamicBarTabs->blockSignals(false);

			if (restoreIdx >= 0)
				mDynamicBarTabs->setCurrentIndex(restoreIdx);
		}
		else
		{
			mDynamicBarTabs->blockSignals(false);
		}
	}

	// ===== 饼状图：单页显示所有指标 =====
	if (mPieChartWidget)
	{
		mPieChartWidget->setChartType(static_cast<ChartWidget::ChartType>(isPie ? chartType : ChartWidget::Pie2D));
		mPieChartWidget->setData(mDynamicStatsRows, metricCols, keyToName);
	}

	// 切换当前显示页面
	mDynamicChartStack->setCurrentIndex(isPie ? 1 : 0);
	mDynamicChartStack->blockSignals(false);
}

void MapServiceDialog::onDynamicMetricItemChanged(QTreeWidgetItem* item, int /*column*/)
{
	if (!item || !mDynamicMetricTree) return;

	// 先断开信号防止递归
	mDynamicMetricTree->blockSignals(true);

	Qt::CheckState state = item->checkState(0);

	if (item->parent() == nullptr)
	{
		// 父节点：联动所有子节点
		for (int i = 0; i < item->childCount(); ++i)
			item->child(i)->setCheckState(0, state);
	}
	else
	{
		// 子节点变动时更新父节点
		QTreeWidgetItem* parent = item->parent();
		if (parent)
		{
			bool allChecked = true, allUnchecked = true;
			for (int i = 0; i < parent->childCount(); ++i)
			{
				Qt::CheckState cs = parent->child(i)->checkState(0);
				if (cs != Qt::Checked)  allChecked = false;
				if (cs != Qt::Unchecked) allUnchecked = false;
			}
			if (allChecked)
				parent->setCheckState(0, Qt::Checked);
			else if (allUnchecked)
				parent->setCheckState(0, Qt::Unchecked);
			else
				parent->setCheckState(0, Qt::PartiallyChecked);
		}
	}

	mDynamicMetricTree->blockSignals(false);

	// 指标选择变化后自动刷新图表
	refreshDynamicChart();
}

// ===================== 动态统计导出 =====================

// 收集已启用指标的列名（有序）及键→显示名映射
static void collectDynamicMetricCols(const QList<DynamicStatsRow>& rows,
	QStringList& metricCols,
	QMap<QString, QString>& keyToName)
{
	const auto& metrics = StatMetrics::allMetrics();
	for (const auto& m : metrics)
		keyToName[m.key] = m.displayName;

	QSet<QString> colSet;
	for (const auto& row : rows)
	{
		for (auto it = row.metrics.constBegin(); it != row.metrics.constEnd(); ++it)
			colSet.insert(it.key());
	}
	metricCols = colSet.values();
	std::sort(metricCols.begin(), metricCols.end());
}

void MapServiceDialog::onDynamicStatExportExcel()
{
	if (mDynamicStatsRows.isEmpty())
	{
		QMessageBox::warning(this, QString::fromUtf8("无数据"),
			QString::fromUtf8("请先点击刷新统计获取数据后再导出。"));
		return;
	}

	QString filePath = QFileDialog::getSaveFileName(this,
		QString::fromUtf8("导出动态统计 Excel"),
		QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/动态统计.xlsx",
		QString::fromUtf8("Excel 文件 (*.xlsx);;所有文件 (*.*)"));
	if (filePath.isEmpty()) return;

	// 构建指标映射
	DynamicStatisticsConfig config = buildDynamicStatsConfig();
	QStringList metricCols;
	QMap<QString, QString> keyToName;
	collectDynamicMetricCols(mDynamicStatsRows, metricCols, keyToName);

	// 粒度中文
	QMap<QString, QString> granularityNames;
	granularityNames[QStringLiteral("year")]    = QString::fromUtf8("按年");
	granularityNames[QStringLiteral("quarter")] = QString::fromUtf8("按季度");
	granularityNames[QStringLiteral("month")]   = QString::fromUtf8("按月");

	XlsxWriter xlsx;

	// ==================== Sheet 1: 概览摘要 ====================
	xlsx.addSheet(QString::fromUtf8("概览摘要"));
	xlsx.setColumnWidths({ 25, 30 });

	xlsx.writeHeader({
		QString::fromUtf8("项目"),
		QString::fromUtf8("内容")
	});

	xlsx.writeRow({ QString::fromUtf8("报表类型"), QString::fromUtf8("动态统计报告") });
	xlsx.writeRow({ QString::fromUtf8("生成时间"),
		QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") });
	xlsx.writeRow({ QString::fromUtf8("统计范围"),
		config.dateFrom + " ~ " + config.dateTo });
	xlsx.writeRow({ QString::fromUtf8("频次粒度"),
		granularityNames.value(config.granularity, config.granularity) });
	xlsx.writeRow({ QString::fromUtf8("时间周期数"),
		QString::number(mDynamicStatsRows.size()) });
	xlsx.writeRow({ QString::fromUtf8("启用指标数"),
		QString::number(metricCols.size()) });

	// 汇总指标
	xlsx.writeRow({ "", "" });
	xlsx.writeRow({
		QString::fromUtf8("--- 各指标合计 ---"), ""
	});
	for (const auto& col : metricCols)
	{
		qint64 sum = 0;
		for (const auto& row : mDynamicStatsRows)
			sum += row.metrics.value(col).toLongLong();
		xlsx.writeRow({
			keyToName.value(col, col),
			QString::number(sum)
		});
	}

	// ==================== Sheet 2: 动态统计数据 ====================
	xlsx.addSheet(QString::fromUtf8("动态统计数据"));

	QVector<int> colWidths;
	colWidths.append(18);  // 时间周期列
	for (int i = 0; i < metricCols.size(); ++i)
		colWidths.append(14);
	xlsx.setColumnWidths(colWidths);

	QStringList headers;
	headers << QString::fromUtf8("时间周期");
	for (const auto& col : metricCols)
		headers << keyToName.value(col, col);
	xlsx.writeHeader(headers);

	for (const auto& row : mDynamicStatsRows)
	{
		QStringList dataRow;
		dataRow << row.timePeriod;
		for (const auto& col : metricCols)
		{
			QVariant val = row.metrics.value(col);
			dataRow << (val.isNull() ? QStringLiteral("0") : val.toString());
		}
		xlsx.writeRow(dataRow);
	}

	if (!xlsx.save(filePath))
	{
		QMessageBox::critical(this, QString::fromUtf8("导出失败"),
			QString::fromUtf8("无法保存文件: %1").arg(filePath));
		return;
	}

	addExportHistory(QFileInfo(filePath).fileName(), QStringLiteral("Excel"), filePath);
	QMessageBox::information(this, QString::fromUtf8("导出成功"),
		QString::fromUtf8("动态统计已导出到:\n%1").arg(filePath));
}

void MapServiceDialog::onDynamicStatExportPdf()
{
	if (mDynamicStatsRows.isEmpty())
	{
		QMessageBox::warning(this, QString::fromUtf8("无数据"),
			QString::fromUtf8("请先点击刷新统计获取数据后再导出。"));
		return;
	}

	QString filePath = QFileDialog::getSaveFileName(this,
		QString::fromUtf8("导出动态统计 PDF"),
		QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/动态统计.pdf",
		QString::fromUtf8("PDF 文件 (*.pdf);;所有文件 (*.*)"));
	if (filePath.isEmpty()) return;

	// 构建指标映射
	DynamicStatisticsConfig config = buildDynamicStatsConfig();
	QStringList metricCols;
	QMap<QString, QString> keyToName;
	collectDynamicMetricCols(mDynamicStatsRows, metricCols, keyToName);

	// 粒度中文
	QMap<QString, QString> granularityNames;
	granularityNames[QStringLiteral("year")]    = QString::fromUtf8("按年");
	granularityNames[QStringLiteral("quarter")] = QString::fromUtf8("按季度");
	granularityNames[QStringLiteral("month")]   = QString::fromUtf8("按月");

	// 使用 QPrinter + QPainter 直接绘制 PDF
	QPrinter printer(QPrinter::HighResolution);
	printer.setOutputFormat(QPrinter::PdfFormat);
	printer.setOutputFileName(filePath);
	printer.setPageSize(QPrinter::A4);
	printer.setPageOrientation(QPageLayout::Landscape);  // 横向以容纳更多列
	printer.setPageMargins(QMarginsF(12, 12, 12, 12), QPageLayout::Millimeter);

	QPainter painter;
	if (!painter.begin(&printer))
	{
		QMessageBox::critical(this, QString::fromUtf8("导出失败"),
			QString::fromUtf8("无法创建 PDF 文件。"));
		return;
	}

	painter.setRenderHint(QPainter::Antialiasing);

	QRectF pageRect = printer.pageRect(QPrinter::DevicePixel);
	qreal pageW = pageRect.width();
	qreal pageH = pageRect.height();
	qreal dpiScale = pageW / 260.0;  // A4 landscape ≈ 260mm 可用宽

	// 字体
	auto makeFont = [&](int ptSize, bool bold = false) {
		QFont f(QStringLiteral("Microsoft YaHei"));
		f.setPixelSize(qRound(ptSize * dpiScale * 0.3528));
		f.setBold(bold);
		return f;
	};
	QFont titleFont = makeFont(14, true);
	QFont subtitleFont = makeFont(7);
	QFont headerFont = makeFont(7, true);
	QFont cellFont = makeFont(7);
	QFont cardLabelFont = makeFont(6);
	QFont cardValueFont = makeFont(12, true);

	qreal y = 0;
	qreal margin = 4 * dpiScale;

	// ===== 标题 =====
	painter.setFont(titleFont);
	painter.setPen(QColor(44, 62, 80));
	painter.drawText(QRectF(0, y, pageW, 7 * dpiScale), Qt::AlignCenter,
		QString::fromUtf8("动态统计报告"));
	y += 7 * dpiScale + 1 * dpiScale;

	// ===== 副标题 =====
	painter.setFont(subtitleFont);
	painter.setPen(QColor(102, 102, 102));
	QString infoStr = QString::fromUtf8("生成时间：%1    统计范围：%2 ~ %3    粒度：%4    指标数：%5")
		.arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
		.arg(config.dateFrom, config.dateTo)
		.arg(granularityNames.value(config.granularity, config.granularity))
		.arg(metricCols.size());
	painter.drawText(QRectF(0, y, pageW, 5 * dpiScale), Qt::AlignCenter, infoStr);
	y += 5 * dpiScale + 4 * dpiScale;

	// ===== 概览卡片（各指标合计） =====
	int cardCount = qMin(metricCols.size(), 6);
	if (cardCount > 0)
	{
		qreal cardW = (pageW - (cardCount + 1) * margin) / cardCount;
		qreal cardH = 15 * dpiScale;
		qreal cardY = y;

		for (int i = 0; i < cardCount; ++i)
		{
			const QString& col = metricCols[i];
			qint64 sum = 0;
			for (const auto& row : mDynamicStatsRows)
				sum += row.metrics.value(col).toLongLong();

			qreal cx = margin + i * (cardW + margin);
			QRectF cardRect(cx, cardY, cardW, cardH);
			painter.setPen(QPen(QColor(222, 226, 230), dpiScale * 0.3));
			painter.setBrush(QColor(248, 249, 250));
			painter.drawRoundedRect(cardRect, 2 * dpiScale, 2 * dpiScale);

			painter.setPen(QColor(74, 144, 217));
			painter.setFont(cardValueFont);
			painter.drawText(QRectF(cx, cardY + 1 * dpiScale, cardW, cardH * 0.5),
				Qt::AlignCenter, QString::number(sum));

			painter.setPen(QColor(108, 117, 125));
			painter.setFont(cardLabelFont);
			painter.drawText(QRectF(cx, cardY + cardH * 0.5, cardW, cardH * 0.5 - 2 * dpiScale),
				Qt::AlignCenter, keyToName.value(col, col));
		}
		y += 15 * dpiScale + 5 * dpiScale;
	}

	// ===== 数据表格 =====
	int totalCols = 1 + metricCols.size();
	qreal colSpacing = 3 * dpiScale;
	qreal timeColW = 30 * dpiScale;
	// 根据可用宽度动态计算 dataColW，确保不超出页面
	qreal availW = pageW - 2 * margin - timeColW - totalCols * colSpacing;
	qreal dataColW = totalCols > 1 ? qMax(12 * dpiScale, availW / (totalCols - 1)) : 0;

	qreal tableH = 5 * dpiScale;  // 表头行高
	qreal dataRowH = 4 * dpiScale;

	// 表头背景
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(52, 73, 94));
	painter.drawRoundedRect(QRectF(margin, y, pageW - 2 * margin, tableH), 2 * dpiScale, 2 * dpiScale);

	painter.setPen(Qt::white);
	painter.setFont(headerFont);
	painter.drawText(QRectF(margin, y, timeColW, tableH), Qt::AlignCenter,
		QString::fromUtf8("时间周期"));

	for (int c = 0; c < metricCols.size(); ++c)
	{
		qreal cx = margin + timeColW + colSpacing + c * (dataColW + colSpacing);
		painter.drawText(QRectF(cx, y, dataColW, tableH), Qt::AlignCenter,
			keyToName.value(metricCols[c], metricCols[c]));
	}
	y += tableH + 1 * dpiScale;

	// 数据行（交替背景）
	for (int r = 0; r < mDynamicStatsRows.size(); ++r)
	{
		if (y + dataRowH > pageH - 10 * dpiScale)
		{
			printer.newPage();
			y = margin;

			// 重复表头
			painter.setPen(Qt::NoPen);
			painter.setBrush(QColor(52, 73, 94));
			painter.drawRoundedRect(QRectF(margin, y, pageW - 2 * margin, tableH), 2 * dpiScale, 2 * dpiScale);
			painter.setPen(Qt::white);
			painter.setFont(headerFont);
			painter.drawText(QRectF(margin, y, timeColW, tableH), Qt::AlignCenter,
				QString::fromUtf8("时间周期"));
			for (int c = 0; c < metricCols.size(); ++c)
			{
				qreal cx = margin + timeColW + colSpacing + c * (dataColW + colSpacing);
				painter.drawText(QRectF(cx, y, dataColW, tableH), Qt::AlignCenter,
					keyToName.value(metricCols[c], metricCols[c]));
			}
			y += tableH + 1 * dpiScale;
		}

		// 交替行背景
		QColor rowBg = (r % 2 == 0) ? QColor(255, 255, 255) : QColor(245, 247, 250);
		painter.setPen(QPen(QColor(233, 236, 239), dpiScale * 0.2));
		painter.setBrush(rowBg);
		painter.drawRoundedRect(QRectF(margin, y, pageW - 2 * margin, dataRowH), 1 * dpiScale, 1 * dpiScale);

		painter.setPen(QColor(44, 62, 80));
		painter.setFont(cellFont);
		painter.drawText(QRectF(margin, y, timeColW, dataRowH), Qt::AlignCenter,
			mDynamicStatsRows[r].timePeriod);

		for (int c = 0; c < metricCols.size(); ++c)
		{
			qreal cx = margin + timeColW + colSpacing + c * (dataColW + colSpacing);
			QVariant val = mDynamicStatsRows[r].metrics.value(metricCols[c]);
			painter.drawText(QRectF(cx, y, dataColW, dataRowH), Qt::AlignCenter,
				val.isNull() ? QStringLiteral("0") : val.toString());
		}
		y += dataRowH + 1 * dpiScale;
	}

	// ===== 页脚 =====
	painter.setFont(subtitleFont);
	painter.setPen(QColor(150, 150, 150));
	painter.drawText(QRectF(pageW - 40 * dpiScale, pageH - 6 * dpiScale, 38 * dpiScale, 5 * dpiScale),
		int(Qt::AlignRight | Qt::AlignVCenter),
		QString::fromUtf8("页 1"));

	painter.end();

	addExportHistory(QFileInfo(filePath).fileName(), QStringLiteral("PDF"), filePath);
	QMessageBox::information(this, QString::fromUtf8("导出成功"),
		QString::fromUtf8("动态统计 PDF 已导出到:\n%1").arg(filePath));
}

void MapServiceDialog::onDynamicStatExportPng()
{
	if (mDynamicStatsRows.isEmpty())
	{
		QMessageBox::warning(this, QString::fromUtf8("无数据"),
			QString::fromUtf8("请先点击刷新统计获取数据后再导出。"));
		return;
	}

	QString filePath = QFileDialog::getSaveFileName(this,
		QString::fromUtf8("导出动态统计 PNG"),
		QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/动态统计.png",
		QString::fromUtf8("PNG 图片 (*.png);;所有文件 (*.*)"));
	if (filePath.isEmpty()) return;

	// 构建指标映射
	DynamicStatisticsConfig config = buildDynamicStatsConfig();
	QStringList metricCols;
	QMap<QString, QString> keyToName;
	collectDynamicMetricCols(mDynamicStatsRows, metricCols, keyToName);

	// 粒度中文
	QMap<QString, QString> granularityNames;
	granularityNames[QStringLiteral("year")]    = QString::fromUtf8("按年");
	granularityNames[QStringLiteral("quarter")] = QString::fromUtf8("按季度");
	granularityNames[QStringLiteral("month")]   = QString::fromUtf8("按月");

	// 高分辨率渲染（使用 pixelSize 确保跨 DPI 渲染一致）
	int margin = 40;
	int titleH = 48;
	int subtitleH = 28;
	int cardH = 100;
	int cardValueAreaH = cardH * 3 / 5;
	int cardLabelAreaH = cardH - cardValueAreaH;
	int sectionGap = 28;
	int cardCount = qMin(metricCols.size(), 6);

	// 读取图表类型（0=柱状图, 1=饼状图）
	int chartType = mDynamicChartTypeCombo ? mDynamicChartTypeCombo->currentData().toInt() : 0;

	// 预计算画布宽度（高分辨率）
	int canvasW = 1600;

	// 预计算画布高度（根据图表类型估算）
	int chartAreaH = 0;
	if (chartType == 1)
	{
		// 饼状图：标题行 + 饼图区 + 图例
		chartAreaH = 520;
	}
	else
	{
		// 柱状图：每个指标 = 标题 + N条柱
		int barsPerMetric = mDynamicStatsRows.size();
		chartAreaH = metricCols.size() * (38 + barsPerMetric * 38 + 32) + 30;
	}
	chartAreaH = qMax(chartAreaH, 200);

	int canvasH = margin * 2 + titleH + subtitleH + sectionGap;
	if (cardCount > 0) canvasH += cardH + sectionGap;
	canvasH += chartAreaH + sectionGap + 20;
	canvasH = qMax(canvasH, 700);

	QImage image(canvasW, canvasH, QImage::Format_ARGB32);
	image.fill(Qt::white);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);

	// 字体（使用 pixelSize 避免系统 DPI 缩放导致文本压盖）
	QFont titleFont(QStringLiteral("Microsoft YaHei"));
	titleFont.setPixelSize(33);
	titleFont.setBold(true);
	QFont subtitleFont(QStringLiteral("Microsoft YaHei"));
	subtitleFont.setPixelSize(18);
	QFont cardValueFont(QStringLiteral("Arial"));
	cardValueFont.setPixelSize(33);
	cardValueFont.setBold(true);
	QFont cardLabelFont(QStringLiteral("Microsoft YaHei"));
	cardLabelFont.setPixelSize(16);

	int y = margin;

	// ===== 标题 =====
	painter.setFont(titleFont);
	painter.setPen(QColor(44, 62, 80));
	painter.drawText(QRectF(0, y, canvasW, titleH), Qt::AlignCenter,
		QString::fromUtf8("动态统计报告"));
	y += titleH + 4;

	// ===== 副标题 =====
	painter.setFont(subtitleFont);
	painter.setPen(QColor(120, 120, 120));
	QString infoStr = QString::fromUtf8("生成时间：%1    统计范围：%2 ~ %3    粒度：%4")
		.arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
		.arg(config.dateFrom, config.dateTo)
		.arg(granularityNames.value(config.granularity, config.granularity));
	painter.drawText(QRectF(0, y, canvasW, subtitleH), Qt::AlignCenter, infoStr);
	y += subtitleH + sectionGap;

	// ===== 概览卡片 =====
	if (cardCount > 0)
	{
		int cardSectionW = canvasW - margin * 2;
		int cardW = (cardSectionW - (cardCount - 1) * 12) / cardCount;

		for (int i = 0; i < cardCount; ++i)
		{
			const QString& col = metricCols[i];
			qint64 sum = 0;
			for (const auto& row : mDynamicStatsRows)
				sum += row.metrics.value(col).toLongLong();

			int cx = margin + i * (cardW + 12);
			QRect cardRect(cx, y, cardW, cardH);

			painter.setPen(Qt::NoPen);
			painter.setBrush(QColor(245, 247, 250));
			painter.drawRoundedRect(cardRect, 6, 6);

			painter.setPen(QColor(74, 144, 217));
			painter.setFont(cardValueFont);
			painter.drawText(QRect(cx, y + 6, cardW, cardValueAreaH - 6),
				Qt::AlignCenter, QString::number(sum));

			painter.setPen(QColor(108, 117, 125));
			painter.setFont(cardLabelFont);
			painter.drawText(QRect(cx + 6, y + cardValueAreaH, cardW - 12, cardLabelAreaH - 6),
				Qt::AlignCenter | Qt::TextWordWrap, keyToName.value(col, col));
		}
		y += cardH + sectionGap;
	}

	// ===== 图表区域（根据图表类型选择渲染方式） =====

	// 颜色列表
	QList<QColor> barColors = {
		QColor(74, 144, 217), QColor(91, 143, 249), QColor(97, 221, 170),
		QColor(246, 189, 22), QColor(255, 107, 107), QColor(93, 112, 146),
		QColor(154, 132, 230), QColor(255, 156, 87)
	};
	// 饼图配色 — 暖色调，与图表控件保持一致
	const QList<QColor> pieColors = ChartWidget::pieColors();

	int chartSectionGap = 32;
	int chartCanvasW = canvasW - margin * 2;

	if (chartType == 1)
	{
		// ========== 饼状图 ==========
		// 构建各指标合计数据
		QMap<QString, int> metricTotals;  // 指标显示名 → 合计值
		for (const auto& col : metricCols)
		{
			qint64 sum = 0;
			for (const auto& row : mDynamicStatsRows)
				sum += row.metrics.value(col).toLongLong();
			if (sum > 0)
				metricTotals[keyToName.value(col, col)] = static_cast<int>(sum);
		}

		if (!metricTotals.isEmpty())
		{
			int total = 0;
			for (auto it = metricTotals.constBegin(); it != metricTotals.constEnd(); ++it)
				total += it.value();

			// 饼状图标题（先绘制标题，再根据剩余空间计算饼图位置，避免压盖）
			QFont pieTitleFont(QStringLiteral("Microsoft YaHei"));
			pieTitleFont.setPixelSize(21);
			pieTitleFont.setBold(true);
			painter.setFont(pieTitleFont);
			painter.setPen(QColor(74, 144, 217));
			painter.drawText(QRect(margin, y, chartCanvasW, 30),
				int(Qt::AlignLeft | Qt::AlignVCenter),
				QString::fromUtf8("指标分布（饼状图）"));
			y += 46;

			// 基于标题后的 y 重新计算饼图位置，确保饼图在标题下方
			// 顶面为椭圆：整体高度 ≈ 1.24*水平半径 + 侧壁厚度
			int pieRadius = qMin(190, qMin(chartCanvasW - 300, (canvasH - y - 60) * 5 / 8));
			if (pieRadius < 60) pieRadius = 60;
			int pieRadiusY = qRound(pieRadius * pie3DVerticalScale());
			int pieDepth   = qBound(8, pieRadius / 3, 36);
			int pieCenterX = margin + 20 + pieRadius;
			int pieCenterY = y + pieRadiusY + 10;
			int legendX = pieCenterX + pieRadius + 40;

			// ---- 立体饼图（与图表控件共用同一实现，保证风格统一）----
			QVector<QPair<QString, int>> pieItems;
			pieItems.reserve(metricTotals.size());
			for (auto it = metricTotals.constBegin(); it != metricTotals.constEnd(); ++it)
				pieItems.append(qMakePair(it.key(), it.value()));

			ChartWidget::drawPie3D(painter, QPoint(pieCenterX, pieCenterY), pieRadius, pieDepth,
				pieItems, pieColors);

			// 图例
			int legendItemH = 28;
			int legendY = pieCenterY - pieRadiusY + 5;
			int colorIdx = 0;
			int legendMaxW = 0;
		QFont legendFont(QStringLiteral("Microsoft YaHei"));
		legendFont.setPixelSize(16);
		for (auto it = metricTotals.constBegin(); it != metricTotals.constEnd(); ++it, ++colorIdx)
			{
				int ly = legendY + colorIdx * legendItemH;
				QColor color = pieColors[colorIdx % pieColors.size()];

				// 颜色块
				painter.setPen(Qt::NoPen);
				painter.setBrush(color);
				painter.drawRect(legendX, ly + 5, 16, 16);

				// 标签
				painter.setFont(legendFont);
				painter.setPen(QColor(60, 60, 60));
				double pct = total > 0 ? (100.0 * it.value() / total) : 0;
				QString label = QString("%1  %2 (%3%)")
					.arg(it.key()).arg(it.value()).arg(pct, 0, 'f', 1);
				painter.drawText(QRect(legendX + 20, ly, canvasW - legendX - 40, legendItemH),
					int(Qt::AlignLeft | Qt::AlignVCenter), label);

				QRect br;
				painter.drawText(QRect(legendX + 20, ly, canvasW - legendX - 40, legendItemH),
					int(Qt::AlignLeft | Qt::AlignVCenter), label, &br);
				legendMaxW = qMax(legendMaxW, br.width());
			}

			int contentH = qMax(pieRadiusY * 2 + pieDepth, colorIdx * legendItemH);
			y += contentH + chartSectionGap;
		}
	}
	else
	{
		// ========== 柱状图（默认） ==========
		for (const auto& col : metricCols)
		{
			int itemCount = mDynamicStatsRows.size();
			if (itemCount == 0) continue;

			// 收集数据
			qint64 total = 0;
			qint64 maxVal = 0;
			QVector<QPair<QString, qint64>> data;
			for (const auto& row : mDynamicStatsRows)
			{
				qint64 val = row.metrics.value(col).toLongLong();
				data.append({ row.timePeriod, val });
				total += val;
				maxVal = qMax(maxVal, val);
			}
			if (maxVal == 0) maxVal = 1;

		// 章节标题
		QFont chartTitleFont(QStringLiteral("Microsoft YaHei"));
		chartTitleFont.setPixelSize(20);
		chartTitleFont.setBold(true);
			painter.setFont(chartTitleFont);
			painter.setPen(QColor(74, 144, 217));
			painter.drawText(QRect(margin, y, chartCanvasW, 30),
				int(Qt::AlignLeft | Qt::AlignVCenter), keyToName.value(col, col));
			y += 38;

			int barX = margin + 90;
			int barMaxW = chartCanvasW - 180;
			int barH = 30;
			int barGap = 8;

		QFont barLabelFont(QStringLiteral("Microsoft YaHei"));
		barLabelFont.setPixelSize(16);
		QFont barValFont(QStringLiteral("Microsoft YaHei"));
		barValFont.setPixelSize(15);

			for (int i = 0; i < itemCount; ++i)
			{
				int barW = static_cast<int>(static_cast<double>(data[i].second) / maxVal * barMaxW);
				if (barW < 2) barW = 2;

				// 标签
				painter.setFont(barLabelFont);
				painter.setPen(QColor(60, 60, 60));
				painter.drawText(QRect(margin, y, barX - margin - 10, barH),
					int(Qt::AlignRight | Qt::AlignVCenter), data[i].first);

				// 柱形
				QColor color = barColors[i % barColors.size()];
				painter.setPen(Qt::NoPen);
				painter.setBrush(color);
				painter.drawRoundedRect(QRect(barX, y + 4, barW, barH - 8), 4, 4);

				// 数值
				painter.setFont(barValFont);
				painter.setPen(QColor(60, 60, 60));
				double pct = total > 0 ? (100.0 * data[i].second / total) : 0;
				QString valStr = QString("%1 (%2%)").arg(data[i].second).arg(pct, 0, 'f', 1);
				painter.drawText(QRect(barX + barW + 15, y, 150, barH),
					int(Qt::AlignLeft | Qt::AlignVCenter), valStr);

				y += barH + barGap;
			}
			y += chartSectionGap;
		}
	}

	painter.end();

	if (!image.save(filePath, "PNG"))
	{
		QMessageBox::critical(this, QString::fromUtf8("导出失败"),
			QString::fromUtf8("无法保存 PNG 图片: %1").arg(filePath));
		return;
	}

	addExportHistory(QFileInfo(filePath).fileName(), QStringLiteral("PNG"), filePath);
	QMessageBox::information(this, QString::fromUtf8("导出成功"),
		QString::fromUtf8("动态统计 PNG 已导出到:\n%1").arg(filePath));
}

// ===================== 地图装裱统计报表导出 =====================

void MapServiceDialog::onMountingStatExport()
{
	if (!mDAO) return;

	QString dateFrom = mDynamicDateFrom ? mDynamicDateFrom->date().toString("yyyy-MM-dd") : QString();
	QString dateTo   = mDynamicDateTo   ? mDynamicDateTo->date().toString("yyyy-MM-dd")   : QString();

	QList<MountingStatsRow> rows = mDAO->getMountingStatistics(dateFrom, dateTo);

	if (rows.isEmpty())
	{
		QMessageBox::information(this, QString::fromUtf8("提示"),
			QString::fromUtf8("当前筛选时间范围内没有地图装裱服务单。"));
		return;
	}

	QString filePath = QFileDialog::getSaveFileName(this,
		QString::fromUtf8("导出地图装裱统计报表"),
		QStringLiteral("地图装裱统计_%1.xlsx").arg(QDate::currentDate().toString("yyyyMMdd")),
		QString::fromUtf8("Excel 文件 (*.xlsx)"));

	if (filePath.isEmpty()) return;

	XlsxWriter xlsx;
	xlsx.setSheetName(QString::fromUtf8("地图装裱统计"));

	// 10列: A=序号, B=时间, C=服务部门, D-H=图框装裱费(5子列), I=占位, J=小计
	QVector<int> colWidths = { 8, 22, 30, 18, 14, 10, 16, 16, 2, 16 };
	xlsx.setColumnWidths(colWidths);

	// --- 行1：主表头 ---
	QStringList header1 = {
		QString::fromUtf8("序号"),
		QString::fromUtf8("时间"),
		QString::fromUtf8("服务部门\n（服务单位）"),
		QString::fromUtf8("图框装裱费"),
		QString(),      // D4 占位
		QString(),      // E4 占位
		QString(),      // F4 占位
		QString(),      // G4 占位
		QString(),      // H4 占位
		QString::fromUtf8("小计（元）"),
	};
	xlsx.writeHeader(header1);
	xlsx.mergeCells(1, 4, 1, 8);  // 合并 D1:H1 "图框装裱费"

	// 合并 A1:A2（序号）、B1:B2（时间）、C1:C2（服务部门）、J1:J2（小计）
	xlsx.mergeCells(1, 1, 2, 1);
	xlsx.mergeCells(1, 2, 2, 2);
	xlsx.mergeCells(1, 3, 2, 3);
	xlsx.mergeCells(1, 10, 2, 10);

	// --- 行2：子表头 ---
	QStringList header2 = {
		QString(),           // A2
		QString(),           // B2
		QString(),           // C2
		QString::fromUtf8("图框规格（米）"),
		QString::fromUtf8("材质"),
		QString::fromUtf8("数量"),
		QString::fromUtf8("单价（元）"),
		QString::fromUtf8("金额（元）"),
		QString(),           // I2 占位
		QString()            // J2
	};
	xlsx.writeRow(header2);

	// --- 数据行 ---
	int totalQty = 0;
	int seq = 1;
	for (const auto& row : rows)
	{
		QStringList dataRow;
		dataRow << QString::number(seq++);           // 序号
		dataRow << row.submitTime;                   // 时间
		dataRow << row.unitName;                     // 服务部门
		dataRow << row.frameSpec;                    // 图框规格（米）
		dataRow << row.material;                     // 材质
		dataRow << QString::number(row.totalQuantity);// 数量
		dataRow << QString();                        // 单价（元）— 不预填
		dataRow << QString();                        // 金额（元）— 不预填
		dataRow << QString();                        // 占位列（对齐）
		dataRow << QString();                        // 小计（元）— 不预填
		xlsx.writeRow(dataRow);

		totalQty += row.totalQuantity;
	}

	// --- 合计行 ---
	QStringList sumRow;
	sumRow << QString() << QString() << QString::fromUtf8("合计")
		   << QString() << QString()
		   << QString::number(totalQty)
		   << QString() << QString() << QString() << QString();
	xlsx.writeRow(sumRow);

	if (!xlsx.save(filePath))
	{
		QMessageBox::critical(this, QString::fromUtf8("导出失败"),
			QString::fromUtf8("无法保存 Excel 文件: %1").arg(filePath));
		return;
	}

	addExportHistory(QFileInfo(filePath).fileName(), QStringLiteral("XLSX"), filePath);
	QMessageBox::information(this, QString::fromUtf8("导出成功"),
		QString::fromUtf8("地图装裱统计报表已导出到:\n%1\n\n共 %2 条记录，合计数量: %3")
			.arg(filePath).arg(rows.size()).arg(totalQty));
}

void MapServiceDialog::onMountingLedgerExport()
{
	if (!mDAO) return;

	// 仅导出地图装裱图件个数>0 的服务单
	QList<MountingStatsRow> rows = mDAO->getMountingLedger();
	if (rows.isEmpty())
	{
		QMessageBox::information(this, QString::fromUtf8("提示"),
			QString::fromUtf8("当前没有地图装裱图件个数大于 0 的服务单。"));
		return;
	}

	// 加载装裱单价配置（与模板同目录 scripts/prices.json）
	QString appDir = QCoreApplication::applicationDirPath();
	QString pricePath = QDir(appDir).filePath(QStringLiteral("scripts/prices.json"));
	PriceConfig prices;
	const bool priceLoaded = prices.load(pricePath);

	// 数据行（9 列：序号/时间/服务部门/图框规格/材质/数量/单价/金额/小计）
	QVector<QStringList> dataRows;
	int totalQty = 0;
	double subtotalTotal = 0.0; // 所有记录小计（元）总和
	QSet<QString> unmatched; // 未匹配到单价的 规格×材质
	for (const auto& row : rows)
	{
		double unitPrice = 0.0;
		if (priceLoaded)
		{
			unitPrice = prices.lookup(row.frameSpec, row.material);
			if (unitPrice < 0.0)
			{
				unmatched.insert(row.frameSpec + QStringLiteral(" / ") + row.material);
				unitPrice = 0.0;
			}
		}
		const double amount = unitPrice * row.totalQuantity;

		QStringList dataRow;
		dataRow << QString::number(row.seq);                     // 序号
		dataRow << row.submitTime;                               // 时间 ← 服务提交时间
		dataRow << row.unitName;                                 // 服务部门 ← 服务单位
		dataRow << row.frameSpec;                                // 图框规格（米）← 装裱尺寸
		dataRow << row.material;                                 // 材质 ← 装裱材质
		dataRow << QString::number(row.totalQuantity);           // 数量 ← 装裱数量汇总
		dataRow << (unitPrice > 0.0 ? QString::number(unitPrice, 'f', 2) : QString()); // 单价（元）
		dataRow << (amount > 0.0 ? QString::number(amount, 'f', 2) : QString());         // 金额（元）
		dataRow << (amount > 0.0 ? QString::number(amount, 'f', 2) : QString());         // 小计（元）
		dataRows.append(dataRow);
		totalQty += row.totalQuantity;
		subtotalTotal += amount;
	}

	// 弹出保存对话框，由用户选择保存位置（默认带日期，避免覆盖原文件）
	QString filePath = QFileDialog::getSaveFileName(this,
		QString::fromUtf8("导出装裱清单"),
		QStringLiteral("装裱清单_%1.xlsx").arg(QDate::currentDate().toString("yyyyMMdd")),
		QString::fromUtf8("Excel 文件 (*.xlsx)"));

	if (filePath.isEmpty()) return;

	// 若输出文件已存在且正被 Excel/WPS 打开，先提示用户关闭，避免保存失败
	while (QFile::exists(filePath) && isFileLocked(filePath))
	{
		QMessageBox::StandardButton btn = QMessageBox::question(
			this, QString::fromUtf8("文件被占用"),
			QString::fromUtf8("文件正在被 Excel/WPS 打开：\n%1\n\n"
							   "请先关闭该文件，然后点击【重试】继续导出；\n"
							   "点击【取消】放弃本次导出。")
				.arg(QDir::toNativeSeparators(filePath)),
			QMessageBox::Retry | QMessageBox::Cancel, QMessageBox::Retry);
		if (btn == QMessageBox::Cancel)
			return;
	}

	bool saved = false;

	// 优先使用安装目录自带的模板（scripts/zbqd.xlsx）填充数据，保留模板格式
	QString templatePath = QDir(appDir).filePath(QStringLiteral("scripts/zbqd.xlsx"));
	if (QFile::exists(templatePath))
	{
		XlsxTemplateFiller filler;
		if (filler.load(templatePath)
			&& filler.fillData(2, dataRows, QStringLiteral("\u5408  \u8BA1"))
			&& filler.save(filePath))
		{
			saved = true;
		}
		else
		{
			QMessageBox::warning(this, QString::fromUtf8("模板填充失败"),
				QString::fromUtf8("读取模板 %1 填充失败:\n%2\n\n将改用内置格式重新生成。")
					.arg(templatePath, filler.lastError()));
		}
	}

	if (!saved)
	{
		// 回退：从零生成（模板不存在或填充失败时）
		XlsxWriter xlsx;
		xlsx.setSheetName(QString::fromUtf8("装裱清单"));

		// 9列: A=序号, B=时间, C=服务部门, D=图框规格（米）, E=材质, F=数量, G=单价, H=金额, I=小计
		QVector<int> colWidths = { 8, 22, 30, 18, 22, 10, 10, 12, 12 };
		xlsx.setColumnWidths(colWidths);

		// 表头
		QStringList header = {
			QString::fromUtf8("序号"),
			QString::fromUtf8("时间"),
			QString::fromUtf8("服务部门\n（服务单位）"),
			QString::fromUtf8("图框规格（米）"),
			QString::fromUtf8("材质"),
			QString::fromUtf8("数量"),
			QString::fromUtf8("单价（元）"),
			QString::fromUtf8("金额（元）"),
			QString::fromUtf8("小计（元）"),
		};
		xlsx.writeHeader(header);

		for (const auto& row : dataRows)
			xlsx.writeRow(row);

		// 合计行：小计 = 所有记录小计总和
		QStringList summaryRow;
		summaryRow << QString::fromUtf8("合  计");
		summaryRow << QString() << QString();
		summaryRow << QString() << QString() << QString::number(totalQty);
		summaryRow << QString() << QString() << QString::number(subtotalTotal, 'f', 2);
		xlsx.writeRow(summaryRow);

		saved = xlsx.save(filePath);
		if (!saved)
		{
			QMessageBox::critical(this, QString::fromUtf8("导出失败"),
				QString::fromUtf8("无法保存装裱清单文件:\n%1\n\n请确认该文件未被其他程序占用。").arg(filePath));
			return;
		}
	}

	QString msg = QString::fromUtf8("装裱清单已导出到:\n%1\n\n共 %2 条记录，合计数量: %3")
		.arg(filePath).arg(rows.size()).arg(totalQty);
	if (!unmatched.isEmpty())
	{
		QStringList items = unmatched.values();
		std::sort(items.begin(), items.end());
		msg += QString::fromUtf8("\n\n以下 规格/材质 在 prices.json 中未配置单价，金额按 0 计算:\n%1")
			.arg(items.join(QStringLiteral("\n")));
	}
	QMessageBox::information(this, QString::fromUtf8("导出成功"), msg);
}

// ============================================================================
// 服务台账导出
// ============================================================================

// 收集某服务单的所有服务形式详情（6 种），用于开本/数量汇总
struct LedgerForm
{
	QString name;
	QList<ServiceFormDetail> details;
};

static QList<LedgerForm> collectLedgerForms(const ServiceRecord& rec)
{
	QList<LedgerForm> forms;
	forms << LedgerForm{ QStringLiteral("地图编制"), compilationDetailsFromJson(rec.compilationDetails) };
	forms << LedgerForm{ QStringLiteral("地图打印"), serviceFormDetailsFromJson(rec.printDetails) };
	forms << LedgerForm{ QStringLiteral("地图装裱"), serviceFormDetailsFromJson(rec.mountingDetails) };
	forms << LedgerForm{ QStringLiteral("地图成品"), serviceFormDetailsFromJson(rec.productDetails) };
	forms << LedgerForm{ QStringLiteral("数据发送"), serviceFormDetailsFromJson(rec.dataDetails) };
	forms << LedgerForm{ QStringLiteral("其他"), serviceFormDetailsFromJson(rec.otherDetails) };
	return forms;
}

// 判断某服务形式是否包含有效记录（items 非空 或 图名非空）
static bool formHasContent(const LedgerForm& form)
{
	for (const auto& d : form.details)
	{
		if (!d.items.isEmpty() || !d.mapName.isEmpty())
			return true;
	}
	return false;
}

// 开本：收集所有服务形式中的尺寸(size)，去重后用中文顿号连接
static QString collectLedgerSheetSize(const QList<LedgerForm>& forms)
{
	QStringList sizes;
	for (const auto& form : forms)
	{
		for (const auto& d : form.details)
		{
			const QString sz = d.size.trimmed();
			if (!sz.isEmpty() && !sizes.contains(sz))
				sizes.append(sz);
		}
	}
	return sizes.join(QStringLiteral("、"));
}

// 服务形式：有记录的服务形式名称，用中文顿号连接
static QString collectLedgerFormNames(const QList<LedgerForm>& forms)
{
	QStringList names;
	for (const auto& form : forms)
	{
		if (formHasContent(form))
			names.append(form.name);
	}
	return names.join(QStringLiteral("、"));
}

// 汇总：对 forms 中所有 detail.items 的 quantity 求和（仅按 kind 命中则累加）
static int sumLedgerQuantity(const QList<LedgerForm>& forms,
							 const QStringList& kinds = QStringList())
{
	int total = 0;
	for (const auto& form : forms)
	{
		for (const auto& d : form.details)
		{
			for (const auto& item : d.items)
			{
				if (kinds.isEmpty() || kinds.contains(item.kind))
					total += item.quantity;
			}
		}
	}
	return total;
}

// 仅对某一服务形式（名称）的 items 求和
static int sumLedgerQuantityByForm(const QList<LedgerForm>& forms, const QString& formName)
{
	for (const auto& form : forms)
	{
		if (form.name == formName)
			return sumLedgerQuantity(QList<LedgerForm>{ form });
	}
	return 0;
}

void MapServiceDialog::onServiceLedgerExport()
{
	if (!mDAO) return;

	QList<ServiceRecord> records = mDAO->getAllRecords();
	if (records.isEmpty())
	{
		QMessageBox::information(this, QString::fromUtf8("提示"),
			QString::fromUtf8("当前没有服务单，无法导出服务台账。"));
		return;
	}

	// 构建数据行（19 列，顺序与 dtfw.xlsx 模板一致）
	QVector<QStringList> dataRows;
	// 各数量列合计（供合计行使用）
	int totalFrame = 0, totalCompile = 0, totalMounting = 0;
	int totalSingle = 0, totalAtlas = 0, totalGov = 0, totalCarto = 0, totalVector = 0, totalOther = 0;

	int seq = 0;
	for (const auto& rec : records)
	{
		++seq;

		QList<LedgerForm> forms = collectLedgerForms(rec);

		const int sumAll      = sumLedgerQuantity(forms);                                  // (5) 图幅数
		const int sumCompile  = sumLedgerQuantityByForm(forms, QStringLiteral("地图编制")); // (6) 编制图幅数
		const int sumMount    = sumLedgerQuantityByForm(forms, QStringLiteral("地图装裱")); // (7) 装裱数
		const int sumSingle   = sumLedgerQuantity(forms, { QStringLiteral("单张图") });
		const int sumAtlas    = sumLedgerQuantity(forms, { QStringLiteral("图集图册") });
		const int sumGov      = sumLedgerQuantity(forms, { QStringLiteral("政务图包") });
		const int sumCarto    = sumLedgerQuantity(forms, { QStringLiteral("制图数据") });
		const int sumVector   = sumLedgerQuantity(forms, { QStringLiteral("矢量数据") });
		const int sumOther    = sumLedgerQuantity(forms, { QStringLiteral("其他") });

		// (15) 类别：只取冒号前面的数字（兼容中英文冒号）
		QString category = rec.category.trimmed();
		int colonEn = category.indexOf(QLatin1Char(':'));
		int colonCn = category.indexOf(QString::fromUtf8("："));
		int colonIdx = -1;
		if (colonEn >= 0 && colonCn >= 0)
			colonIdx = qMin(colonEn, colonCn);
		else
			colonIdx = (colonEn >= 0) ? colonEn : colonCn;
		if (colonIdx > 0)
			category = category.left(colonIdx).trimmed();

		QStringList dataRow;
		dataRow << QString::number(seq);                                  // 1 序号
		dataRow << rec.submitTime;                                        // 2 日期（服务提交时间）
		dataRow << buildServiceContent(rec);                              // 3 服务内容
		dataRow << collectLedgerSheetSize(forms);                         // 4 开本
		dataRow << QString::number(sumAll);                               // 5 图幅数
		dataRow << QString::number(sumCompile);                           // 6 编制图幅数
		dataRow << QString::number(sumMount);                             // 7 装裱数
		dataRow << QString::number(sumSingle);                            // 8 单张图
		dataRow << QString::number(sumAtlas);                             // 9 图集图册
		dataRow << QString::number(sumGov);                               // 10 政务图包
		dataRow << QString::number(sumCarto);                             // 11 制图数据
		dataRow << QString::number(sumVector);                            // 12 矢量数据
		dataRow << QString::number(sumOther);                             // 13 其他
		dataRow << rec.unitName;                                          // 14 服务单位
		dataRow << category;                                              // 15 类别
		dataRow << collectLedgerFormNames(forms);                         // 16 服务形式
		dataRow << rec.arrangedOffice;                                    // 17 安排领导
		dataRow << QString();                                             // 18 经费（先不填）
		dataRow << rec.remarks;                                           // 19 备注
		dataRows.append(dataRow);

		totalFrame += sumAll;
		totalCompile += sumCompile;
		totalMounting += sumMount;
		totalSingle += sumSingle;
		totalAtlas += sumAtlas;
		totalGov += sumGov;
		totalCarto += sumCarto;
		totalVector += sumVector;
		totalOther += sumOther;
	}

	// 合计行数值（列0=“合 计”，其余数量列求和；列4为“合 计”文字）
	QStringList summaryValues = QStringList(QStringList() << QString()
		<< QString() << QString() << QString() << QString()
		<< QString() << QString() << QString() << QString()
		<< QString() << QString() << QString() << QString()
		<< QString() << QString() << QString() << QString()
		<< QString() << QString());
	summaryValues[1] = QString();
	summaryValues[2] = QString();
	summaryValues[3] = QString();
	summaryValues[4] = QString::number(totalFrame);
	summaryValues[5] = QString::number(totalCompile);
	summaryValues[6] = QString::number(totalMounting);
	summaryValues[7] = QString::number(totalSingle);
	summaryValues[8] = QString::number(totalAtlas);
	summaryValues[9] = QString::number(totalGov);
	summaryValues[10] = QString::number(totalCarto);
	summaryValues[11] = QString::number(totalVector);
	summaryValues[12] = QString::number(totalOther);

	// 弹出保存对话框
	QString filePath = QFileDialog::getSaveFileName(this,
		QString::fromUtf8("导出服务台账"),
		QStringLiteral("地图服务台账_%1.xlsx").arg(QDate::currentDate().toString("yyyyMMdd")),
		QString::fromUtf8("Excel 文件 (*.xlsx)"));

	if (filePath.isEmpty()) return;

	while (QFile::exists(filePath) && isFileLocked(filePath))
	{
		QMessageBox::StandardButton btn = QMessageBox::question(
			this, QString::fromUtf8("文件被占用"),
			QString::fromUtf8("文件正在被 Excel/WPS 打开：\n%1\n\n"
							   "请先关闭该文件，然后点击【重试】继续导出；\n"
							   "点击【取消】放弃本次导出。")
				.arg(QDir::toNativeSeparators(filePath)),
			QMessageBox::Retry | QMessageBox::Cancel, QMessageBox::Retry);
		if (btn == QMessageBox::Cancel)
			return;
	}

	bool saved = false;

	// 优先使用运行目录 scripts 下的 dtfw.xlsx 模板填充数据，保留模板格式
	QString appDir = QCoreApplication::applicationDirPath();
	QString templatePath = QDir(appDir).filePath(QStringLiteral("scripts/dtfw.xlsx"));
	if (QFile::exists(templatePath))
	{
		XlsxTemplateFiller filler;
		if (filler.load(templatePath)
			&& filler.fillData(1, dataRows, QStringLiteral("\u5408  \u8BA1"), summaryValues)
			&& filler.save(filePath))
		{
			saved = true;
		}
		else
		{
			QMessageBox::warning(this, QString::fromUtf8("模板填充失败"),
				QString::fromUtf8("读取模板 %1 填充失败:\n%2\n\n将改用内置格式重新生成。")
					.arg(templatePath, filler.lastError()));
		}
	}

	if (!saved)
	{
		// 回退：从零生成（模板不存在或填充失败时）
		XlsxWriter xlsx;
		xlsx.setSheetName(QString::fromUtf8("服务台账"));

		QVector<int> colWidths = { 6, 12, 50, 14, 8, 8, 8, 8, 8, 8, 8, 8, 8, 24, 8, 14, 20, 10, 24 };
		xlsx.setColumnWidths(colWidths);

		QStringList header = {
			QStringLiteral("序号"), QStringLiteral("日期"), QStringLiteral("服务内容"),
			QStringLiteral("开本"), QStringLiteral("图幅数"), QStringLiteral("编制图幅数"),
			QStringLiteral("装裱数"), QStringLiteral("单张图"), QStringLiteral("图集图册"),
			QStringLiteral("政务图包"), QStringLiteral("制图数据"), QStringLiteral("矢量数据"),
			QStringLiteral("其他"), QStringLiteral("服务单位"), QStringLiteral("类别"),
			QStringLiteral("服务形式"), QStringLiteral("安排领导"), QStringLiteral("经费"),
			QStringLiteral("备注"),
		};
		xlsx.writeHeader(header);

		for (const auto& row : dataRows)
			xlsx.writeRow(row);

		// 合计行
		QStringList summaryRow;
		summaryRow << QStringLiteral("合  计");
		for (int c = 1; c < 19; ++c)
			summaryRow << (c >= 4 && c <= 12 ? summaryValues[c] : QString());
		xlsx.writeRow(summaryRow);

		saved = xlsx.save(filePath);
		if (!saved)
		{
			QMessageBox::critical(this, QString::fromUtf8("导出失败"),
				QString::fromUtf8("无法保存服务台账文件:\n%1\n\n请确认该文件未被其他程序占用。").arg(filePath));
			return;
		}
	}

	QString msg = QString::fromUtf8("服务台账已导出到:\n%1\n\n共 %2 条服务单记录。")
		.arg(filePath).arg(records.size());
	QMessageBox::information(this, QString::fromUtf8("导出成功"), msg);
}
