#include "gdb_layer_selector_dialog.h"
#include "GeneratedFiles/Release/ui_gdb_layer_selector_dialog.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "cpl_conv.h"

#include <QMessageBox>
#include <QListWidgetItem>
// ==================== 构造 / 析构 ====================

GDBLayerSelectorDialog::GDBLayerSelectorDialog(const QString& gdbPath, QWidget* parent)
	: QDialog(parent)
	, ui(new Ui::GDBLayerSelectorDialog)
{
	ui->setupUi(this);

	// 连接信号
	connect(ui->mSelectAllBtn, &QPushButton::clicked, this, &GDBLayerSelectorDialog::onSelectAll);
	connect(ui->mDeselectAllBtn, &QPushButton::clicked, this, &GDBLayerSelectorDialog::onDeselectAll);
	connect(ui->mOkBtn, &QPushButton::clicked, this, &GDBLayerSelectorDialog::onOk);
	connect(ui->mCancelBtn, &QPushButton::clicked, this, &QDialog::reject);

	// 确保 GDAL 已注册
	static bool gdalRegistered = false;
	if (!gdalRegistered)
	{
		GDALAllRegister();
		gdalRegistered = true;
	}

	populateLayers(gdbPath);
}

GDBLayerSelectorDialog::~GDBLayerSelectorDialog()
{
	delete ui;
}

// ==================== 公共方法 ====================

QStringList GDBLayerSelectorDialog::selectedLayers() const
{
	QStringList result;
	for (int i = 0; i < ui->mLayerListWidget->count(); ++i)
	{
		QListWidgetItem* item = ui->mLayerListWidget->item(i);
		if (item && item->checkState() == Qt::Checked && i < mAllLayerNames.size())
		{
			result << mAllLayerNames[i];
		}
	}


	return result;
}

QStringList GDBLayerSelectorDialog::allLayerNames(const QString& gdbPath)
{
	QStringList names;
	GDALDataset* poDS = static_cast<GDALDataset*>(
		GDALOpenEx(gdbPath.toUtf8().constData(),
				   GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
	if (!poDS) return names;

	int nLayers = poDS->GetLayerCount();
	for (int i = 0; i < nLayers; ++i)
	{
		OGRLayer* poLayer = poDS->GetLayer(i);
		if (poLayer)
		{
			const char* pszName = poLayer->GetName();
			if (pszName)
				names << QString::fromUtf8(pszName);
		}
	}
	GDALClose(poDS);
	return names;
}

// ==================== 私有槽 ====================

void GDBLayerSelectorDialog::onSelectAll()
{
	for (int i = 0; i < ui->mLayerListWidget->count(); ++i)
	{
		QListWidgetItem* item = ui->mLayerListWidget->item(i);
		if (item)
			item->setCheckState(Qt::Checked);
	}
}

void GDBLayerSelectorDialog::onDeselectAll()
{
	for (int i = 0; i < ui->mLayerListWidget->count(); ++i)
	{
		QListWidgetItem* item = ui->mLayerListWidget->item(i);
		if (item)
			item->setCheckState(Qt::Unchecked);
	}
}

void GDBLayerSelectorDialog::onOk()
{
	if (selectedLayers().isEmpty())
	{
		QMessageBox::warning(this, "提示", "请至少选择一个图层进行入库。");
		return;
	}
	accept();
}

// ==================== 私有方法 ====================

void GDBLayerSelectorDialog::populateLayers(const QString& gdbPath)
{

	GDALDataset* poDS = static_cast<GDALDataset*>(
		GDALOpenEx(gdbPath.toUtf8().constData(),
				   GDAL_OF_VECTOR, nullptr, nullptr, nullptr));

	if (!poDS)
	{
		QString gdalError = QString::fromUtf8(CPLGetLastErrorMsg());

		QMessageBox::critical(this, "错误",
			QString("无法打开 GDB 文件:\n%1\n\nGDAL 错误: %2")
				.arg(gdbPath, gdalError));
		return;
	}

	int nLayers = poDS->GetLayerCount();

	// 获取 GDB 驱动名称确认格式
	QString driverName = QString::fromUtf8(poDS->GetDriverName());

	if (nLayers == 0)
	{

		QMessageBox::information(this, "提示", "该 GDB 文件中没有找到矢量图层。");
		GDALClose(poDS);
		return;
	}

	// 遍历所有图层，添加到列表中
	for (int i = 0; i < nLayers; ++i)
	{
		OGRLayer* poLayer = poDS->GetLayer(i);
		if (!poLayer)
		{
			continue;
		}

		QString layerName = QString::fromUtf8(poLayer->GetName());
		OGRwkbGeometryType eGeomType = poLayer->GetGeomType();
		QString geomTypeName = QString::fromUtf8(OGRGeometryTypeToName(eGeomType));

		// 尝试获取要素数量（GDB可能返回 -1）
		GIntBig rawCount = poLayer->GetFeatureCount(false);
		int featureCount = (rawCount >= 0) ? static_cast<int>(rawCount) : -1;


		// 构建显示文本
		QString displayText;
		if (featureCount >= 0)
			displayText = QString("%1  [%2, %3 要素]")
				.arg(layerName, geomTypeName)
				.arg(featureCount);
		else
			displayText = QString("%1  [%2]")
				.arg(layerName, geomTypeName);

		QListWidgetItem* item = new QListWidgetItem(displayText);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(Qt::Checked); // 默认全选
		ui->mLayerListWidget->addItem(item);

		mAllLayerNames << layerName;
	}

	GDALClose(poDS);


	// 设置对话框标题显示图层数
	setWindowTitle(QString("选择要入库的GDB图层 (%1 个图层)").arg(nLayers));
}
