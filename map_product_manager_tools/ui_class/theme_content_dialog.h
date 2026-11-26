#ifndef THEME_CONTENT_DIALOG_H
#define THEME_CONTENT_DIALOG_H

#include "ui_theme_content_dialog.h"
#include "database/theme_dao.h"
#include "database/product_dao.h"
#include <QDialog>
#include <QSet>

class QTreeWidgetItem;

/**
 * @brief 添加主题内容对话框
 *
 * 双击"连接数据库"按钮连接数据库，展开数据目录勾选数据；
 * 在"主题类别"下拉选择主题，在"主题"文本框中输入主题名称，确定后完成添加。
 */
class ThemeContentDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ThemeContentDialog(QWidget* parent = nullptr);
	~ThemeContentDialog();

	// 获取添加的主题内容（对话框 accepted 后有效）
	ThemeContentRecord getContent() const { return mContent; }

protected:
	// 每次显示时刷新主题下拉
	void showEvent(QShowEvent* event) override;

	// 捕获"连接数据库"按钮的双击事件
	bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
	void onConnectDatabase();     // 双击连接数据库
	void onOk();
	void onSelectAll();           // 全选
	void onClearAll();            // 清空
	void onDataTreeItemChanged(QTreeWidgetItem* item, int column);  // 级联勾选

private:
	void loadThemeCombo();
	void buildDataTree();
	void populateDataTreeChildren(QTreeWidgetItem* parentItem, int parentDirId);
	void collectCheckedProducts(QTreeWidgetItem* item, QStringList& ids, QStringList& names);

	bool mUpdatingCheck = false;  // 级联勾选时防止信号递归
	Ui::ThemeContentDialog ui;
	ThemeDAO mThemeDAO;
	ProductDAO mProductDAO;
	ThemeContentRecord mContent;
	QSet<int> mProductLeafDirIds;   // 固定叶子目录（AI/PDF/其它、影像/晕渲、文档资料/表格资料/其它）
	QSet<int> mFixedDirIds;         // 固定分类目录
};

#endif // THEME_CONTENT_DIALOG_H
