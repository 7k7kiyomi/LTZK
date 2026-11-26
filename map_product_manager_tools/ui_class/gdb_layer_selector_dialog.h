#pragma once

#include <QDialog>
#include <QStringList>

namespace Ui {
class GDBLayerSelectorDialog;
}

class GDBLayerSelectorDialog : public QDialog
{
	Q_OBJECT

public:
	explicit GDBLayerSelectorDialog(const QString& gdbPath, QWidget* parent = nullptr);
	~GDBLayerSelectorDialog();

	/// 返回用户勾选的图层名称列表
	QStringList selectedLayers() const;

	/// 枚举 GDB/MDB 中所有矢量图层名称（用于批量导入自动全选，不弹出对话框）
	static QStringList allLayerNames(const QString& gdbPath);

private slots:
	void onSelectAll();
	void onDeselectAll();
	void onOk();

private:
	void populateLayers(const QString& gdbPath);

	Ui::GDBLayerSelectorDialog* ui;
	QStringList mAllLayerNames;
};
