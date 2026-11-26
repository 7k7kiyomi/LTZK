#ifndef VERSION_DIFF_DIALOG_H
#define VERSION_DIFF_DIALOG_H

#include <QDialog>
#include "database/product_metadata.h"
#include "ui_version_diff_dialog.h"

class ProductDAO;

/**
 * @brief 版本回溯与差异比对对话框
 * 
 * 实现全版本修改记录的查看、版本回溯、版本间差异比对功能。
 */
class VersionDiffDialog : public QDialog
{
	Q_OBJECT

public:
	explicit VersionDiffDialog(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~VersionDiffDialog() override;

signals:
	/// 版本回退完成信号，通知外部对话框刷新数据
	void rollbackCompleted(int productId);

private slots:
	void onProductSelected();
	void onVersionSelected();
	void onRollback();
	void onCompareVersions();
	void onRefreshHistory();

private:
	void setupConnections();
	void setupTableColumns();
	void loadProductList();
	void loadVersionHistory(int productId);
	void showVersionDetail(const VersionRecord& record);
	void performDiff(int productId, int versionA, int versionB);
	QString generateDiffReport(const VersionRecord& v1, const VersionRecord& v2);
	
	// 数据
	QList<ProductMetadata> mProducts;
	QList<VersionRecord> mVersions;
	int mCurrentProductId = -1;
	int mCurrentVersionNumber = -1;
	
	ProductDAO* mDAO = nullptr;

	Ui::VersionDiffDialog ui;
};

#endif // VERSION_DIFF_DIALOG_H
