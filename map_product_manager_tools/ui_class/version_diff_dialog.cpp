#include "version_diff_dialog.h"
#include "database/product_dao.h"
#include "core/file_storage_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QHeaderView>
#include <QMessageBox>
#include <QScrollArea>
#include <QDateTime>
#include <QFileInfo>
#include <algorithm>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "qgsgui.h"

VersionDiffDialog::VersionDiffDialog(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	mDAO = new ProductDAO(this);

	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	setupTableColumns();
	setupConnections();
	loadProductList();

	// 版本回退权限控制：操作员/管理员才可用，否则按钮置灰
	if (!gCurrentUserSession.canVersionDiff())
	{
		ui.mRollbackBtn->setEnabled(false);
		ui.mRollbackBtn->setToolTip(QStringLiteral("当前用户无版本回退权限，请联系管理员"));
	}

	setWindowTitle("版本回溯与差异比对");
	resize(1000, 700);
}

VersionDiffDialog::~VersionDiffDialog()
{
}

void VersionDiffDialog::setupTableColumns()
{
	ui.mVersionTable->setColumnCount(6);
	ui.mVersionTable->setHorizontalHeaderLabels({
		"版本号", "变更说明", "变更人", "文件大小", "变更时间", "父版本"
	});
	ui.mVersionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui.mVersionTable->horizontalHeader()->setStretchLastSection(true);
}

void VersionDiffDialog::setupConnections()
{
	connect(ui.mProductCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &VersionDiffDialog::onProductSelected);
	connect(ui.mVersionTable, &QTableWidget::cellClicked,
			this, &VersionDiffDialog::onVersionSelected);
	connect(ui.mCompareBtn, &QPushButton::clicked,
			this, &VersionDiffDialog::onCompareVersions);
	connect(ui.mRollbackBtn, &QPushButton::clicked,
			this, &VersionDiffDialog::onRollback);
	connect(ui.mRefreshBtn, &QPushButton::clicked,
			this, &VersionDiffDialog::onRefreshHistory);
}

void VersionDiffDialog::onProductSelected()
{
	int idx = ui.mProductCombo->currentIndex();
	if (idx < 0 || idx >= mProducts.size()) return;

	mCurrentProductId = mProducts[idx].id;
	loadVersionHistory(mCurrentProductId);
}

void VersionDiffDialog::onVersionSelected()
{
	int row = ui.mVersionTable->currentRow();
	if (row < 0 || row >= mVersions.size()) return;

	const auto& record = mVersions[row];
	mCurrentVersionNumber = record.versionNumber;
	showVersionDetail(record);
}

void VersionDiffDialog::onRollback()
{
	if (mCurrentProductId < 0 || mCurrentVersionNumber < 0)
		return;

	QString operatorName = gCurrentUserSession.userName;
	if (operatorName.isEmpty())
		operatorName = QStringLiteral("未知用户");

	mDAO->rollbackToVersion(mCurrentProductId, mCurrentVersionNumber, operatorName);
	loadVersionHistory(mCurrentProductId);
	emit rollbackCompleted(mCurrentProductId);
}

void VersionDiffDialog::onCompareVersions()
{
	if (mCurrentProductId < 0)
	{
		QMessageBox::warning(this, "提示", "请先选择产品");
		return;
	}

	int versionA = ui.mVersionACombo->currentText().toInt();
	int versionB = ui.mVersionBCombo->currentText().toInt();

	if (versionA == versionB)
	{
		QMessageBox::warning(this, "提示", "请选择两个不同的版本进行比对");
		return;
	}

	performDiff(mCurrentProductId, versionA, versionB);
}

void VersionDiffDialog::onRefreshHistory()
{
	loadProductList();
	if (mCurrentProductId > 0)
		loadVersionHistory(mCurrentProductId);
}

void VersionDiffDialog::loadProductList()
{
	mProducts = mDAO->getAllProducts();
	ui.mProductCombo->clear();
	for (const auto& meta : mProducts)
	{
		ui.mProductCombo->addItem(QString("[%1] %2").arg(meta.id).arg(meta.productName), meta.id);
	}
}

void VersionDiffDialog::loadVersionHistory(int productId)
{
	mVersions = mDAO->getVersionHistory(productId);

	// 检查版本1是否已存在于版本历史中，若不存在则从产品元数据补齐
	bool hasVersion1 = false;
	for (const auto& v : mVersions)
	{
		if (v.versionNumber == 1)
		{
			hasVersion1 = true;
			break;
		}
	}
	if (!hasVersion1)
	{
		for (const auto& product : mProducts)
		{
			if (product.id == productId)
			{
				VersionRecord initialVersion;
				initialVersion.productId = product.id;
				initialVersion.versionNumber = product.currentVersion;
				initialVersion.filePath = product.filePath;
				initialVersion.fileHash = product.fileHash;
				initialVersion.fileSize = product.fileSize;
				initialVersion.changeNote = QStringLiteral("初始版本");
				initialVersion.changedBy = product.createdBy;
				initialVersion.changedAt = product.createdAt.isValid() ? product.createdAt : QDateTime::currentDateTime();
				initialVersion.parentVersion = 0;  // 根版本，无父版本

				mVersions.append(initialVersion);
				break;
			}
		}
		// 按版本号降序排列
		std::sort(mVersions.begin(), mVersions.end(),
				  [](const VersionRecord& a, const VersionRecord& b) {
					  return a.versionNumber > b.versionNumber;
				  });
	}

	ui.mVersionTable->setRowCount(0);
	ui.mVersionCountLabel->setText(QString("版本历史 (共 %1 个版本)").arg(mVersions.size()));

	// 更新版本选择下拉框
	ui.mVersionACombo->clear();
	ui.mVersionBCombo->clear();

	for (int i = 0; i < mVersions.size(); ++i)
	{
		const auto& rec = mVersions[i];
		ui.mVersionTable->insertRow(i);
		ui.mVersionTable->setItem(i, 0, new QTableWidgetItem(QString::number(rec.versionNumber)));
		ui.mVersionTable->setItem(i, 1, new QTableWidgetItem(rec.changeNote));
		ui.mVersionTable->setItem(i, 2, new QTableWidgetItem(rec.changedBy));
		ui.mVersionTable->setItem(i, 3, new QTableWidgetItem(formatFileSize(rec.fileSize)));
		ui.mVersionTable->setItem(i, 4, new QTableWidgetItem(rec.changedAt.toString("yyyy-MM-dd hh:mm:ss")));
		ui.mVersionTable->setItem(i, 5, new QTableWidgetItem(rec.parentVersion > 0 ? QString::number(rec.parentVersion) : QString::fromUtf8("无")));

		ui.mVersionACombo->addItem(QString::number(rec.versionNumber));
		ui.mVersionBCombo->addItem(QString::number(rec.versionNumber));
	}

	// 默认选择最新版本和前一版本比对
	if (mVersions.size() >= 2)
	{
		ui.mVersionACombo->setCurrentIndex(0);
		ui.mVersionBCombo->setCurrentIndex(1);
	}
}

void VersionDiffDialog::showVersionDetail(const VersionRecord& record)
{
	// 从产品列表中查找 dataId，用于构建 product_storage 路径
	QString dataId;
	for (const auto& product : mProducts)
	{
		if (product.id == record.productId)
		{
			dataId = product.dataId;
			break;
		}
	}

	// 构建 product_storage 中的实际存储路径
	QString storageFilePath = record.filePath;
	if (!dataId.isEmpty())
	{
		FileStorageManager storageMgr;
		QString storageDir = storageMgr.getProductStoragePath(dataId, record.versionNumber);
		QFileInfo origFileInfo(record.filePath);
		storageFilePath = storageDir + "/" + origFileInfo.fileName();
	}

	QString html;
	html += "<div>";

	// ── 标题 ──
	html += "<div style='font-weight: bold; font-size: 28px; padding: 10px 0 4px 0; margin-bottom: 8px;'>"
			"【版本详情】</div>";

	// ── 信息表格 ──
	html += "<table style='border-collapse: separate; border-spacing: 0px 4px; width: 100%;'>";

	auto addInfoRow = [&](const QString& label, const QString& value) {
		html += "<tr>"
				"<td style='text-align: right; padding: 6px 12px 6px 0;"
				" white-space: nowrap; vertical-align: top; font-size: 26px;'>"
				+ label +
				"</td>"
				"<td style='padding: 6px 10px; font-size: 26px;'>"
				+ value +
				"</td>"
				"</tr>";
	};

	addInfoRow(QString::fromUtf8("版本号"), QString::number(record.versionNumber));
	addInfoRow(QString::fromUtf8("文件路径"), storageFilePath);
	addInfoRow(QString::fromUtf8("文件哈希"), record.fileHash);
	addInfoRow(QString::fromUtf8("文件大小"), formatFileSize(record.fileSize));
	addInfoRow(QString::fromUtf8("变更人"), record.changedBy);
	addInfoRow(QString::fromUtf8("变更时间"), record.changedAt.toString("yyyy-MM-dd hh:mm:ss"));
	addInfoRow(QString::fromUtf8("父版本"), record.parentVersion > 0 ? QString::number(record.parentVersion) : QString::fromUtf8("无"));

	html += "</table></div>";

	ui.mVersionDetailLabel->setText(html);
	ui.mChangeNoteEdit->setText(record.changeNote);
}

void VersionDiffDialog::performDiff(int productId, int versionA, int versionB)
{
	auto versions = mDAO->compareVersions(productId, versionA, versionB);

	if (versions.size() < 2)
	{
		ui.mDiffResultEdit->setText("无法获取版本信息进行比对");
		return;
	}

	QString report = generateDiffReport(versions[0], versions[1]);
	ui.mDiffResultEdit->setHtml(report);
}

QString VersionDiffDialog::generateDiffReport(const VersionRecord& v1, const VersionRecord& v2)
{
	QString html;
	html += "<h3>版本差异比对报告</h3>";
	html += "<table border='1' cellpadding='8' cellspacing='0' style='border-collapse:collapse; width:100%; font-size:20px;'>";

	auto addRow = [&](const QString& label, const QString& valA, const QString& valB) {
		html += QString("<tr>"
			"<td><b>%1</b></td>"
			"<td>版本 %2: %3</td>"
			"<td>版本 %4: %5</td>"
			"</tr>").arg(label)
			.arg(v1.versionNumber).arg(valA)
			.arg(v2.versionNumber).arg(valB);
	};

	addRow("文件哈希", v1.fileHash.left(16) + "...", v2.fileHash.left(16) + "...");
	addRow("文件大小", formatFileSize(v1.fileSize),
			formatFileSize(v2.fileSize));
	addRow("变更说明", v1.changeNote, v2.changeNote);
	addRow("变更人", v1.changedBy, v2.changedBy);
	addRow("变更时间", v1.changedAt.toString("yyyy-MM-dd hh:mm:ss"),
			v2.changedAt.toString("yyyy-MM-dd hh:mm:ss"));

	// 文件内容差异判断
	if (v1.fileHash != v2.fileHash)
	{
		html += "<tr><td colspan='3'>"
				"<b>⚠ 文件内容已变更</b>（文件哈希不同）</td></tr>";
	}
	else
	{
		html += "<tr><td colspan='3'>"
				"<b>✓ 文件内容未变更</b></td></tr>";
	}

	html += "</table>";
	return html;
}
