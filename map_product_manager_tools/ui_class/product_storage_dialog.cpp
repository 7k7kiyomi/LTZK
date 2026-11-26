#include "product_storage_dialog.h"
#include "data_import_wizard.h"
#include "gdb_layer_selector_dialog.h"
#include "theme_assign_dialog.h"
#include "core/metadata_extractor.h"
#include "core/data_importer.h"
#include "core/add_to_map_helper.h"
#include "core/directory_helper.h"
#include "core/layer_icon_helper.h"
#include "database/product_dao.h"
#include "database/postgis_connector.h"
#include "database/schema_manager.h"
#include <qgsapplication.h>
#include <qgsmessagelog.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
#include <QProgressDialog>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QFontMetrics>
#include <QInputDialog>
#include <QUuid>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>
#include <QApplication>
#include <QFrame>
#include <QStyle>
#include <QTreeWidget>
#include <QSet>
#include <QTreeWidgetItem>
#include <QSettings>
#include <QPainter>
#include <QPixmap>
#include <functional>
#include "qgsgui.h"
#include "qgsvectorlayer.h"
#include "qgsvectorfilewriter.h"
#include "qgsrasterlayer.h"
#include "qgsproject.h"
#include "qgsdatasourceuri.h"
#include "qgscoordinatetransform.h"
#include "qgscoordinatetransformcontext.h"
#include "qgscoordinatereferencesystem.h"
#include "qgswkbtypes.h"
#include <libpq-fe.h>
#include "se_range_export.h"
#include "se_main_area_clip.h"
#include "map_extent_draw_tool.h"
#include <qgsmapcanvas.h>
#include <qgsmapsettings.h>

namespace {
// 显示导出结果窗口：宽高比适中、正文自动换行、内容过长时出现滚动条。
// 宽度参考最长单行，避免窗口过窄；高度按内容换行高度计算并设上限。
void showExportSummaryBox(QWidget* parent, const QString& title,
	const QString& message)
{
	QDialog dlg(parent);
	dlg.setWindowTitle(title);
	dlg.setWindowModality(Qt::WindowModal);

	QVBoxLayout* lay = new QVBoxLayout(&dlg);
	lay->setContentsMargins(18, 16, 18, 12);
	lay->setSpacing(10);

	// 标题行（信息图标 + 标题）
	QHBoxLayout* head = new QHBoxLayout;
	QLabel* iconLbl = new QLabel(&dlg);
	iconLbl->setPixmap(dlg.style()->standardIcon(QStyle::SP_MessageBoxInformation)
		.pixmap(32, 32));
	head->addWidget(iconLbl);
	QLabel* titleLbl = new QLabel(title, &dlg);
	QFont tf = titleLbl->font();
	tf.setBold(true);
	tf.setPointSize(tf.pointSize() + 1);
	titleLbl->setFont(tf);
	titleLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	head->addWidget(titleLbl, 1);
	lay->addLayout(head);

	// 正文：只读、自动换行、超长可滚动
	QTextBrowser* body = new QTextBrowser(&dlg);
	body->setReadOnly(true);
	body->setFrameShape(QFrame::StyledPanel);
	body->setLineWrapMode(QTextEdit::WidgetWidth);
	body->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	body->setPlainText(message);
	lay->addWidget(body, 1);

	// 底部按钮
	QHBoxLayout* btns = new QHBoxLayout;
	btns->addStretch(1);
	QPushButton* okBtn = new QPushButton(QStringLiteral("确定"), &dlg);
	okBtn->setDefault(true);
	btns->addWidget(okBtn);
	lay->addLayout(btns);
	QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

	// 计算合适的初始尺寸：
	// 宽度参考最长单行（上限避免超宽），高度按各段文字换行后的总行高估算
	QFontMetrics fm(body->font());
	int maxLinePx = 0;
	const QStringList parts = message.split(QLatin1Char('\n'));
	for (const QString& part : parts)
		maxLinePx = qMax(maxLinePx, fm.horizontalAdvance(part));

	const int width = qBound(
		560,
		maxLinePx + 120,
		880);
	const int textWidth = qMax(
		width - 72,
		220); // 估算正文可用宽度
	const int lineSpacing = fm.lineSpacing();
	int totalPx = 0;
	for (const QString& part : parts)
	{
		const int partPx = fm.horizontalAdvance(part);
		const int wrappedLines =
			qMax(1, (partPx + textWidth - 1) / textWidth);
		totalPx += wrappedLines * lineSpacing;
	}
	const int bodyH = qBound(
		180,
		totalPx + 30,
		500);
	const int height = qBound(
		320,
		bodyH + 170,
		660);

	dlg.resize(width, height);
	dlg.exec();
}
} // namespace


ProductStorageDialog::ProductStorageDialog(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	mExtractor = new MetadataExtractor(this);
	mDAO = new ProductDAO(this);
	mImporter = new DataImporter(this);

	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	setupTableColumns();
	setupConnections();
	initDetailTabs();

	// 读取数据库名作为目录树根节点
	QSettings settings(QStringLiteral("GarMap"), QStringLiteral("MapProductManager"));
	mDatabaseName = settings.value(QStringLiteral("db/database"), QStringLiteral("数据库")).toString();
	if (mDatabaseName.isEmpty())
		mDatabaseName = QStringLiteral("数据库");

	loadProductList();
	loadProductTypeTree();
	loadDirectoryTree();

	// 主分割器默认比例：左侧约 1/4，右侧 3/4，与整体页面协调
	ui.mainSplitter->setSizes({ 360, 840 });
	ui.mainSplitter->setStretchFactor(0, 3);
	ui.mainSplitter->setStretchFactor(1, 7);

	setWindowTitle("成果存储与管理");
	resize(1200, 700);
}

ProductStorageDialog::~ProductStorageDialog()
{
}

void ProductStorageDialog::setupTableColumns()
{
	// 表头与基本元数据（27项 CSV 字段）保持一致
	ui.mProductTable->setColumnCount(11);
	ui.mProductTable->setHorizontalHeaderLabels({
		QString::fromUtf8("数据名称"),
		QString::fromUtf8("数据唯一标识"),
		QString::fromUtf8("密级"),
		QString::fromUtf8("数据格式"),
		QString::fromUtf8("数据量"),
		QString::fromUtf8("所在市州"),
		QString::fromUtf8("比例尺分母"),
		QString::fromUtf8("项目名称"),
		QString::fromUtf8("生产单位"),
		QString::fromUtf8("生产日期"),
		QString::fromUtf8("更新时间")
	});
	ui.mProductTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui.mProductTable->horizontalHeader()->setStretchLastSection(true);
}

void ProductStorageDialog::setupConnections()
{
	connect(ui.mRefreshBtn, &QPushButton::clicked, this, &ProductStorageDialog::onRefreshList);
	connect(ui.mDeleteBtn, &QPushButton::clicked, this, &ProductStorageDialog::onDeleteProduct);
	connect(ui.mAddToMapBtn, &QPushButton::clicked, this, &ProductStorageDialog::onAddToMap);
	connect(ui.mProductTable, &QTableWidget::cellClicked, this, &ProductStorageDialog::onProductSelected);
	connect(ui.mProductTypeCombo, &QComboBox::currentTextChanged, this, &ProductStorageDialog::updateRasterFieldsVisibility);
	connect(mExtractor, &MetadataExtractor::extractionProgress, this, &ProductStorageDialog::onExtractionProgress);
	connect(mImporter, &DataImporter::importProgress, this, [this](int percent, const QString& msg) {
		ui.mProgressBar->setValue(percent);
		ui.mProgressBar->setFormat(msg + " (%p%)");
	});
	connect(ui.mProductTypeTree, &QTreeWidget::itemClicked, this, &ProductStorageDialog::onTypeTreeItemClicked);
	connect(ui.mDirectoryTree, &QTreeWidget::itemClicked, this, &ProductStorageDialog::onDirectoryTreeItemClicked);
	// 目录树节点勾选联动（父子级联 + 三态半选）
	connect(ui.mDirectoryTree, &QTreeWidget::itemChanged, this, &ProductStorageDialog::onDirectoryTreeItemChanged);

	// 地图成果树：支持多选与右键菜单（添加到地图）
	ui.mDirectoryTree->setContextMenuPolicy(Qt::CustomContextMenu);
	ui.mDirectoryTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	connect(ui.mDirectoryTree, &QWidget::customContextMenuRequested, this, &ProductStorageDialog::onDirectoryTreeContextMenu);

	// 数据库 schema 切换后自动刷新产品列表
	connect(PostgisConnector::instance(), &PostgisConnector::schemaChanged, this, [this](const QString&) {
		onRefreshList();
	});
}

void ProductStorageDialog::quickUpload(const QString& filePath) {
	QFileInfo fi(filePath);

	ui.mProductNameEdit->setText(fi.baseName());
	ui.mFileFormatEdit->setText(fi.suffix().toLower());
	ui.mFileSizeLabel->setText(formatFileSize(fi.size()));

	ProductType detected = MetadataExtractor::detectProductType(filePath);
	ui.mProductTypeCombo->setCurrentText(productTypeToString(detected));

	mIsGdbMultiLayer = false;
	mSelectedGdbLayers.clear();
	mSelectedGdbPath.clear();

	if (fi.suffix().compare("gdb", Qt::CaseInsensitive) == 0 ||
		fi.suffix().compare("mdb", Qt::CaseInsensitive) == 0) {
		if (mImportSilent)
		{
			// 批量/右键导入模式：不弹出图层选择器，自动选择该 GDB 中全部图层
			mSelectedGdbLayers = GDBLayerSelectorDialog::allLayerNames(filePath);
			if (mSelectedGdbLayers.isEmpty())
			{
				ui.mProductNameEdit->clear();
				QgsMessageLog::logMessage(
					QStringLiteral("[导入] GDB 中无可用矢量图层: %1").arg(filePath),
					"MapProductTools", Qgis::Warning);
				return;
			}
			mSelectedGdbPath = filePath;
			mIsGdbMultiLayer = true;
			ui.mProductNameEdit->setText(fi.baseName());
			ui.mFileSizeLabel->setText(
				QString("%1 (%2 个图层)").arg(fi.suffix().toUpper()).arg(mSelectedGdbLayers.size()));
		}
		else
		{
			GDBLayerSelectorDialog layerDlg(filePath, this);
			if (layerDlg.exec() != QDialog::Accepted) {
				ui.mProductNameEdit->clear();
				return;
			}
			mSelectedGdbPath = filePath;
			mSelectedGdbLayers = layerDlg.selectedLayers();
			mIsGdbMultiLayer = true;
			ui.mProductNameEdit->setText(fi.baseName());
			ui.mFileSizeLabel->setText(
				QString("%1 (%2 个图层)").arg(fi.suffix().toUpper()).arg(mSelectedGdbLayers.size()));
		}
	}

	onUploadFile(filePath);
}

void ProductStorageDialog::onUploadFile(const QString& filePath)
{
	mLastImportOk = false;
	if (filePath.isEmpty())
	{
		if (!mImportSilent)
			QMessageBox::warning(this, "提示", "请先选择文件");
		return;
	}

	// ============= GDB 多图层导入分支 =============
	if (mIsGdbMultiLayer && !mSelectedGdbLayers.isEmpty())
	{
		QString productType = ui.mProductTypeCombo->currentText().trimmed();
		QString versionNote = ui.mVersionEdit->text().trimmed();


		processGdbLayers(mSelectedGdbPath, mSelectedGdbLayers, productType, versionNote);

		ui.mProgressBar->setVisible(false);
		ui.mProgressLabel->setVisible(false);
			clearForm();
		loadProductTypeTree();
		loadProductList();
		return;
	}

	ui.mProgressBar->setVisible(true);
	ui.mProgressBar->setValue(0);

	// 仅刷新界面绘制，不处理用户输入事件，避免导入期间重入导致崩溃
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

	// 自动提取元数据
	ProductMetadata meta = mExtractor->extractMetadata(filePath);

	// 补充基本信息
	meta.productName = ui.mProductNameEdit->text();
	meta.productType = stringToProductType(ui.mProductTypeCombo->currentText());
	meta.versionNote = ui.mVersionNoteEdit->toPlainText();
	meta.createdBy = "YOUR_PASSWORD";
	meta.updatedBy = "YOUR_PASSWORD";

	// 检查是否已存在同名产品（精确匹配，使用已加载的产品列表）
	ProductMetadata existingMeta;
	bool isUpdate = false;  // 是否为更新已有产品
	for (const auto& p : mProducts)
	{
		if (p.productName == meta.productName)
		{
			existingMeta = p;
			isUpdate = true;
			break;
		}
	}

	// 如果 mProducts 中没找到，再用数据库精确查询一次
	if (!isUpdate)
	{
		ProductDAO::SearchCondition nameCond;
		nameCond.keyword = meta.productName;
		nameCond.limit = 100;
		auto existingProducts = mDAO->searchProducts(nameCond);
		for (const auto& p : existingProducts)
		{
			if (p.productName == meta.productName)
			{
				existingMeta = p;
				isUpdate = true;
				break;
			}
		}
	}

	if (isUpdate)
	{
		// 同名产品已存在，检查哈希是否相同
		if (existingMeta.fileHash == meta.fileHash)
		{
			// 哈希相同，文件内容未变化，拒绝上传
			ui.mProgressBar->setVisible(false);
			if (!mImportSilent)
			{
				QMessageBox::information(this, "提示",
					QString("同名产品「%1」已存在，且文件哈希（SHA256）与已有数据完全相同，不建议进行数据入库。\n"
							"当前版本: %2")
						.arg(meta.productName)
						.arg(existingMeta.currentVersion));
			}
			return;
		}

		// 哈希不同，版本号自动 +1
		int newVersion = existingMeta.currentVersion + 1;
		meta.currentVersion = newVersion;
		meta.id = existingMeta.id;
		meta.dataId = existingMeta.dataId;  // 保持同一 UUID

		// 更新 UI 上的版本号
		ui.mVersionEdit->setText(QString::number(newVersion));

	}
	else
	{
		// 全新产品
		meta.currentVersion = 1;
		meta.dataId = QUuid::createUuid().toString(QUuid::WithoutBraces);
		ui.mVersionEdit->setText("1");
	}

	// 自动按产品类型分配到对应目录（若指定了导入目标目录则分配到目标目录下）
	{
		if (mImportTargetDirId > 0)
		{
			meta.parentDirId = mImportTargetDirId;
			meta.directoryPath = mDAO->getDirectory(mImportTargetDirId).name;
		}
		else
		{
			QString typeDirName = productTypeToString(meta.productType);
			int dirId = mDAO->findOrCreateDirectory(typeDirName, 0);  // 根级目录
			if (dirId > 0)
			{
				meta.parentDirId = dirId;
				meta.directoryPath = typeDirName;
			}
		}
	}

	// 文件数据由 BLOB 存储，本地只保留原始路径引用
	meta.filePath = filePath;

	// 生成缩略图
	QString thumbnailPath = QDir::tempPath() + "/pmt_thumbnails/" + meta.dataId + ".png";
	QDir().mkpath(QFileInfo(thumbnailPath).absolutePath());
	if (MetadataExtractor::generateThumbnail(filePath, thumbnailPath))
	{
		meta.thumbnailPath = thumbnailPath;
	}

	// 生成图层表名
	QString layerTableName;
	if (meta.productType == ProductType::Vector || meta.productType == ProductType::Raster)
	{
		QFileInfo fi(filePath);
		QString safeName = fi.completeBaseName().toLower();
		safeName.replace(QRegularExpression("[^a-z0-9_\\x{4e00}-\\x{9fff}]"), "_");
		if (!safeName.isEmpty() && !safeName.at(0).isLetter())
			safeName = "l_" + safeName;
		if (safeName.isEmpty())
			safeName = "layer_data";
		layerTableName = safeName + "_v" + QString::number(meta.currentVersion);
		meta.layerTableName = layerTableName;
	}

	// ===== 第一步：完整数据入库（矢量/栅格）——先入库，再写元数据 =====
	bool dataImported = true;  // 非矢量/栅格类型视为不需要入库，直接成功
	int featureCount = -1;
	QString importError;

	if (meta.productType == ProductType::Vector)
	{
		ui.mProgressBar->setValue(10);
		ui.mProgressBar->setFormat("正在将矢量数据导入 PostGIS...");
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		int srcSrid = 0;
		if (meta.crs.startsWith("EPSG:"))
			srcSrid = meta.crs.mid(5).toInt();

		featureCount = mImporter->importVectorToPostGIS(
			filePath, layerTableName, srcSrid, 4490);

		if (featureCount < 0)
		{
			dataImported = false;
			importError = mImporter->lastError();
		}
		else if (featureCount == 0)
		{
			dataImported = false;
			const VectorImportStats& stats = mImporter->lastVectorStats();

			// 根据统计信息生成具体的错误原因
			if (stats.allSkipped())
			{
				importError = QString(
					"文件包含 %1 个要素，但全部没有空间几何数据（纯属性表）。\n"
					"该文件可能是不含几何字段的 CSV/DBF 等格式，\n"
					"不支持作为空间数据入库。")
					.arg(stats.totalRead);
			}
			else if (stats.allInsertFailed())
			{
				importError = QString(
					"文件包含 %1 个要素，但全部 INSERT 失败。\n"
					"可能原因：字段类型不兼容、几何数据损坏、或编码问题。\n"
					"请查看 QGIS 日志面板「数据入库」分类了解详情。")
					.arg(stats.totalRead);
			}
			else if (stats.noFeatures())
			{
				importError = QString(
					"GDAL 打开了文件，但未读取到任何要素。\n"
					"源文件可能确实为空，或文件格式无法被 GDAL 正确解析。\n"
					"文件路径: %1").arg(filePath);
			}
			else
			{
				importError = QString(
					"未成功导入任何要素。\n"
					"统计: 总读取 %1 个, 成功 %2, 无几何跳过 %3, INSERT失败 %4。\n"
					"请查看 QGIS 日志面板「数据入库」分类了解详情。")
					.arg(stats.totalRead).arg(stats.featureCount)
					.arg(stats.skipNoGeom).arg(stats.skipInsertFail);
			}

			// 清理空表
			SchemaManager schemaMgr;
			schemaMgr.dropDataLayerTable(layerTableName);
		}
	}
	else if (meta.productType == ProductType::Raster)
	{
		ui.mProgressBar->setValue(10);
		ui.mProgressBar->setFormat("正在将栅格数据导入 PostGIS...");
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		if (mImporter->importRasterToPostGIS(filePath, layerTableName, 4490))
		{
			featureCount = 1;
			// 从栅格表中获取 BLOB OID（支持跨机器网络导出）
			QString oidQuery = QString("SELECT file_oid FROM \"%1\" LIMIT 1").arg(layerTableName);
			QVariantMap oidResult = PostgisConnector::instance()->executeQueryOne(oidQuery);
			if (!oidResult.isEmpty())
			{
				meta.fileOid = oidResult["file_oid"].toInt();
			}
		}
		else
		{
			dataImported = false;
			importError = mImporter->lastError();
		}
	}

	// ===== 入库失败：清理并报错，不写元数据 =====
	if (!dataImported)
	{
		ui.mProgressBar->setVisible(false);
	
		QString detailMsg = importError.isEmpty()
			? "未知错误，请检查 QGIS 日志面板（「数据入库」和「成果存储」分类）获取详细信息"
			: importError;

		// 提取第一行作为简短原因
		QString shortReason = detailMsg.section('\n', 0, 0);

		if (!mImportSilent)
		{
			QMessageBox::critical(this, "入库失败",
				QString("数据入库失败，产品未保存！\n\n"
						"原因：%1\n\n"
						"排查建议：\n"
						"  1. 在 QGIS 菜单「视图 → 面板 → 日志消息」中查看详细日志\n"
						"  2. 确认源文件是有效的空间数据格式（如 GeoJSON、Shapefile、GPKG 等）\n"
						"  3. 检查数据库连接是否正常\n"
						"  4. 如果文件是纯属性表（CSV/DBF 不含几何），不支持空间入库").arg(shortReason));
		}
		return;
	}

	// 非矢量/非栅格类型（CAD, AI, CDR, PDF, Document, Archive, Other）：
	// 上传到 PostgreSQL BLOB，支持跨机器网络导出
	if (meta.productType != ProductType::Vector && meta.productType != ProductType::Raster)
	{
		int oid = PostgisConnector::instance()->loImport(filePath);
		if (oid > 0)
		{
			meta.fileOid = oid;
			QgsMessageLog::logMessage(
				QString::fromUtf8("BLOB loImport 成功: %1 → OID=%2").arg(filePath).arg(oid),
				QString::fromUtf8("成果仓"),
				Qgis::Info);
		}
		else
		{
			QString err = PostgisConnector::instance()->lastError();
			QgsMessageLog::logMessage(
				QString::fromUtf8("BLOB loImport 失败: %1, 错误=%2").arg(filePath).arg(err),
				QString::fromUtf8("成果仓"),
				Qgis::Critical);

			if (mImportSilent)
			{
				// 批量导入：BLOB 上传失败不阻断，记录日志后继续入库（仅本机可导出）
				QgsMessageLog::logMessage(
					QString::fromUtf8("批量导入: BLOB 上传失败，仅本机可导出: %1").arg(meta.productName),
					QString::fromUtf8("成果仓"),
					Qgis::Warning);
			}
			else
			{
				QMessageBox::StandardButton answer = QMessageBox::warning(this,
					QString::fromUtf8("BLOB 上传失败"),
					QString::fromUtf8(
						"文件未存入数据库 BLOB，将无法从其他机器导出！\n\n"
						"文件: %1\n"
						"错误: %2\n\n"
						"是否仍要继续入库？（仅本机可导出）")
						.arg(meta.productName).arg(err),
					QMessageBox::Yes | QMessageBox::No,
					QMessageBox::No);

				if (answer == QMessageBox::No)
				{
					ui.mProgressBar->setVisible(false);
					return;
				}
			}
		}
	}

	ui.mProgressBar->setValue(70);
	ui.mProgressBar->setFormat("正在写入元数据...");
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

	// ===== 第二步：写入/更新元数据到数据库 =====
	int productId = -1;

	if (isUpdate)
	{
		// 更新已有产品（updateProduct 内部会根据哈希自动创建版本）
		if (!mDAO->updateProduct(meta))
		{
			QString errMsg = PostgisConnector::instance()->lastError();
			ui.mProgressBar->setVisible(false);
			if (!mImportSilent)
				QMessageBox::critical(this, "错误", "产品更新失败\n" + errMsg);
			return;
		}
		productId = meta.id;
	}
	else
	{
		// 新建产品
		productId = mDAO->insertProduct(meta);
		if (productId <= 0)
		{
			// 元数据写入失败，回滚：删除已入库的图层表
			if (meta.productType == ProductType::Vector || meta.productType == ProductType::Raster)
			{
				SchemaManager schemaMgr;
				schemaMgr.dropDataLayerTable(layerTableName);
			}

			QString errMsg = PostgisConnector::instance()->lastError();

			ui.mProgressBar->setVisible(false);
			if (!mImportSilent)
				QMessageBox::critical(this, "错误", "元数据写入失败，已清理入库数据\n" + errMsg);
			return;
		}

		// 提取并写入类型专属元数据
		mDAO->enrichWithTypeMeta(productId);

		// 新建产品时创建初始版本记录
		VersionRecord verRec;
		verRec.productId = productId;
		verRec.versionNumber = 1;
		verRec.filePath = filePath;
		verRec.fileHash = meta.fileHash;
		verRec.fileSize = meta.fileSize;
		verRec.fileFormat = meta.fileFormat;
		verRec.layerTableName = meta.layerTableName;
		verRec.fileOid = meta.fileOid;
		verRec.changeNote = meta.versionNote.isEmpty() ? "初始版本" : meta.versionNote;
		verRec.changedBy = meta.createdBy;
		verRec.parentVersion = 0;  // 根版本，无父版本
		mDAO->insertVersionRecord(verRec);
	}

	// ===== 第三步：注册图层 =====
	QString importMsg;
	if (meta.productType == ProductType::Vector && featureCount > 0)
	{
		mDAO->registerLayer(productId, layerTableName, meta.geometryType, 4490, featureCount);
		importMsg = QString("，已导入 %1 个空间要素到图层表 %2")
			.arg(featureCount).arg(layerTableName);
	}
	else if (meta.productType == ProductType::Raster && featureCount > 0)
	{
		mDAO->registerLayer(productId, layerTableName, "RASTER", 4490, 1);
		importMsg = QString("，栅格数据已导入到图层表 %1").arg(layerTableName);
	}

	ui.mProgressBar->setValue(100);

	QString resultMsg;
	if (isUpdate)
	{
		resultMsg = QString("成果更新成功！\n产品ID: %1\n版本号: %2\nUUID: %3%4")
			.arg(productId).arg(meta.currentVersion).arg(meta.dataId).arg(importMsg);
	}
	else
	{
		resultMsg = QString("成果上传成功！\n产品ID: %1\nUUID: %2%3")
			.arg(productId).arg(meta.dataId).arg(importMsg);
	}
	mLastImportOk = true;
	if (!mImportSilent)
		QMessageBox::information(this, "成功", resultMsg);
	clearForm();
	loadProductTypeTree();
	loadProductList();

	ui.mProgressBar->setVisible(false);
}

void ProductStorageDialog::onDeleteProduct()
{
	int row = ui.mProductTable->currentRow();
	if (row < 0) return;

	// 从 item data 中获取 productId
	QTableWidgetItem* item = ui.mProductTable->item(row, 0);
	if (!item) return;
	int productId = item->data(Qt::UserRole).toInt();
	if (productId <= 0) return;

	auto reply = QMessageBox::question(this, "确认删除",
		QString("确定要删除产品 ID=%1 吗？此操作不可恢复。").arg(productId),
		QMessageBox::Yes | QMessageBox::No);

	if (reply == QMessageBox::Yes)
	{
		if (mDAO->deleteProduct(productId))
		{
			loadProductTypeTree();
			loadProductList();
			clearForm();
		}
	}
}

void ProductStorageDialog::onRefreshList()
{
	mCurrentTypeFilter.clear();
	// 先重新从数据库加载 mProducts（此时 search_path 已指向新 schema）
	loadProductList();
	// 再基于最新的 mProducts 构建分类树
	loadProductTypeTree();
}

void ProductStorageDialog::onProductSelected(int row, int /*col*/)
{
	if (row < 0) return;

	ui.mDeleteBtn->setEnabled(true);
	ui.mAddToMapBtn->setEnabled(true);

	// 从 item data 中获取 productId 以精确查找
	QTableWidgetItem* item = ui.mProductTable->item(row, 0);
	if (!item) return;
	int productId = item->data(Qt::UserRole).toInt();
	if (productId <= 0) return;

	// 在 mProducts 中查找对应记录
	const ProductMetadata* found = nullptr;
	for (const auto& p : mProducts)
	{
		if (p.id == productId)
		{
			found = &p;
			break;
		}
	}
	if (!found) return;

	fillForm(*found);
}

void ProductStorageDialog::onExtractionProgress(int percent, const QString& message)
{
	ui.mProgressBar->setValue(percent);
	ui.mProgressBar->setFormat(message + " (%p%)");
}

void ProductStorageDialog::loadProductList()
{
	// 目录过滤：按目录加载（递归到所有子目录，不叠加类型过滤）
	if (mCurrentDirId > 0)
	{
		QSet<int> dirIds;
		DirectoryHelper::collectDirectoryIds(*mDAO, mCurrentDirId, dirIds);
		QList<ProductMetadata> all = mDAO->getAllProducts();
		mProducts.clear();
		for (const auto& meta : all)
		{
			if (dirIds.contains(meta.parentDirId))
				mProducts.append(meta);
		}
	}
	else if (mCurrentDirId == 0)
	{
		mProducts = mDAO->getAllProducts();
	}
	else
	{
		mProducts = mDAO->getAllProducts();
	}

	ui.mProductTable->setRowCount(0);
	int row = 0;
	for (const auto& meta : mProducts)
	{
		// 目录过滤模式下不做类型过滤
		if (mCurrentDirId >= 0)
		{
			// 直接插入表格（类型过滤跳过）
		}
		else if (!mCurrentTypeFilter.isEmpty())
		{
			QString typeStr = productTypeToString(meta.productType);
			if (mCurrentTypeFilter.contains(QLatin1Char(';')))
			{
				// 多类型过滤（制图文件父节点），分号分隔
				const QStringList parts = mCurrentTypeFilter.split(QLatin1Char(';'), Qt::SkipEmptyParts);
				if (!parts.contains(typeStr))
					continue;
			}
			else if (typeStr != mCurrentTypeFilter)
				continue;
		}

		ui.mProductTable->insertRow(row);
		// 基本元数据字段（与表头一致的 11 列）
		ui.mProductTable->setItem(row, 0, new QTableWidgetItem(meta.productName));
		ui.mProductTable->setItem(row, 1, new QTableWidgetItem(meta.dataId));
		ui.mProductTable->setItem(row, 2, new QTableWidgetItem(securityLevelToString(meta.securityLevel)));
		ui.mProductTable->setItem(row, 3, new QTableWidgetItem(meta.fileFormat));
		ui.mProductTable->setItem(row, 4, new QTableWidgetItem(formatFileSize(meta.fileSize)));
		ui.mProductTable->setItem(row, 5, new QTableWidgetItem(meta.city));
		ui.mProductTable->setItem(row, 6, new QTableWidgetItem(meta.scale));
		ui.mProductTable->setItem(row, 7, new QTableWidgetItem(meta.projectName));
		ui.mProductTable->setItem(row, 8, new QTableWidgetItem(meta.producer));
		ui.mProductTable->setItem(row, 9, new QTableWidgetItem(meta.productionDate.isValid() ? meta.productionDate.toString("yyyy-MM-dd") : QString()));
		ui.mProductTable->setItem(row, 10, new QTableWidgetItem(meta.updatedAt.toString("yyyy-MM-dd hh:mm")));

		// 存储 productId 在 item data 中供选中时查找
		ui.mProductTable->item(row, 0)->setData(Qt::UserRole, meta.id);
		++row;
	}
}

void ProductStorageDialog::loadProductTypeTree()
{
	ui.mProductTypeTree->clear();

	// 类型树始终基于全库产品统计（不受目录过滤影响），
	// 与点击类型节点后右侧表格显示的全库按类型过滤结果保持一致
	const QList<ProductMetadata> allProducts = mDAO->getAllProducts();

	// 统计各类型数量
	QMap<QString, int> typeCounts;
	for (const auto& meta : allProducts)
	{
		QString typeStr = productTypeToString(meta.productType);
		typeCounts[typeStr]++;
	}

	// 根节点：全部成果
	int totalCount = allProducts.size();
	auto* rootItem = new QTreeWidgetItem(ui.mProductTypeTree);
	rootItem->setText(0, QString::fromUtf8("全部成果 (%1)").arg(totalCount));
	rootItem->setData(0, Qt::UserRole, ""); // 空字符串表示无过滤
	rootItem->setIcon(0, style()->standardIcon(QStyle::SP_DirHomeIcon));
	rootItem->setExpanded(true);

	// ── 矢量数据 ──
	{
		int cnt = typeCounts.value(QString::fromUtf8("矢量数据"), 0);
		auto* item = new QTreeWidgetItem(rootItem);
		item->setText(0, QString::fromUtf8("矢量数据 (%1)").arg(cnt));
		item->setData(0, Qt::UserRole, QString::fromUtf8("矢量数据"));
		item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
	}

	// ── 栅格数据 ──
	{
		int cnt = typeCounts.value(QString::fromUtf8("栅格数据"), 0);
		auto* item = new QTreeWidgetItem(rootItem);
		item->setText(0, QString::fromUtf8("栅格数据 (%1)").arg(cnt));
		item->setData(0, Qt::UserRole, QString::fromUtf8("栅格数据"));
		item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
	}

	// ── 制图文件（含 CAD / AI / CDR / PDF） ──
	{
		QStringList drawingTypes = {
			QStringLiteral("CAD"),
			QStringLiteral("AI"),
			QStringLiteral("CDR"),
			QStringLiteral("PDF")
		};
		int drawingTotal = 0;
		for (const auto& t : drawingTypes)
			drawingTotal += typeCounts.value(t, 0);

		auto* drawingItem = new QTreeWidgetItem(rootItem);
		drawingItem->setText(0, QString::fromUtf8("制图文件 (%1)").arg(drawingTotal));
		drawingItem->setData(0, Qt::UserRole, drawingTypes.join(QStringLiteral(";")));  // "CAD;AI;CDR;PDF"
		drawingItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
	}

	// ── 其它类型文件（文档+压缩包+其他统一归并）──
	{
		int cnt = typeCounts.value(QStringLiteral("文档文件"), 0)
		        + typeCounts.value(QStringLiteral("压缩包"), 0)
		        + typeCounts.value(QStringLiteral("Other"), 0);
		auto* item = new QTreeWidgetItem(rootItem);
		item->setText(0, QString::fromUtf8("其它类型文件 (%1)").arg(cnt));
		item->setData(0, Qt::UserRole, QStringLiteral("文档文件;压缩包;Other"));
		item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
	}

	// 默认选中根节点
	ui.mProductTypeTree->setCurrentItem(rootItem);
}

void ProductStorageDialog::onTypeTreeItemClicked(QTreeWidgetItem* item, int column)
{
	Q_UNUSED(column);
	if (!item) return;

	// 切换到类型分类页时，重置目录过滤，使用类型过滤
	mCurrentDirId = -1;
	mCurrentTypeFilter = item->data(0, Qt::UserRole).toString();
	loadProductList();
}

// ============================================================================
// 地图成果固定目录树
// ============================================================================

// 递归将 state 应用到 node 的整棵子树（含 node 自身）
static void applyDirectoryCheckState(QTreeWidgetItem* node, Qt::CheckState state)
{
	if (!node) return;
	node->setCheckState(0, state);
	for (int i = 0; i < node->childCount(); ++i)
		applyDirectoryCheckState(node->child(i), state);
}

// 依据各节点的直接子节点勾选状态，自下而上刷新父节点三态（全选/全不选/半选）
static void refreshDirectoryCheckStates(QTreeWidgetItem* childItem)
{
	if (!childItem) return;
	QTreeWidgetItem* parent = childItem->parent();
	while (parent)
	{
		bool allChecked = true;
		bool allUnchecked = true;
		for (int i = 0; i < parent->childCount(); ++i)
		{
			const Qt::CheckState st = parent->child(i)->checkState(0);
			if (st != Qt::Checked) allChecked = false;
			if (st != Qt::Unchecked) allUnchecked = false;
		}
		Qt::CheckState next = allChecked ? Qt::Checked
			: (allUnchecked ? Qt::Unchecked : Qt::PartiallyChecked);
		if (parent->checkState(0) != next)
			parent->setCheckState(0, next);
		parent = parent->parent();
	}
}

// 递归填充某个目录节点下的所有子目录
static void populateDirectoryChildren(QTreeWidgetItem* parentItem, int parentDirId,
	ProductDAO& dao, QStyle* style, const QSet<int>& fixedDirIds, const QSet<int>& productLeafDirIds)
{
	const auto children = dao.getChildDirectories(parentDirId);
	for (const auto& child : children)
	{
		auto* ci = new QTreeWidgetItem(parentItem);
		ci->setText(0, child.name); // 地图成果目录树为纯结构展示，不显示统计数目
		ci->setData(0, Qt::UserRole, child.id);
		ci->setData(0, Qt::UserRole + 1, 0); // 0=目录节点
		// 目录节点可勾选，勾选状态用于控制"导出所选数据"范围（默认不勾选，按需勾选）
		ci->setFlags(ci->flags() | Qt::ItemIsUserCheckable);
		ci->setCheckState(0, Qt::Unchecked);

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
				// 产品数据节点同样可勾选（用于精确控制导出范围，默认不勾选）
				pi->setFlags(pi->flags() | Qt::ItemIsUserCheckable);
				pi->setCheckState(0, Qt::Unchecked);
			}
		}

		populateDirectoryChildren(ci, child.id, dao, style, fixedDirIds, productLeafDirIds);
	}
}

void ProductStorageDialog::loadDirectoryTree()
{
	ui.mDirectoryTree->clear();

	// 重建期间阻止信号，避免逐个 setCheckState 触发级联勾选逻辑
	ui.mDirectoryTree->blockSignals(true);

	// 根节点 = 数据库名称（dirId=0 表示全部）
	auto* root = new QTreeWidgetItem(ui.mDirectoryTree);
	root->setText(0, mDatabaseName);
	root->setData(0, Qt::UserRole, 0);
	root->setIcon(0, style()->standardIcon(QStyle::SP_DirHomeIcon));
	root->setFlags(root->flags() | Qt::ItemIsUserCheckable);
	root->setCheckState(0, Qt::Unchecked);
	root->setExpanded(true);

	// 幂等创建固定目录节点
	auto ids = DirectoryHelper::ensureFixedDirectories(*mDAO);
	QSet<int> fixedDirIds;
	fixedDirIds << ids.drawingResults << ids.resultAI << ids.resultPDF << ids.resultOther
	            << ids.drawingElements << ids.elementImage << ids.elementShading << ids.elementFeature
	            << ids.drawingMaterials << ids.materialDoc << ids.materialTable << ids.materialOther;

	// 需要在节点下展示产品数据子节点的固定叶子节点
	QSet<int> productLeafDirIds;
	productLeafDirIds << ids.resultAI << ids.resultPDF << ids.resultOther
	                  << ids.elementImage << ids.elementShading
	                  << ids.materialDoc << ids.materialTable << ids.materialOther;

	// 制图成果 / 制图要素 / 制图资料（从数据库实际目录加载，含产品数）
	populateDirectoryChildren(root, 0, *mDAO, style(), fixedDirIds, productLeafDirIds);

	ui.mDirectoryTree->expandAll();

	ui.mDirectoryTree->blockSignals(false);
}

void ProductStorageDialog::executeExternalDirectoryOperation(
	const QString& operation,
	int directoryId,
	int productId,
	const QList<int>& productIds)
{
	QTreeWidgetItem* root = ui.mDirectoryTree->topLevelItem(0);
	if (!root)
		return;

	QTreeWidgetItem* target = nullptr;
	std::function<void(QTreeWidgetItem*)> findTarget = [&](QTreeWidgetItem* item) {
		if (!item || target)
			return;

		const int nodeKind = item->data(0, Qt::UserRole + 1).toInt();
		const int itemId = item->data(0, Qt::UserRole).toInt();
		if ((productId > 0 && nodeKind == 1 && itemId == productId) ||
			(productId <= 0 && nodeKind != 1 && itemId == qMax(directoryId, 0)))
		{
			target = item;
			return;
		}

		for (int index = 0; index < item->childCount(); ++index)
			findTarget(item->child(index));
	};
	findTarget(root);
	if (!target)
		return;

	// 主程序传入的是数据库列表中选中的产品 ID。将这些 ID 映射到本对话框
	// 的目录树勾选状态，后续直接复用现有 collectCurrentNodeProducts() 逻辑。
	if (productId <= 0 && !productIds.isEmpty())
	{
		setDirectorySubtreeCheck(root, Qt::Unchecked);
		const QSet<int> selectedIds = QSet<int>(productIds.begin(), productIds.end());
		mDirectoryCheckSyncing = true;
		std::function<void(QTreeWidgetItem*)> applySelection = [&](QTreeWidgetItem* item) {
			if (!item)
				return;
			if (item->data(0, Qt::UserRole + 1).toInt() == 1 &&
				selectedIds.contains(item->data(0, Qt::UserRole).toInt()))
			{
				item->setCheckState(0, Qt::Checked);
				refreshDirectoryCheckStates(item);
			}
			for (int index = 0; index < item->childCount(); ++index)
				applySelection(item->child(index));
		};
		applySelection(root);
		mDirectoryCheckSyncing = false;
	}

	ui.mDirectoryTree->setCurrentItem(target);
	if (operation == QStringLiteral("create-directory"))
		onCreateDirectory();
	else if (operation == QStringLiteral("delete-directory"))
		onDeleteDirectory();
	else if (operation == QStringLiteral("rename-directory"))
		onRenameDirectory();
	else if (operation == QStringLiteral("export"))
		onExportData();
	else if (operation == QStringLiteral("range"))
		onExportByRange();
	else if (operation == QStringLiteral("main-area"))
		onExportByMainAreaClip();
	else if (operation == QStringLiteral("theme"))
		onAddSelectionToTheme();
	else if (operation == QStringLiteral("map"))
		onAddDirSelectionToMap();
}

void ProductStorageDialog::onDirectoryTreeItemClicked(QTreeWidgetItem* item, int column)
{
	Q_UNUSED(column);
	if (!item) return;

	// 产品数据节点：按所属目录加载产品列表，并定位选中该产品
	if (item->data(0, Qt::UserRole + 1).toInt() == 1)
	{
		int productId = item->data(0, Qt::UserRole).toInt();
		auto meta = mDAO->getProduct(productId);
		if (meta.id <= 0) return;

		mCurrentTypeFilter.clear();
		mCurrentDirId = meta.parentDirId;
		loadProductList();

		for (int r = 0; r < ui.mProductTable->rowCount(); ++r)
		{
			QTableWidgetItem* ti = ui.mProductTable->item(r, 0);
			if (ti && ti->data(Qt::UserRole).toInt() == productId)
			{
				ui.mProductTable->selectRow(r);
				onProductSelected(r, 0);
				break;
			}
		}
		return;
	}

	// 切换到地图成果页时，重置类型过滤，使用目录过滤
	mCurrentTypeFilter.clear();
	mCurrentDirId = item->data(0, Qt::UserRole).toInt();
	loadProductList();
}

// 目录树节点勾选联动：勾选/取消勾选某节点时同步其整棵子树，并刷新父节点三态
void ProductStorageDialog::onDirectoryTreeItemChanged(QTreeWidgetItem* item, int column)
{
	if (!item || column != 0) return;
	if (mDirectoryCheckSyncing) return;

	const Qt::CheckState state = item->checkState(0);
	if (state == Qt::PartiallyChecked)
		return; // 半选状态仅由父节点按子节点汇总得出，不作为向下传播的目标

	mDirectoryCheckSyncing = true;
	for (int i = 0; i < item->childCount(); ++i)
		applyDirectoryCheckState(item->child(i), state);
	refreshDirectoryCheckStates(item);
	mDirectoryCheckSyncing = false;
}

// 右键菜单批量勾选/取消勾选：整棵子树置为指定状态后刷新父节点三态
void ProductStorageDialog::setDirectorySubtreeCheck(QTreeWidgetItem* node, Qt::CheckState state)
{
	if (!node) return;
	mDirectoryCheckSyncing = true;
	applyDirectoryCheckState(node, state);
	refreshDirectoryCheckStates(node);
	mDirectoryCheckSyncing = false;
}

void ProductStorageDialog::onDirectoryTreeContextMenu(const QPoint& pos)
{
	auto* item = ui.mDirectoryTree->itemAt(pos);
	if (!item) return;

	// 记录当前右键节点为选中项，便于后续操作基于该节点
	ui.mDirectoryTree->setCurrentItem(item);

	int nodeKind = item->data(0, Qt::UserRole + 1).toInt(); // 0=目录节点 1=产品数据节点
	int dirId = item->data(0, Qt::UserRole).toInt();

	QMenu menu(this);

	// 目录节点（制图成果/制图要素/制图资料及其子节点、导入的目录）：目录管理 + 数据导入导出
	if (nodeKind == 0)
	{
		QAction* actNewDir = menu.addAction(QStringLiteral("新建目录"));
		QAction* actDeleteDir = menu.addAction(QStringLiteral("删除目录"));
		QAction* actRename = menu.addAction(QStringLiteral("重命名"));
		menu.addSeparator();
		QAction* actCheckAll = menu.addAction(QStringLiteral("勾选本节点下全部成果"));
		QAction* actUncheckAll = menu.addAction(QStringLiteral("取消勾选本节点下全部成果"));
		menu.addSeparator();
		QAction* actImport = menu.addAction(QStringLiteral("导入数据"));
		QAction* actExport = menu.addAction(QStringLiteral("导出所选数据"));
		QAction* actRange = menu.addAction(QStringLiteral("按范围导出"));
		QAction* actMainArea = menu.addAction(QStringLiteral("按制图导出"));
		menu.addSeparator();
		QAction* actAddToMap = menu.addAction(QStringLiteral("添加到地图"));
		QAction* actAddToTheme = menu.addAction(QStringLiteral("添加到主题"));

		// 根节点（数据库名称，dirId=0）不允许删除/重命名
		if (dirId == 0)
		{
			actDeleteDir->setEnabled(false);
			actRename->setEnabled(false);
		}

		QAction* chosen = menu.exec(ui.mDirectoryTree->viewport()->mapToGlobal(pos));
		if (chosen == actNewDir)
			onCreateDirectory();
		else if (chosen == actDeleteDir)
			onDeleteDirectory();
		else if (chosen == actRename)
			onRenameDirectory();
		else if (chosen == actCheckAll)
			setDirectorySubtreeCheck(item, Qt::Checked);
		else if (chosen == actUncheckAll)
			setDirectorySubtreeCheck(item, Qt::Unchecked);
		else if (chosen == actImport)
			onImportData();
		else if (chosen == actExport)
			onExportData();
		else if (chosen == actRange)
			onExportByRange();
		else if (chosen == actMainArea)
			onExportByMainAreaClip();
		else if (chosen == actAddToMap)
			onAddDirSelectionToMap();
		else if (chosen == actAddToTheme)
			onAddSelectionToTheme();
	}
	else
	{
		// 产品数据节点：导出所选数据 + 范围裁剪导出 + 添加到地图/主题
		QAction* actExport = menu.addAction(QStringLiteral("导出所选数据"));
		QAction* actRange = menu.addAction(QStringLiteral("按范围导出"));
		QAction* actMainArea = menu.addAction(QStringLiteral("按制图导出"));
		menu.addSeparator();
		QAction* actAddToMap = menu.addAction(QStringLiteral("添加到地图"));
		QAction* actAddToTheme = menu.addAction(QStringLiteral("添加到主题"));

		QAction* chosen = menu.exec(ui.mDirectoryTree->viewport()->mapToGlobal(pos));
		if (chosen == actExport)
			onExportData();
		else if (chosen == actRange)
			onExportByRange();
		else if (chosen == actMainArea)
			onExportByMainAreaClip();
		else if (chosen == actAddToMap)
			onAddDirSelectionToMap();
		else if (chosen == actAddToTheme)
			onAddSelectionToTheme();
	}
}

void ProductStorageDialog::onAddDirSelectionToMap()
{
	if (!mQGisIface)
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("未获取到 QGIS 接口，无法添加到地图"));
		return;
	}

	QList<QTreeWidgetItem*> items = ui.mDirectoryTree->selectedItems();
	if (items.isEmpty())
	{
		QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中需要添加到地图的节点"));
		return;
	}

	// 汇总所有需要添加到地图的产品（去重）
	const QList<ProductMetadata> allProducts = mDAO->getAllProducts();
	QSet<int> productIds;
	QList<ProductMetadata> metas;
	for (auto* item : items)
	{
		if (!item) continue;

		// 产品数据节点：直接添加该产品
		if (item->data(0, Qt::UserRole + 1).toInt() == 1)
		{
			int pid = item->data(0, Qt::UserRole).toInt();
			if (productIds.contains(pid)) continue;
			auto meta = mDAO->getProduct(pid);
			if (meta.id <= 0) continue;
			productIds.insert(pid);
			metas.append(meta);
			continue;
		}

		// 目录节点：递归收集该目录下所有子孙产品（根节点 dirId=0 表示全部，不处理）
		int dirId = item->data(0, Qt::UserRole).toInt();
		if (dirId <= 0) continue;
		QSet<int> dirIds;
		DirectoryHelper::collectDirectoryIds(*mDAO, dirId, dirIds);
		for (const auto& meta : allProducts)
		{
			if (dirIds.contains(meta.parentDirId) && !productIds.contains(meta.id))
			{
				productIds.insert(meta.id);
				metas.append(meta);
			}
		}
	}

	if (metas.isEmpty())
	{
		QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("所选节点下没有可添加到地图的数据"));
		return;
	}

	int okCount = 0;
	for (const auto& meta : metas)
	{
		if (AddToMapHelper::addToMapWithFeedback(this, mQGisIface, meta))
			++okCount;
	}

	if (okCount < metas.size())
	{
		QMessageBox::warning(this, QStringLiteral("提示"),
			QStringLiteral("已将 %1/%2 个数据添加到地图，其余数据不支持添加到地图或加载失败。")
				.arg(okCount).arg(metas.size()));
	}
}

// 目录树右键"添加到主题"：把当前节点下选择的成果加入指定主题
void ProductStorageDialog::onAddSelectionToTheme()
{
	if (!PostgisConnector::instance() || !PostgisConnector::instance()->isConnected())
	{
		QMessageBox::warning(this, QStringLiteral("添加到主题"),
			QStringLiteral("数据库未连接，请先在「数据库连接配置」中连接数据库。"));
		return;
	}

	auto* item = ui.mDirectoryTree->currentItem();
	if (!item) return;

	// 待加入数据：优先取目录树中"勾选"的成果；
	// 未勾选任何成果时，取该节点下的全部成果
	QList<ProductMetadata> metas = collectCurrentNodeProducts();
	bool fromChecked = !metas.isEmpty();
	if (!fromChecked)
		metas = collectAllProductsUnderCurrentNode();
	if (metas.isEmpty())
	{
		QMessageBox::information(this, QStringLiteral("添加到主题"),
			QStringLiteral("所选节点下没有可加入主题的数据。"));
		return;
	}

	// 弹出对话框：选择目标主题 + 输入主题名称（默认取节点名称）
	ThemeAssignDialog dlg(metas, this);
	dlg.setDefaultContentName(item->text(0));
	if (dlg.exec() != QDialog::Accepted) return;

	QMessageBox::information(this, QStringLiteral("添加到主题"),
		QStringLiteral("已将 %1 个数据加入主题「%2」的主题内容「%3」。")
			.arg(metas.size())
			.arg(dlg.themeDisplayName())
			.arg(dlg.contentName()));
}

// ============ 目录树右键菜单：新建/删除/重命名/导入/导出 ============

void ProductStorageDialog::onCreateDirectory()
{
	auto* item = ui.mDirectoryTree->currentItem();
	if (!item) return;
	int parentDirId = item->data(0, Qt::UserRole).toInt();
	if (parentDirId < 0) parentDirId = 0;

	bool ok = false;
	QString name = QInputDialog::getText(this, "新建目录",
		QStringLiteral("在当前节点「%1」下新建子目录，请输入目录名称：").arg(item->text(0)),
		QLineEdit::Normal, QString(), &ok);
	if (!ok || name.trimmed().isEmpty()) return;

	int newId = mDAO->findOrCreateDirectory(name.trimmed(), parentDirId, 0);
	if (newId <= 0)
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("新建目录失败"));
		return;
	}
	loadDirectoryTree();
}

void ProductStorageDialog::onDeleteDirectory()
{
	auto* item = ui.mDirectoryTree->currentItem();
	if (!item) return;
	int dirId = item->data(0, Qt::UserRole).toInt();
	if (dirId <= 0)
	{
		QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("根节点不允许删除"));
		return;
	}

	// 统计该目录及其子树下的产品数量
	QList<int> subIds = mDAO->collectSubtreeDirectoryIds(dirId);
	int productCount = 0;
	for (int id : subIds)
		productCount += mDAO->getProductsByDirectory(id).size();

	QString msg = QStringLiteral("确定要删除目录「%1」及其所有子目录吗？")
		.arg(item->text(0));
	if (productCount > 0)
		msg += QStringLiteral("\n该目录下共有 %1 条产品数据，将一并删除！").arg(productCount);
	msg += QStringLiteral("\n\n此操作不可恢复，是否继续？");

	QMessageBox::StandardButton answer = QMessageBox::question(this,
		QStringLiteral("确认删除"), msg,
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	if (answer != QMessageBox::Yes) return;

	// 先删除所有子目录下的产品（从叶子到根顺序删除目录）
	QList<int> idsToDelete = mDAO->collectSubtreeDirectoryIds(dirId);
	// 按目录深度从深到浅排序：直接使用递归方式逐层删除
	std::function<void(int)> deleteTree = [&](int curId)
	{
		const auto children = mDAO->getChildDirectories(curId);
		for (const auto& c : children)
			deleteTree(c.id);
		// 删除当前目录下的产品
		const auto products = mDAO->getProductsByDirectory(curId);
		for (const auto& p : products)
			mDAO->deleteProduct(p.id);
		// 删除目录本身
		mDAO->deleteDirectory(curId);
	};
	deleteTree(dirId);

	loadDirectoryTree();
	loadProductList();
	loadProductTypeTree();
	clearForm();
}

void ProductStorageDialog::onRenameDirectory()
{
	auto* item = ui.mDirectoryTree->currentItem();
	if (!item) return;
	int dirId = item->data(0, Qt::UserRole).toInt();
	if (dirId <= 0)
	{
		QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("根节点不允许重命名"));
		return;
	}

	bool ok = false;
	QString newName = QInputDialog::getText(this, "重命名目录",
		QStringLiteral("请输入新的目录名称："),
		QLineEdit::Normal, item->text(0), &ok);
	if (!ok || newName.trimmed().isEmpty() || newName.trimmed() == item->text(0)) return;

	DirectoryNode node = mDAO->getDirectory(dirId);
	node.name = newName.trimmed();
	if (!mDAO->updateDirectory(node))
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("重命名失败"));
		return;
	}
	loadDirectoryTree();
}

// 导入单个文件到指定目录节点
bool ProductStorageDialog::importFileToDir(const QString& filePath, int targetDirId)
{
	if (filePath.isEmpty()) return false;

	// 快速导入（预填表单后自动入库），并指定目标目录
	mImportTargetDirId = targetDirId;
	mImportSilent = true;
	mLastImportOk = false;
	quickUpload(filePath);
	mImportTargetDirId = -1;
	mImportSilent = false;
	return mLastImportOk;
}

// 递归导入文件夹：保持源文件系统的目录组织方式，自动建立各级目录
void ProductStorageDialog::importPathRecursive(const QString& path, int targetDirId,
	int& imported, int& failed)
{
	QFileInfo fi(path);
	if (fi.isDir())
	{
		// gdb 目录属于单个数据源（FileGDB），作为文件整体导入，不递归其内部文件
		if (fi.suffix().compare("gdb", Qt::CaseInsensitive) == 0 ||
			fi.suffix().compare("mdb", Qt::CaseInsensitive) == 0)
		{
			if (importFileToDir(fi.absoluteFilePath(), targetDirId))
				++imported;
			else
				++failed;
			return;
		}

		// 普通文件夹：在目标目录下创建同名子目录（用文件夹图标表示）
		int subDirId = mDAO->findOrCreateDirectory(fi.fileName(), targetDirId, 0);
		if (subDirId <= 0) subDirId = targetDirId;

		QDir dir(path);
		const auto entries = dir.entryInfoList(QDir::Files | QDir::Dirs
			| QDir::NoDotAndDotDot | QDir::Hidden);
		for (const auto& e : entries)
			importPathRecursive(e.absoluteFilePath(), subDirId, imported, failed);
	}
	else if (fi.isFile())
	{
		if (importFileToDir(fi.absoluteFilePath(), targetDirId))
			++imported;
		else
			++failed;
	}
}

// 导出单个产品到指定本地目录（参考 product_search_dialog::exportDataFile 的核心逻辑）
bool ProductStorageDialog::exportProductFile(const ProductMetadata& meta, const QString& outputDir)
{
	auto* db = PostgisConnector::instance();
	if (!db->isConnected()) return false;

	QDir outDir(outputDir);
	if (!outDir.exists() && !outDir.mkpath(".")) return false;

	if (meta.productType == ProductType::Raster)
	{
		// 栅格：从 BLOB 导出（优先 file_oid，其次从图层表查询）
		int rasterOid = meta.fileOid;
		if (rasterOid <= 0 && !meta.layerTableName.isEmpty())
		{
			QVariantMap row = db->executeQueryOne(
				QString("SELECT file_oid FROM \"%1\" LIMIT 1").arg(meta.layerTableName));
			if (!row.isEmpty())
				rasterOid = row.value("file_oid").toInt();
		}
		if (rasterOid <= 0) return false;

		QString outPath = outDir.filePath(
			meta.productName + "." + meta.fileFormat.toLower());
		if (QFile::exists(outPath)) QFile::remove(outPath);
		return PostgisConnector::instance()->loExport(rasterOid, outPath);
	}

	if (meta.productType == ProductType::Vector)
	{
		// 矢量：从 PostGIS 图层表导出为 Shapefile
		if (meta.layerTableName.isEmpty()) return false;

		PGconn* conn = db->nativeConnection();
		if (!conn) return false;
		QString host = QString::fromUtf8(PQhost(conn));
		QString port = QString::fromUtf8(PQport(conn));
		QString dbName = QString::fromUtf8(PQdb(conn));
		QString user = QString::fromUtf8(PQuser(conn));
		QString password = QString::fromUtf8(PQpass(conn));

		// 解析 schema 与真实表名
		QString actualSchema = db->searchPath();
		if (actualSchema.isEmpty()) actualSchema = "public";
		QString findSchemaSql = QString(
			"SELECT table_schema, table_name FROM information_schema.tables "
			"WHERE LOWER(table_name) = LOWER('%1') LIMIT 1").arg(meta.layerTableName);
		auto schemaResult = db->executeQueryOne(findSchemaSql);
		QString foundSchema = schemaResult.value("table_schema").toString();
		QString realTableName = schemaResult.value("table_name").toString();
		if (!foundSchema.isEmpty()) actualSchema = foundSchema;
		QString effectiveTableName = realTableName.isEmpty()
			? meta.layerTableName.toLower() : realTableName;

		QgsDataSourceUri dsUri;
		dsUri.setConnection(host, port, dbName, user, password);
		dsUri.setDataSource(actualSchema, effectiveTableName, "geom", QString(), QString());
		dsUri.setUseEstimatedMetadata(true);

		QgsVectorLayer::LayerOptions layerOptions;
		QgsVectorLayer* vectorLayer = new QgsVectorLayer(
			dsUri.uri(), meta.layerTableName, "YOUR_PASSWORD", layerOptions);
		if (!vectorLayer->isValid())
		{
			delete vectorLayer;
			return false;
		}

		QString basePath = outDir.filePath(meta.productName);
		QStringList suffixes = { ".shp", ".shx", ".dbf", ".prj", ".cpg", ".qpj" };
		for (const QString& sfx : suffixes)
		{
			QFile::remove(basePath + sfx);
		}

		QgsVectorFileWriter::SaveVectorOptions options;
		options.driverName = "ESRI Shapefile";
		options.layerName = meta.layerTableName;
		options.fileEncoding = "UTF-8";
		options.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteFile;

		QString errorMsg;
		QString newFilename;
		QgsVectorFileWriter::WriterError writerError = QgsVectorFileWriter::writeAsVectorFormatV3(
			vectorLayer, basePath, vectorLayer->transformContext(), options, &errorMsg, &newFilename);
		delete vectorLayer;
		return writerError == QgsVectorFileWriter::NoError;
	}

	// 其它类型（CAD/AI/PDF/Document/Archive 等）：BLOB 导出
	if (meta.fileOid <= 0) return false;
	QString fileName = meta.productName;
	if (!meta.fileFormat.isEmpty())
		fileName += "." + meta.fileFormat.toLower();
	QString outPath = outDir.filePath(fileName);
	if (QFile::exists(outPath)) QFile::remove(outPath);
	return PostgisConnector::instance()->loExport(meta.fileOid, outPath);
}

// 导入数据：弹出数据导入向导，选择数据类型、数据目录并填写元数据后入库
void ProductStorageDialog::onImportData()
{
	DataImportWizard wizard(this);

	// 将导入的数据挂载到当前所选目录节点下（根节点除外）
	auto* item = ui.mDirectoryTree->currentItem();
	if (item && item->data(0, Qt::UserRole + 1).toInt() == 0)
	{
		int dirId = item->data(0, Qt::UserRole).toInt();
		if (dirId > 0)
			wizard.setTargetParentDirId(dirId);
	}

	wizard.exec();

	// 导入完成后自动刷新目录树、类型树与产品列表，使新入库的产品显示在树中
	loadDirectoryTree();
	loadProductList();
	loadProductTypeTree();
}

// 导出数据：目录节点仅导出目录树中已勾选节点（目录/成果）覆盖的产品；产品节点导出该单个产品
void ProductStorageDialog::onExportData()
{
	auto* item = ui.mDirectoryTree->currentItem();
	if (!item) return;

	// 汇总需要导出的产品
	QList<ProductMetadata> metas;
	int nodeKind = item->data(0, Qt::UserRole + 1).toInt();
	if (nodeKind == 1)
	{
		// 产品数据节点：导出单个产品
		int pid = item->data(0, Qt::UserRole).toInt();
		auto meta = mDAO->getProduct(pid);
		if (meta.id > 0) metas.append(meta);
	}
	else
	{
		// 目录节点：仅导出该目录下"已勾选"的成果（默认均不勾选；勾选到的目录/产品节点才纳入导出范围）
		int dirId = item->data(0, Qt::UserRole).toInt();
		QSet<int> dirIds;
		DirectoryHelper::collectDirectoryIds(*mDAO, dirId, dirIds);

		// 未勾选的产品数据节点：排除其对应产品
		QSet<int> excludedProductIds;
		// 未勾选的目录节点：排除其整棵子目录下的产品
		QSet<int> excludedDirIds;

		// 从当前节点出发向下扫描目录树，收集被取消勾选的节点范围
		std::function<void(QTreeWidgetItem*)> scan = [&](QTreeWidgetItem* nd) {
			if (!nd) return;
			for (int i = 0; i < nd->childCount(); ++i)
			{
				QTreeWidgetItem* ch = nd->child(i);
				if (!ch) continue;
				const Qt::CheckState st = ch->checkState(0);
				const int kind = ch->data(0, Qt::UserRole + 1).toInt();
				if (kind == 1) // 产品数据节点
				{
					if (st == Qt::Unchecked)
						excludedProductIds.insert(ch->data(0, Qt::UserRole).toInt());
					continue; // 产品节点无子节点
				}
				// 目录节点：未勾选则整棵子树排除，否则继续向下扫描
				if (st == Qt::Unchecked)
				{
					QSet<int> subIds;
					DirectoryHelper::collectDirectoryIds(*mDAO, ch->data(0, Qt::UserRole).toInt(), subIds);
					excludedDirIds.unite(subIds);
					continue;
				}
				scan(ch);
			}
		};
		scan(item);

		for (const auto& meta : mDAO->getAllProducts())
		{
			if (dirIds.contains(meta.parentDirId)
				&& !excludedDirIds.contains(meta.parentDirId)
				&& !excludedProductIds.contains(meta.id))
				metas.append(meta);
		}
	}

	if (metas.isEmpty())
	{
		QMessageBox::information(this, QStringLiteral("提示"),
			QStringLiteral("所选节点下没有可导出的数据。\n"
							"目录节点的导出范围为目录树中勾选的成果；\n"
							"请先在目录树中勾选需要导出的目录/成果节点，再执行导出。"));
		return;
	}

	QString outputDir = QFileDialog::getExistingDirectory(this,
		QStringLiteral("选择导出目录"), QString());
	if (outputDir.isEmpty()) return;

	int okCount = 0;
	QStringList failedNames;
	for (const auto& meta : metas)
	{
		if (exportProductFile(meta, outputDir))
			++okCount;
		else
			failedNames << meta.productName;
	}

	QString msg = QStringLiteral("导出完成：成功 %1/%2 个。")
		.arg(okCount).arg(metas.size());
	if (!failedNames.isEmpty())
		msg += QStringLiteral("\n失败：%1").arg(failedNames.join("、"));
	msg += QStringLiteral("\n导出目录：%1").arg(outputDir);
	showExportSummaryBox(this, QStringLiteral("导出完成"), msg);
}

// ==================== 按范围导出 / 按主区裁切导出 ====================
static bool isSpatialClippable(const ProductMetadata& meta)
{
	return meta.productType == ProductType::Vector || meta.productType == ProductType::Raster;
}

static QList<ProductMetadata> spatialClippableOnly(const QList<ProductMetadata>& src, int& skippedCount)
{
	QList<ProductMetadata> out;
	skippedCount = 0;
	for (const auto& meta : src)
	{
		if (isSpatialClippable(meta))
			out.append(meta);
		else
			++skippedCount;
	}
	return out;
}

QList<ProductMetadata> ProductStorageDialog::collectCurrentNodeProducts() const
{
	QList<ProductMetadata> metas;
	auto* item = ui.mDirectoryTree->currentItem();
	if (!item) return metas;

	int nodeKind = item->data(0, Qt::UserRole + 1).toInt();
	if (nodeKind == 1)
	{
		// 产品数据节点：该单个产品
		int pid = item->data(0, Qt::UserRole).toInt();
		auto meta = mDAO->getProduct(pid);
		if (meta.id > 0) metas.append(meta);
		return metas;
	}

	// 目录节点：递归收集该目录下所有产品，再按勾选范围剔除
	int dirId = item->data(0, Qt::UserRole).toInt();
	QSet<int> dirIds;
	DirectoryHelper::collectDirectoryIds(*mDAO, dirId, dirIds);

	// 未勾选的产品数据节点：排除其对应产品
	QSet<int> excludedProductIds;
	// 未勾选的目录节点：排除其整棵子目录下的产品
	QSet<int> excludedDirIds;

	std::function<void(QTreeWidgetItem*)> scan = [&](QTreeWidgetItem* nd) {
		if (!nd) return;
		for (int i = 0; i < nd->childCount(); ++i)
		{
			QTreeWidgetItem* ch = nd->child(i);
			if (!ch) continue;
			const Qt::CheckState st = ch->checkState(0);
			const int kind = ch->data(0, Qt::UserRole + 1).toInt();
			if (kind == 1) // 产品数据节点
			{
				if (st == Qt::Unchecked)
					excludedProductIds.insert(ch->data(0, Qt::UserRole).toInt());
				continue; // 产品节点无子节点
			}
			// 目录节点：未勾选则整棵子树排除，否则继续向下扫描
			if (st == Qt::Unchecked)
			{
				QSet<int> subIds;
				DirectoryHelper::collectDirectoryIds(*mDAO, ch->data(0, Qt::UserRole).toInt(), subIds);
				excludedDirIds.unite(subIds);
				continue;
			}
			scan(ch);
		}
	};
	scan(item);

	for (const auto& meta : mDAO->getAllProducts())
	{
		if (dirIds.contains(meta.parentDirId)
			&& !excludedDirIds.contains(meta.parentDirId)
			&& !excludedProductIds.contains(meta.id))
			metas.append(meta);
	}
	return metas;
}

QList<ProductMetadata> ProductStorageDialog::collectAllProductsUnderCurrentNode() const
{
	QList<ProductMetadata> metas;
	auto* item = ui.mDirectoryTree->currentItem();
	if (!item) return metas;

	// 产品数据节点：该单个产品
	if (item->data(0, Qt::UserRole + 1).toInt() == 1)
	{
		const int pid = item->data(0, Qt::UserRole).toInt();
		auto meta = mDAO->getProduct(pid);
		if (meta.id > 0) metas.append(meta);
		return metas;
	}

	// 目录节点：忽略勾选状态，收集其下全部产品；根节点（dirId<=0）表示全部成果
	const int dirId = item->data(0, Qt::UserRole).toInt();
	QSet<int> dirIds;
	if (dirId > 0)
		DirectoryHelper::collectDirectoryIds(*mDAO, dirId, dirIds);

	for (const auto& meta : mDAO->getAllProducts())
	{
		if (dirId <= 0 || dirIds.contains(meta.parentDirId))
			metas.append(meta);
	}
	return metas;
}

QgsCoordinateReferenceSystem ProductStorageDialog::currentMapCrs() const
{
	QgsCoordinateReferenceSystem crs;
	if (!mQGisIface) return crs;
	QgsMapCanvas* canvas = mQGisIface->mapCanvas();
	if (canvas)
		crs = canvas->mapSettings().destinationCrs();
	return crs;
}

bool ProductStorageDialog::runClipExportAndReport(const QList<ProductMetadata>& clippables,
	const MapProductClipEngine::ClipOptions& opt, const QString& title)
{
	if (clippables.isEmpty()) return false;

	// ---------- 导出进度显示（支持取消） ----------
	const int total = clippables.size();
	QProgressDialog progress(QStringLiteral("正在导出…"), QStringLiteral("取消"),
		0, total > 1 ? total : 0, this);
	progress.setWindowTitle(title);
	progress.setWindowModality(Qt::WindowModal);
	progress.setMinimumDuration(0);
	progress.setAutoClose(false);
	progress.setAutoReset(false);
	if (total > 1)
		progress.setRange(0, total);   // 多成果：进度条 = 已完成的成果数
	else
		progress.setRange(0, 0);       // 单成果：忙碌动画 + 阶段文字说明
	progress.setValue(0);
	progress.show();
	QApplication::processEvents();

	MapProductClipEngine::ClipOptions opt2 = opt;
	opt2.progress = [&progress](int done, int totalCount,
		const QString& message) -> bool
	{
		progress.setLabelText(message);
		if (totalCount > 1)
		{
			int v = done;
			if (v < 0) v = 0;
			else if (v > totalCount) v = totalCount;
			progress.setValue(v);
		}
		QApplication::processEvents();
		return !progress.wasCanceled();
	};

	const QList<MapProductClipEngine::ClipResult> results =
		MapProductClipEngine::clipProducts(clippables, opt2);

	const bool canceled = progress.wasCanceled();
	progress.close();
	QApplication::processEvents();

	if (canceled)
	{
		QMessageBox::information(this, title,
			QStringLiteral("导出已取消。\n已完成并写出的成果文件将保留在输出位置。"));
		return false;
	}

	int okCount = 0;
	QStringList failedNames;
	QStringList notes;
	for (const auto& r : results)
	{
		if (r.ok)
		{
			++okCount;
			if (!r.note.isEmpty()) notes << QStringLiteral("%1：%2").arg(r.productName, r.note);
		}
		else
		{
			QString err = r.error;
			if (err.isEmpty()) err = QStringLiteral("未知错误");
			failedNames << QStringLiteral("%1（%2）").arg(r.productName, err);
		}
	}

	QString msg = QStringLiteral("%1完成：成功 %2/%3 个。")
		.arg(title).arg(okCount).arg(results.size());
	if (!failedNames.isEmpty())
		msg += QStringLiteral("\n失败：\n%1").arg(failedNames.join("\n"));
	if (!notes.isEmpty())
		msg += QStringLiteral("\n\n说明：\n%1").arg(notes.join("\n"));
	showExportSummaryBox(this, title, msg);
	return failedNames.isEmpty();
}

// 依据所选输出格式解析裁剪导出的输出目标（「按范围导出」与「按主区裁切导出」共用）：
//   - GPKG / GDB：所有矢量成果写入同一个容器文件（explicitOutputFile +
//     mergeLayersIntoOneFile）；栅格无法写入矢量容器，落到该文件所在目录按
//     产品名输出 .tif（outputDir）；
//   - Shapefile：目标为文件夹，每个成果在该目录下导出为独立 shp（outputDir）。
// 成功返回 true；路径为空/不可用或用户取消覆盖时返回 false（已弹窗提示）。
static bool resolveClipOutputTarget(QWidget* parent, const QString& fmt,
	const QString& typed, MapProductClipEngine::ClipOptions& opt)
{
	const bool toGpkg = fmt.contains(QStringLiteral("GeoPackage"), Qt::CaseInsensitive);
	const bool toGdb = fmt.contains(QStringLiteral("GDB"), Qt::CaseInsensitive);
	const bool mergeIntoOneFile = toGpkg || toGdb;
	opt.vectorDriver = toGpkg ? QStringLiteral("GPKG")
		: (toGdb ? QStringLiteral("OpenFileGDB") : QStringLiteral("ESRI Shapefile"));

	if (typed.isEmpty())
	{
		QMessageBox::information(parent, QStringLiteral("提示"),
			QStringLiteral("未指定输出路径，无法执行导出。"));
		return false;
	}

	if (mergeIntoOneFile)
	{
		// 单一目标文件：补齐扩展名，必要时确认覆盖
		const QString wantSuffix = toGpkg ? QStringLiteral(".gpkg") : QStringLiteral(".gdb");
		QString finalFile = typed;
		if (!finalFile.endsWith(wantSuffix, Qt::CaseInsensitive))
		{
			const QFileInfo fi(finalFile);
			if (fi.suffix().isEmpty())
				finalFile += wantSuffix;
			else
				finalFile = fi.absolutePath() + "/" + fi.completeBaseName() + wantSuffix;
		}
		if (QFileInfo::exists(finalFile))
		{
			QMessageBox box(QMessageBox::Question, QStringLiteral("文件已存在"),
				QStringLiteral("目标文件已存在：\n%1\n\n是否覆盖？").arg(finalFile),
				QMessageBox::Yes | QMessageBox::No, parent);
			box.setButtonText(QMessageBox::Yes, QStringLiteral("覆盖"));
			box.setButtonText(QMessageBox::No, QStringLiteral("取消"));
			if (box.exec() != QMessageBox::Yes) return false;
		}
		// 目标文件所在目录若不存在则创建
		const QString parentDir = QFileInfo(finalFile).absolutePath();
		if (!parentDir.isEmpty() && !QDir(parentDir).exists() && !QDir().mkpath(parentDir))
		{
			QMessageBox::information(parent, QStringLiteral("提示"),
				QStringLiteral("无法创建输出目录：\n%1").arg(parentDir));
			return false;
		}
		opt.overwriteExisting = true; // 覆盖已在上面确认，容器文件由引擎清理后重建
		opt.mergeLayersIntoOneFile = true;
		opt.explicitOutputFile = finalFile;
		// 栅格无法写入矢量容器：落到目标文件所在目录，按产品名输出 .tif
		opt.outputDir = parentDir;
		return true;
	}

	// Shapefile：目标为文件夹，每个成果导出为独立 shp
	QString outDir = typed;
	if (!QFileInfo(outDir).isDir())
	{
		const QFileInfo fi(outDir);
		if (fi.suffix().isEmpty())
		{
			// 形如目录但尚不存在：自动创建
			if (!QDir().mkpath(outDir))
			{
				QMessageBox::information(parent, QStringLiteral("提示"),
					QStringLiteral("无法创建输出文件夹：\n%1").arg(outDir));
				return false;
			}
		}
		else
		{
			outDir = fi.absolutePath();
		}
	}
	if (outDir.isEmpty() || !QDir(outDir).exists())
	{
		QMessageBox::information(parent, QStringLiteral("提示"),
			QStringLiteral("未设置有效的输出文件夹，无法执行导出。"));
		return false;
	}
	opt.outputDir = outDir;
	opt.overwriteExisting = true; // 目录内同名文件直接覆盖
	return true;
}

void ProductStorageDialog::onExportByRange()
{
	auto* item = ui.mDirectoryTree->currentItem();
	if (!item) return;

	const QList<ProductMetadata> allMetas = collectCurrentNodeProducts();
	if (allMetas.isEmpty())
	{
		QMessageBox::information(this, QStringLiteral("提示"),
			QStringLiteral("所选节点下没有可导出的数据。\n"
							"目录节点的裁剪导出范围为目录树中勾选的矢量/栅格成果；\n"
							"请先在目录树中勾选需要裁剪的成果节点，再执行操作。"));
		return;
	}

	int skipped = 0;
	const QList<ProductMetadata> clippables = spatialClippableOnly(allMetas, skipped);
	if (clippables.isEmpty())
	{
		QMessageBox::warning(this, QStringLiteral("提示"),
			QStringLiteral("所选节点下的成果均不是矢量/栅格类型，无法按范围裁剪导出。"));
		return;
	}
	if (skipped > 0)
	{
		QMessageBox::information(this, QStringLiteral("提示"),
			QStringLiteral("所选节点中有 %1 个非空间类型成果（CAD/AI/PDF/文档等）已跳过，\n"
							"仅对 %2 个矢量/栅格成果执行按范围裁剪导出。")
				.arg(skipped).arg(clippables.size()));
	}

	QgsMapCanvas* canvas = mQGisIface ? mQGisIface->mapCanvas() : nullptr;
	QgsCoordinateReferenceSystem mapCrs = currentMapCrs();

	int pendingMode = -1;    // 手动绘制完成后，重新弹出对话框要恢复的模式
	int pendingFormat = -1;  // 手动绘制完成后，重新弹出对话框要恢复的输出格式
	QString pendingOutput;   // 手动绘制完成后，重新弹出对话框要恢复的输出路径
	bool hasDrawn = false;   // 是否已有绘制结果
	QgsRectangle drawnRect;

	for (;;)
	{
		CSE_RangeExportDialogUi dlg(canvas, mQGisIface, this);
		dlg.resetDrawRequested();
		dlg.setAutoLoadAfterExport(true);
		if (pendingMode >= 0) dlg.setModeIndex(pendingMode);
		if (pendingFormat >= 0) dlg.setFormatIndex(pendingFormat);
		if (!pendingOutput.isEmpty()) dlg.setOutputPath(pendingOutput);
		if (hasDrawn)
		{
			dlg.setExtent(drawnRect);
			if (mapCrs.isValid()) dlg.setExtentCrs(mapCrs);
		}

		if (dlg.exec() != QDialog::Accepted) return;

		// 用户点击"开始绘制"：关闭对话框后到地图上交互绘制，再重新弹出
		if (dlg.drawRequested())
		{
			if (!canvas)
			{
				QMessageBox::warning(this, QStringLiteral("提示"),
					QStringLiteral("当前未获取到 QGIS 地图画布，无法使用“手动绘制”模式。\n"
									"请使用“输入坐标”或“选择要素”方式确定范围。"));
				return;
			}
			mapCrs = currentMapCrs();
			QgsRectangle r;

			// 手动绘制仅支持矩形；绘制前自动最小化"成果存储与管理"对话框，
			// 让用户在主界面的地图窗口拖动绘制矩形范围
			const bool wasVisible = isVisible();
			showMinimized();

			const bool drawOk = MapExtentDrawTool::drawExtentOnCanvas(canvas,
				MapExtentDrawTool::DrawRect, r);

			// 绘制完毕后恢复并激活本窗口，继续"按范围导出"流程（重新弹出对话框回填范围）
			if (wasVisible)
			{
				showNormal();
				raise();
				activateWindow();
			}
			if (!drawOk)
			{
				QMessageBox::information(this, QStringLiteral("提示"),
					QStringLiteral("已取消范围绘制（或绘制的范围无效），未执行导出。"));
				return;
			}
			drawnRect = r;
			hasDrawn = true;
			pendingMode = static_cast<int>(CSE_RangeExportDialogUi::RangeManualDraw);
			pendingFormat = dlg.formatIndex();
			pendingOutput = dlg.outputPath();
			continue;
		}

		if (!dlg.hasExtent())
		{
			QMessageBox::information(this, QStringLiteral("提示"),
				QStringLiteral("尚未确定导出范围，无法执行导出。"));
			return;
		}

		// 确定范围及其坐标系
		const QgsRectangle rect = dlg.exportExtent();
		const int mode = dlg.modeIndex();
		QgsCoordinateReferenceSystem extentCrs;
		if (mode == static_cast<int>(CSE_RangeExportDialogUi::RangeSelectFeature))
			extentCrs = dlg.exportExtentCrs(); // 取要素所在图层坐标系
		else
			extentCrs = mapCrs; // 输入坐标/手动绘制默认按地图坐标系

		MapProductClipEngine::ClipOptions opt;
		// 裁剪几何：
		//   - 选择要素方式且选中要素为面/多面：直接使用要素多边形对每个源图层做精确裁剪；
		//   - 其余情况（线/点要素、输入坐标、手动绘制）：按外包矩形（最小外接矩形）裁剪
		QgsGeometry clipGeom = QgsGeometry::fromWkt(rect.asWktPolygon());
		if (mode == static_cast<int>(CSE_RangeExportDialogUi::RangeSelectFeature)
			&& dlg.hasFeatureGeometry())
		{
			const QgsGeometry feat = dlg.exportFeatureGeometry();
			if (!feat.isNull() && !feat.isEmpty()
				&& QgsWkbTypes::geometryType(feat.wkbType()) == QgsWkbTypes::PolygonGeometry)
			{
				clipGeom = feat;
			}
		}
		opt.clipGeom = clipGeom;
		opt.clipCrs = extentCrs;
		opt.fileEncoding = QStringLiteral("UTF-8");
		opt.autoLoadToMap = dlg.autoLoadAfterExport();
		opt.qgisIface = mQGisIface;

		// 输出格式与路径（与"按主区裁切导出"一致）：
		//   Shapefile       —— 输出到文件夹，每个成果按产品名导出为独立 shp；
		//   GeoPackage/GDB  —— 所有矢量成果写入同一个目标文件（每个成果一个图层），
		//                      栅格无法写入容器，落到该文件所在目录按产品名输出 .tif
		if (!resolveClipOutputTarget(this, dlg.format(), dlg.outputPath().trimmed(), opt))
			return;

		runClipExportAndReport(clippables, opt, QStringLiteral("按范围导出"));
		return;
	}
}

void ProductStorageDialog::onExportByMainAreaClip()
{
	auto* item = ui.mDirectoryTree->currentItem();
	if (!item) return;

	const QList<ProductMetadata> allMetas = collectCurrentNodeProducts();
	if (allMetas.isEmpty())
	{
		QMessageBox::information(this, QStringLiteral("提示"),
			QStringLiteral("所选节点下没有可导出的数据。\n"
							"目录节点的裁剪导出范围为目录树中勾选的矢量/栅格成果；\n"
							"请先在目录树中勾选需要裁剪的成果节点，再执行操作。"));
		return;
	}

	int skipped = 0;
	const QList<ProductMetadata> clippables = spatialClippableOnly(allMetas, skipped);
	if (clippables.isEmpty())
	{
		QMessageBox::warning(this, QStringLiteral("提示"),
			QStringLiteral("所选节点下的成果均不是矢量/栅格类型，无法按主区裁切导出。"));
		return;
	}
	if (skipped > 0)
	{
		QMessageBox::information(this, QStringLiteral("提示"),
			QStringLiteral("所选节点中有 %1 个非空间类型成果（CAD/AI/PDF/文档等）已跳过，\n"
							"仅对 %2 个矢量/栅格成果执行按主区裁切导出。")
				.arg(skipped).arg(clippables.size()));
	}

	CSE_MainAreaClipDialogUi dlg(this);
	dlg.setAutoLoadAfterExport(true);
	if (dlg.exec() != QDialog::Accepted) return;

	const QString mainAreaPath = dlg.mainAreaPath().trimmed();
	if (mainAreaPath.isEmpty() || !QFileInfo::exists(mainAreaPath))
	{
		QMessageBox::warning(this, QStringLiteral("提示"),
			QStringLiteral("未选择有效的主区数据文件。"));
		return;
	}

	// 读取主区所选要素的合并外包矩形（与参考算法一致），并取主区坐标系
	QgsCoordinateReferenceSystem mainCrs;
	const QgsRectangle clipRect =
		computeMainAreaClipRect(mainAreaPath, dlg.selectedFeatureIds(), &mainCrs);
	if (clipRect.isNull() || clipRect.isEmpty())
	{
		QMessageBox::warning(this, QStringLiteral("提示"),
			QStringLiteral("未能从主区数据中读取所选要素，请先在要素列表中勾选主区要素。"));
		return;
	}

	MapProductClipEngine::ClipOptions opt;
	opt.clipGeom = QgsGeometry::fromWkt(clipRect.asWktPolygon());
	opt.clipCrs = mainCrs;
	opt.fileEncoding = dlg.encoding();
	opt.overwriteExisting = dlg.overwriteExisting();
	opt.autoLoadToMap = dlg.autoLoadAfterExport();
	opt.qgisIface = mQGisIface;

	// 输出格式与路径：
	//   GeoPackage / GDB —— 所有矢量成果写入同一个目标文件（每个成果一个图层）
	//   Shapefile        —— 输出到文件夹，每个成果导出为独立 shp
	if (!resolveClipOutputTarget(this, dlg.format(), dlg.outputFilePath().trimmed(), opt))
		return;

	runClipExportAndReport(clippables, opt, QStringLiteral("按主区裁切导出"));
}

void ProductStorageDialog::clearForm()
{
	// 如果 detailGroup 已被 initDetailTabs 重构，旧表单控件已销毁，跳过
	if (!mDetailTabWidget)
	{
		ui.mProductNameEdit->clear();
		ui.mFileFormatEdit->clear();
		ui.mFileSizeLabel->clear();
		ui.mFileHashLabel->clear();
		ui.mMinXEdit->clear();
		ui.mMinYEdit->clear();
		ui.mMaxXEdit->clear();
		ui.mMaxYEdit->clear();
		ui.mCRSEdit->clear();
		ui.mGeometryTypeEdit->clear();
		ui.mBandCountEdit->clear();
		ui.mResolutionEdit->clear();
		ui.mVersionEdit->setText("1");
		ui.mVersionNoteEdit->clear();
	}
	mCurrentMeta = ProductMetadata();
	ui.mDeleteBtn->setEnabled(false);
	ui.mAddToMapBtn->setEnabled(false);

	// 清空 Tab 详情页
	clearBasicMetaTab();
	clearSpecializedMetaTab();

	// 重置 GDB 多图层状态
	mIsGdbMultiLayer = false;
	mSelectedGdbLayers.clear();
	mSelectedGdbPath.clear();

	// 隐藏进度信息
	ui.mProgressBar->setVisible(false);
	ui.mProgressLabel->setVisible(false);
}

void ProductStorageDialog::fillForm(const ProductMetadata& meta)
{
	mCurrentMeta = meta;

	// 如果 detailGroup 已被 initDetailTabs 重构为 Tab 页，旧表单控件已销毁，跳过
	if (!mDetailTabWidget)
	{
		// 填充旧的简单表单（保持兼容，部分字段展示在左侧）
		ui.mProductNameEdit->setText(meta.productName);
		ui.mProductTypeCombo->setCurrentText(productTypeToString(meta.productType));
		ui.mFileFormatEdit->setText(meta.fileFormat);
		ui.mFileSizeLabel->setText(formatFileSize(meta.fileSize));
		ui.mFileHashLabel->setText(meta.fileHash);
		ui.mMinXEdit->setText(QString::number(meta.minX, 'f', 6));
		ui.mMinYEdit->setText(QString::number(meta.minY, 'f', 6));
		ui.mMaxXEdit->setText(QString::number(meta.maxX, 'f', 6));
		ui.mMaxYEdit->setText(QString::number(meta.maxY, 'f', 6));
		ui.mCRSEdit->setText(meta.crs);
		ui.mGeometryTypeEdit->setText(meta.geometryType);
		ui.mBandCountEdit->setText(QString::number(meta.bandCount));
		ui.mResolutionEdit->setText(QString::number(meta.pixelResolution, 'f', 6));
		ui.mVersionEdit->setText(QString::number(meta.currentVersion));
		ui.mVersionNoteEdit->setText(meta.versionNote);

		updateRasterFieldsVisibility();
	}

	// 更新详情 Tab 控件
	fillBasicMetaTab(meta);
	fillSpecializedMetaTab(meta);
}

void ProductStorageDialog::updateRasterFieldsVisibility()
{
	bool isRaster = (ui.mProductTypeCombo->currentText() == "栅格数据");
	ui.bandCountLabel->setVisible(isRaster);
	ui.mBandCountEdit->setVisible(isRaster);
	ui.resolutionLabel->setVisible(isRaster);
	ui.mResolutionEdit->setVisible(isRaster);
}

// ===================== QTabWidget 详情页 =====================

void ProductStorageDialog::initDetailTabs()
{
	QGroupBox* detailGroup = ui.detailGroup;
	if (!detailGroup) return;

	QLayout* oldLayout = detailGroup->layout();
	if (oldLayout)
	{
		QLayoutItem* child;
		while ((child = oldLayout->takeAt(0)) != nullptr)
		{
			if (child->widget())
				child->widget()->deleteLater();
			delete child;
		}
		delete oldLayout;
	}

	QVBoxLayout* vbox = new QVBoxLayout(detailGroup);
	mDetailTabWidget = new QTabWidget(detailGroup);
	mDetailTabWidget->setObjectName("mDetailTabWidget");
	vbox->addWidget(mDetailTabWidget);

	// ─── Tab 1: 基本元数据 ───
	QScrollArea* basicScroll = new QScrollArea();
	basicScroll->setWidgetResizable(true);
	basicScroll->setFrameShape(QFrame::NoFrame);

	QWidget* basicPage = new QWidget();
	basicPage->setObjectName("basicPage");
	QVBoxLayout* basicPageLayout = new QVBoxLayout(basicPage);
	basicPageLayout->setContentsMargins(0, 0, 0, 0);
	basicPageLayout->setSpacing(0);

	// 辅助：添加分组标题
	auto addSectionTitle = [&](const QString& title) {
		QLabel* titleLabel = new QLabel(title);
		QFont font = titleLabel->font();
		font.setBold(true);
		titleLabel->setFont(font);
		basicPageLayout->addWidget(titleLabel);
		QFrame* line = new QFrame();
		line->setFrameShape(QFrame::HLine);
		basicPageLayout->addWidget(line);
	};

	// 辅助：添加表单行（带交替背景色）
	int rowIndex = 0;
	auto addField = [&](const QString& labelText, QLabel*& outLabel) {
		QHBoxLayout* row = new QHBoxLayout();
		row->setContentsMargins(20, 3, 12, 3);

		QLabel* lb = new QLabel(labelText);
		lb->setMinimumWidth(110);
		QFont labelFont = lb->font();
		labelFont.setBold(true);
		lb->setFont(labelFont);
		lb->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		row->addWidget(lb);

		QLabel* vl = new QLabel();
		vl->setTextInteractionFlags(Qt::TextSelectableByMouse);
		vl->setWordWrap(true);
		vl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		row->addWidget(vl, 1);
		outLabel = vl;

		QWidget* wrapper = new QWidget();
		wrapper->setLayout(row);
		basicPageLayout->addWidget(wrapper);
		rowIndex++;
	};

	// 27 项基本元数据，分组展示
	addSectionTitle(QString::fromUtf8("基本信息"));
	addField(QString::fromUtf8("数据名称："), mLabelProductName);
	addField(QString::fromUtf8("数据唯一标识："), mLabelDataId);
	addField(QString::fromUtf8("数据描述："), mLabelDescription);
	addField(QString::fromUtf8("数据来源："), mLabelSource);
	addField(QString::fromUtf8("版本说明："), mLabelVersionNote);
	addField(QString::fromUtf8("标签："), mLabelTags);

	addSectionTitle(QString::fromUtf8("格式与存储"));
	addField(QString::fromUtf8("数据格式："), mLabelFileFormat);
	addField(QString::fromUtf8("数据量："), mLabelFileSize);
	addField(QString::fromUtf8("是否压缩："), mLabelIsCompressed);

	addSectionTitle(QString::fromUtf8("空间信息"));
	addField(QString::fromUtf8("空间范围："), mLabelBounds);
	addField(QString::fromUtf8("中心点经度："), mLabelCenterLon);
	addField(QString::fromUtf8("中心点纬度："), mLabelCenterLat);
	addField(QString::fromUtf8("坐标参考系："), mLabelCrs);
	addField(QString::fromUtf8("比例尺分母："), mLabelScale);

	addSectionTitle(QString::fromUtf8("时间与周期"));
	addField(QString::fromUtf8("数据现势性起始："), mLabelStartDatetime);
	addField(QString::fromUtf8("数据现势性截止："), mLabelEndDatetime);
	addField(QString::fromUtf8("创建时间："), mLabelCreatedAt);
	addField(QString::fromUtf8("更新时间："), mLabelUpdatedAt);

	addSectionTitle(QString::fromUtf8("管理与来源"));
	addField(QString::fromUtf8("密级："), mLabelSecurityLevel);
	addField(QString::fromUtf8("创建人员："), mLabelCreatedBy);
	addField(QString::fromUtf8("所在市州："), mLabelCity);
	addField(QString::fromUtf8("项目名称："), mLabelProjectName);
	addField(QString::fromUtf8("生产单位："), mLabelProducer);
	addField(QString::fromUtf8("生产日期："), mLabelProductionDate);
	addField(QString::fromUtf8("编制信息："), mLabelCompilationInfo);
	addField(QString::fromUtf8("汇交情况："), mLabelDeliveryStatus);
	addField(QString::fromUtf8("汇交时间："), mLabelDeliveryTime);

	basicPageLayout->addStretch();
	basicScroll->setWidget(basicPage);
	mDetailTabWidget->addTab(basicScroll, QString::fromUtf8("基本元数据"));

	// ─── Tab 2: 专有元数据（按类型动态填充，样式与基本元数据一致） ───
	mSpecializedScroll = new QScrollArea();
	mSpecializedScroll->setWidgetResizable(true);
	mSpecializedScroll->setFrameShape(QFrame::NoFrame);

	mSpecializedMetaPage = new QWidget();
	mSpecializedMetaPage->setObjectName("specializedPage");
	mSpecializedLayout = new QVBoxLayout(mSpecializedMetaPage);
	mSpecializedLayout->setContentsMargins(0, 0, 0, 0);
	mSpecializedLayout->setSpacing(0);

	mSpecializedScroll->setWidget(mSpecializedMetaPage);
	mDetailTabWidget->addTab(mSpecializedScroll, QString::fromUtf8("专有元数据"));
}

void ProductStorageDialog::fillBasicMetaTab(const ProductMetadata& meta)
{
	if (!mLabelDataId) return;

	mLabelDataId->setText(meta.dataId);
	mLabelProductName->setText(meta.productName);
	mLabelDescription->setText(meta.description);
	mLabelSource->setText(meta.source);
	mLabelVersionNote->setText(meta.versionNote);
	mLabelFileFormat->setText(meta.fileFormat);
	mLabelFileSize->setText(formatFileSize(meta.fileSize));
	mLabelIsCompressed->setText(meta.isCompressed);

	// 空间范围
	if (meta.minX != 0 || meta.maxX != 0)
	{
		mLabelBounds->setText(QString("(%1, %2) - (%3, %4)")
			.arg(meta.minX, 0, 'f', 6)
			.arg(meta.minY, 0, 'f', 6)
			.arg(meta.maxX, 0, 'f', 6)
			.arg(meta.maxY, 0, 'f', 6));
	}
	else
	{
		mLabelBounds->setText(QString());
	}

	mLabelCenterLon->setText(meta.centerLon);
	mLabelCenterLat->setText(meta.centerLat);

	mLabelStartDatetime->setText(meta.startDatetime.isValid()
		? meta.startDatetime.toString("yyyy-MM-dd hh:mm")
		: QString());
	mLabelEndDatetime->setText(meta.endDatetime.isValid()
		? meta.endDatetime.toString("yyyy-MM-dd hh:mm")
		: QString());

	mLabelSecurityLevel->setText(securityLevelToString(meta.securityLevel));
	mLabelCreatedBy->setText(meta.createdBy);
	mLabelCreatedAt->setText(meta.createdAt.toString("yyyy-MM-dd hh:mm"));
	mLabelUpdatedAt->setText(meta.updatedAt.toString("yyyy-MM-dd hh:mm"));
	mLabelTags->setText(meta.tags);
	mLabelCity->setText(meta.city);
	mLabelScale->setText(meta.scale);
	mLabelProjectName->setText(meta.projectName);
	mLabelProducer->setText(meta.producer);
	mLabelProductionDate->setText(meta.productionDate.isValid()
		? meta.productionDate.toString("yyyy-MM-dd")
		: QString());
	mLabelCrs->setText(meta.crs);
	mLabelCompilationInfo->setText(meta.compilationInfo);
	mLabelDeliveryStatus->setText(meta.deliveryStatus);
	mLabelDeliveryTime->setText(meta.deliveryTime.isValid()
		? meta.deliveryTime.toString("yyyy-MM-dd hh:mm")
		: QString());
}

void ProductStorageDialog::fillSpecializedMetaTab(const ProductMetadata& meta)
{
	if (!mSpecializedLayout) return;

	clearSpecializedMetaTab();

	int productId = meta.id;
	if (productId <= 0) return;

	auto addSectionTitle = [&](const QString& title) {
		QLabel* titleLabel = new QLabel(title);
		QFont font = titleLabel->font();
		font.setBold(true);
		titleLabel->setFont(font);
		mSpecializedLayout->addWidget(titleLabel);
		QFrame* line = new QFrame();
		line->setFrameShape(QFrame::HLine);
		mSpecializedLayout->addWidget(line);
	};

	int rowIndex = 0;
	auto addField = [&](const QString& labelText, const QString& value) {
		QHBoxLayout* row = new QHBoxLayout();
		row->setContentsMargins(20, 3, 12, 3);

		QLabel* lb = new QLabel(labelText);
		lb->setMinimumWidth(110);
		QFont labelFont = lb->font();
		labelFont.setBold(true);
		lb->setFont(labelFont);
		lb->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		row->addWidget(lb);

		QLabel* vl = new QLabel(value.isEmpty() ? QString::fromUtf8("—") : value);
		vl->setTextInteractionFlags(Qt::TextSelectableByMouse);
		vl->setWordWrap(true);
		vl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		row->addWidget(vl, 1);

		QWidget* wrapper = new QWidget();
		wrapper->setLayout(row);
		mSpecializedLayout->addWidget(wrapper);
		rowIndex++;
	};

	switch (meta.productType)
	{
	case ProductType::Vector:
	{
		ProductVectorMeta vMeta = mDAO->getVectorMeta(productId);
		addSectionTitle(QString::fromUtf8("矢量数据元数据"));
		addField(QString::fromUtf8("几何类型："), vMeta.geomType);
		addField(QString::fromUtf8("比例尺分母："), vMeta.invScale > 0 ? QString::number(vMeta.invScale) : QString());
		addField(QString::fromUtf8("坐标系类型："), vMeta.csType);
		addField(QString::fromUtf8("大地基准："), vMeta.geodeticDatum);
		addField(QString::fromUtf8("地图投影代码："), vMeta.epsgCode);
		addField(QString::fromUtf8("地图投影描述："), vMeta.projDesc);
		addField(QString::fromUtf8("属性字段说明："), vMeta.fieldDesc);
		addSectionTitle(QString::fromUtf8("系统自动提取"));
		addField(QString::fromUtf8("要素数量："), QString::number(vMeta.featureCount));
		addField(QString::fromUtf8("字段数量："), QString::number(vMeta.fieldCount));
		addField(QString::fromUtf8("入库表名："), vMeta.layerTableName);
		break;
	}
	case ProductType::Raster:
	{
		ProductRasterMeta rMeta = mDAO->getRasterMeta(productId);
		addSectionTitle(QString::fromUtf8("栅格数据元数据"));
		addField(QString::fromUtf8("卫星名称："), rMeta.satelliteName);
		addField(QString::fromUtf8("传感器类型："), rMeta.sensorType);
		addField(QString::fromUtf8("成像时间："), rMeta.acquireTime.isValid()
			? rMeta.acquireTime.toString("yyyy-MM-dd hh:mm") : QString());
		addField(QString::fromUtf8("影像地面分辨率："), rMeta.gsd > 0 ? QString::number(rMeta.gsd, 'f', 6) : QString());
		addField(QString::fromUtf8("分辨率单位："), rMeta.resolutionUnit);
		addField(QString::fromUtf8("影像色彩类型："), rMeta.colorType);
		addField(QString::fromUtf8("颜色级数："), rMeta.bitDepth > 0 ? QString::number(rMeta.bitDepth) : QString());
		addField(QString::fromUtf8("波段数："), QString::number(rMeta.bandCount));
		addField(QString::fromUtf8("无数据值："), QString::number(rMeta.nodataValue));
		addField(QString::fromUtf8("坐标系类型："), rMeta.csType);
		addField(QString::fromUtf8("大地基准："), rMeta.geodeticDatum);
		addField(QString::fromUtf8("地图投影代码："), rMeta.epsgCode);
		addField(QString::fromUtf8("地图投影描述："), rMeta.projDesc);
		addField(QString::fromUtf8("平面单位："), rMeta.planarUnit);
		addSectionTitle(QString::fromUtf8("系统自动提取"));
		addField(QString::fromUtf8("像素宽度："), QString::number(rMeta.pixelWidth));
		addField(QString::fromUtf8("像素高度："), QString::number(rMeta.pixelHeight));
		addField(QString::fromUtf8("像元类型："), rMeta.pixelType);
		addField(QString::fromUtf8("入库表名："), rMeta.layerTableName);
		break;
	}
	case ProductType::CAD:
	case ProductType::AI:
	case ProductType::CDR:
	case ProductType::PDF:
	{
		ProductDiagramMeta dMeta = mDAO->getDiagramMeta(productId);
		addSectionTitle(QString::fromUtf8("制图成果元数据"));
		addField(QString::fromUtf8("图名："), dMeta.dataId);
		addField(QString::fromUtf8("图集系列名："), dMeta.mapSeries);
		addField(QString::fromUtf8("制图区域："), dMeta.cityPrefecture);
		addField(QString::fromUtf8("比例尺分母："), dMeta.mapScale > 0 ? QString::number(dMeta.mapScale) : QString());
		addField(QString::fromUtf8("地图投影："), dMeta.projDesc);
		addField(QString::fromUtf8("制图完成日期："), dMeta.productionDate.isValid()
			? dMeta.productionDate.toString("yyyy-MM-dd") : QString());
		addField(QString::fromUtf8("审图号："), dMeta.approvalNo);
		addField(QString::fromUtf8("制图软件及版本："), dMeta.cartoSoftware);
		addField(QString::fromUtf8("数据格式："), dMeta.format);
		addField(QString::fromUtf8("图件尺寸："), dMeta.paperSize);
		addField(QString::fromUtf8("是否含图例："), dMeta.legendIncluded ? QString::fromUtf8("是") : QString::fromUtf8("否"));
		addField(QString::fromUtf8("修改人："), dMeta.modifier);
		addField(QString::fromUtf8("最后修改时间："), dMeta.lastModified.isValid()
			? dMeta.lastModified.toString("yyyy-MM-dd") : QString());
		addField(QString::fromUtf8("是否印刷级："), dMeta.printReady ? QString::fromUtf8("是") : QString::fromUtf8("否"));
		addField(QString::fromUtf8("色彩模式："), dMeta.colorMode);
		addField(QString::fromUtf8("输出分辨率："), dMeta.dpi > 0 ? QString::number(dMeta.dpi) : QString());
		addField(QString::fromUtf8("项目名称："), dMeta.projName);
		addField(QString::fromUtf8("关联栅格资产ID："), dMeta.rasterIds);
		addField(QString::fromUtf8("关联矢量资产ID："), dMeta.vectorIds);
		addField(QString::fromUtf8("数据现势性截止："), dMeta.endDatetime.isValid()
			? dMeta.endDatetime.toString("yyyy-MM-dd hh:mm") : QString());
		addField(QString::fromUtf8("制图人："), dMeta.mapProductor);
		addField(QString::fromUtf8("是否有数学基础："), dMeta.hasMathBase ? QString::fromUtf8("是") : QString::fromUtf8("否"));
		break;
	}
	default:
	{
		ProductDocumentMeta docMeta = mDAO->getDocumentMeta(productId);
		if (docMeta.productId > 0)
		{
			addSectionTitle(QString::fromUtf8("文档数据元数据"));
			addField(QString::fromUtf8("来源："), docMeta.publisher);
			addField(QString::fromUtf8("文件类型："), docMeta.fileType);
			addField(QString::fromUtf8("文件格式："), docMeta.format);
			addField(QString::fromUtf8("文件大小："), docMeta.fileSize);
			addField(QString::fromUtf8("语言类型："), docMeta.languageType);
			addField(QString::fromUtf8("关键字："), docMeta.keyWords);
			addField(QString::fromUtf8("内容摘要："), docMeta.summary);
			addField(QString::fromUtf8("质量问题记录："), docMeta.qualityIssues);
			addField(QString::fromUtf8("收集时间："), docMeta.collectTime.isValid()
				? docMeta.collectTime.toString("yyyy-MM-dd hh:mm") : QString());
			addField(QString::fromUtf8("数据现势性截止："), docMeta.endDatetime.isValid()
				? docMeta.endDatetime.toString("yyyy-MM-dd hh:mm") : QString());
			addField(QString::fromUtf8("收集人："), docMeta.collector);
			addField(QString::fromUtf8("收集目的："), docMeta.collectPurpose);
			addField(QString::fromUtf8("项目名称："), docMeta.projectName);
			addField(QString::fromUtf8("是否压缩："), docMeta.isCompressed ? QString::fromUtf8("是") : QString::fromUtf8("否"));
		}
		else
		{
			QLabel* noData = new QLabel(QString::fromUtf8("该类型暂无专有元数据"));
			noData->setAlignment(Qt::AlignCenter);
			mSpecializedLayout->addWidget(noData);
		}
		break;
	}
	}

	mSpecializedLayout->addStretch();
}

void ProductStorageDialog::clearBasicMetaTab()
{
	if (mLabelDataId) mLabelDataId->clear();
	if (mLabelProductName) mLabelProductName->clear();
	if (mLabelDescription) mLabelDescription->clear();
	if (mLabelSource) mLabelSource->clear();
	if (mLabelVersionNote) mLabelVersionNote->clear();
	if (mLabelFileFormat) mLabelFileFormat->clear();
	if (mLabelFileSize) mLabelFileSize->clear();
	if (mLabelIsCompressed) mLabelIsCompressed->clear();
	if (mLabelBounds) mLabelBounds->clear();
	if (mLabelCenterLon) mLabelCenterLon->clear();
	if (mLabelCenterLat) mLabelCenterLat->clear();
	if (mLabelStartDatetime) mLabelStartDatetime->clear();
	if (mLabelEndDatetime) mLabelEndDatetime->clear();
	if (mLabelSecurityLevel) mLabelSecurityLevel->clear();
	if (mLabelCreatedBy) mLabelCreatedBy->clear();
	if (mLabelCreatedAt) mLabelCreatedAt->clear();
	if (mLabelUpdatedAt) mLabelUpdatedAt->clear();
	if (mLabelTags) mLabelTags->clear();
	if (mLabelCity) mLabelCity->clear();
	if (mLabelScale) mLabelScale->clear();
	if (mLabelProjectName) mLabelProjectName->clear();
	if (mLabelProducer) mLabelProducer->clear();
	if (mLabelProductionDate) mLabelProductionDate->clear();
	if (mLabelCrs) mLabelCrs->clear();
	if (mLabelCompilationInfo) mLabelCompilationInfo->clear();
	if (mLabelDeliveryStatus) mLabelDeliveryStatus->clear();
	if (mLabelDeliveryTime) mLabelDeliveryTime->clear();
}

void ProductStorageDialog::clearSpecializedMetaTab()
{
	if (!mSpecializedLayout) return;
	// 移除所有动态添加的子控件和布局项
	QLayoutItem* child;
	while ((child = mSpecializedLayout->takeAt(0)) != nullptr)
	{
		if (child->widget())
			child->widget()->deleteLater();
		delete child;
	}
}

// 添加到地图

void ProductStorageDialog::onAddToMap()
{
	int row = ui.mProductTable->currentRow();
	if (row < 0) return;

	// 从 item data 获取 productId 并精确查找
	QTableWidgetItem* item = ui.mProductTable->item(row, 0);
	if (!item) return;
	int productId = item->data(Qt::UserRole).toInt();
	if (productId <= 0) return;

	const ProductMetadata* metaPtr = nullptr;
	for (const auto& p : mProducts)
	{
		if (p.id == productId) { metaPtr = &p; break; }
	}
	if (!metaPtr) return;
	const ProductMetadata& meta = *metaPtr;

	// 提示逻辑与元数据管理、成果检索对话框统一，均由共享辅助函数处理
	AddToMapHelper::addToMapWithFeedback(this, mQGisIface, meta);
}

// 从 PostGIS 加载图层到 QGIS 地图画布

bool ProductStorageDialog::loadLayerToMap(const ProductMetadata& meta)
{
	return AddToMapHelper::addLayerToMap(mQGisIface, meta);
}

// 大小写不敏感查找 PostgreSQL 中实际的表名（委托给共享辅助函数）
QString ProductStorageDialog::resolveTableName(const QString& schema, const QString& candidateName)
{
	return AddToMapHelper::resolveTableName(schema, candidateName);
}

// GDB 多图层入库处理

void ProductStorageDialog::processGdbLayers(const QString& gdbPath, const QStringList& layerNames,
											 const QString& productType, const QString& versionNote)
{
	int total = layerNames.size();
	int success = 0;
	int failed = 0;

	QFileInfo gdbFi(gdbPath);
	QString gdbBaseName = gdbFi.baseName();


	ui.mProgressBar->setVisible(true);
	ui.mProgressBar->setRange(0, total);
	ui.mProgressBar->setValue(0);
	ui.mProgressLabel->setVisible(true);

	for (int i = 0; i < total; ++i)
	{
		const QString& layerName = layerNames[i];


		// 更新进度
		ui.mProgressBar->setValue(i);
		updateProgressText(
			QString("正在入库图层 [%1/%2]: %3...").arg(i + 1).arg(total).arg(layerName));

		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		// ===== 为每个图层创建独立的元数据 =====
		ProductMetadata meta;
		meta.productName = gdbBaseName + "_" + layerName;
		meta.productType = stringToProductType(productType);
		meta.fileFormat = gdbFi.suffix().toLower();
		meta.versionNote = versionNote;
		meta.createdBy = "YOUR_PASSWORD";
		meta.updatedBy = "YOUR_PASSWORD";
		meta.currentVersion = 1;
		meta.dataId = QUuid::createUuid().toString(QUuid::WithoutBraces);
		meta.filePath = gdbPath;
		meta.fileSize = gdbFi.size();

		// 提取该图层的空间元数据（CRS、几何类型、空间范围）
		{
			ProductMetadata spatialMeta = mExtractor->extractLayerMetadata(gdbPath, layerName);
			meta.crs = spatialMeta.crs;
			meta.geometryType = spatialMeta.geometryType;
			meta.minX = spatialMeta.minX;
			meta.minY = spatialMeta.minY;
			meta.maxX = spatialMeta.maxX;
			meta.maxY = spatialMeta.maxY;
			meta.spatialExtentWKT = spatialMeta.spatialExtentWKT;
		}


		// 检查同名产品是否存在
		ProductMetadata existingMeta;
		bool isUpdate = false;
		for (const auto& p : mProducts)
		{
			if (p.productName == meta.productName)
			{
				existingMeta = p;
				isUpdate = true;
				break;
			}
		}
		if (!isUpdate)
		{
			ProductDAO::SearchCondition nameCond;
			nameCond.keyword = meta.productName;
			nameCond.limit = 100;
			auto existingProducts = mDAO->searchProducts(nameCond);
			for (const auto& p : existingProducts)
			{
				if (p.productName == meta.productName)
				{
					existingMeta = p;
					isUpdate = true;
					break;
				}
			}
		}

		if (isUpdate)
		{
			int newVersion = existingMeta.currentVersion + 1;
			meta.currentVersion = newVersion;
			meta.id = existingMeta.id;
			meta.dataId = existingMeta.dataId;

		}
		else
		{
		}

		// 分配目录（若指定了导入目标目录则分配到目标目录下）
		if (mImportTargetDirId > 0)
		{
			meta.parentDirId = mImportTargetDirId;
			meta.directoryPath = mDAO->getDirectory(mImportTargetDirId).name;
		}
		else
		{
			QString typeDirName = productTypeToString(meta.productType);
			int dirId = mDAO->findOrCreateDirectory(typeDirName, 0);
			if (dirId > 0)
			{
				meta.parentDirId = dirId;
				meta.directoryPath = typeDirName;
			}
		}

		// 文件数据由 BLOB 存储，本地只保留原始路径引用
		meta.filePath = gdbPath;

		// 缩略图
		QString thumbnailPath = QDir::tempPath() + "/pmt_thumbnails/" + meta.dataId + ".png";
		QDir().mkpath(QFileInfo(thumbnailPath).absolutePath());
		bool thumbOk = MetadataExtractor::generateThumbnail(gdbPath, thumbnailPath);
		if (thumbOk)
		{
			meta.thumbnailPath = thumbnailPath;
		}
		else
		{
		}

		// 图层表名：文件名_图层名
		QString safeBaseName = gdbBaseName;
		safeBaseName.replace(QRegularExpression("[^a-zA-Z0-9_\\x{4e00}-\\x{9fff}]"), "_");
		if (!safeBaseName.isEmpty() && !safeBaseName.at(0).isLetter())
			safeBaseName = "f_" + safeBaseName;
		if (safeBaseName.isEmpty())
			safeBaseName = "dataset";

		QString safeLayerName = layerName;
		safeLayerName.replace(QRegularExpression("[^a-zA-Z0-9_\\x{4e00}-\\x{9fff}]"), "_");
		if (!safeLayerName.isEmpty() && !safeLayerName.at(0).isLetter())
			safeLayerName = "l_" + safeLayerName;
		if (safeLayerName.isEmpty())
			safeLayerName = "layer";

		// PostGIS 目标表名：gdb/mdb文件名_图层名_v版本号
		QString layerTableName = safeBaseName + "_" + safeLayerName + "_v" + QString::number(meta.currentVersion);

		QgsMessageLog::logMessage(
			QStringLiteral("[processGdbLayers] gdbBaseName=%1 safeBaseName=%2 safeLayerName=%3 → layerTableName=%4")
				.arg(gdbBaseName, safeBaseName, safeLayerName, layerTableName),
			"MapProductTools", Qgis::Info);

		meta.layerTableName = layerTableName;


		// 数据入库：仅导入指定图层

		updateProgressText(
			QString("正在将图层 [%1/%2] %3 导入 PostGIS...").arg(i + 1).arg(total).arg(layerName));

		int featureCount = mImporter->importVectorLayerToPostGIS(
			gdbPath, layerName, layerTableName, 0, 4490);


		if (featureCount < 0)
		{
			failed++;
			QString errMsg = mImporter->lastError();
			updateProgressText(
				QString("图层 %1 导入失败: %2").arg(layerName, errMsg));

			// 清理失败时创建的表
			SchemaManager schemaMgr;
			schemaMgr.dropDataLayerTable(layerTableName);
			continue;
		}

		if (featureCount == 0)
		{
			failed++;
			SchemaManager schemaMgr;
			schemaMgr.dropDataLayerTable(layerTableName);
			updateProgressText(
				QString("图层 %1 无有效要素，已跳过。").arg(layerName));
			continue;
		}

		// 写入元数据

		int productId;
		if (isUpdate)
		{
			if (!mDAO->updateProduct(meta))
			{
				SchemaManager schemaMgr;
				schemaMgr.dropDataLayerTable(layerTableName);
				failed++;
				continue;
			}
			productId = meta.id;
		}
		else
		{
			productId = mDAO->insertProduct(meta);
			if (productId <= 0)
			{
				SchemaManager schemaMgr;
				schemaMgr.dropDataLayerTable(layerTableName);
				failed++;
				continue;
			}
		}

		// 注册图层
		mDAO->registerLayer(productId, layerTableName, meta.geometryType, 4490, featureCount);
		mDAO->enrichWithTypeMeta(productId);

		// 创建版本记录
		if (!isUpdate)
		{
			VersionRecord verRec;
			verRec.productId = productId;
			verRec.versionNumber = 1;
			verRec.filePath = gdbPath;
			verRec.fileHash = meta.fileHash;
			verRec.fileSize = meta.fileSize;
			verRec.fileFormat = meta.fileFormat;
			verRec.layerTableName = layerTableName;
			verRec.changeNote = versionNote.isEmpty()
				? QString("从 %1 导入图层 %2").arg(meta.fileFormat.toUpper(), layerName)
				: versionNote;
			verRec.changedBy = meta.createdBy;
			verRec.parentVersion = meta.currentVersion > 1 ? (meta.currentVersion - 1) : 0;

			mDAO->insertVersionRecord(verRec);
		}

		success++;

		updateProgressText(
			QString("图层 %1 入库成功 (%2 个要素)").arg(layerName).arg(featureCount));

		// 刷新本地缓存
		mProducts = mDAO->getAllProducts();
	}

	// 最终进度
	ui.mProgressBar->setValue(total);
	updateProgressText(
		QString("%1 图层导入完成！成功: %2, 失败: %3").arg(gdbFi.suffix().toUpper()).arg(success).arg(failed));


	mLastImportOk = (success > 0);
	if (!mImportSilent)
		QMessageBox::information(this, QString("%1 导入完成").arg(gdbFi.suffix().toUpper()),
			QString("%1 图层导入完成。\n成功: %2 个图层\n失败: %3 个图层")
				.arg(gdbFi.suffix().toUpper()).arg(success).arg(failed));
}

// 更新进度条文字说明

void ProductStorageDialog::updateProgressText(const QString& text, int value)
{
	ui.mProgressLabel->setText(text);
	ui.mProgressLabel->setVisible(true);

	if (value >= 0)
		ui.mProgressBar->setValue(value);
}
