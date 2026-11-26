#include "product_search_dialog.h"
#include "database/product_dao.h"
#include "database/postgis_connector.h"
#include "core/directory_helper.h"
#include "core/calendar_helper.h"
#include "core/add_to_map_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <qgis.h>
#include <QFileInfo>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QPalette>
#include <gdal.h>
#include <gdal_priv.h>
#include <ogr_api.h>
#include <ogr_srs_api.h>
#include <ogrsf_frmts.h>
#include <cpl_conv.h>
#include <libpq-fe.h>
#include <QgsMessageLog.h>
#include <QMenu>
#include <QSet>
#include <algorithm>

#include "qgsgui.h"
#include <qgsvectorlayer.h>
#include <qgsvectorfilewriter.h>
#include <qgsrasterlayer.h>
#include <qgsdatasourceuri.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <qgscoordinatereferencesystem.h>
#include "qgsmapcanvas.h"
#include "qgsmapcanvasitem.h"
#include "qgsmapsettings.h"
#include "qgsmaptoolpan.h"
#include "qgsproject.h"
#include <QPainter>
#include <cmath>

// ===================== GraticuleItem 实现 =====================

GraticuleItem::GraticuleItem(QgsMapCanvas* canvas)
	: QgsMapCanvasItem(canvas)
{
	// 经纬线样式：浅灰色细线
	mLinePen = QPen(QColor(120, 120, 120, 180), 1.0, Qt::DotLine);

	// 标注样式：深灰色小字
	mLabelPen = QPen(QColor(80, 80, 80));
	mLabelFont = QFont("Arial", 8);
}

void GraticuleItem::paint(QPainter* painter)
{
	if (!mMapCanvas)
		return;

	const QgsMapSettings& settings = mMapCanvas->mapSettings();
	QgsRectangle extent = settings.visibleExtent();
	const QRect& canvasRect = mMapCanvas->rect();

	if (canvasRect.width() <= 0 || canvasRect.height() <= 0)
		return;

	double xMin = extent.xMinimum();
	double xMax = extent.xMaximum();
	double yMin = extent.yMinimum();
	double yMax = extent.yMaximum();

	// 根据画布像素尺寸自适应经纬线条数
	const double targetPixelSpacing = 120.0;
	int numLonLines = qMax(2, static_cast<int>(canvasRect.width() / targetPixelSpacing) - 1);
	int numLatLines = qMax(2, static_cast<int>(canvasRect.height() / targetPixelSpacing));

	double lonStep = (xMax - xMin) / (numLonLines - 1);
	double latStep = (yMax - yMin) / (numLatLines - 1);

	// 辅助函数：格式化角度值，带方向标识
	auto formatLon = [](double val) -> QString {
		double absVal = std::abs(val);
		QString dir = (val >= 0) ? QString::fromUtf8("E") : QString::fromUtf8("W");
		return QString::number(absVal, 'f', 4) + QString::fromUtf8("°") + dir;
	};
	auto formatLat = [](double val) -> QString {
		double absVal = std::abs(val);
		QString dir = (val >= 0) ? QString::fromUtf8("N") : QString::fromUtf8("S");
		return QString::number(absVal, 'f', 4) + QString::fromUtf8("°") + dir;
	};

	painter->save();

	// ========== 在地图坐标空间绘制经纬线 ==========
	painter->setPen(mLinePen);

	for (int i = 0; i < numLonLines; ++i)
	{
		double x = xMin + i * lonStep;
		QPointF top = toCanvasCoordinates(QgsPointXY(x, yMax));
		QPointF bottom = toCanvasCoordinates(QgsPointXY(x, yMin));
		painter->drawLine(top, bottom);
	}

	for (int i = 0; i < numLatLines; ++i)
	{
		double y = yMin + i * latStep;
		QPointF left = toCanvasCoordinates(QgsPointXY(xMin, y));
		QPointF right = toCanvasCoordinates(QgsPointXY(xMax, y));
		painter->drawLine(left, right);
	}

	// ========== 切换到 widget 像素坐标空间绘制标注和比例尺 ==========
	// QgsMapCanvasItem 的 paint 在调用前已经应用了地图坐标变换，
	// 需要先恢复为 widget 坐标，才能在画布边界上固定绘制。
	painter->resetTransform();

	painter->setPen(mLabelPen);
	painter->setFont(mLabelFont);

	const int margin = 3;

	// 估算标注文字尺寸，用于角部安全边距
	QString sampleLonLabel = formatLon(xMin);
	QString sampleLatLabel = formatLat(yMin);
	QRectF lonSample = painter->boundingRect(QRectF(), Qt::AlignCenter, sampleLonLabel);
	QRectF latSample = painter->boundingRect(QRectF(), Qt::AlignCenter, sampleLatLabel);
	const double lonLabelW = lonSample.width();
	const double lonLabelH = lonSample.height();
	const double latLabelW = latSample.width();
	const double latLabelH = latSample.height();

	// ==================== 四角刻度值（独立绘制，不受四边循环控制） ====================
	// 策略：每个角只显示一种刻度，保证在控件内
	// 左上角 — 纬度，右上角 — 纬度
	// 左下角 — 经度，右下角 — 经度

	// 左上角 — 纬度
	{
		double y = yMax;
		QPointF pos = toCanvasCoordinates(QgsPointXY(xMin, y));
		QString label = formatLat(y);
		double textX = static_cast<double>(canvasRect.left()) + margin;
		double textY = pos.y() + latLabelH / 3.0;
		if (textY < static_cast<double>(canvasRect.top()) + latLabelH - margin)
			textY = static_cast<double>(canvasRect.top()) + latLabelH - margin;
		if (textY + latLabelH > static_cast<double>(canvasRect.bottom()))
			textY = static_cast<double>(canvasRect.bottom()) - margin;
		painter->drawText(QPointF(textX, textY), label);
	}

	// 右上角 — 纬度
	{
		double y = yMax;
		QPointF pos = toCanvasCoordinates(QgsPointXY(xMax, y));
		QString label = formatLat(y);
		double textX = static_cast<double>(canvasRect.right()) - latLabelW - margin;
		double textY = pos.y() + latLabelH / 3.0;
		if (textY < static_cast<double>(canvasRect.top()) + latLabelH - margin)
			textY = static_cast<double>(canvasRect.top()) + latLabelH - margin;
		if (textY + latLabelH > static_cast<double>(canvasRect.bottom()))
			textY = static_cast<double>(canvasRect.bottom()) - margin;
		painter->drawText(QPointF(textX, textY), label);
	}

	// 右下角 — 经度（底边贴底，居中对齐经纬线交点）
	{
		double x = xMax;
		QPointF pos = toCanvasCoordinates(QgsPointXY(x, yMin));
		QString label = formatLon(x);
		double textX = pos.x() - lonLabelW / 2.0;
		if (textX + lonLabelW > static_cast<double>(canvasRect.right()) - margin)
			textX = static_cast<double>(canvasRect.right()) - lonLabelW - margin;
		double textY = static_cast<double>(canvasRect.bottom()) - margin;
		if (textY - lonLabelH < static_cast<double>(canvasRect.top()))
			textY = static_cast<double>(canvasRect.top()) + lonLabelH;
		painter->drawText(QPointF(textX, textY), label);
	}

	// ==================== 四边中间刻度（排除角部区域） ====================

	// 顶部 — 经度标注（跳过首尾，首尾已在角部绘制）
	for (int i = 1; i < numLonLines - 1; ++i)
	{
		double x = xMin + i * lonStep;
		QPointF pos = toCanvasCoordinates(QgsPointXY(x, yMax));
		QString label = formatLon(x);
		double textX = pos.x() - lonLabelW / 2.0;
		if (textX < static_cast<double>(canvasRect.left()) + margin)
			textX = static_cast<double>(canvasRect.left()) + margin;
		if (textX + lonLabelW > static_cast<double>(canvasRect.right()) - margin)
			textX = static_cast<double>(canvasRect.right()) - lonLabelW - margin;
		double textY = static_cast<double>(canvasRect.top()) + lonLabelH - margin;
		painter->drawText(QPointF(textX, textY), label);
	}

	// 底部 — 经度标注（跳过首尾和最后一个，最后一个在右下角单独绘制）
	for (int i = 1; i < numLonLines - 1; ++i)
	{
		double x = xMin + i * lonStep;
		QPointF pos = toCanvasCoordinates(QgsPointXY(x, yMin));
		QString label = formatLon(x);
		double textX = pos.x() - lonLabelW / 2.0;
		if (textX < static_cast<double>(canvasRect.left()) + margin)
			textX = static_cast<double>(canvasRect.left()) + margin;
		if (textX + lonLabelW > static_cast<double>(canvasRect.right()) - margin)
			textX = static_cast<double>(canvasRect.right()) - lonLabelW - margin;
		double textY = static_cast<double>(canvasRect.bottom()) - margin;
		painter->drawText(QPointF(textX, textY), label);
	}

	// 左侧 — 纬度标注（跳过首尾，最后一条避开左下角比例尺区域）
	for (int i = 1; i < numLatLines - 1; ++i)
	{
		double y = yMin + i * latStep;
		QPointF pos = toCanvasCoordinates(QgsPointXY(xMin, y));
		QString label = formatLat(y);
		double textX = static_cast<double>(canvasRect.left()) + margin;
		double textY = pos.y() + latLabelH / 3.0;
		if (textY < static_cast<double>(canvasRect.top()) + latLabelH)
			textY = static_cast<double>(canvasRect.top()) + latLabelH;
		// 最下面一条纬度标注需要避开左下角比例尺和经度值区域（底部约 45px）
		if (textY > static_cast<double>(canvasRect.bottom()) - 45)
			textY = static_cast<double>(canvasRect.bottom()) - 45;
		painter->drawText(QPointF(textX, textY), label);
	}

	// 右侧 — 纬度标注（跳过首尾）
	for (int i = 1; i < numLatLines - 1; ++i)
	{
		double y = yMin + i * latStep;
		QPointF pos = toCanvasCoordinates(QgsPointXY(xMax, y));
		QString label = formatLat(y);
		double textX = static_cast<double>(canvasRect.right()) - latLabelW - margin;
		double textY = pos.y() + latLabelH / 3.0;
		if (textY < static_cast<double>(canvasRect.top()) + latLabelH)
			textY = static_cast<double>(canvasRect.top()) + latLabelH;
		if (textY > static_cast<double>(canvasRect.bottom()) - margin)
			textY = static_cast<double>(canvasRect.bottom()) - margin;
		painter->drawText(QPointF(textX, textY), label);
	}

	// ========== 左下角 — 地图比例尺 ==========
	// 比例尺绘制在左下角，其上方留给左下角经度值
	// scale() 返回比例尺分母 (1:N 中的 N)，由 QGIS 根据 CRS 和 DPI 自动计算
	double scaleDenominator = settings.scale();
	if (scaleDenominator > 0)
	{
		// 公式：地面距离(m) = 像素数 * 0.0254 / DPI * scaleDenominator
		double outputDpi = settings.outputDpi();
		if (outputDpi <= 0) outputDpi = 96.0;
		double metersPerPixel = 0.0254 / outputDpi * scaleDenominator;

		const double targetBarPixels = 120.0;
		double targetMeters = targetBarPixels * metersPerPixel;

		// 圆整到美观的数值 (1, 2, 2.5, 5 的倍数)
		double magnitude = std::pow(10.0, std::floor(std::log10(targetMeters)));
		double normalized = targetMeters / magnitude;
		double niceValue;
		if (normalized <= 1.5)      niceValue = 1.0;
		else if (normalized <= 2.5) niceValue = 2.0;
		else if (normalized <= 5.0) niceValue = 2.5;
		else if (normalized <= 7.5) niceValue = 5.0;
		else                        niceValue = 10.0;
		double barMeters = niceValue * magnitude;
		double barPixels = barMeters / metersPerPixel;

		// 比例尺位置：左下角（widget 像素坐标）
		const int barX = 8;
		const int barY = canvasRect.bottom() - 65;  // 上移，给左下角经度留空间
		const int tickHeight = 12;

		QPen barPen(QColor(40, 40, 40), 3.5);
		painter->setPen(barPen);

		// 比例尺条主体
		painter->drawLine(QPointF(barX, barY), QPointF(barX + barPixels, barY));
		// 左端竖线
		painter->drawLine(QPointF(barX, barY - tickHeight / 2), QPointF(barX, barY + tickHeight / 2));
		// 右端竖线
		painter->drawLine(QPointF(barX + barPixels, barY - tickHeight / 2),
			QPointF(barX + barPixels, barY + tickHeight / 2));
		// 中间竖线
		painter->drawLine(QPointF(barX + barPixels / 2.0, barY - tickHeight / 2),
			QPointF(barX + barPixels / 2.0, barY + tickHeight / 2));

		// 比例尺文字
		painter->setPen(mLabelPen);
		painter->setFont(mLabelFont);

		QString barLabel;
		if (barMeters >= 1000.0)
			barLabel = QString::number(barMeters / 1000.0, 'f', 1) + QString::fromUtf8(" km");
		else
			barLabel = QString::number(static_cast<int>(barMeters)) + QString::fromUtf8(" m");

		QRectF labelRect = painter->boundingRect(QRectF(), Qt::AlignCenter, barLabel);
		painter->drawText(QPointF(barX + barPixels / 2.0 - labelRect.width() / 2.0,
			barY - tickHeight / 2 - 2), barLabel);

		// 比例尺数值文字 (1:N)
		QString scaleText = QString("1:%1").arg(static_cast<long long>(scaleDenominator));
		QRectF scaleRect = painter->boundingRect(QRectF(), Qt::AlignCenter, scaleText);
		painter->drawText(QPointF(barX + barPixels / 2.0 - scaleRect.width() / 2.0,
			barY + tickHeight / 2 + scaleRect.height() + 1), scaleText);
	}

	// 左下角 — 经度值，贴底显示
	{
		double x = xMin;
		QPointF pos = toCanvasCoordinates(QgsPointXY(x, yMin));
		QString label = formatLon(x);
		double textX = pos.x() - lonLabelW / 2.0;
		if (textX < static_cast<double>(canvasRect.left()) + margin)
			textX = static_cast<double>(canvasRect.left()) + margin;
		if (textX + lonLabelW > static_cast<double>(canvasRect.right()))
			textX = static_cast<double>(canvasRect.right()) - lonLabelW - margin;
		double textY = static_cast<double>(canvasRect.bottom()) - margin;
		if (textY - lonLabelH < static_cast<double>(canvasRect.top()))
			textY = static_cast<double>(canvasRect.top()) + lonLabelH;
		painter->drawText(QPointF(textX, textY), label);
	}

	painter->restore();
}

// ===================== ProductSearchDialog =====================

// 前置声明（定义在 fillBasicMetadata 之前）
static void clearLayout(QVBoxLayout* layout);

ProductSearchDialog::ProductSearchDialog(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	mDAO = new ProductDAO(this);

	ui.setupUi(this);

	ui.mResultTable->setAlternatingRowColors(true);

	// 结果表格支持多选（Ctrl/Shift 点选多行），便于批量添加到地图或导出
	ui.mResultTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
	ui.mResultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui.mResultTable->setContextMenuPolicy(Qt::CustomContextMenu);

	QgsGui::enableAutoGeometryRestore(this);
	setupTableColumns();
	setupConnections();

	// 创建 QgsMapCanvas 并嵌入 mMapCanvasPlaceholder
	mMapCanvas = new QgsMapCanvas();
	mMapCanvas->setMinimumSize(200, 200);
	// 地图画布默认背景为白色
	mMapCanvas->setCanvasColor(QColor(255, 255, 255));
	mMapCanvas->enableAntiAliasing(true);
	mMapCanvas->setVisible(false);

	// 设置左键拖拽为漫游模式
	mPanTool = new QgsMapToolPan(mMapCanvas);
	mMapCanvas->setMapTool(mPanTool);

	QVBoxLayout* pgLayout = qobject_cast<QVBoxLayout*>(ui.previewGroup->layout());
	if (pgLayout)
	{
		pgLayout->removeWidget(ui.mMapCanvasPlaceholder);
		ui.mMapCanvasPlaceholder->hide();
		pgLayout->addWidget(mMapCanvas);
	}

	setWindowTitle("成果检索与预览");
	resize(1100, 750);

	// 默认两栏：左侧（检索+表格）60%，右侧（地图预览+详细信息）40%
	QList<int> sizes;
	sizes << static_cast<int>(width() * 0.60)
		  << static_cast<int>(width() * 0.40);
	ui.splitter->setSizes(sizes);

	createDetailPanel();
}

ProductSearchDialog::~ProductSearchDialog()
{
}

void ProductSearchDialog::setupTableColumns()
{
	ui.mResultTable->setColumnCount(7);
	ui.mResultTable->setHorizontalHeaderLabels({
		"ID", "产品名称", "类型", "格式", "密级", "版本", "更新时间"
	});
	ui.mResultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui.mResultTable->horizontalHeader()->setStretchLastSection(true);
}

void ProductSearchDialog::setupConnections()
{
	mDebounceTimer = new QTimer(this);
	mDebounceTimer->setSingleShot(true);
	mDebounceTimer->setInterval(350);
	connect(mDebounceTimer, &QTimer::timeout, this, &ProductSearchDialog::onSearch);

	connect(ui.mSearchBtn, &QPushButton::clicked, this, &ProductSearchDialog::onSearch);
	connect(ui.mResetBtn, &QPushButton::clicked, this, &ProductSearchDialog::onReset);
	connect(ui.mExportBtn, &QPushButton::clicked, this, &ProductSearchDialog::onExportResults);
	connect(ui.mAddToMapBtn, &QPushButton::clicked, this, &ProductSearchDialog::onAddToMap);
	connect(ui.mResultTable, &QTableWidget::cellClicked, this, &ProductSearchDialog::onResultSelected);
	// 结果表格右键菜单（添加到地图 / 导出数据文件）
	connect(ui.mResultTable, &QTableWidget::customContextMenuRequested,
	        this, &ProductSearchDialog::onResultTableContextMenu);
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
	connect(ui.mSpatialFilterCheck, &QCheckBox::toggled, this, [this](bool checked) {
		ui.mMinXEdit->setEnabled(checked);
		ui.mMinYEdit->setEnabled(checked);
		ui.mMaxXEdit->setEnabled(checked);
		ui.mMaxYEdit->setEnabled(checked);
		scheduleSearch();
	});

	// 检索条件变更 → 自动防抖搜索
	connect(ui.mKeywordEdit, &QLineEdit::textChanged, this, &ProductSearchDialog::scheduleSearch);
	connect(ui.mProductTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
	        this, &ProductSearchDialog::scheduleSearch);
	connect(ui.mProductCategoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
	        this, &ProductSearchDialog::scheduleSearch);
	connect(ui.mDateFromEdit, &QLineEdit::textChanged, this, &ProductSearchDialog::scheduleSearch);
	connect(ui.mDateToEdit, &QLineEdit::textChanged, this, &ProductSearchDialog::scheduleSearch);
	connect(ui.mMinXEdit, &QLineEdit::textChanged, this, &ProductSearchDialog::scheduleSearch);
	connect(ui.mMinYEdit, &QLineEdit::textChanged, this, &ProductSearchDialog::scheduleSearch);
	connect(ui.mMaxXEdit, &QLineEdit::textChanged, this, &ProductSearchDialog::scheduleSearch);
	connect(ui.mMaxYEdit, &QLineEdit::textChanged, this, &ProductSearchDialog::scheduleSearch);

	// 日期选择器
	setupDateEdit(ui.mDateFromEdit);
	setupDateEdit(ui.mDateToEdit);

	// 数据库 schema 切换后自动清空搜索结果
	connect(PostgisConnector::instance(), &PostgisConnector::schemaChanged, this, [this](const QString&) {
		onReset();
	});
}

void ProductSearchDialog::onSearch()
{
	mCurrentPage = 1;
	performSearch();
}

void ProductSearchDialog::scheduleSearch()
{
	mDebounceTimer->start();  // 重启 350ms 定时器，连续输入时只触发一次
}

void ProductSearchDialog::onReset()
{
	ui.mKeywordEdit->clear();
	ui.mProductTypeCombo->setCurrentIndex(0);
	ui.mProductCategoryCombo->setCurrentIndex(0);
	ui.mDateFromEdit->clear();
	ui.mDateToEdit->clear();
	ui.mMinXEdit->clear();
	ui.mMinYEdit->clear();
	ui.mMaxXEdit->clear();
	ui.mMaxYEdit->clear();
	ui.mSpatialFilterCheck->setChecked(false);

	ui.mResultTable->setRowCount(0);
	ui.mResultCountLabel->setText("共找到 0 条结果");
	if (mBasicLayout) clearLayout(mBasicLayout);
	if (mSpeLayout) clearLayout(mSpeLayout);
	if (mPreviewWarningLabel) mPreviewWarningLabel->setVisible(false);
}

void ProductSearchDialog::onExportResults()
{
	if (mSearchResults.isEmpty())
	{
		QMessageBox::warning(this, "提示", "没有可导出的结果");
		return;
	}

	// 确定要导出的数据：优先导出当前选中的行（支持多选），未选中时按确认导出整页
	QList<ProductMetadata> toExport = selectedProducts();
	if (toExport.isEmpty())
	{
		// 未选中时导出全部
		int ret = QMessageBox::question(this, "批量导出",
			QString("未选中具体记录，是否导出当前页全部 %1 条数据？").arg(mSearchResults.size()),
			QMessageBox::Yes | QMessageBox::No);
		if (ret != QMessageBox::Yes) return;
		toExport = mSearchResults;
	}

	// 选择导出目录
	QString outputDir = QFileDialog::getExistingDirectory(this,
		"选择导出目录", QString());
	if (outputDir.isEmpty()) return;

	// 重置导出标志
	mOverwriteAll = false;
	mSkipAll = false;
	mExportCancelled = false;

	// 禁用导出按钮，防止重复点击
	ui.mExportBtn->setEnabled(false);

	// 设置进度条
	ui.mExportProgressBar->setMinimum(0);
	ui.mExportProgressBar->setMaximum(toExport.size());
	ui.mExportProgressBar->setValue(0);
	ui.mExportProgressBar->setFormat("正在准备导出...");
	ui.mExportProgressBar->setVisible(true);
	QApplication::processEvents();

	int successCount = 0;
	QStringList failedItems;

	for (int i = 0; i < toExport.size(); ++i)
	{
		const auto& meta = toExport[i];

		ui.mExportProgressBar->setFormat(
			QString("正在导出 (%1/%2): %3").arg(i + 1).arg(toExport.size()).arg(meta.productName));

		// 直接导出到所选目录（不再为每个成果创建“名称_ID”子文件夹）
		if (exportDataFile(meta, outputDir))
		{
			successCount++;
		}
		else
		{
			if (mExportCancelled)
			{
				break;
			}
			failedItems.append(QString("%1 (ID:%2)").arg(meta.productName).arg(meta.id));
		}

		ui.mExportProgressBar->setValue(i + 1);
		QApplication::processEvents();
	}

	// 隐藏进度条
	ui.mExportProgressBar->setVisible(false);
	QApplication::processEvents();

	// 恢复按钮
	ui.mExportBtn->setEnabled(true);

	// 显示结果
	QString resultMsg = QString("数据文件导出完成。\n目标目录: %1\n成功: %2 条").arg(outputDir).arg(successCount);
	if (!failedItems.isEmpty())
	{
		resultMsg += QString("\n失败: %1 条\n\n失败列表:\n%2")
			.arg(failedItems.size())
			.arg(failedItems.join("\n"));
	}
	QMessageBox::information(this, "导出完成", resultMsg);
}

void ProductSearchDialog::onPreviewSelected()
{
	int row = ui.mResultTable->currentRow();
	if (row < 0 || row >= mSearchResults.size()) return;

	updateProductDetail(mSearchResults[row]);
}

void ProductSearchDialog::onAddToMap()
{
	if (mSearchResults.isEmpty())
	{
		QMessageBox::warning(this, "提示", "没有可添加到地图的结果");
		return;
	}

	// 多选支持：收集表格中所有选中行对应的成果
	QList<ProductMetadata> toAdd = selectedProducts();
	if (toAdd.isEmpty())
	{
		QMessageBox::warning(this, "提示", "请先在结果列表中选中一条或多条记录");
		return;
	}

	if (toAdd.size() == 1)
	{
		// 单条：失败时由辅助函数弹出具体原因
		AddToMapHelper::addToMapWithFeedback(this, mQGisIface, toAdd.first());
		return;
	}

	// 多条：逐个静默加载，结束后统一汇总结果
	int success = 0;
	QStringList failed;
	for (const auto& meta : toAdd)
	{
		if (AddToMapHelper::addLayerToMap(mQGisIface, meta))
			++success;
		else
			failed << meta.productName;
	}
	if (failed.isEmpty())
		QMessageBox::information(this, "添加到地图",
			QString("已成功添加 %1 个图层到地图。").arg(success));
	else
		QMessageBox::warning(this, "添加到地图",
			QString("成功 %1 个，失败 %2 个。").arg(success).arg(failed.size()));
}

// 收集结果表格当前选中行对应的成果（按行号升序、去重）
QList<ProductMetadata> ProductSearchDialog::selectedProducts() const
{
	QList<ProductMetadata> result;
	if (mSearchResults.isEmpty())
		return result;

	QSet<int> rowSet;
	const QList<QTableWidgetItem*> items = ui.mResultTable->selectedItems();
	for (const QTableWidgetItem* item : items)
		rowSet.insert(item->row());

	QList<int> rows = rowSet.values();
	std::sort(rows.begin(), rows.end());

	for (int row : rows)
	{
		if (row >= 0 && row < mSearchResults.size())
			result.append(mSearchResults[row]);
	}
	return result;
}

// 结果表格右键菜单：右键未选中行时切换为仅选中该行，右键已选行则保留多选
void ProductSearchDialog::onResultTableContextMenu(const QPoint& pos)
{
	QTableWidgetItem* item = ui.mResultTable->itemAt(pos);
	if (!item)
		return;

	const int row = item->row();
	if (row < 0 || row >= mSearchResults.size())
		return;

	// 若右键行不在当前选中集中，则将其作为唯一选中
	bool rowSelected = false;
	const QList<QTableWidgetItem*> items = ui.mResultTable->selectedItems();
	for (const QTableWidgetItem* it : items)
	{
		if (it->row() == row)
		{
			rowSelected = true;
			break;
		}
	}
	if (!rowSelected)
	{
		ui.mResultTable->clearSelection();
		ui.mResultTable->selectRow(row);
		ui.mResultTable->setCurrentCell(row, 0);
		// 与单击行为保持一致：同步刷新右侧详细信息与地图预览
		onResultSelected(row, 0);
	}

	QMenu menu(this);
	QAction* actAddToMap = menu.addAction(QStringLiteral("添加到地图"));
	QAction* actExport = menu.addAction(QStringLiteral("导出数据文件"));
	QAction* chosen = menu.exec(ui.mResultTable->viewport()->mapToGlobal(pos));
	if (chosen == actAddToMap)
		onAddToMap();
	else if (chosen == actExport)
		onExportResults();
}

void ProductSearchDialog::onResultSelected(int row, int /*col*/)
{
	if (row < 0 || row >= mSearchResults.size()) return;

	const ProductMetadata& meta = mSearchResults[row];
	updateProductDetail(meta);

	// 自动触发地图预览
	autoPreview(meta);
}

void ProductSearchDialog::onPageChanged()
{
	performSearch();
}

void ProductSearchDialog::performSearch()
{
	ProductDAO::SearchCondition cond;
	cond.keyword = ui.mKeywordEdit->text().trimmed();

	// 产品类型：支持单类型和大类分组
	QString ptypeText = ui.mProductTypeCombo->currentText();
	if (ptypeText == QStringLiteral("制图文件"))
	{
		cond.productTypes = {ProductType::CAD, ProductType::AI, ProductType::CDR, ProductType::PDF};
	}
	else if (ptypeText == QStringLiteral("其它类型文件"))
	{
		cond.productTypes = {ProductType::Document, ProductType::Archive, ProductType::Other};
	}
	else
	{
		cond.productType = (ui.mProductTypeCombo->currentIndex() == 0)
			? ProductType::Other
			: stringToProductType(ptypeText);
	}

	cond.dateFrom = ui.mDateFromEdit->text().trimmed();
	cond.dateTo = ui.mDateToEdit->text().trimmed();
	cond.limit = mPageSize;
	cond.offset = (mCurrentPage - 1) * mPageSize;

	// 空间过滤
	if (ui.mSpatialFilterCheck->isChecked())
	{
		cond.hasSpatialFilter = true;
		cond.minX = ui.mMinXEdit->text().toDouble();
		cond.minY = ui.mMinYEdit->text().toDouble();
		cond.maxX = ui.mMaxXEdit->text().toDouble();
		cond.maxY = ui.mMaxYEdit->text().toDouble();
	}

	// 地图成果类型：按"制图成果-AI"等分类节点的整棵子树过滤
	QString category = ui.mProductCategoryCombo->currentText();
	if (!category.isEmpty() && category != QStringLiteral("不限"))
	{
		int dashIdx = category.indexOf('-');
		if (dashIdx > 0)
		{
			QString first = category.left(dashIdx);
			QString second = category.mid(dashIdx + 1);
			auto ids = DirectoryHelper::ensureFixedDirectories(*mDAO);
			int secondLevelId = -1;
			if (first == QStringLiteral("制图成果"))
			{
				if (second == QStringLiteral("AI")) secondLevelId = ids.resultAI;
				else if (second == QStringLiteral("PDF")) secondLevelId = ids.resultPDF;
				else secondLevelId = ids.resultOther;
			}
			else if (first == QStringLiteral("制图要素"))
			{
				if (second == QStringLiteral("影像")) secondLevelId = ids.elementImage;
				else if (second == QStringLiteral("晕渲")) secondLevelId = ids.elementShading;
				else secondLevelId = ids.elementFeature;
			}
			else if (first == QStringLiteral("制图资料"))
			{
				if (second == QStringLiteral("文档资料")) secondLevelId = ids.materialDoc;
				else if (second == QStringLiteral("表格资料")) secondLevelId = ids.materialTable;
				else secondLevelId = ids.materialOther;
			}
			if (secondLevelId > 0)
				cond.directoryIds = mDAO->collectSubtreeDirectoryIds(secondLevelId);
		}
	}

	// 执行搜索
	mTotalResults = mDAO->countSearchResults(cond);
	mTotalPages = qMax(1, (mTotalResults + mPageSize - 1) / mPageSize);
	mSearchResults = mDAO->searchProducts(cond);

	// 更新分页控件
	ui.mPageSpinBox->setMaximum(mTotalPages);
	ui.mPageSpinBox->setValue(mCurrentPage);
	ui.mTotalPageLabel->setText(QString("/ %1 页").arg(mTotalPages));
	ui.mResultCountLabel->setText(QString("共找到 %1 条结果").arg(mTotalResults));

	// 填充结果表格
	ui.mResultTable->setRowCount(0);
	for (int i = 0; i < mSearchResults.size(); ++i)
	{
		const auto& meta = mSearchResults[i];
		ui.mResultTable->insertRow(i);
		ui.mResultTable->setItem(i, 0, new QTableWidgetItem(QString::number(meta.id)));
		ui.mResultTable->setItem(i, 1, new QTableWidgetItem(meta.productName));
		ui.mResultTable->setItem(i, 2, new QTableWidgetItem(productTypeToDisplayName(meta.productType)));
		ui.mResultTable->setItem(i, 3, new QTableWidgetItem(meta.fileFormat));
		ui.mResultTable->setItem(i, 4, new QTableWidgetItem(securityLevelToString(meta.securityLevel)));
		ui.mResultTable->setItem(i, 5, new QTableWidgetItem(QString::number(meta.currentVersion)));
		ui.mResultTable->setItem(i, 6, new QTableWidgetItem(meta.updatedAt.toString("yyyy-MM-dd hh:mm")));
	}
}

void ProductSearchDialog::updateProductDetail(const ProductMetadata& meta)
{
	// 展开面板（如果折叠状态）
	if (!mDetailContainer->isVisible())
	{
		mDetailContainer->setVisible(true);
		mDetailToggleBtn->setText(QString::fromUtf8("▼ 详细信息"));
	}

	fillBasicMetadata(meta);
	fillSpecializedMetadata(meta);
}

static void clearLayout(QVBoxLayout* layout)
{
	if (!layout) return;
	QLayoutItem* child;
	while ((child = layout->takeAt(0)) != nullptr)
	{
		if (child->widget()) child->widget()->deleteLater();
		if (child->layout())
		{
			QLayoutItem* sub;
			while ((sub = child->layout()->takeAt(0)) != nullptr)
			{
				if (sub->widget()) sub->widget()->deleteLater();
				delete sub;
			}
		}
		delete child;
	}
}

void ProductSearchDialog::fillBasicMetadata(const ProductMetadata& meta)
{
	clearLayout(mBasicLayout);

	auto addSectionTitle = [&](const QString& title) {
		auto* lbl = new QLabel(title);
		QFont font = lbl->font();
		font.setBold(true);
		lbl->setFont(font);
		mBasicLayout->addWidget(lbl);
	};

	int row = 0;
	auto addROField = [&](const QString& label, const QString& val) {
		auto* rowWidget = new QWidget();

		auto* hbox = new QHBoxLayout(rowWidget);
		hbox->setContentsMargins(16, 2, 12, 2);
		hbox->setSpacing(0);

		auto* lbl = new QLabel(label);
		lbl->setMinimumWidth(110);
		lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

		auto* ed = new QLineEdit(val.isEmpty() ? QString("-") : val);
		ed->setReadOnly(true);
		ed->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

		hbox->addWidget(lbl);
		hbox->addWidget(ed, 1);
		mBasicLayout->addWidget(rowWidget);
		row++;
	};

	// ── 【基本信息】 ──
	addSectionTitle(QString::fromUtf8("【基本信息】"));
	addROField(QString::fromUtf8("数据唯一标识:"), meta.dataId);
	addROField(QString::fromUtf8("数据名称:"), meta.productName);
	addROField(QString::fromUtf8("数据描述:"), meta.description);
	addROField(QString::fromUtf8("数据来源:"), meta.source);
	addROField(QString::fromUtf8("版本说明:"), meta.versionNote);
	addROField(QString::fromUtf8("标签:"),
		meta.tags.isEmpty() ? QString("-") : QString(meta.tags).replace(QLatin1Char(';'), QString::fromUtf8(", ")));

	// ── 【格式与存储】 ──
	addSectionTitle(QString::fromUtf8("【格式与存储】"));
	addROField(QString::fromUtf8("数据格式:"), meta.fileFormat);
	addROField(QString::fromUtf8("数据量:"), formatFileSize(meta.fileSize));
	addROField(QString::fromUtf8("是否压缩:"), meta.isCompressed.isEmpty() ? QString("-") : meta.isCompressed);

	// ── 【空间信息】 ──
	addSectionTitle(QString::fromUtf8("【空间信息】"));
	QString bounds = meta.bounds.isEmpty()
		? QString("[%1, %2] - [%3, %4]")
			.arg(meta.minX, 0, 'f', 4).arg(meta.minY, 0, 'f', 4)
			.arg(meta.maxX, 0, 'f', 4).arg(meta.maxY, 0, 'f', 4)
		: meta.bounds;
	addROField(QString::fromUtf8("空间范围:"), bounds);
	addROField(QString::fromUtf8("中心点经度:"), meta.centerLon.isEmpty() ? QString("-") : meta.centerLon);
	addROField(QString::fromUtf8("中心点纬度:"), meta.centerLat.isEmpty() ? QString("-") : meta.centerLat);
	addROField(QString::fromUtf8("坐标参考系:"), meta.crs);
	addROField(QString::fromUtf8("比例尺分母:"), meta.scale);
	addROField(QString::fromUtf8("几何类型:"), meta.geometryType.isEmpty() ? QString("-") : meta.geometryType);

	// ── 【时间与周期】 ──
	addSectionTitle(QString::fromUtf8("【时间与周期】"));
	addROField(QString::fromUtf8("现势性起始:"),
		meta.startDatetime.isValid() ? meta.startDatetime.toString("yyyy-MM-dd hh:mm") : QString("-"));
	addROField(QString::fromUtf8("现势性截止:"),
		meta.endDatetime.isValid() ? meta.endDatetime.toString("yyyy-MM-dd hh:mm") : QString("-"));
	addROField(QString::fromUtf8("创建时间:"),
		meta.createdAt.isValid() ? meta.createdAt.toString("yyyy-MM-dd hh:mm:ss") : QString("-"));
	addROField(QString::fromUtf8("更新时间:"),
		meta.updatedAt.isValid() ? meta.updatedAt.toString("yyyy-MM-dd hh:mm:ss") : QString("-"));

	// ── 【管理与来源】 ──
	addSectionTitle(QString::fromUtf8("【管理与来源】"));
	addROField(QString::fromUtf8("密级:"), securityLevelToString(meta.securityLevel));
	addROField(QString::fromUtf8("创建人员:"), meta.createdBy);
	addROField(QString::fromUtf8("更新人员:"), meta.updatedBy);
	addROField(QString::fromUtf8("所在市州:"), meta.city);
	addROField(QString::fromUtf8("项目名称:"), meta.projectName);
	addROField(QString::fromUtf8("生产单位:"), meta.producer);
	addROField(QString::fromUtf8("生产日期:"),
		meta.productionDate.isValid() ? meta.productionDate.toString("yyyy-MM-dd") : QString("-"));
	addROField(QString::fromUtf8("编制信息:"), meta.compilationInfo);
	addROField(QString::fromUtf8("汇交情况:"), meta.deliveryStatus);
	addROField(QString::fromUtf8("汇交时间:"),
		meta.deliveryTime.isValid() ? meta.deliveryTime.toString("yyyy-MM-dd hh:mm") : QString("-"));
	addROField(QString::fromUtf8("审图号:"), meta.approvalNumber);

	// ── 【系统字段】 ──
	addSectionTitle(QString::fromUtf8("【系统字段】"));
	addROField(QString::fromUtf8("产品ID:"), QString::number(meta.id));
	addROField(QString::fromUtf8("产品类型:"), productTypeToString(meta.productType));
	addROField(QString::fromUtf8("版本号:"), QString::number(meta.currentVersion));
	addROField(QString::fromUtf8("PostGIS 图层表:"), meta.layerTableName);
	addROField(QString::fromUtf8("目录路径:"), meta.directoryPath);
	addROField(QString::fromUtf8("原始文件路径:"), meta.filePath);

	mBasicLayout->addStretch();
}

void ProductSearchDialog::fillSpecializedMetadata(const ProductMetadata& meta)
{
	clearLayout(mSpeLayout);

	if (meta.id <= 0)
	{
		auto* lbl = new QLabel(QString::fromUtf8("暂无专有元数据"));
		lbl->setAlignment(Qt::AlignCenter);
		mSpeLayout->addWidget(lbl);
		return;
	}

	int row = 0;
	auto addSpeField = [&](const QString& label, const QString& val) {
		auto* rowWidget = new QWidget();

		auto* hbox = new QHBoxLayout(rowWidget);
		hbox->setContentsMargins(16, 2, 12, 2);
		hbox->setSpacing(0);

		auto* lbl = new QLabel(label);
		lbl->setMinimumWidth(110);
		lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

		auto* ed = new QLineEdit(val.isEmpty() ? QString("-") : val);
		ed->setReadOnly(true);
		ed->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

		hbox->addWidget(lbl);
		hbox->addWidget(ed, 1);
		mSpeLayout->addWidget(rowWidget);
		row++;
	};

	switch (meta.productType)
	{
	case ProductType::Vector:
	{
		ProductVectorMeta vm = mDAO->getVectorMeta(meta.id);
		addSpeField(QString::fromUtf8("几何类型:"), vm.geomType);
		addSpeField(QString::fromUtf8("比例尺分母:"), QString::number(vm.invScale));
		addSpeField(QString::fromUtf8("坐标系类型:"), vm.csType);
		addSpeField(QString::fromUtf8("大地基准:"), vm.geodeticDatum);
		addSpeField(QString::fromUtf8("地图投影代码:"), vm.epsgCode);
		addSpeField(QString::fromUtf8("地图投影描述:"), vm.projDesc);
		addSpeField(QString::fromUtf8("要素数量:"), QString::number(vm.featureCount));
		addSpeField(QString::fromUtf8("字段数量:"), QString::number(vm.fieldCount));
		addSpeField(QString::fromUtf8("属性字段说明:"), vm.fieldDesc.isEmpty() ? QString("-") : vm.fieldDesc);
		addSpeField(QString::fromUtf8("入库表名:"), vm.layerTableName);
		break;
	}
	case ProductType::Raster:
	{
		ProductRasterMeta rm = mDAO->getRasterMeta(meta.id);
		addSpeField(QString::fromUtf8("卫星名称:"), rm.satelliteName);
		addSpeField(QString::fromUtf8("传感器类型:"), rm.sensorType);
		addSpeField(QString::fromUtf8("成像时间:"),
			rm.acquireTime.isValid() ? rm.acquireTime.toString("yyyy-MM-dd hh:mm") : QString("-"));
		addSpeField(QString::fromUtf8("地面分辨率:"), QString::number(rm.gsd));
		addSpeField(QString::fromUtf8("分辨率单位:"), rm.resolutionUnit);
		addSpeField(QString::fromUtf8("色彩类型:"), rm.colorType);
		addSpeField(QString::fromUtf8("颜色级数:"), QString::number(rm.bitDepth));
		addSpeField(QString::fromUtf8("波段数:"), QString::number(rm.bandCount));
		addSpeField(QString::fromUtf8("无数据值:"), QString::number(rm.nodataValue));
		addSpeField(QString::fromUtf8("坐标系类型:"), rm.csType);
		addSpeField(QString::fromUtf8("大地基准:"), rm.geodeticDatum);
		addSpeField(QString::fromUtf8("地图投影代码:"), rm.epsgCode);
		addSpeField(QString::fromUtf8("地图投影描述:"), rm.projDesc);
		addSpeField(QString::fromUtf8("平面单位:"), rm.planarUnit);
		addSpeField(QString::fromUtf8("像素宽度:"), QString::number(rm.pixelWidth));
		addSpeField(QString::fromUtf8("像素高度:"), QString::number(rm.pixelHeight));
		addSpeField(QString::fromUtf8("像元类型:"), rm.pixelType);
		addSpeField(QString::fromUtf8("入库表名:"), rm.layerTableName);
		break;
	}
	case ProductType::CAD:
	case ProductType::AI:
	case ProductType::CDR:
	case ProductType::PDF:
	{
		ProductDiagramMeta dm = mDAO->getDiagramMeta(meta.id);
		addSpeField(QString::fromUtf8("图名:"), dm.dataId);
		addSpeField(QString::fromUtf8("图集系列名:"), dm.mapSeries);
		addSpeField(QString::fromUtf8("制图区域:"), dm.cityPrefecture);
		addSpeField(QString::fromUtf8("比例尺分母:"), QString::number(dm.mapScale));
		addSpeField(QString::fromUtf8("地图投影:"), dm.projDesc);
		addSpeField(QString::fromUtf8("制图完成日期:"),
			dm.productionDate.isValid() ? dm.productionDate.toString("yyyy-MM-dd") : QString("-"));
		addSpeField(QString::fromUtf8("审图号:"), dm.approvalNo);
		addSpeField(QString::fromUtf8("制图软件:"), dm.cartoSoftware);
		addSpeField(QString::fromUtf8("数据格式:"), dm.format);
		addSpeField(QString::fromUtf8("图件尺寸:"), dm.paperSize);
		addSpeField(QString::fromUtf8("含图例:"), dm.legendIncluded ? QString::fromUtf8("是") : QString::fromUtf8("否"));
		addSpeField(QString::fromUtf8("修改人:"), dm.modifier);
		addSpeField(QString::fromUtf8("最后修改时间:"),
			dm.lastModified.isValid() ? dm.lastModified.toString("yyyy-MM-dd") : QString("-"));
		addSpeField(QString::fromUtf8("印刷级:"), dm.printReady ? QString::fromUtf8("是") : QString::fromUtf8("否"));
		addSpeField(QString::fromUtf8("色彩模式:"), dm.colorMode);
		addSpeField(QString::fromUtf8("输出分辨率:"), QString::number(dm.dpi));
		addSpeField(QString::fromUtf8("项目名称:"), dm.projName);
		addSpeField(QString::fromUtf8("关联栅格资产ID:"), dm.rasterIds);
		addSpeField(QString::fromUtf8("关联矢量资产ID:"), dm.vectorIds);
		addSpeField(QString::fromUtf8("现势性截止:"),
			dm.endDatetime.isValid() ? dm.endDatetime.toString("yyyy-MM-dd") : QString("-"));
		addSpeField(QString::fromUtf8("制图人:"), dm.mapProductor);
		addSpeField(QString::fromUtf8("有数学基础:"), dm.hasMathBase ? QString::fromUtf8("是") : QString::fromUtf8("否"));
		break;
	}
	case ProductType::Document:
	{
		ProductDocumentMeta doc = mDAO->getDocumentMeta(meta.id);
		addSpeField(QString::fromUtf8("来源:"), doc.publisher);
		addSpeField(QString::fromUtf8("文件类型:"), doc.fileType);
		addSpeField(QString::fromUtf8("文件格式:"), doc.format);
		addSpeField(QString::fromUtf8("文件大小:"), doc.fileSize);
		addSpeField(QString::fromUtf8("语言类型:"), doc.languageType);
		addSpeField(QString::fromUtf8("关键字:"), doc.keyWords);
		addSpeField(QString::fromUtf8("内容摘要:"), doc.summary);
		addSpeField(QString::fromUtf8("质量问题记录:"), doc.qualityIssues);
		addSpeField(QString::fromUtf8("收集时间:"),
			doc.collectTime.isValid() ? doc.collectTime.toString("yyyy-MM-dd hh:mm") : QString("-"));
		addSpeField(QString::fromUtf8("现势性截止:"),
			doc.endDatetime.isValid() ? doc.endDatetime.toString("yyyy-MM-dd") : QString("-"));
		addSpeField(QString::fromUtf8("收集人:"), doc.collector);
		addSpeField(QString::fromUtf8("收集目的:"), doc.collectPurpose);
		addSpeField(QString::fromUtf8("项目名称:"), doc.projectName);
		addSpeField(QString::fromUtf8("是否压缩:"), doc.isCompressed ? QString::fromUtf8("是") : QString::fromUtf8("否"));
		break;
	}
	default:
	{
		auto* lbl = new QLabel(QString::fromUtf8("该类型暂无专有元数据"));
		lbl->setAlignment(Qt::AlignCenter);
		mSpeLayout->addWidget(lbl);
		break;
	}
	}

	mSpeLayout->addStretch();
}

// ── 创建可折叠详细信息面板 ──
void ProductSearchDialog::createDetailPanel()
{
	// 清空 UI 中 detailGroupLayout 原有内容
	QLayout* dgLayout = ui.detailGroupLayout;
	QLayoutItem* child;
	while ((child = dgLayout->takeAt(0)) != nullptr)
	{
		if (child->widget()) child->widget()->deleteLater();
		delete child;
	}

	// 隐藏 QGroupBox 自带标题，由折叠按钮代替
	ui.detailGroup->setTitle(QString());

	// ── 折叠按钮 ──
	mDetailToggleBtn = new QToolButton(ui.detailGroup);
	mDetailToggleBtn->setText(QString::fromUtf8("▶ 详细信息"));
	mDetailToggleBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
	mDetailToggleBtn->setAutoRaise(true);
	mDetailToggleBtn->setCursor(Qt::PointingHandCursor);
	mDetailToggleBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	connect(mDetailToggleBtn, &QToolButton::clicked, this, &ProductSearchDialog::toggleDetailPanel);
	dgLayout->addWidget(mDetailToggleBtn);

	// ── 折叠内容容器 ──
	mDetailContainer = new QWidget(ui.detailGroup);
	mDetailContainer->setVisible(false);  // 默认折叠

	QVBoxLayout* containerLayout = new QVBoxLayout(mDetailContainer);
	containerLayout->setContentsMargins(0, 0, 0, 0);
	containerLayout->setSpacing(0);

	// ── Tab 分页：基础元数据 / 专有元数据 ──
	mDetailTabWidget = new QTabWidget(mDetailContainer);

	// Tab 0: 基础元数据 ── 使用 QScrollArea + 动态表单（与元数据管理对话框风格一致）
	{
		QWidget* basicTab = new QWidget();
		mBasicScroll = new QScrollArea(basicTab);
		mBasicScroll->setWidgetResizable(true);
		mBasicScroll->setFrameShape(QFrame::NoFrame);

		mBasicPage = new QWidget();
		mBasicScroll->setWidget(mBasicPage);

		mBasicLayout = new QVBoxLayout(mBasicPage);
		mBasicLayout->setContentsMargins(12, 8, 12, 8);
		mBasicLayout->setSpacing(2);
		mBasicLayout->addStretch();

		QVBoxLayout* tabLayout = new QVBoxLayout(basicTab);
		tabLayout->setContentsMargins(0, 0, 0, 0);
		tabLayout->addWidget(mBasicScroll);

		mDetailTabWidget->addTab(basicTab, QString::fromUtf8("基础元数据"));
	}

	// Tab 1: 专有元数据 ── 使用 QScrollArea + 动态表单（与元数据管理对话框风格一致）
	{
		QWidget* speTab = new QWidget();
		mSpeScroll = new QScrollArea(speTab);
		mSpeScroll->setWidgetResizable(true);
		mSpeScroll->setFrameShape(QFrame::NoFrame);

		mSpePage = new QWidget();
		mSpeScroll->setWidget(mSpePage);

		mSpeLayout = new QVBoxLayout(mSpePage);
		mSpeLayout->setContentsMargins(12, 8, 12, 8);
		mSpeLayout->setSpacing(2);
		mSpeLayout->addStretch();

		QVBoxLayout* tabLayout = new QVBoxLayout(speTab);
		tabLayout->setContentsMargins(0, 0, 0, 0);
		tabLayout->addWidget(mSpeScroll);

		mDetailTabWidget->addTab(speTab, QString::fromUtf8("专有元数据"));
	}

	containerLayout->addWidget(mDetailTabWidget);
	dgLayout->addWidget(mDetailContainer);
}

void ProductSearchDialog::toggleDetailPanel()
{
	bool visible = !mDetailContainer->isVisible();
	mDetailContainer->setVisible(visible);
	mDetailToggleBtn->setText(visible
		? QString::fromUtf8("▼ 详细信息")
		: QString::fromUtf8("▶ 详细信息"));
}

ProductSearchDialog::OverwriteChoice ProductSearchDialog::confirmOverwrite(const QString& filePath)
{
	if (mOverwriteAll) return OverwriteYes;
	if (mSkipAll)    return OverwriteNo;

	QMessageBox msgBox(this);
	msgBox.setWindowTitle(QString::fromUtf8("文件已存在"));
	msgBox.setText(QString::fromUtf8("文件已存在:\n%1\n\n是否覆盖？").arg(filePath));
	msgBox.setIcon(QMessageBox::Question);

	QPushButton* btnOverwrite    = msgBox.addButton(QString::fromUtf8("覆盖"),     QMessageBox::YesRole);
	QPushButton* btnOverwriteAll = msgBox.addButton(QString::fromUtf8("覆盖全部"), QMessageBox::YesRole);
	QPushButton* btnSkip         = msgBox.addButton(QString::fromUtf8("跳过"),     QMessageBox::NoRole);
	QPushButton* btnCancel       = msgBox.addButton(QString::fromUtf8("取消"),     QMessageBox::RejectRole);

	msgBox.setDefaultButton(btnOverwrite);
	msgBox.exec();

	QAbstractButton* clicked = msgBox.clickedButton();
	if (clicked == btnOverwriteAll)
	{
		mOverwriteAll = true;
		return OverwriteAll;
	}
	if (clicked == btnOverwrite)
	{
		return OverwriteYes;
	}
	if (clicked == btnSkip)
	{
		return OverwriteNo;
	}
	mExportCancelled = true;
	return OverwriteCancel;
}

bool ProductSearchDialog::exportDataFile(const ProductMetadata& meta, const QString& outputDir)
{
	auto* db = PostgisConnector::instance();
	if (!db->isConnected())
	{
		return false;
	}

	// 创建输出目录
	QDir dir;
	if (!dir.mkpath(outputDir))
	{
		return false;
	}

	// 根据产品类型和数据存储方式选择导出策略
	QString outputFilePath;

	if (meta.productType == ProductType::Raster)
	{
		// 栅格数据：优先从 BLOB（Large Object）导出
		int rasterOid = meta.fileOid;

		// 老数据 fileOid 可能为 0，从栅格元数据表回退查询
		if (rasterOid <= 0 && !meta.layerTableName.isEmpty())
		{
			QVariantMap row = db->executeQueryOne(
				QString("SELECT file_oid FROM \"%1\" LIMIT 1").arg(meta.layerTableName));
			if (!row.isEmpty())
				rasterOid = row.value("file_oid").toInt();
		}

		if (rasterOid > 0)
		{
			outputFilePath = outputDir + "/" + meta.productName + "." + meta.fileFormat.toLower();

			// 检查目标文件是否已存在
			if (QFile::exists(outputFilePath))
			{
				OverwriteChoice choice = confirmOverwrite(outputFilePath);
				if (choice == OverwriteCancel || choice == OverwriteNo)
					return false;
				QFile::remove(outputFilePath);
			}

			PGconn* conn = db->nativeConnection();
			bool exportOk = PostgisConnector::instance()->loExport(rasterOid, outputFilePath);
			if (!exportOk)
			{
				QgsMessageLog::logMessage(
					QString::fromUtf8("栅格 BLOB 导出失败: OID=%1, 错误=%2")
						.arg(rasterOid).arg(PostgisConnector::instance()->lastError()),
					QString::fromUtf8("成果仓"), Qgis::Warning);
				return false;
			}
		}
		else
		{
			QgsMessageLog::logMessage(
				QString::fromUtf8("栅格数据导出失败: BLOB 不可用 (%1)").arg(meta.productName),
				QString::fromUtf8("成果仓"), Qgis::Warning);
			return false;
		}
	}
	else if (meta.productType == ProductType::Vector)
	{
		// 矢量数据：从 PostGIS 图层表导出
		if (!meta.layerTableName.isEmpty())
		{
			// 构建 QgsDataSourceUri
			PGconn* conn = db->nativeConnection();
			QString host = QString::fromUtf8(PQhost(conn));
			QString port = QString::fromUtf8(PQport(conn));
			QString dbName = QString::fromUtf8(PQdb(conn));
			QString user = QString::fromUtf8(PQuser(conn));
			QString password = QString::fromUtf8(PQpass(conn));

			// 查询表实际所在的 schema
			QString actualSchema = db->searchPath();
			if (actualSchema.isEmpty()) actualSchema = "public";

			// PostgreSQL 中不加双引号建表时标识符自动转小写
			// 所以用 LOWER() 确保匹配
			QString findSchemaSql = QString(
				"SELECT table_schema, table_name FROM information_schema.tables "
				"WHERE LOWER(table_name) = LOWER('%1') LIMIT 1")
				.arg(meta.layerTableName);
			auto schemaResult = db->executeQueryOne(findSchemaSql);
			QString foundSchema = schemaResult.value("table_schema").toString();
			QString realTableName = schemaResult.value("table_name").toString();
			if (!foundSchema.isEmpty())
			{
				actualSchema = foundSchema;
			}
			if (!realTableName.isEmpty())
			{
			}

			QString effectiveTableName = realTableName.isEmpty() ? meta.layerTableName.toLower() : realTableName;

			QgsDataSourceUri dsUri;
			dsUri.setConnection(host, port, dbName, user, password);
			dsUri.setDataSource(actualSchema, effectiveTableName, "geom", QString(), QString());
			dsUri.setUseEstimatedMetadata(true);

			// 创建 QgsVectorLayer 加载 PostGIS 数据
			QgsVectorLayer::LayerOptions layerOptions;
			QgsVectorLayer* vectorLayer = new QgsVectorLayer(
				dsUri.uri(), meta.layerTableName, "YOUR_PASSWORD", layerOptions);

			if (!vectorLayer->isValid())
			{
				return false;
			}

			// 确定导出文件路径（不手动追加 .shp，由 QgsVectorFileWriter 根据驱动自动补全）
			QString vectorBasePath = outputDir + "/" + meta.productName;

			// 检查 SHP 文件是否已存在
			QString shpPath = vectorBasePath + ".shp";
			if (QFile::exists(shpPath))
			{
				OverwriteChoice choice = confirmOverwrite(shpPath);
				if (choice == OverwriteCancel || choice == OverwriteNo)
				{
					delete vectorLayer;
					return false;
				}
			}

			// 先删除旧文件
			QStringList exts = { ".shp", ".shx", ".dbf", ".prj", ".cpg", ".qpj" };
			for (const auto& ext : exts)
				QFile::remove(outputDir + "/" + meta.productName + ext);

			// 构建导出选项
			QgsVectorFileWriter::SaveVectorOptions options;
			options.driverName = "ESRI Shapefile";
			options.layerName = meta.layerTableName;
			options.fileEncoding = "UTF-8";
			options.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteFile;

			// 同步导出
			QgsCoordinateReferenceSystem crs = vectorLayer->crs();
			QgsCoordinateTransform ct;
			if (crs.isValid())
			{
				options.ct = ct;  // 无坐标转换
			}

			QString errorMsg;
			QString newFilename;
			QgsVectorFileWriter::WriterError writerError = QgsVectorFileWriter::writeAsVectorFormatV3(
				vectorLayer, vectorBasePath, vectorLayer->transformContext(), options, &errorMsg, &newFilename);

			delete vectorLayer;

			if (writerError != QgsVectorFileWriter::NoError)
			{
				return false;
			}

			// 使用 writeAsVectorFormatV3 返回的实际文件名（包含驱动自动追加的扩展名）
			outputFilePath = newFilename.isEmpty() ? vectorBasePath + ".shp" : newFilename;
		}
		else
		{
			QgsMessageLog::logMessage(
				QString::fromUtf8("矢量数据导出失败: 未入库 PostGIS (%1)").arg(meta.productName),
				QString::fromUtf8("成果仓"), Qgis::Warning);
			return false;
		}
	}
	else
	{
		// 其他类型（CAD, AI, CDR, PDF, Document, Archive, Other）：
		// 优先从 BLOB 导出，支持跨机器网络访问
		bool exported = false;

		if (meta.fileOid > 0)
		{
			// 从 PostgreSQL BLOB（Large Object）导出
			QString fileName;
			QFileInfo fi(meta.filePath);
			if (fi.fileName().isEmpty())
			{
				fileName = meta.productName + "." + meta.fileFormat.toLower();
			}
			else
			{
				fileName = fi.fileName();
			}
			outputFilePath = outputDir + "/" + fileName;

			// 检查目标文件是否已存在
			if (QFile::exists(outputFilePath))
			{
				OverwriteChoice choice = confirmOverwrite(outputFilePath);
				if (choice == OverwriteCancel || choice == OverwriteNo)
					return false;
				QFile::remove(outputFilePath);
			}

			PGconn* conn = db->nativeConnection();
			exported = PostgisConnector::instance()->loExport(meta.fileOid, outputFilePath);
			if (exported)
			{
				QgsMessageLog::logMessage(
					QString::fromUtf8("BLOB 导出成功: %1 (OID=%2)").arg(fileName).arg(meta.fileOid),
					QString::fromUtf8("成果仓"),
					Qgis::Info);
			}
			else
			{
				QgsMessageLog::logMessage(
					QString::fromUtf8("BLOB lo_export 失败: %1, OID=%2, 错误=%3")
						.arg(fileName).arg(meta.fileOid)
						.arg(PostgisConnector::instance()->lastError()),
					QString::fromUtf8("成果仓"),
					Qgis::Warning);
			}
		}
		else
		{
			QgsMessageLog::logMessage(
				QString::fromUtf8("导出失败: BLOB 不可用 (%1)").arg(meta.productName),
				QString::fromUtf8("成果仓"), Qgis::Critical);
			return false;
		}
	}

	return true;
}

void ProductSearchDialog::autoPreview(const ProductMetadata& meta)
{
	if (!mMapCanvas) return;

	// 清空当前画布
	mMapCanvas->setLayers(QList<QgsMapLayer*>());
	disconnect(mExtentConnection);
	delete mGraticule;
	mGraticule = nullptr;
	mMapCanvas->refresh();
	mMapCanvas->setVisible(false);

	// 仅对矢量和栅格类型支持地图画布预览，其他类型在用户信息栏左侧红色提示
	if (meta.productType != ProductType::Vector && meta.productType != ProductType::Raster)
	{
		if (mPreviewWarningLabel)
		{
			mPreviewWarningLabel->setText(QStringLiteral(
				"\u26A0 \u5F53\u524D\u4EA7\u54C1\u7C7B\u578B\u4E0D\u652F\u6301\u5728\u7EBF\u9884\u89C8\uFF0C\u8BF7\u5BFC\u51FA\u540E\u67E5\u770B"));
			mPreviewWarningLabel->setVisible(true);
		}
		return;
	}
	// 支持预览时隐藏警告
	if (mPreviewWarningLabel)
		mPreviewWarningLabel->setVisible(false);

	// 矢量必须有 layerTableName；栅格允许无 layerTableName（可从本地文件/BLOB 加载）
	if (meta.productType == ProductType::Vector && meta.layerTableName.isEmpty())
	{
	}

	// 获取 PostGIS 连接信息（矢量和栅格 BLOB 导出都需要）
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected())
	{
	}

	PGconn* pg = db->nativeConnection();
	if (!pg) return;

	QString host = QString::fromUtf8(PQhost(pg));
	QString port = QString::fromUtf8(PQport(pg));
	QString dbName = QString::fromUtf8(PQdb(pg));
	QString user = QString::fromUtf8(PQuser(pg));
	QString password = QString::fromUtf8(PQpass(pg));

	// 查询表实际所在的 schema（仅矢量需要）
	QString actualSchema = db->searchPath();
	if (actualSchema.isEmpty()) actualSchema = "public";
	QString effectiveTableName;

	if (meta.productType == ProductType::Vector)
	{
		QString findSchemaSql = QString(
			"SELECT table_schema, table_name FROM information_schema.tables "
			"WHERE LOWER(table_name) = LOWER('%1') LIMIT 1")
			.arg(meta.layerTableName);
		auto schemaResult = db->executeQueryOne(findSchemaSql);
		QString foundSchema = schemaResult.value("table_schema").toString();
		QString realTableName = schemaResult.value("table_name").toString();
		if (!foundSchema.isEmpty()) actualSchema = foundSchema;
		effectiveTableName = realTableName.isEmpty() ? meta.layerTableName.toLower() : realTableName;
	}

	if (meta.productType == ProductType::Vector)
	{
		QgsDataSourceUri dsUri;
		dsUri.setConnection(host, port, dbName, user, password);
		dsUri.setDataSource(actualSchema, effectiveTableName, "geom", QString(), "id");
		dsUri.setUseEstimatedMetadata(true);

		QgsVectorLayer* layer = new QgsVectorLayer(dsUri.uri(), meta.productName, "YOUR_PASSWORD");
		if (layer->isValid())
		{
			QgsProject::instance()->addMapLayer(layer, false);
			mMapCanvas->setLayers(QList<QgsMapLayer*>() << layer);
			mMapCanvas->setExtent(layer->extent());
			mMapCanvas->zoomToFullExtent();
			ensureGeographicCRS();
			mMapCanvas->setVisible(true);
			mMapCanvas->refresh();
			addGraticule();
		}
	}
	else if (meta.productType == ProductType::Raster)
	{
		// 栅格预览：先导出到 temp 目录，再加载本地文件
		// 文件名中包含 OID 或 fileHash，确保版本回退后不会使用旧缓存
		QString tempDir = QCoreApplication::applicationDirPath() + "/temp";
		QDir().mkpath(tempDir);

		// 老数据 fileOid 可能为 0，从栅格元数据表回退查询
		int rasterOid = meta.fileOid;
		if (rasterOid <= 0 && !meta.layerTableName.isEmpty())
		{
			QVariantMap row = db->executeQueryOne(
				QString("SELECT file_oid FROM \"%1\" LIMIT 1").arg(meta.layerTableName));
			if (!row.isEmpty())
				rasterOid = row.value("file_oid").toInt();
		}

		QString localRasterPath;

		if (rasterOid > 0)
		{
			// 使用 OID 作为缓存标识，避免版本回退后复用旧缓存
			localRasterPath = tempDir + "/" + meta.productName + "_oid" + QString::number(rasterOid) + "." + meta.fileFormat.toLower();
		}
		else if (!meta.filePath.isEmpty())
		{
			QFileInfo fi(meta.filePath);
			QString targetFileName = fi.fileName();
			if (targetFileName.isEmpty())
			{
				targetFileName = meta.productName + "." + meta.fileFormat.toLower();
			}
			localRasterPath = tempDir + "/" + targetFileName;
		}
		else
		{
		}

		if (QFile::exists(localRasterPath))
		{
			// 已缓存，直接使用
		}
		else
		{
			if (rasterOid > 0)
			{
				if (!PostgisConnector::instance()->loExport(rasterOid, localRasterPath))
				{
					return;
				}
			}
			else
			{
				if (!QFile::exists(meta.filePath))
				{
					return;
				}
				if (!QFile::copy(meta.filePath, localRasterPath))
				{
					return;
				}
			}
		}

		QgsRasterLayer* layer = new QgsRasterLayer(localRasterPath, meta.productName);
		if (layer->isValid())
		{
			QgsProject::instance()->addMapLayer(layer, false);
			mMapCanvas->setLayers(QList<QgsMapLayer*>() << layer);
			mMapCanvas->setExtent(layer->extent());
			mMapCanvas->zoomToFullExtent();
			ensureGeographicCRS();
			mMapCanvas->setVisible(true);
			mMapCanvas->refresh();
			addGraticule();
		}
	}
}

void ProductSearchDialog::clearPreview()
{
	if (mMapCanvas)
	{
		disconnect(mExtentConnection);
		delete mGraticule;
		mGraticule = nullptr;
		mMapCanvas->setLayers(QList<QgsMapLayer*>());
		mMapCanvas->refresh();
		mMapCanvas->setVisible(false);
	}
	if (mPreviewWarningLabel)
		mPreviewWarningLabel->setVisible(false);
}

void ProductSearchDialog::addGraticule()
{
	if (!mMapCanvas) return;

	delete mGraticule;
	mGraticule = new GraticuleItem(mMapCanvas);

	// 将经纬网 rect 设置为一个足够大的范围，确保覆盖整个画布
	QgsRectangle extent = mMapCanvas->mapSettings().visibleExtent();
	mGraticule->setRect(extent);
	mGraticule->updatePosition();

	// 连接画布范围变化信号，确保漫游/缩放时经纬网实时刷新
	mExtentConnection = connect(mMapCanvas, &QgsMapCanvas::extentsChanged, this, [this]() {
		if (mGraticule && mMapCanvas)
		{
			QgsRectangle newExtent = mMapCanvas->mapSettings().visibleExtent();
			mGraticule->setRect(newExtent);
			mGraticule->updatePosition();
			mGraticule->update();
		}
	});
}

void ProductSearchDialog::ensureGeographicCRS()
{
	if (!mMapCanvas) return;

	// 获取画布当前图层列表
	QList<QgsMapLayer*> layers = mMapCanvas->layers();
	if (layers.isEmpty()) return;

	// 取第一个图层的 CRS 作为数据源坐标系
	QgsCoordinateReferenceSystem srcCrs = layers.first()->crs();
	if (!srcCrs.isValid()) return;

	// 如果已经是地理坐标系（经纬度），无需转换
	if (srcCrs.isGeographic())
	{
	}

	// 目标坐标系：EPSG:4490 (CGCS2000 地理坐标系)
	QgsCoordinateReferenceSystem dstCrs = QgsCoordinateReferenceSystem::fromEpsgId(4490);
	if (!dstCrs.isValid())
	{
	}

	// 设置画布的目标 CRS，QGIS 会自动将图层从原始 CRS 重投影到 EPSG:4490
	mMapCanvas->setDestinationCrs(dstCrs);

	// 重新设置 extent：需要将原始 extent 转换到目标 CRS
	QgsCoordinateTransform transform(srcCrs, dstCrs, QgsProject::instance()->transformContext());
	QgsRectangle srcExtent = mMapCanvas->extent();
	QgsRectangle dstExtent;
	try
	{
		dstExtent = transform.transformBoundingBox(srcExtent);
	}
	catch (...)
	{
	}

	mMapCanvas->setExtent(dstExtent);
	mMapCanvas->refresh();
}
