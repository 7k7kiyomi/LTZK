#include "theme_manager_dialog.h"
#include "theme_add_dialog.h"
#include "theme_content_dialog.h"
#include "database/product_dao.h"
#include "database/postgis_connector.h"
#include "core/layer_icon_helper.h"
#include "core/add_to_map_helper.h"
#include "core/export_data_helper.h"
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidgetItem>
#include <QApplication>
#include <QStyle>
#include <QMap>
#include <QMenu>
#include <QFileDialog>
#include "qgsgui.h"
#include "qgisinterface.h"
namespace {

// 树节点类型（存于 Qt::UserRole）
enum NodeType
{
	CategoryNode = 0,  // 数据类别（顶层）
	ThemeNode = 1,     // 主题
	ContentNode = 2,   // 主题内容名称
	DataNode = 3,      // 数据
};

QString esc(const QString& s)
{
	return s.toHtmlEscaped();
}

// 详情表格的一行（空值跳过）
QString kv(const QString& key, const QString& value)
{
	if (value.isEmpty())
		return QString();
	return QStringLiteral("<tr><td style=\"color:#777;white-space:nowrap;padding:2px 14px 2px 0;vertical-align:top;\">%1</td>"
		"<td style=\"padding:2px;vertical-align:top;\">%2</td></tr>")
		.arg(esc(key), esc(value));
}

QString sectionTitle(const QString& text)
{
	return QStringLiteral("<p style=\"font-weight:bold;color:#2a6fb0;margin:6px 0 2px 0;\">%1</p>")
		.arg(esc(text));
}

} // namespace

ThemeManagerDialog::ThemeManagerDialog(QgisInterface* iface, QWidget* parent, Qt::WindowFlags flags)
	: QDialog(parent, flags)
	, mQGisIface(iface)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);
	// 左右面板比例 3:7（主题目录 : 详情）
	ui.mSplitter->setSizes({ 294, 686 });
	ui.mThemeTree->setHeaderHidden(true);
	ui.mThemeTree->setColumnCount(1);
	ui.mThemeTree->setRootIsDecorated(true);
	ui.mThemeTree->setContextMenuPolicy(Qt::CustomContextMenu);

	ui.mDetailBrowser->setOpenExternalLinks(false);

	connect(ui.mAddThemeBtn, &QPushButton::clicked, this, &ThemeManagerDialog::onAddTheme);
	connect(ui.mDeleteThemeBtn, &QPushButton::clicked, this, &ThemeManagerDialog::onDeleteTheme);
	connect(ui.mRefreshBtn, &QPushButton::clicked, this, &ThemeManagerDialog::onRefresh);
	connect(ui.mAddContentBtn, &QPushButton::clicked, this, &ThemeManagerDialog::onAddContent);
	connect(ui.mThemeTree, &QTreeWidget::currentItemChanged,
		this, &ThemeManagerDialog::onThemeTreeCurrentItemChanged);
	connect(ui.mThemeTree, &QTreeWidget::customContextMenuRequested,
		this, &ThemeManagerDialog::onThemeTreeContextMenu);
}

ThemeManagerDialog::~ThemeManagerDialog()
{
}

void ThemeManagerDialog::showEvent(QShowEvent* event)
{
	QDialog::showEvent(event);
	loadThemeTree();
}

void ThemeManagerDialog::loadThemeTree()
{
	ui.mThemeTree->clear();
	mAllThemes = mDAO.getAllThemes();
	mAllContents.clear();

	QMap<QString, QTreeWidgetItem*> categoryNodes;
	ProductDAO productDAO;
	QStyle* style = QApplication::style();

	for (const auto& theme : mAllThemes)
	{
		// 第 0 级：数据类别
		const QString cat = theme.dataCategory.isEmpty() ? tr("未分类") : theme.dataCategory;
		QTreeWidgetItem* catItem = nullptr;
		if (categoryNodes.contains(cat))
		{
			catItem = categoryNodes.value(cat);
		}
		else
		{
			catItem = new QTreeWidgetItem(ui.mThemeTree);
			catItem->setText(0, cat);
			catItem->setIcon(0, style->standardIcon(QStyle::SP_DirHomeIcon));
			catItem->setData(0, Qt::UserRole, CategoryNode);
			catItem->setData(0, Qt::UserRole + 1, cat);
			catItem->setToolTip(0, tr("数据类别：%1").arg(cat));
			categoryNodes.insert(cat, catItem);
		}

		// 第 1 级：主题
		auto* themeItem = new QTreeWidgetItem(catItem);
		themeItem->setText(0, theme.name);
		themeItem->setIcon(0, style->standardIcon(QStyle::SP_DirIcon));
		themeItem->setData(0, Qt::UserRole, ThemeNode);
		themeItem->setData(0, Qt::UserRole + 1, theme.id);
		themeItem->setToolTip(0, tr("主题ID：%1").arg(theme.id));
		themeItem->setExpanded(true);

		// 第 2 级：主题内容名称；第 3 级：数据
		const auto contents = mDAO.getContentsByTheme(theme.id);
		for (const auto& c : contents)
		{
			auto* ci = new QTreeWidgetItem(themeItem);
			ci->setText(0, c.contentName.isEmpty() ? tr("未命名主题内容") : c.contentName);
			ci->setIcon(0, style->standardIcon(QStyle::SP_FileDialogNewFolder));
			ci->setData(0, Qt::UserRole, ContentNode);
			ci->setData(0, Qt::UserRole + 1, c.id);
			ci->setToolTip(0, tr("主题内容ID：%1").arg(c.id));
			ci->setExpanded(true);

			for (int i = 0; i < c.productIds.size(); ++i)
			{
				const int pid = c.productIds.at(i).toInt();
				ProductMetadata p = productDAO.getProduct(pid);
				auto* pi = new QTreeWidgetItem(ci);
				pi->setText(0, !p.productName.isEmpty() ? p.productName
					: (i < c.productNames.size() ? c.productNames.at(i) : QStringLiteral("数据 %1").arg(pid)));
				int kind = LayerIconHelper::layerIconKindForProduct(p);
				pi->setIcon(0, kind >= 0 ? LayerIconHelper::drawLayerIcon(kind)
					: style->standardIcon(QStyle::SP_FileIcon));
				pi->setData(0, Qt::UserRole, DataNode);
				pi->setData(0, Qt::UserRole + 1, pid);
				pi->setToolTip(0, tr("数据ID：%1").arg(pid));
			}
			mAllContents.append(c);
		}
	}

	ui.mThemeTree->expandAll();

	if (mAllThemes.isEmpty())
	{
		showEmptyDetail();
		ui.mStatusLabel->setText(tr("暂无主题，请点击「添加主题」创建"));
	}
	else
	{
		selectFirstThemeNode();
		ui.mStatusLabel->setText(tr("共 %1 个主题").arg(mAllThemes.size()));
	}
}

void ThemeManagerDialog::selectFirstThemeNode()
{
	for (int c = 0; c < ui.mThemeTree->topLevelItemCount(); ++c)
	{
		QTreeWidgetItem* cat = ui.mThemeTree->topLevelItem(c);
		for (int i = 0; i < cat->childCount(); ++i)
		{
			QTreeWidgetItem* t = cat->child(i);
			if (t->data(0, Qt::UserRole).toInt() == ThemeNode)
			{
				ui.mThemeTree->setCurrentItem(t);
				return;
			}
		}
	}
}

void ThemeManagerDialog::selectThemeNode(int themeId)
{
	for (int c = 0; c < ui.mThemeTree->topLevelItemCount(); ++c)
	{
		QTreeWidgetItem* cat = ui.mThemeTree->topLevelItem(c);
		for (int i = 0; i < cat->childCount(); ++i)
		{
			QTreeWidgetItem* t = cat->child(i);
			if (t->data(0, Qt::UserRole).toInt() == ThemeNode
				&& t->data(0, Qt::UserRole + 1).toInt() == themeId)
			{
				ui.mThemeTree->setCurrentItem(t);
				return;
			}
		}
	}
}

void ThemeManagerDialog::onThemeTreeCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* /*previous*/)
{
	if (!current)
	{
		showEmptyDetail();
		return;
	}

	switch (current->data(0, Qt::UserRole).toInt())
	{
	case CategoryNode:
		showCategoryDetail(current);
		break;
	case ThemeNode:
		showThemeDetail(current->data(0, Qt::UserRole + 1).toInt());
		break;
	case ContentNode:
		showContentDetail(current->data(0, Qt::UserRole + 1).toInt());
		break;
	case DataNode:
		showProductDetail(current->data(0, Qt::UserRole + 1).toInt());
		break;
	default:
		showEmptyDetail();
		break;
	}
}

void ThemeManagerDialog::showEmptyDetail()
{
	ui.mDetailBrowser->setHtml(
		QStringLiteral("<p style=\"color:#999;\">%1</p>")
		.arg(esc(tr("请在左侧目录树中选择节点查看详情"))));
}

void ThemeManagerDialog::showCategoryDetail(QTreeWidgetItem* categoryItem)
{
	const QString cat = categoryItem->data(0, Qt::UserRole + 1).toString();
	int themeCount = 0, contentCount = 0, dataCount = 0;
	for (int i = 0; i < categoryItem->childCount(); ++i)
	{
		QTreeWidgetItem* t = categoryItem->child(i);
		++themeCount;
		for (int j = 0; j < t->childCount(); ++j)
		{
			++contentCount;
			dataCount += t->child(j)->childCount();
		}
	}

	QString html = sectionTitle(tr("数据类别"));
	html += QStringLiteral("<table>");
	html += kv(tr("类别名称"), cat);
	html += kv(tr("主题数量"), QString::number(themeCount));
	html += kv(tr("主题内容数量"), QString::number(contentCount));
	html += kv(tr("数据数量"), QString::number(dataCount));
	html += QStringLiteral("</table>");
	ui.mDetailBrowser->setHtml(html);
}

void ThemeManagerDialog::showThemeDetail(int themeId)
{
	int contentCount = 0, dataCount = 0;
	for (const auto& c : mAllContents)
	{
		if (c.themeId == themeId)
		{
			++contentCount;
			dataCount += c.productIds.size();
		}
	}

	const ThemeRecord theme = mDAO.getTheme(themeId);
	QString html = sectionTitle(tr("主题信息"));
	html += QStringLiteral("<table>");
	html += kv(tr("主题名称"), theme.name);
	html += kv(tr("数据类别"), theme.dataCategory);
	html += kv(tr("创建人"), theme.createdBy);
	html += kv(tr("创建时间"), theme.createdAt.isValid()
		? theme.createdAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString());
	html += kv(tr("主题内容数"), QString::number(contentCount));
	html += kv(tr("数据总数"), QString::number(dataCount));
	html += QStringLiteral("</table>");
	ui.mDetailBrowser->setHtml(html);
}

void ThemeManagerDialog::showContentDetail(int contentId)
{
	const ThemeContentRecord* found = nullptr;
	for (const auto& c : mAllContents)
	{
		if (c.id == contentId)
		{
			found = &c;
			break;
		}
	}
	if (!found)
	{
		showEmptyDetail();
		return;
	}

	QString themeName;
	for (const auto& t : mAllThemes)
	{
		if (t.id == found->themeId)
		{
			themeName = t.name;
			break;
		}
	}

	QString html = sectionTitle(tr("主题内容"));
	html += QStringLiteral("<table>");
	html += kv(tr("内容名称"), found->contentName);
	html += kv(tr("所属主题"), themeName);
	html += kv(tr("数据数量"), QString::number(found->productIds.size()));
	html += kv(tr("创建人"), found->createdBy);
	html += kv(tr("创建时间"), found->createdAt.isValid()
		? found->createdAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString());
	html += QStringLiteral("</table>");

	if (!found->productIds.isEmpty())
	{
		html += sectionTitle(tr("数据列表"));
		html += QStringLiteral("<table>");
		for (int i = 0; i < found->productIds.size(); ++i)
		{
			QString nm = i < found->productNames.size()
				? found->productNames.at(i)
				: QStringLiteral("数据 %1").arg(found->productIds.at(i));
			html += kv(tr("数据 %1").arg(i + 1), nm);
		}
		html += QStringLiteral("</table>");
	}
	ui.mDetailBrowser->setHtml(html);
}

void ThemeManagerDialog::showProductDetail(int productId)
{
	ProductDAO productDAO;
	ProductMetadata p = productDAO.getProduct(productId);
	if (p.id <= 0)
	{
		ui.mDetailBrowser->setHtml(
			QStringLiteral("<p style=\"color:#999;\">%1</p>")
			.arg(esc(tr("未找到数据（ID: %1）").arg(productId))));
		return;
	}

	QString html = sectionTitle(tr("数据信息"));
	html += QStringLiteral("<table>");
	html += kv(tr("数据名称"), p.productName);
	html += kv(tr("数据唯一标识"), p.dataId);
	html += kv(tr("数据类型"), productTypeToString(p.productType));
	html += kv(tr("数据格式"), p.fileFormat);
	html += kv(tr("比例尺分母"), p.scale);
	html += kv(tr("密级"), securityLevelToString(p.securityLevel));
	html += kv(tr("几何类型"), p.geometryType);
	html += kv(tr("所在市州"), p.city);
	html += kv(tr("项目名称"), p.projectName);
	html += kv(tr("生产单位"), p.producer);
	html += kv(tr("创建人"), p.createdBy);
	html += kv(tr("创建时间"), p.createdAt.isValid()
		? p.createdAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString());
	html += kv(tr("数据描述"), p.description);
	html += QStringLiteral("</table>");
	ui.mDetailBrowser->setHtml(html);
}

void ThemeManagerDialog::onAddTheme()
{
	if (!PostgisConnector::instance() || !PostgisConnector::instance()->isConnected())
	{
		QMessageBox::warning(this, tr("添加主题"),
			tr("数据库未连接，请先在「数据库连接配置」中连接数据库。"));
		return;
	}

	ThemeAddDialog dlg(this);
	if (dlg.exec() != QDialog::Accepted)
		return;

	// 添加成功后刷新目录树并选中新主题
	loadThemeTree();
	selectThemeNode(dlg.getTheme().id);
}

void ThemeManagerDialog::onDeleteTheme()
{
	QTreeWidgetItem* cur = ui.mThemeTree->currentItem();
	if (!cur)
	{
		QMessageBox::information(this, tr("删除"),
			tr("请先在目录树中选择要删除的主题或主题内容。"));
		return;
	}

	const int type = cur->data(0, Qt::UserRole).toInt();
	if (type == ThemeNode)
	{
		const int themeId = cur->data(0, Qt::UserRole + 1).toInt();
		const QString name = cur->text(0);

		QMessageBox box(QMessageBox::Question, tr("删除主题"),
			tr("确定要删除主题「%1」吗？该主题下的所有主题内容将一并删除。").arg(name),
			QMessageBox::Yes | QMessageBox::No, this);
		box.setButtonText(QMessageBox::Yes, tr("是"));
		box.setButtonText(QMessageBox::No, tr("否"));
		if (box.exec() != QMessageBox::Yes)
			return;

		if (!mDAO.deleteTheme(themeId))
		{
			QMessageBox::warning(this, tr("删除主题"), tr("主题删除失败，请查看日志。"));
			return;
		}
		loadThemeTree();
		ui.mStatusLabel->setText(tr("已删除主题「%1」").arg(name));
	}
	else if (type == ContentNode)
	{
		const int contentId = cur->data(0, Qt::UserRole + 1).toInt();
		const QString name = cur->text(0);

		QMessageBox box(QMessageBox::Question, tr("删除主题内容"),
			tr("确定要删除主题内容「%1」及其关联数据吗？").arg(name),
			QMessageBox::Yes | QMessageBox::No, this);
		box.setButtonText(QMessageBox::Yes, tr("是"));
		box.setButtonText(QMessageBox::No, tr("否"));
		if (box.exec() != QMessageBox::Yes)
			return;

		if (!mDAO.deleteContent(contentId))
		{
			QMessageBox::warning(this, tr("删除主题内容"), tr("主题内容删除失败，请查看日志。"));
			return;
		}
		loadThemeTree();
		ui.mStatusLabel->setText(tr("已删除主题内容「%1」").arg(name));
	}
	else
	{
		QMessageBox::information(this, tr("删除"),
			tr("请选择要删除的主题或主题内容节点。"));
	}
}

void ThemeManagerDialog::onRefresh()
{
	loadThemeTree();
	ui.mStatusLabel->setText(tr("主题目录已刷新"));
}

void ThemeManagerDialog::onAddContent()
{
	if (!PostgisConnector::instance() || !PostgisConnector::instance()->isConnected())
	{
		QMessageBox::warning(this, tr("添加主题内容"),
			tr("数据库未连接，请先在「数据库连接配置」中连接数据库。"));
		return;
	}

	// 记录当前选中的主题节点（用于刷新后定位）
	int prevThemeId = -1;
	QTreeWidgetItem* cur = ui.mThemeTree->currentItem();
	if (cur)
	{
		QTreeWidgetItem* p = cur;
		while (p && p->data(0, Qt::UserRole).toInt() != ThemeNode)
			p = p->parent();
		if (p)
			prevThemeId = p->data(0, Qt::UserRole + 1).toInt();
	}

	ThemeContentDialog dlg(this);
	if (dlg.exec() != QDialog::Accepted)
		return;

	loadThemeTree();
	if (prevThemeId > 0)
		selectThemeNode(prevThemeId);
}

// ===================== 目录树右键菜单：添加到地图 / 导出数据 =====================

void ThemeManagerDialog::onThemeTreeContextMenu(const QPoint& pos)
{
	QTreeWidgetItem* item = ui.mThemeTree->itemAt(pos);
	if (!item)
		return;

	QList<ProductMetadata> products;
	collectProductsUnderItem(item, products);

	QMenu menu(this);
	QAction* addToMapAction = menu.addAction(tr("添加到地图"));
	QAction* exportAction = menu.addAction(tr("导出数据"));
	QAction* chosen = menu.exec(ui.mThemeTree->viewport()->mapToGlobal(pos));
	if (chosen == addToMapAction)
		addToMapProducts(products);
	else if (chosen == exportAction)
		exportProducts(products);
}

void ThemeManagerDialog::collectProductsUnderItem(QTreeWidgetItem* item, QList<ProductMetadata>& out) const
{
	if (!item)
		return;

	if (item->data(0, Qt::UserRole).toInt() == DataNode)
	{
		const int pid = item->data(0, Qt::UserRole + 1).toInt();
		ProductDAO dao;
		ProductMetadata meta = dao.getProduct(pid);
		if (meta.id > 0)
		{
			bool exists = false;
			for (const auto& m : out)
			{
				if (m.id == meta.id)
				{
					exists = true;
					break;
				}
			}
			if (!exists)
				out.append(meta);
		}
	}

	for (int i = 0; i < item->childCount(); ++i)
		collectProductsUnderItem(item->child(i), out);
}

void ThemeManagerDialog::addToMapProducts(const QList<ProductMetadata>& products)
{
	if (products.isEmpty())
	{
		QMessageBox::information(this, tr("添加到地图"), tr("该节点下没有可添加到地图的数据。"));
		return;
	}

	int success = 0;
	QStringList failed;
	if (products.size() == 1)
	{
		// 单条数据：与成果检索等入口行为一致（失败时弹窗说明原因）
		if (AddToMapHelper::addToMapWithFeedback(this, mQGisIface, products.first()))
			success = 1;
		else
			failed << products.first().productName;
	}
	else
	{
		// 批量：逐个静默加载，最后统一汇总
		for (const auto& meta : products)
		{
			if (AddToMapHelper::addLayerToMap(mQGisIface, meta))
				++success;
			else
				failed << meta.productName;
		}
	}

	if (products.size() > 1)
	{
		if (failed.isEmpty())
			QMessageBox::information(this, tr("添加到地图"), tr("已成功添加 %1 个图层到地图。").arg(success));
		else
			QMessageBox::warning(this, tr("添加到地图"),
				tr("成功 %1 个，失败 %2 个。").arg(success).arg(failed.size()));
	}
	else if (!failed.isEmpty())
	{
		// 单条失败：addToMapWithFeedback 已弹出具体原因，无需重复提示
	}
}

void ThemeManagerDialog::exportProducts(const QList<ProductMetadata>& products)
{
	if (products.isEmpty())
	{
		QMessageBox::information(this, tr("导出数据"), tr("该节点下没有可导出的数据。"));
		return;
	}

	const QString outputDir = QFileDialog::getExistingDirectory(this, tr("选择导出目录"));
	if (outputDir.isEmpty())
		return;

	QApplication::setOverrideCursor(Qt::WaitCursor);
	const int success = ExportDataHelper::exportProducts(this, products, outputDir);
	QApplication::restoreOverrideCursor();

	QMessageBox::information(this, tr("导出完成"),
		tr("导出完成。\n目标目录: %1\n成功: %2 个，失败: %3 个。")
			.arg(outputDir).arg(success).arg(products.size() - success));
}
