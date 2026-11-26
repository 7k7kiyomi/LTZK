#ifndef THEME_MANAGER_DIALOG_H
#define THEME_MANAGER_DIALOG_H

#include "ui_theme_manager_dialog.h"
#include "database/theme_dao.h"
#include "database/theme_record.h"
#include "database/product_metadata.h"
#include <QDialog>

class QTreeWidgetItem;
class QgisInterface;

/**
 * @brief 主题管理对话框
 *
 * 左侧为主题目录树：数据类别 → 主题 → 主题内容名称 → 数据；
 * 右侧为选中节点的详情面板。
 * 支持添加主题、删除主题/内容、刷新主题、添加主题内容；
 * 目录树节点右键支持"添加到地图"、"导出数据"。
 */
class ThemeManagerDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ThemeManagerDialog(QgisInterface* iface,
		QWidget* parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
	~ThemeManagerDialog();

protected:
	// 每次显示时刷新主题目录树
	void showEvent(QShowEvent* event) override;

private slots:
	void onAddTheme();
	void onDeleteTheme();
	void onRefresh();
	void onAddContent();
	void onThemeTreeCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
	void onThemeTreeContextMenu(const QPoint& pos);

private:
	void loadThemeTree();
	void selectFirstThemeNode();
	void selectThemeNode(int themeId);
	void showEmptyDetail();
	void showCategoryDetail(QTreeWidgetItem* categoryItem);
	void showThemeDetail(int themeId);
	void showContentDetail(int contentId);
	void showProductDetail(int productId);

	// 收集节点子树下的所有数据（去重）
	void collectProductsUnderItem(QTreeWidgetItem* item, QList<ProductMetadata>& out) const;
	// 将数据添加到地图 / 导出数据
	void addToMapProducts(const QList<ProductMetadata>& products);
	void exportProducts(const QList<ProductMetadata>& products);

	Ui::ThemeManagerDialog ui;
	QgisInterface* mQGisIface = nullptr;
	ThemeDAO mDAO;
	QList<ThemeRecord> mAllThemes;
	QList<ThemeContentRecord> mAllContents;
};

#endif // THEME_MANAGER_DIALOG_H
