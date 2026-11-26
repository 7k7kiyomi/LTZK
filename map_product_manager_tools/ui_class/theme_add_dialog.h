#ifndef THEME_ADD_DIALOG_H
#define THEME_ADD_DIALOG_H

#include "ui_theme_add_dialog.h"
#include "database/theme_dao.h"
#include <QDialog>

/**
 * @brief 添加主题对话框
 *
 * 选择"数据类别（矢量、栅格、AI文件等）"，
 * 在"主题类别"中输入新的主题类别或下拉选择已有的主题。
 */
class ThemeAddDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ThemeAddDialog(QWidget* parent = nullptr);
	~ThemeAddDialog();

	// 获取创建的主题（对话框 accepted 后有效）
	ThemeRecord getTheme() const { return mTheme; }

private slots:
	void onOk();

private:
	Ui::ThemeAddDialog ui;
	ThemeDAO mDAO;
	ThemeRecord mTheme;
};

#endif // THEME_ADD_DIALOG_H
