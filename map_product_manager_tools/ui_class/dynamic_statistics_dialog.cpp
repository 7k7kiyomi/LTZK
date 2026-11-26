#include "dynamic_statistics_dialog.h"
#include "ui_dynamic_statistics_dialog.h"
#include "database/service_dao.h"
#include "core/xlsx_writer.h"

#include <qgis.h>
#include <qgsmessagelog.h>
#include <QFileDialog>
#include <QMessageBox>
#include <QDate>
#include <QDateTime>
#include <QInputDialog>
#include <QLineEdit>
#include <QApplication>
#include <QPrinter>
#include <QPainter>
#include <QPixmap>
#include <QDesktopServices>
#include <QUrl>
#include <QPageSize>
#include <QPageLayout>
#include <QHeaderView>

// 构造/析构

DynamicStatisticsDialog::DynamicStatisticsDialog(QWidget* parent)
	: QDialog(parent)
	, ui(new Ui::DynamicStatisticsDialog)
	, m_dao(new ServiceDAO(this))
{
	ui->setupUi(this);
	initUI();
}

DynamicStatisticsDialog::~DynamicStatisticsDialog()
{
	delete ui;
}

// 初始化

void DynamicStatisticsDialog::initUI()
{
	// 窗口设置
	setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);
	resize(1200, 800);

	// 初始化指标树
	initMetricTree();

	// 初始化图件种类列表
	initMapCategoryList();

	// 统计表格样式
	ui->statsTable->horizontalHeader()->setStretchLastSection(true);
	ui->statsTable->verticalHeader()->setVisible(false);
	ui->statsTable->setAlternatingRowColors(true);

	// 连接信号 - 查询
	connect(ui->refreshBtn, &QPushButton::clicked, this, &DynamicStatisticsDialog::onRefreshClicked);

	// 连接信号 - 指标列选择
	connect(ui->selectAllMetricsBtn, &QPushButton::clicked, this, &DynamicStatisticsDialog::onSelectAllMetrics);
	connect(ui->deselectAllMetricsBtn, &QPushButton::clicked, this, &DynamicStatisticsDialog::onDeselectAllMetrics);

	// 连接信号 - 图件种类管理
	connect(ui->addCategoryBtn, &QPushButton::clicked, this, &DynamicStatisticsDialog::onAddCategory);
	connect(ui->editCategoryBtn, &QPushButton::clicked, this, &DynamicStatisticsDialog::onEditCategory);
	connect(ui->removeCategoryBtn, &QPushButton::clicked, this, &DynamicStatisticsDialog::onRemoveCategory);
	connect(ui->resetCategoryBtn, &QPushButton::clicked, this, &DynamicStatisticsDialog::onResetCategory);

	// 连接信号 - 导出
	connect(ui->exportExcelBtn, &QPushButton::clicked, this, &DynamicStatisticsDialog::onExportExcel);
	connect(ui->exportPdfBtn, &QPushButton::clicked, this, &DynamicStatisticsDialog::onExportPdf);
	connect(ui->exportPngBtn, &QPushButton::clicked, this, &DynamicStatisticsDialog::onExportPng);
}

// ---- 指标树初始化 ----

void DynamicStatisticsDialog::initMetricTree()
{
	auto* tree = ui->metricTree;
	tree->clear();

	// 按分类分组
	QMap<QString, QTreeWidgetItem*> groups;
	QStringList groupOrder = { QStringLiteral("服务统计"), QStringLiteral("用图统计"), QStringLiteral("经费") };

	for (const auto& groupName : groupOrder)
	{
		auto* groupItem = new QTreeWidgetItem(tree);
		groupItem->setText(0, groupName);
		groupItem->setFlags(groupItem->flags() | Qt::ItemIsUserCheckable);
		groupItem->setCheckState(0, Qt::Unchecked);
		groupItem->setExpanded(true);
		groups[groupName] = groupItem;
	}

	const auto metrics = StatMetrics::allMetrics();
	for (const auto& m : metrics)
	{
		auto* child = new QTreeWidgetItem();
		child->setText(0, m.displayName);
		child->setData(0, Qt::UserRole, m.key);  // 存储键名
		child->setData(0, Qt::UserRole + 1, m.category); // 存储分类
		child->setFlags(child->flags() | Qt::ItemIsUserCheckable);
		child->setCheckState(0, Qt::Unchecked);

		if (groups.contains(m.category))
		{
			groups[m.category]->addChild(child);
		}
	}

	// 默认全选
	onSelectAllMetrics();

	// 连接 item 勾选变化
	connect(tree, &QTreeWidget::itemChanged, this, &DynamicStatisticsDialog::onMetricItemChanged);
}

// ---- 图件种类列表初始化 ----

void DynamicStatisticsDialog::initMapCategoryList()
{
	loadMapCategories();
}

void DynamicStatisticsDialog::loadMapCategories()
{
	ui->mapCategoryList->clear();
	m_mapCategories = MapCategoryManager::load();

	for (const auto& item : m_mapCategories)
	{
		auto* listItem = new QListWidgetItem(item.name);
		listItem->setFlags(listItem->flags() | Qt::ItemIsUserCheckable);
		listItem->setCheckState(item.enabled ? Qt::Checked : Qt::Unchecked);
		ui->mapCategoryList->addItem(listItem);
	}
}

// 查询统计

DynamicStatisticsConfig DynamicStatisticsDialog::buildConfig() const
{
	DynamicStatisticsConfig config;

	// 时间粒度
	int granIdx = ui->granularityCombo->currentIndex();
	switch (granIdx)
	{
	case 0: config.granularity = QStringLiteral("year");    break;
	case 1: config.granularity = QStringLiteral("month");   break;
	case 2: config.granularity = QStringLiteral("quarter"); break;
	default: config.granularity = QStringLiteral("year");   break;
	}

	// 日期范围
	config.dateFrom = ui->dateFromEdit->date().toString("yyyy-MM-dd");
	config.dateTo = ui->dateToEdit->date().toString("yyyy-MM-dd");

	// 启用的统计列
	config.enabledColumns = getEnabledMetricKeys();

	// 图件种类
	config.mapCategories = m_mapCategories;

	return config;
}

QStringList DynamicStatisticsDialog::getEnabledMetricKeys() const
{
	QStringList keys;
	auto* tree = ui->metricTree;
	for (int i = 0; i < tree->topLevelItemCount(); ++i)
	{
		auto* group = tree->topLevelItem(i);
		for (int j = 0; j < group->childCount(); ++j)
		{
			auto* child = group->child(j);
			if (child->checkState(0) == Qt::Checked)
			{
				keys.append(child->data(0, Qt::UserRole).toString());
			}
		}
	}
	return keys;
}

void DynamicStatisticsDialog::populateStatsTable(const QList<DynamicStatsRow>& rows)
{
	auto* table = ui->statsTable;

	const QStringList colKeys = getEnabledMetricKeys();

	// 构建表头
	QStringList headers;
	headers << QStringLiteral("时间周期");

	// 构建指标键 -> 显示名映射
	QMap<QString, QString> keyToDisplay;
	const auto allMetrics = StatMetrics::allMetrics();
	for (const auto& m : allMetrics)
	{
		keyToDisplay[m.key] = m.displayName;
	}
	// 也查找自定义图件种类
	for (const auto& mc : m_mapCategories)
	{
		if (mc.enabled && colKeys.contains(mc.name))
			keyToDisplay[mc.name] = mc.name;
	}

	for (const QString& k : colKeys)
	{
		headers << keyToDisplay.value(k, k);
	}

	table->clear();
	table->setColumnCount(headers.size());
	table->setHorizontalHeaderLabels(headers);
	table->setRowCount(rows.size());

	for (int r = 0; r < rows.size(); ++r)
	{
		const auto& row = rows[r];

		// 时间周期列
		auto* timeItem = new QTableWidgetItem(getTimePeriodLabel(row.timePeriod));
		timeItem->setTextAlignment(Qt::AlignCenter);
		table->setItem(r, 0, timeItem);

		// 各指标列
		for (int c = 0; c < colKeys.size(); ++c)
		{
			const QString& key = colKeys[c];
			QVariant val = row.metrics.value(key, 0);
			QString displayVal;
			if (val.type() == QVariant::Double || val.type() == QVariant::Int ||
				val.type() == QVariant::LongLong || val.type() == QVariant::ULongLong)
			{
				displayVal = QString::number(val.toLongLong());
			}
			else
			{
				displayVal = val.toString();
			}

			auto* item = new QTableWidgetItem(displayVal);
			item->setTextAlignment(Qt::AlignCenter);
			table->setItem(r, c + 1, item);
		}
	}

	table->resizeColumnsToContents();
	table->horizontalHeader()->setStretchLastSection(true);
}

QString DynamicStatisticsDialog::getTimePeriodLabel(const QString& tp)
{
	// 推断显示标签
	if (tp.contains("-Q"))
	{
		// 季度：2024-Q1 → 2024年 第1季度
		QStringList parts = tp.split("-Q");
		return QString("%1年 第%2季度").arg(parts[0], parts[1]);
	}
	else if (tp.contains("-") && tp.length() == 7)
	{
		// 月份：2024-01 → 2024年01月
		QStringList parts = tp.split("-");
		return QString("%1年%2月").arg(parts[0], parts[1]);
	}
	else
	{
		// 年份：2024 → 2024年
		return tp + QStringLiteral("年");
	}
}

void DynamicStatisticsDialog::onRefreshClicked()
{
	// 验证至少选择一列
	auto keys = getEnabledMetricKeys();
	if (keys.isEmpty())
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请至少勾选一个统计指标列！"));
		return;
	}

	// 构建配置
	auto config = buildConfig();

	QgsMessageLog::logMessage(
		QStringLiteral("[动态统计] 点击刷新: 粒度=%1 指标数=%2 日期=%3~%4")
			.arg(config.granularity)
			.arg(keys.size())
			.arg(config.dateFrom.isEmpty() ? QStringLiteral("*") : config.dateFrom)
			.arg(config.dateTo.isEmpty() ? QStringLiteral("*") : config.dateTo),
		QStringLiteral("MapProductTools"), Qgis::Info);

	// 查询
	QApplication::setOverrideCursor(Qt::WaitCursor);
	auto rows = m_dao->getDynamicStatistics(config);
	QApplication::restoreOverrideCursor();

	QgsMessageLog::logMessage(
		QStringLiteral("[动态统计] 刷新完成: 返回 %1 行数据").arg(rows.size()),
		QStringLiteral("MapProductTools"), Qgis::Info);

	if (rows.isEmpty())
	{
		QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("当前条件下无统计数据。"));
		ui->statsTable->clear();
		ui->statsTable->setRowCount(0);
		return;
	}

	// 填充表格
	populateStatsTable(rows);
}

// 指标列选择

void DynamicStatisticsDialog::onMetricItemChanged(QTreeWidgetItem* item, int column)
{
	Q_UNUSED(column)
	if (!item) return;

	// 如果是组节点，同步子节点勾选状态
	if (item->childCount() > 0)
	{
		Qt::CheckState state = item->checkState(0);
		for (int i = 0; i < item->childCount(); ++i)
		{
			// 先断开信号避免递归
			disconnect(ui->metricTree, &QTreeWidget::itemChanged, this, &DynamicStatisticsDialog::onMetricItemChanged);
			item->child(i)->setCheckState(0, state);
			connect(ui->metricTree, &QTreeWidget::itemChanged, this, &DynamicStatisticsDialog::onMetricItemChanged);
		}
	}
}

void DynamicStatisticsDialog::onSelectAllMetrics()
{
	auto* tree = ui->metricTree;
	disconnect(tree, &QTreeWidget::itemChanged, this, &DynamicStatisticsDialog::onMetricItemChanged);

	for (int i = 0; i < tree->topLevelItemCount(); ++i)
	{
		auto* group = tree->topLevelItem(i);
		group->setCheckState(0, Qt::Checked);
		for (int j = 0; j < group->childCount(); ++j)
		{
			group->child(j)->setCheckState(0, Qt::Checked);
		}
	}

	connect(tree, &QTreeWidget::itemChanged, this, &DynamicStatisticsDialog::onMetricItemChanged);
}

void DynamicStatisticsDialog::onDeselectAllMetrics()
{
	auto* tree = ui->metricTree;
	disconnect(tree, &QTreeWidget::itemChanged, this, &DynamicStatisticsDialog::onMetricItemChanged);

	for (int i = 0; i < tree->topLevelItemCount(); ++i)
	{
		auto* group = tree->topLevelItem(i);
		group->setCheckState(0, Qt::Unchecked);
		for (int j = 0; j < group->childCount(); ++j)
		{
			group->child(j)->setCheckState(0, Qt::Unchecked);
		}
	}

	connect(tree, &QTreeWidget::itemChanged, this, &DynamicStatisticsDialog::onMetricItemChanged);
}

// 图件种类管理

void DynamicStatisticsDialog::onAddCategory()
{
	bool ok;
	QString name = QInputDialog::getText(this,
		QStringLiteral("添加图件种类"),
		QStringLiteral("请输入新的图件种类名称："),
		QLineEdit::Normal, QString(), &ok);
	if (!ok || name.trimmed().isEmpty()) return;
	name = name.trimmed();

	// 检查是否已存在
	for (const auto& item : m_mapCategories)
	{
		if (item.name == name)
		{
			QMessageBox::warning(this, QStringLiteral("提示"),
				QStringLiteral("图件种类 \"%1\" 已存在！").arg(name));
			return;
		}
	}

	// 添加（自定义图件种类没有直接对应的数据库列，标记 dbColumn 为空）
	MapCategoryItem newItem;
	newItem.name = name;
	newItem.dbColumn = QString();  // 需要用户手动配置数据库列映射
	newItem.enabled = true;
	m_mapCategories.append(newItem);

	// 保存到 QSettings
	MapCategoryManager::save(m_mapCategories);

	// 刷新列表
	loadMapCategories();

	QMessageBox::information(this, QStringLiteral("提示"),
		QStringLiteral("已添加图件种类 \"%1\"。\n注意：如需关联数据库列，请使用编辑功能设置对应字段。").arg(name));
}

void DynamicStatisticsDialog::onEditCategory()
{
	auto* current = ui->mapCategoryList->currentItem();
	if (!current)
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择要编辑的图件种类！"));
		return;
	}

	int idx = ui->mapCategoryList->currentRow();
	if (idx < 0 || idx >= m_mapCategories.size()) return;

	MapCategoryItem& cat = m_mapCategories[idx];

	bool ok;
	QString name = QInputDialog::getText(this,
		QStringLiteral("编辑图件种类"),
		QStringLiteral("名称："),
		QLineEdit::Normal, cat.name, &ok);
	if (!ok || name.trimmed().isEmpty()) return;
	cat.name = name.trimmed();

	// 可选：编辑数据库列映射
	QString dbCol = QInputDialog::getText(this,
		QStringLiteral("数据库列映射"),
		QStringLiteral("对应的数据库字段名（留空则仅作分类统计）："),
		QLineEdit::Normal, cat.dbColumn, &ok);
	if (ok)
		cat.dbColumn = dbCol.trimmed();

	// 同步启用状态
	cat.enabled = (current->checkState() == Qt::Checked);

	MapCategoryManager::save(m_mapCategories);
	loadMapCategories();
}

void DynamicStatisticsDialog::onRemoveCategory()
{
	auto* current = ui->mapCategoryList->currentItem();
	if (!current)
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择要删除的图件种类！"));
		return;
	}

	int idx = ui->mapCategoryList->currentRow();
	if (idx < 0 || idx >= m_mapCategories.size()) return;

	QString name = m_mapCategories[idx].name;
	auto reply = QMessageBox::question(this, QStringLiteral("确认删除"),
		QStringLiteral("确定删除图件种类 \"%1\" 吗？").arg(name),
		QMessageBox::Yes | QMessageBox::No);

	if (reply == QMessageBox::Yes)
	{
		m_mapCategories.removeAt(idx);
		MapCategoryManager::save(m_mapCategories);
		loadMapCategories();
	}
}

void DynamicStatisticsDialog::onResetCategory()
{
	auto reply = QMessageBox::question(this, QStringLiteral("确认恢复"),
		QStringLiteral("确定恢复为默认图件种类配置吗？\n所有自定义修改将被清除！"),
		QMessageBox::Yes | QMessageBox::No);

	if (reply == QMessageBox::Yes)
	{
		// 清除自定义配置
		QSettings settings;
		settings.remove(MapCategoryManager::settingsKey());

		m_mapCategories.clear();
		loadMapCategories();
	}
}

// 导出

QString DynamicStatisticsDialog::buildExportTitle() const
{
	QString gran;
	switch (ui->granularityCombo->currentIndex())
	{
	case 0: gran = QStringLiteral("按年统计"); break;
	case 1: gran = QStringLiteral("按月统计"); break;
	case 2: gran = QStringLiteral("按季度统计"); break;
	default: gran = QStringLiteral("统计"); break;
	}

	QString range = QStringLiteral("全部");
	if (ui->dateFromEdit->date() > QDate(2020, 1, 1) || ui->dateToEdit->date() < QDate(2099, 12, 31))
	{
		range = QStringLiteral("%1 ~ %2")
			.arg(ui->dateFromEdit->date().toString("yyyy-MM-dd"),
					ui->dateToEdit->date().toString("yyyy-MM-dd"));
	}

	return QStringLiteral("地图产品动态统计 (%1, %2)").arg(gran, range);
}

void DynamicStatisticsDialog::setupTableForExport(QTableWidget& table) const
{
	table.horizontalHeader()->setVisible(true);
	table.verticalHeader()->setVisible(false);
	table.resizeColumnsToContents();
}

void DynamicStatisticsDialog::onExportExcel()
{
	if (ui->statsTable->rowCount() == 0)
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("无统计结果可导出，请先查询！"));
		return;
	}

	QString defaultName = QStringLiteral("动态统计_%1.xlsx")
		.arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
	QString filePath = QFileDialog::getSaveFileName(this,
		QStringLiteral("导出 Excel"),
		QDir::homePath() + "/" + defaultName,
		QStringLiteral("Excel 文件 (*.xlsx)"));

	if (filePath.isEmpty()) return;

	QApplication::setOverrideCursor(Qt::WaitCursor);

	XlsxWriter xlsx;
	xlsx.setSheetName(QStringLiteral("动态统计"));

	// 标题行（合并）
	QString title = buildExportTitle();
	xlsx.writeRow({ title });

	// 表头
	QStringList headers;
	for (int c = 0; c < ui->statsTable->columnCount(); ++c)
	{
		headers << ui->statsTable->horizontalHeaderItem(c)->text();
	}
	xlsx.writeHeader(headers);

	// 数据行
	for (int r = 0; r < ui->statsTable->rowCount(); ++r)
	{
		QStringList row;
		for (int c = 0; c < ui->statsTable->columnCount(); ++c)
		{
			auto* item = ui->statsTable->item(r, c);
			row << (item ? item->text() : QString());
		}
		xlsx.writeRow(row);
	}

	// 合并标题行
	if (headers.size() > 1)
	{
		xlsx.mergeCells(1, 1, 1, headers.size());
	}

	// 设置列宽
	QVector<int> widths(headers.size(), 16);
	widths[0] = 20; // 时间周期列稍宽
	xlsx.setColumnWidths(widths);

	bool ok = xlsx.save(filePath);
	QApplication::restoreOverrideCursor();

	if (ok)
	{
		if (QMessageBox::question(this, QStringLiteral("导出成功"),
			QStringLiteral("Excel 文件已保存到：\n%1\n\n是否打开文件？").arg(filePath),
			QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
		{
			QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
		}
	}
	else
	{
		QMessageBox::critical(this, QStringLiteral("导出失败"), QStringLiteral("保存文件失败！"));
	}
}

void DynamicStatisticsDialog::onExportPdf()
{
	if (ui->statsTable->rowCount() == 0)
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("无统计结果可导出，请先查询！"));
		return;
	}

	QString defaultName = QStringLiteral("动态统计_%1.pdf")
		.arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
	QString filePath = QFileDialog::getSaveFileName(this,
		QStringLiteral("导出 PDF"),
		QDir::homePath() + "/" + defaultName,
		QStringLiteral("PDF 文件 (*.pdf)"));

	if (filePath.isEmpty()) return;

	QApplication::setOverrideCursor(Qt::WaitCursor);

	QPrinter printer(QPrinter::HighResolution);
	printer.setOutputFormat(QPrinter::PdfFormat);
	printer.setOutputFileName(filePath);
	printer.setPageSize(QPageSize(QPageSize::A3));
	printer.setPageOrientation(QPageLayout::Landscape);
	printer.setPageMargins(QMarginsF(10, 15, 10, 15), QPageLayout::Millimeter);

	QPainter painter;
	if (!painter.begin(&printer))
	{
		QApplication::restoreOverrideCursor();
		QMessageBox::critical(this, QStringLiteral("导出失败"), QStringLiteral("无法创建 PDF 文件！"));
		return;
	}

	QFont titleFont(QStringLiteral("Microsoft YaHei"), 21, QFont::Bold);
	QFont headerFont(QStringLiteral("Microsoft YaHei"), 15, QFont::Bold);
	QFont cellFont(QStringLiteral("Microsoft YaHei"), 14);

	int yOffset = 30;
	int xOffset = 30;

	// 标题
	painter.setFont(titleFont);
	painter.drawText(xOffset, yOffset, buildExportTitle());
	yOffset += 30;

	// 计算列宽
	auto* table = ui->statsTable;
	int cols = table->columnCount();
	int rows = table->rowCount();
	int pageWidth = printer.pageRect(QPrinter::DevicePixel).width() - 2 * xOffset;
	int baseColWidth = pageWidth / cols;

	// 表头
	painter.setFont(headerFont);
	for (int c = 0; c < cols; ++c)
	{
		QRect rect(xOffset + c * baseColWidth, yOffset, baseColWidth, 25);
		painter.fillRect(rect, QColor(200, 220, 240));
		painter.drawRect(rect);
		painter.drawText(rect.adjusted(4, 0, -4, 0), Qt::AlignCenter,
			table->horizontalHeaderItem(c) ? table->horizontalHeaderItem(c)->text() : "");
	}

	// 数据行
	yOffset += 25;
	painter.setFont(cellFont);
	for (int r = 0; r < rows; ++r)
	{
		for (int c = 0; c < cols; ++c)
		{
			QRect rect(xOffset + c * baseColWidth, yOffset + r * 22, baseColWidth, 22);

			// 交替行颜色
			if (r % 2 == 1)
			{
				painter.fillRect(rect, QColor(245, 245, 250));
			}
			painter.drawRect(rect);

			auto* item = table->item(r, c);
			painter.drawText(rect.adjusted(4, 0, -4, 0), Qt::AlignCenter,
				item ? item->text() : "");
		}
	}

	painter.end();
	QApplication::restoreOverrideCursor();

	if (QMessageBox::question(this, QStringLiteral("导出成功"),
		QStringLiteral("PDF 文件已保存到：\n%1\n\n是否打开文件？").arg(filePath),
		QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
	{
		QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
	}
}

void DynamicStatisticsDialog::onExportPng()
{
	if (ui->statsTable->rowCount() == 0)
	{
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("无统计结果可导出，请先查询！"));
		return;
	}

	QString defaultName = QStringLiteral("动态统计_%1.png")
		.arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
	QString filePath = QFileDialog::getSaveFileName(this,
		QStringLiteral("导出 PNG 图片"),
		QDir::homePath() + "/" + defaultName,
		QStringLiteral("PNG 图片 (*.png)"));

	if (filePath.isEmpty()) return;

	QApplication::setOverrideCursor(Qt::WaitCursor);

	// 计算表格所需尺寸
	auto* table = ui->statsTable;
	int totalWidth = table->verticalHeader()->width() + table->horizontalHeader()->length() + 40;
	int totalHeight = table->horizontalHeader()->height() +
		table->rowCount() * table->rowHeight(0) + 60;

	totalWidth = qMax(800, qMin(totalWidth, 3000));
	totalHeight = qMax(400, qMin(totalHeight, 5000));

	// 使用 QPixmap 渲染表格为图片
	QPixmap pixmap(totalWidth, totalHeight);
	pixmap.fill(Qt::white);

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);

	// 标题
	QFont titleFont(QStringLiteral("Microsoft YaHei"), 21, QFont::Bold);
	painter.setFont(titleFont);
	painter.setPen(Qt::black);
	painter.drawText(20, 30, buildExportTitle());

	// 表格渲染
	int xStart = 20, yStart = 55;
	int colCount = table->columnCount();
	int rowCount = table->rowCount();
	int colWidth = (totalWidth - 2 * xStart) / colCount;
	int rowHeight = 26;

	QFont headerFont(QStringLiteral("Microsoft YaHei"), 15, QFont::Bold);
	QFont cellFont(QStringLiteral("Microsoft YaHei"), 14);

	// 表头
	painter.setFont(headerFont);
	for (int c = 0; c < colCount; ++c)
	{
		QRect rect(xStart + c * colWidth, yStart, colWidth, rowHeight + 4);
		painter.fillRect(rect, QColor(52, 152, 219));
		painter.setPen(QPen(QColor(30, 90, 140), 1));
		painter.drawRect(rect);
		painter.setPen(Qt::white);
		painter.drawText(rect.adjusted(4, 0, -4, 0), Qt::AlignCenter,
			table->horizontalHeaderItem(c) ? table->horizontalHeaderItem(c)->text() : "");
	}

	// 数据行
	painter.setFont(cellFont);
	painter.setPen(Qt::black);
	for (int r = 0; r < rowCount; ++r)
	{
		for (int c = 0; c < colCount; ++c)
		{
			QRect rect(xStart + c * colWidth, yStart + rowHeight + 4 + r * rowHeight,
					   colWidth, rowHeight);

			// 交替行颜色
			if (r % 2 == 1)
			{
				painter.fillRect(rect, QColor(235, 245, 255));
			}
			painter.drawRect(rect);

			auto* item = table->item(r, c);
			painter.drawText(rect.adjusted(4, 0, -4, 0), Qt::AlignCenter,
				item ? item->text() : "");
		}
	}

	painter.end();

	bool ok = pixmap.save(filePath, "PNG");
	QApplication::restoreOverrideCursor();

	if (ok)
	{
		if (QMessageBox::question(this, QStringLiteral("导出成功"),
			QStringLiteral("PNG 图片已保存到：\n%1\n\n是否打开文件？").arg(filePath),
			QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
		{
			QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
		}
	}
	else
	{
		QMessageBox::critical(this, QStringLiteral("导出失败"), QStringLiteral("保存图片失败！"));
	}
}
