#ifndef THEME_ASSIGN_DIALOG_H
#define THEME_ASSIGN_DIALOG_H

#include "ui_theme_assign_dialog.h"
#include "database/theme_dao.h"
#include "database/product_metadata.h"
#include <QDialog>
#include <QList>

/**
 * @brief 添加到主题对话框
 *
 * 由"成果存储与管理"目录树右键"添加到主题"触发：
 * 展示待加入主题的数据清单，选择目标主题并输入主题名称，
 * 确定后把所选数据作为一个"主题内容"写入目标主题。
 * 若目标主题下已存在同名主题内容，则询问是否将数据追加到其中。
 */
class ThemeAssignDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ThemeAssignDialog(const QList<ProductMetadata>& products, QWidget* parent = nullptr);
	~ThemeAssignDialog();

	// 预填主题名称（默认取右键节点名称）
	void setDefaultContentName(const QString& name);

	// 以下结果在对话框 accepted 后有效
	int themeId() const { return mThemeId; }
	QString contentName() const { return mContentName; }
	QString themeDisplayName() const { return mThemeDisplayName; }

protected:
	void showEvent(QShowEvent* event) override;

private slots:
	void onOk();

private:
	void loadThemeCombo();
	void fillDataList();

	Ui::ThemeAssignDialog ui;
	ThemeDAO mDAO;
	QList<ProductMetadata> mProducts;
	QString mDefaultContentName;
	int mThemeId = -1;
	QString mContentName;
	QString mThemeDisplayName;
};

#endif // THEME_ASSIGN_DIALOG_H
