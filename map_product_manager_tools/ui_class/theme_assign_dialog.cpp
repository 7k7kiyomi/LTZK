#include "theme_assign_dialog.h"
#include "database/postgis_connector.h"
#include <QMessageBox>
#include <QPushButton>
#include <QListWidgetItem>
#include <QFont>

ThemeAssignDialog::ThemeAssignDialog(const QList<ProductMetadata>& products, QWidget* parent)
	: QDialog(parent)
	, mProducts(products)
{
	ui.setupUi(this);

	// 数据清单为只读展示，统一使用微软雅黑 9 号字
	{
		QFont listFont = ui.mDataList->font();
		listFont.setFamily(QStringLiteral("微软雅黑"));
		listFont.setPointSize(9);
		ui.mDataList->setFont(listFont);
	}

	connect(ui.mOkBtn, &QPushButton::clicked, this, &ThemeAssignDialog::onOk);
	connect(ui.mCancelBtn, &QPushButton::clicked, this, &ThemeAssignDialog::reject);

	fillDataList();
}

ThemeAssignDialog::~ThemeAssignDialog()
{
}

void ThemeAssignDialog::setDefaultContentName(const QString& name)
{
	mDefaultContentName = name.trimmed();
	if (!mDefaultContentName.isEmpty())
		ui.mContentNameEdit->setText(mDefaultContentName);
}

void ThemeAssignDialog::showEvent(QShowEvent* event)
{
	QDialog::showEvent(event);
	loadThemeCombo();
	// 主题名称默认取右键节点名称（用户未手动输入时）
	if (ui.mContentNameEdit->text().trimmed().isEmpty() && !mDefaultContentName.isEmpty())
		ui.mContentNameEdit->setText(mDefaultContentName);
}

void ThemeAssignDialog::fillDataList()
{
	ui.mDataList->clear();
	for (const auto& p : mProducts)
	{
		auto* item = new QListWidgetItem(p.productName, ui.mDataList);
		item->setToolTip(QStringLiteral("数据ID：%1\n格式：%2\n类型：%3")
			.arg(p.id)
			.arg(p.fileFormat.toUpper())
			.arg(productTypeToString(p.productType)));
	}
	ui.mDataCountLabel->setText(tr("将加入以下 %1 个数据：").arg(mProducts.size()));
}

void ThemeAssignDialog::loadThemeCombo()
{
	ui.mThemeCombo->clear();
	const auto themes = mDAO.getAllThemes();
	for (const auto& t : themes)
		ui.mThemeCombo->addItem(t.displayName(), t.id);
	if (themes.isEmpty())
		ui.mThemeCombo->addItem(tr("（暂无主题，请先在「主题管理」中添加主题）"), -1);
}

void ThemeAssignDialog::onOk()
{
	if (mProducts.isEmpty())
	{
		QMessageBox::warning(this, tr("添加到主题"), tr("所选节点下没有可加入主题的数据。"));
		return;
	}

	const int themeId = ui.mThemeCombo->currentData().toInt();
	if (themeId <= 0)
	{
		QMessageBox::warning(this, tr("添加到主题"),
			tr("请先在「目标主题」下拉列表中选择主题；\n若列表为空，请先在「主题管理」中添加主题。"));
		return;
	}

	const QString contentName = ui.mContentNameEdit->text().trimmed();
	if (contentName.isEmpty())
	{
		QMessageBox::warning(this, tr("添加到主题"), tr("请输入主题名称。"));
		ui.mContentNameEdit->setFocus();
		return;
	}

	// 收集本次要加入的产品（按 ID 去重）
	QStringList ids;
	QStringList names;
	for (const auto& p : mProducts)
	{
		const QString id = QString::number(p.id);
		if (ids.contains(id)) continue;
		ids << id;
		names << p.productName;
	}
	if (ids.isEmpty())
	{
		QMessageBox::warning(this, tr("添加到主题"), tr("所选节点下没有可加入主题的数据。"));
		return;
	}

	// 目标主题下若已存在同名主题内容，合并其已有数据（询问后追加）
	int existContentId = -1;
	for (const auto& c : mDAO.getContentsByTheme(themeId))
	{
		if (c.contentName != contentName) continue;
		existContentId = c.id;
		for (int i = 0; i < c.productIds.size(); ++i)
		{
			const QString id = c.productIds.at(i).trimmed();
			if (id.isEmpty() || ids.contains(id)) continue;
			ids << id;
			names << (i < c.productNames.size() && !c.productNames.at(i).trimmed().isEmpty()
				? c.productNames.at(i).trimmed() : tr("数据 %1").arg(id));
		}
		break;
	}

	if (existContentId > 0)
	{
		QMessageBox box(QMessageBox::Question, tr("添加到主题"),
			tr("主题「%1」下已存在同名主题内容「%2」，是否将所选数据追加到其中？")
				.arg(ui.mThemeCombo->currentText(), contentName),
			QMessageBox::Yes | QMessageBox::No, this);
		box.setButtonText(QMessageBox::Yes, tr("追加"));
		box.setButtonText(QMessageBox::No, tr("取消"));
		if (box.exec() != QMessageBox::Yes)
			return;
	}

	ThemeContentRecord content;
	content.id = existContentId;
	content.themeId = themeId;
	content.contentName = contentName;
	content.productIds = ids;
	content.productNames = names;
	content.productIdsStr = ids.join(QStringLiteral(","));
	content.productNamesStr = names.join(QStringLiteral(","));
	content.createdBy = gCurrentUserSession.userName;

	const bool ok = (existContentId > 0)
		? mDAO.updateContent(content)
		: (mDAO.insertContent(content) > 0);
	if (!ok)
	{
		QMessageBox::warning(this, tr("添加到主题"),
			tr("主题内容写入失败，请查看日志（可能数据库未连接）。"));
		return;
	}

	mThemeId = themeId;
	mContentName = contentName;
	mThemeDisplayName = ui.mThemeCombo->currentText();
	accept();
}
