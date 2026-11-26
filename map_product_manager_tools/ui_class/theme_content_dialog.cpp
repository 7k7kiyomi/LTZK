#include "theme_content_dialog.h"
#include "database/postgis_connector.h"
#include "database/schema_manager.h"
#include "core/directory_helper.h"
#include "core/layer_icon_helper.h"
#include <QMessageBox>
#include <QPushButton>
#include <QEvent>
#include <QMouseEvent>
#include <QSettings>
#include <QTreeWidgetItem>
#include <QApplication>
#include <QStyle>
#include <QFont>
#include <functional>

ThemeContentDialog::ThemeContentDialog(QWidget* parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	ui.mDataTree->setColumnCount(1);
	ui.mDataTree->setSelectionMode(QAbstractItemView::SingleSelection);
	ui.mDataTree->setExpandsOnDoubleClick(true);
	// 表头"地图成果分类"，与地图服务单「关联地图成果」的树保持一致
	{
		QFont hf = ui.mDataTree->header()->font();
		hf.setFamily(QStringLiteral("微软雅黑"));
		hf.setPointSize(9);
		ui.mDataTree->header()->setFont(hf);
	}

	// 双击"连接数据库"按钮连接数据库
	ui.mConnectBtn->installEventFilter(this);

	connect(ui.mOkBtn, &QPushButton::clicked, this, &ThemeContentDialog::onOk);
	connect(ui.mCancelBtn, &QPushButton::clicked, this, &ThemeContentDialog::reject);
	connect(ui.mSelectAllBtn, &QPushButton::clicked, this, &ThemeContentDialog::onSelectAll);
	connect(ui.mClearAllBtn, &QPushButton::clicked, this, &ThemeContentDialog::onClearAll);
	connect(ui.mDataTree, &QTreeWidget::itemChanged,
		this, &ThemeContentDialog::onDataTreeItemChanged);
}

ThemeContentDialog::~ThemeContentDialog()
{
}

void ThemeContentDialog::showEvent(QShowEvent* event)
{
	QDialog::showEvent(event);
	loadThemeCombo();
}

bool ThemeContentDialog::eventFilter(QObject* obj, QEvent* event)
{
	if (obj == ui.mConnectBtn)
	{
		if (event->type() == QEvent::MouseButtonDblClick)
		{
			onConnectDatabase();
			return true;
		}
		// 单击仅提示（需求要求双击连接）
		if (event->type() == QEvent::MouseButtonRelease)
		{
			auto* me = static_cast<QMouseEvent*>(event);
			if (me && me->button() == Qt::LeftButton)
			{
				ui.mConnStatusLabel->setText(tr("提示：请双击「连接数据库」按钮连接数据库"));
				ui.mConnStatusLabel->setStyleSheet(QStringLiteral("color: rgb(180, 120, 30);"));
				return true;
			}
		}
	}
	return QDialog::eventFilter(obj, event);
}

void ThemeContentDialog::loadThemeCombo()
{
	ui.mThemeCombo->clear();
	const auto themes = mThemeDAO.getAllThemes();
	for (const auto& t : themes)
	{
		ui.mThemeCombo->addItem(t.displayName(), t.id);
	}
	if (themes.isEmpty())
	{
		ui.mThemeCombo->addItem(tr("（暂无主题，请先在主题管理中添加主题）"), -1);
	}
}

void ThemeContentDialog::onConnectDatabase()
{
	auto* db = PostgisConnector::instance();
	if (!db) return;

	QSettings settings("GarMap", "MapProductManager");
	const QString host = settings.value("db/host", "localhost").toString();
	const int port = settings.value("db/port", 5432).toInt();
	const QString dbName = settings.value("db/database", "map_products").toString();
	const QString schema = settings.value("db/schema", "public").toString();
	const QString user = settings.value("db/user", "YOUR_PASSWORD").toString();
	const QString password = settings.value("db/password", "").toString();

	if (host.isEmpty() || dbName.isEmpty() || user.isEmpty())
	{
		QMessageBox::warning(this, tr("连接数据库"),
			tr("未找到数据库连接配置，请先通过「数据库连接配置」菜单配置连接参数。"));
		return;
	}

	if (!schema.isEmpty() && schema != "public")
		db->setSearchPath(schema);

	if (db->isConnected())
	{
		ui.mConnStatusLabel->setText(tr("已连接：%1").arg(dbName));
		ui.mConnStatusLabel->setStyleSheet(QStringLiteral("color: rgb(30, 120, 60);"));
		loadThemeCombo();
		buildDataTree();
		return;
	}

	if (!db->connect(host, port, dbName, user, password))
	{
		QMessageBox::warning(this, tr("连接数据库"),
			tr("数据库连接失败：%1").arg(db->lastError()));
		ui.mConnStatusLabel->setText(tr("连接失败"));
		return;
	}

	// 连接成功后初始化表结构（确保主题表存在）
	SchemaManager schemaMgr;
	schemaMgr.initializeSchema();

	ui.mConnStatusLabel->setText(tr("已连接：%1").arg(dbName));
	ui.mConnStatusLabel->setStyleSheet(QStringLiteral("color: rgb(30, 120, 60);"));

	loadThemeCombo();
	buildDataTree();
}

void ThemeContentDialog::buildDataTree()
{
	ui.mDataTree->clear();

	// 幂等创建固定目录节点，确保树结构与成果存储一致
	auto ids = DirectoryHelper::ensureFixedDirectories(mProductDAO);
	mProductLeafDirIds = {
		ids.resultAI, ids.resultPDF, ids.resultOther,
		ids.elementImage, ids.elementShading,
		ids.materialDoc, ids.materialTable, ids.materialOther
	};

	QSettings settings;
	QString dbName = settings.value("db/database", QStringLiteral("数据库")).toString();
	if (dbName.isEmpty())
		dbName = QStringLiteral("数据库");

	auto* root = new QTreeWidgetItem(ui.mDataTree);
	root->setText(0, dbName);
	root->setData(0, Qt::UserRole, 0);
	root->setData(0, Qt::UserRole + 1, 0);
	root->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirHomeIcon));
	root->setFlags(root->flags() | Qt::ItemIsUserCheckable);
	root->setCheckState(0, Qt::Unchecked);
	root->setExpanded(true);

	populateDataTreeChildren(root, 0);

	ui.mDataTree->expandAll();
}

void ThemeContentDialog::populateDataTreeChildren(QTreeWidgetItem* parentItem, int parentDirId)
{
	QStyle* style = QApplication::style();
	const auto children = mProductDAO.getChildDirectories(parentDirId);
	for (const auto& child : children)
	{
		auto* ci = new QTreeWidgetItem(parentItem);
		ci->setText(0, child.name);
		ci->setData(0, Qt::UserRole, child.id);
		ci->setData(0, Qt::UserRole + 1, 0); // 0=目录节点
		// 图层节点（nodeType==1）：按产品类型显示矢量点/线/面或栅格图标；其余目录统一文件夹图标
		if (child.nodeType == 1)
		{
			const auto layerProducts = mProductDAO.getProductsByDirectory(child.id);
			int kind = -1;
			for (const auto& p : layerProducts)
			{
				kind = LayerIconHelper::layerIconKindForProduct(p);
				if (kind >= 0) break;
			}
			ci->setIcon(0, kind >= 0 ? LayerIconHelper::drawLayerIcon(kind)
				: style->standardIcon(QStyle::SP_FileIcon));
		}
		else
		{
			ci->setIcon(0, style->standardIcon(QStyle::SP_DirIcon));
		}
		// 所有目录节点可勾选（勾选后收集其下全部数据）
		ci->setFlags(ci->flags() | Qt::ItemIsUserCheckable);
		ci->setCheckState(0, Qt::Unchecked);

		// 固定叶子目录（AI/PDF/其它、影像/晕渲、文档资料/表格资料/其它）下展示产品数据子节点
		if (mProductLeafDirIds.contains(child.id))
		{
			const auto products = mProductDAO.getProductsByDirectory(child.id);
			for (const auto& p : products)
			{
				auto* pi = new QTreeWidgetItem(ci);
				pi->setText(0, p.productName);
				pi->setData(0, Qt::UserRole, p.id);
				pi->setData(0, Qt::UserRole + 1, 1); // 1=产品数据节点
				pi->setToolTip(0, QStringLiteral("数据名称: %1\n密级: %2")
					.arg(p.productName, securityLevelToString(p.securityLevel)));
				pi->setFlags(pi->flags() | Qt::ItemIsUserCheckable);
				pi->setCheckState(0, Qt::Unchecked);
				int kind = LayerIconHelper::layerIconKindForProduct(p);
				pi->setIcon(0, kind >= 0 ? LayerIconHelper::drawLayerIcon(kind)
					: style->standardIcon(QStyle::SP_FileIcon));
			}
		}

		populateDataTreeChildren(ci, child.id);
	}
}

void ThemeContentDialog::collectCheckedProducts(QTreeWidgetItem* item, QStringList& ids, QStringList& names)
{
	if (!item) return;
	if (item->checkState(0) == Qt::Checked)
	{
		if (item->data(0, Qt::UserRole + 1).toInt() == 1) // 产品数据节点：直接收集
		{
			const QString id = item->data(0, Qt::UserRole).toString();
			if (!ids.contains(id))
			{
				ids << id;
				names << item->text(0);
			}
		}
		else // 目录节点：收集其下直接产品
		{
			const int dirId = item->data(0, Qt::UserRole).toInt();
			if (dirId > 0)
			{
				const auto prods = mProductDAO.getProductsByDirectory(dirId);
				for (const auto& p : prods)
				{
					const QString id = QString::number(p.id);
					if (!ids.contains(id))
					{
						ids << id;
						names << p.productName;
					}
				}
			}
		}
	}
	for (int i = 0; i < item->childCount(); ++i)
		collectCheckedProducts(item->child(i), ids, names);
}

void ThemeContentDialog::onDataTreeItemChanged(QTreeWidgetItem* item, int column)
{
	if (mUpdatingCheck || !item || column != 0) return;
	// 勾选/取消父节点时，同步其所有子节点（与地图服务单「关联地图成果」一致）
	mUpdatingCheck = true;
	const Qt::CheckState state = item->checkState(0);
	std::function<void(QTreeWidgetItem*)> apply = [&](QTreeWidgetItem* it) {
		for (int i = 0; i < it->childCount(); ++i)
		{
			it->child(i)->setCheckState(0, state);
			apply(it->child(i));
		}
	};
	apply(item);
	mUpdatingCheck = false;
}

void ThemeContentDialog::onSelectAll()
{
	mUpdatingCheck = true;
	std::function<void(QTreeWidgetItem*)> apply = [&](QTreeWidgetItem* item) {
		item->setCheckState(0, Qt::Checked);
		for (int i = 0; i < item->childCount(); ++i)
			apply(item->child(i));
	};
	for (int i = 0; i < ui.mDataTree->topLevelItemCount(); ++i)
		apply(ui.mDataTree->topLevelItem(i));
	mUpdatingCheck = false;
}

void ThemeContentDialog::onClearAll()
{
	mUpdatingCheck = true;
	std::function<void(QTreeWidgetItem*)> apply = [&](QTreeWidgetItem* item) {
		item->setCheckState(0, Qt::Unchecked);
		for (int i = 0; i < item->childCount(); ++i)
			apply(item->child(i));
	};
	for (int i = 0; i < ui.mDataTree->topLevelItemCount(); ++i)
		apply(ui.mDataTree->topLevelItem(i));
	mUpdatingCheck = false;
}

void ThemeContentDialog::onOk()
{
	auto* db = PostgisConnector::instance();
	if (!db || !db->isConnected())
	{
		QMessageBox::warning(this, tr("添加主题内容"),
			tr("数据库未连接，请双击「连接数据库」按钮连接数据库。"));
		return;
	}

	const int themeId = ui.mThemeCombo->currentData().toInt();
	if (themeId <= 0)
	{
		QMessageBox::warning(this, tr("添加主题内容"),
			tr("请先在「主题类别」下拉列表中选择主题。"));
		return;
	}

	const QString contentName = ui.mContentNameEdit->text().trimmed();
	if (contentName.isEmpty())
	{
		QMessageBox::warning(this, tr("添加主题内容"),
			tr("请在「主题名称」文本框中为这组数据输入主题名称。"));
		ui.mContentNameEdit->setFocus();
		return;
	}

	QStringList ids, names;
	for (int i = 0; i < ui.mDataTree->topLevelItemCount(); ++i)
		collectCheckedProducts(ui.mDataTree->topLevelItem(i), ids, names);
	if (ids.isEmpty())
	{
		QMessageBox::warning(this, tr("添加主题内容"),
			tr("请展开数据目录，勾选需要加入主题的数据。"));
		return;
	}

	ThemeContentRecord content;
	content.themeId = themeId;
	content.contentName = contentName;
	content.productIds = ids;
	content.productNames = names;
	content.productIdsStr = ids.join(QStringLiteral(","));
	content.productNamesStr = names.join(QStringLiteral(","));
	content.createdBy = gCurrentUserSession.userName;

	const int newId = mThemeDAO.insertContent(content);
	if (newId <= 0)
	{
		QMessageBox::warning(this, tr("添加主题内容"), tr("主题内容添加失败，请查看日志。"));
		return;
	}

	content.id = newId;
	mContent = content;
	accept();
}
