#include "theme_add_dialog.h"
#include "database/postgis_connector.h"
#include "database/product_metadata.h"
#include <QMessageBox>
#include <QPushButton>

ThemeAddDialog::ThemeAddDialog(QWidget* parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	// 下拉列出已有主题名称，可直接选择或输入新名称
	ui.mThemeNameCombo->addItems(mDAO.getAllThemeNames());

	connect(ui.mOkBtn, &QPushButton::clicked, this, &ThemeAddDialog::onOk);
	connect(ui.mCancelBtn, &QPushButton::clicked, this, &ThemeAddDialog::reject);
}

ThemeAddDialog::~ThemeAddDialog()
{
}

void ThemeAddDialog::onOk()
{
	// 数据库必须已连接
	if (!PostgisConnector::instance() || !PostgisConnector::instance()->isConnected())
	{
		QMessageBox::warning(this, tr("添加主题"),
			tr("数据库未连接，请先在数据库中连接数据库后再添加主题。"));
		return;
	}

	const QString name = ui.mThemeNameCombo->currentText().trimmed();
	if (name.isEmpty())
	{
		QMessageBox::warning(this, tr("添加主题"), tr("请输入或选择主题类别。"));
		ui.mThemeNameCombo->setFocus();
		return;
	}

	// 相同名称的主题已存在时提示（允许用户确认后继续，避免误添加）
	if (mDAO.isThemeNameExists(name))
	{
		QMessageBox::warning(this, tr("添加主题"),
			tr("主题类别「%1」已存在，请更换名称或直接选择已有主题。").arg(name));
		return;
	}

	ThemeRecord theme;
	theme.name = name;
	theme.dataCategory = ui.mDataCategoryCombo->currentText().trimmed();
	theme.createdBy = gCurrentUserSession.userName;
	int newId = mDAO.insertTheme(theme);
	if (newId <= 0)
	{
		QMessageBox::warning(this, tr("添加主题"), tr("主题添加失败，请查看日志。"));
		return;
	}

	theme.id = newId;
	mTheme = theme;
	accept();
}
