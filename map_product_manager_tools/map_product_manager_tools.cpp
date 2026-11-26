#include "map_product_manager_tools.h"

/*------------QGIS include-----------------*/
#include "qgisinterface.h"
#include "qgsguiutils.h"
#include "qgsproject.h"
#include "qgsmessagebar.h"
#include "qgsmessagelog.h"
#include "qgsmapcanvas.h"
/*-------------Qt--------------------*/
#include <QAction>
#include <QMessageBox>
#include <QMenu>
#include <QApplication>
#include <QCoreApplication>
#include <QSettings>
#include <QDir>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

/*-------------内部UI模块---------------------*/
#include "ui_class/db_config_dialog.h"
#include "ui_class/product_storage_dialog.h"
#include "ui_class/metadata_manager_dialog.h"
#include "ui_class/product_search_dialog.h"
#include "ui_class/version_diff_dialog.h"
#include "ui_class/access_control_dialog.h"
#include "ui_class/map_service_dialog.h"
#include "ui_class/dynamic_statistics_dialog.h"
#include "ui_class/login_dialog.h"
#include "ui_class/data_import_wizard.h"
#include "ui_class/theme_manager_dialog.h"

/*-------------数据库模块---------------------*/
#include "database/postgis_connector.h"
#include "database/schema_manager.h"
#include "database/product_metadata.h"
#include "database/product_dao.h"

// ===================== 静态插件元数据 =====================
static const QString sName = QObject::tr("地图成果管理工具");
static const QString sDescription = QObject::tr("实现矢量、栅格、AI/CDR/PDF等多类型制图成果的集中存储、版本管控、元数据管理、检索预览、版本回溯与差异比对功能。");
static const QString sCategory = QObject::tr("地图成果");
static const QString sPluginVersion = QObject::tr("Version 1.0");
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;
static const QString sPluginIcon = QStringLiteral("");

CMapProductManagerPlugin* CMapProductManagerPlugin::s_instance = nullptr;

// ===================== 构造与析构 =====================
CMapProductManagerPlugin::CMapProductManagerPlugin(QgisInterface* qgisInterface)
	: QgisPlugin(sName, sDescription, sCategory, sPluginVersion, sPluginType)
	, mQGisIface(qgisInterface)
{
	s_instance = this;
}

CMapProductManagerPlugin::~CMapProductManagerPlugin()
{
	if (s_instance == this)
		s_instance = nullptr;
}

CMapProductManagerPlugin* CMapProductManagerPlugin::currentInstance()
{
	return s_instance;
}

// ===================== 辅助函数 =====================

/** @brief 显示权限检查拒绝提示 */
static void showAccessDenied(const QString& featureName, QgisInterface* iface)
{
	if (iface)
	{
		iface->messageBar()->pushWarning(
			QObject::tr("权限不足"),
			QObject::tr("您没有权限使用「%1」功能，请联系数据库管理员升级权限。").arg(featureName)
		);
	}
}

/** 未登录时弹出对话框提示进行身份验证 */
static void showLoginRequired(QgisInterface* iface)
{
	if (iface)
	{
		QMessageBox box(QMessageBox::Warning,
			QObject::tr("用户身份验证"),
			QObject::tr("请进行用户身份验证"),
			QMessageBox::Ok,
			iface->mainWindow());
		box.setButtonText(QMessageBox::Ok, QObject::tr("确定"));
		box.exec();
	}
}

static void synchronizePlatformUserSession()
{
	QCoreApplication* app = QCoreApplication::instance();
	if (!app)
		return;

	const QVariantMap sharedSession = app->property("ltzkSharedUserSession").toMap();
	if (sharedSession.isEmpty())
		return;

	gCurrentUserSession.clear();
	gCurrentUserSession.userName = sharedSession.value(QStringLiteral("userName")).toString();
	gCurrentUserSession.role = static_cast<AccessRole>(
		sharedSession.value(QStringLiteral("role"),
			static_cast<int>(AccessRole::DataOperator)).toInt());
	gCurrentUserSession.isLoggedIn = sharedSession.value(QStringLiteral("isLoggedIn")).toBool();
	gCurrentUserSession.isGuest = sharedSession.value(QStringLiteral("isGuest")).toBool();
}

static bool requireAccount(QgisInterface* iface)
{
	synchronizePlatformUserSession();
	if (gCurrentUserSession.isLoggedIn)
		return true;
	if (!gCurrentUserSession.isGuest)
	{
		showLoginRequired(iface);
		return false;
	}

	if (iface)
	{
		QMessageBox::information(
			iface->mainWindow(),
			QObject::tr("需要登录账户"),
			gCurrentUserSession.isGuest
				? QObject::tr("匿名模式无法连接数据库，请登录账户后使用此功能。")
				: QObject::tr("请先登录账户，再使用此功能。"));
	}
	return false;
}

/**
 * @brief 将主程序数据目录使用的手动连接同步到成果管理插件。
 *
 * 用户登录仍然由 database.ini/既有登录流程负责；这里只在数据目录发起
 * 成果操作时临时切换成果管理模块的数据连接，使两套 UI 使用同一连接参数。
 */
static bool syncExternalCatalogConnection(
	const QVariantMap& request,
	QgisInterface* iface)
{
	if (!requireAccount(iface))
		return false;

	const QVariantMap connection = request.value(QStringLiteral("connection")).toMap();
	if (connection.isEmpty())
		return PostgisConnector::instance()->isConnected();

	auto* db = PostgisConnector::instance();
	if (db->connect(
		connection.value(QStringLiteral("host")).toString().trimmed(),
		connection.value(QStringLiteral("port")).toInt(),
		connection.value(QStringLiteral("database")).toString().trimmed(),
		connection.value(QStringLiteral("user")).toString().trimmed(),
		connection.value(QStringLiteral("password")).toString()))
	{
		db->setSearchPath(connection.value(QStringLiteral("schema")).toString().trimmed());
		return true;
	}

	if (iface)
	{
		iface->messageBar()->pushWarning(
			QObject::tr("成果目录操作失败"),
			QObject::tr("无法连接数据列表当前数据库：%1").arg(db->lastError()));
	}
	return false;
}

/**
 * @brief 插件初始化时尝试从配置文件自动登录
 *
 * 从可执行程序所在 bin 目录同级目录的 config/user_login.ini 中读取 [user] 段的
 * user_name 与 password。若两个值均存在，则复用与登录对话框一致的认证逻辑
 * （本地 QSettings 凭据 → 数据库用户表）完成权限设置并填充全局用户会话，
 * 使各对话框右上角的用户信息栏同步显示用户名与角色。
 * 若 ini 文件不存在或字段为空，则不执行任何操作，保持原有"提示登录"逻辑。
 */
static void tryAutoLoginFromConfigFile()
{
	synchronizePlatformUserSession();
	if (gCurrentUserSession.isGuest)
		return;

	// 路径：可执行程序目录（bin）的上一级目录下的 config/user_login.ini
	const QString appDir = QCoreApplication::applicationDirPath();
	const QString iniPath = QDir(appDir).filePath(QStringLiteral("../config/user_login.ini"));

	QSettings ini(QDir::cleanPath(iniPath), QSettings::IniFormat);
	const QString userName = ini.value(QStringLiteral("user/user_name")).toString().trimmed();
	const QString password = ini.value(QStringLiteral("user/password")).toString().trimmed();

	// ini 中没有任何信息 → 保持现有逻辑不变，正常提示需要登录
	if (userName.isEmpty() || password.isEmpty())
		return;

	AccessRole role = AccessRole::DataOperator;
	bool authenticated = false;

	// 1) 本地认证：与 LoginDialog 一致，从 QSettings 读取用户凭据
	{
		QSettings settings(QStringLiteral("GarMap"), QStringLiteral("MapProductManager"));
		settings.beginGroup(QStringLiteral("users"));
		settings.beginGroup(userName);
		const QString storedHash = settings.value(QStringLiteral("passwordHash")).toString();
		const int roleInt = settings.value(QStringLiteral("role"),
			static_cast<int>(AccessRole::DataOperator)).toInt();
		settings.endGroup();
		settings.endGroup();

		if (!storedHash.isEmpty() && storedHash == hashPassword(password))
		{
			role = static_cast<AccessRole>(roleInt);
			authenticated = true;
		}
	}

	// 2) 本地无该用户凭据时，尝试数据库用户表认证
	if (!authenticated)
	{
		ProductDAO dao;
		UserPermission perm = dao.authenticateUser(userName, password);
		if (perm.id > 0)
		{
			role = perm.role;
			authenticated = true;
		}
	}

	if (!authenticated)
		return;  // 认证失败 → 保持未登录状态，正常提示登录

	// 认证成功：设置会话权限，各对话框用户信息栏同步显示
	gCurrentUserSession.userName = userName;
	gCurrentUserSession.role = role;
	gCurrentUserSession.isLoggedIn = true;

	QgsMessageLog::logMessage(
		QStringLiteral("[自动登录] 用户 %1 已通过 user_login.ini 认证，角色=%2")
			.arg(userName, accessRoleToString(role)),
		QStringLiteral("MapProduct"), Qgis::Info);
}

// ===================== initGui =====================
void CMapProductManagerPlugin::initGui()
{
	synchronizePlatformUserSession();

	// 防止重复初始化
	delete mActionLogin;
	delete mActionDbConfig;
	delete mActionProductStorage;
	delete mActionMetadataManager;
	delete mActionProductSearch;
	delete mActionVersionDiff;
	delete mActionAccessControl;
	delete mActionMapService;
	delete mActionDataCatalogOperation;
	delete mActionThemeManager;

	if (!mMainMenu)
	{
		mMainMenu = new QMenu(tr("&地图成果管理"), mQGisIface->mainWindow());
		mMainMenu->setObjectName(QStringLiteral("mMapProductMenu"));
		mQGisIface->addPluginToMenu(tr("&地图成果管理"), mMainMenu->menuAction());

		// --- 0. 用户登录 ---
		QIcon iconLogin(QStringLiteral(":/map_product_manager_tools/icon_login.svg"));
		mActionLogin = new QAction(iconLogin, tr("用户登录"), this);
		mActionLogin->setObjectName(QStringLiteral("mActionLogin"));
		mActionLogin->setWhatsThis(tr("登录成果库管理系统"));
		connect(mActionLogin, &QAction::triggered, this, &CMapProductManagerPlugin::UserLogin);
		mQGisIface->addToolBarIcon(mActionLogin);
		mMainMenu->addAction(mActionLogin);

		mMainMenu->addSeparator();

		// --- 1. 数据库连接配置 ---
		QIcon iconDbConfig(QStringLiteral(":/map_product_manager_tools/icon_db_config.svg"));
		mActionDbConfig = new QAction(iconDbConfig, tr("数据库连接配置"), this);
		mActionDbConfig->setObjectName(QStringLiteral("mActionDbConfig"));
		mActionDbConfig->setWhatsThis(tr("配置PostGIS数据库连接参数，测试连接并初始化数据库Schema"));
		connect(mActionDbConfig, &QAction::triggered, this, &CMapProductManagerPlugin::DbConfig);
		mMainMenu->addAction(mActionDbConfig);

		mMainMenu->addSeparator();

		// --- 1. 成果存储与管理 ---
		QIcon iconStorage(QStringLiteral(":/map_product_manager_tools/icon_product_storage.svg"));
		mActionProductStorage = new QAction(iconStorage, tr("成果存储与管理"), this);
		mActionProductStorage->setObjectName(QStringLiteral("mActionProductStorage"));
		mActionProductStorage->setWhatsThis(tr("上传与管理矢量、栅格、AI/CDR/PDF等多类型制图成果文件，支持版本管控"));
		connect(mActionProductStorage, &QAction::triggered, this, &CMapProductManagerPlugin::ProductStorage);
		mMainMenu->addAction(mActionProductStorage);
		mQGisIface->addToolBarIcon(mActionProductStorage);

		// --- 2. 元数据管理 ---
		QIcon iconMetadata(QStringLiteral(":/map_product_manager_tools/icon_metadata.svg"));
		mActionMetadataManager = new QAction(iconMetadata, tr("元数据管理"), this);
		mActionMetadataManager->setObjectName(QStringLiteral("mActionMetadataManager"));
		mActionMetadataManager->setWhatsThis(tr("自动提取成果元数据，支持人工补录比例尺、编制信息、审图号、密级等信息，支持多级目录与标签分类"));
		connect(mActionMetadataManager, &QAction::triggered, this, &CMapProductManagerPlugin::MetadataManager);
		mMainMenu->addAction(mActionMetadataManager);
		mQGisIface->addToolBarIcon(mActionMetadataManager);

		// --- 3. 成果检索与预览 ---
		QIcon iconSearch(QStringLiteral(":/map_product_manager_tools/icon_product_search.svg"));
		mActionProductSearch = new QAction(iconSearch, tr("成果检索与预览"), this);
		mActionProductSearch->setObjectName(QStringLiteral("mActionProductSearch"));
		mActionProductSearch->setWhatsThis(tr("多条件组合检索、缩略图在线预览、查询结果导出"));
		connect(mActionProductSearch, &QAction::triggered, this, &CMapProductManagerPlugin::ProductSearch);
		mMainMenu->addAction(mActionProductSearch);
		mQGisIface->addToolBarIcon(mActionProductSearch);

		// --- 4. 版本回溯与差异比对 ---
		QIcon iconDiff(QStringLiteral(":/map_product_manager_tools/icon_version_diff.svg"));
		mActionVersionDiff = new QAction(iconDiff, tr("版本回溯与差异比对"), this);
		mActionVersionDiff->setObjectName(QStringLiteral("mActionVersionDiff"));
		mActionVersionDiff->setWhatsThis(tr("查看全版本修改记录，支持版本回溯与差异比对"));
		connect(mActionVersionDiff, &QAction::triggered, this, &CMapProductManagerPlugin::VersionDiff);
		mMainMenu->addAction(mActionVersionDiff);
		mQGisIface->addToolBarIcon(mActionVersionDiff);

		// --- 5. 权限管理 ---
		QIcon iconAccess(QStringLiteral(":/map_product_manager_tools/icon_access_control.svg"));
		mActionAccessControl = new QAction(iconAccess, tr("权限管理"), this);
		mActionAccessControl->setObjectName(QStringLiteral("mActionAccessControl"));
		mActionAccessControl->setWhatsThis(tr("管理数据入库员和数据审核员账户，增删人员、设置初始密码"));
		connect(mActionAccessControl, &QAction::triggered, this, &CMapProductManagerPlugin::AccessControl);
		mMainMenu->addAction(mActionAccessControl);
		mQGisIface->addToolBarIcon(mActionAccessControl);

		mMainMenu->addSeparator();

		// --- 6. 地图服务管理 ---
		QIcon iconMapService(QStringLiteral(":/map_product_manager_tools/icon_map_service.svg"));
		mActionMapService = new QAction(iconMapService, tr("地图服务管理"), this);
		mActionMapService->setObjectName(QStringLiteral("mActionMapService"));
		mActionMapService->setWhatsThis(tr("电子化服务台账、动态统计用图指标、可视化分析报表、服务单填报导出与电子版存档管理"));
		connect(mActionMapService, &QAction::triggered, this, &CMapProductManagerPlugin::MapService);
		mMainMenu->addAction(mActionMapService);
		mQGisIface->addToolBarIcon(mActionMapService);

		// --- 7. 动态统计 ---
		//QIcon iconDynamicStats(QStringLiteral(":/map_product_manager_tools/icon_map_service.svg"));
		//mActionDynamicStats = new QAction(iconDynamicStats, tr("动态统计"), this);
		//mActionDynamicStats->setObjectName(QStringLiteral("mActionDynamicStats"));
		//mActionDynamicStats->setWhatsThis(tr("按年/月/季度灵活统计用图数量、图件种类、覆盖区域、服务数量等，支持导出Excel/PDF/PNG"));
		//connect(mActionDynamicStats, &QAction::triggered, this, &CMapProductManagerPlugin::DynamicStats);
		//mMainMenu->addAction(mActionDynamicStats);

		// --- 8. 数据导入（向导式）---
		QIcon iconDataImport(QStringLiteral(":/map_product_manager_tools/icon_product_storage.svg"));
		mActionDataImport = new QAction(iconDataImport, tr("数据导入"), this);
		mActionDataImport->setObjectName(QStringLiteral("mActionDataImport"));
		mActionDataImport->setWhatsThis(tr("向导式数据导入，支持矢量/栅格/制图文件入库，含元数据填写"));
		connect(mActionDataImport, &QAction::triggered, this, &CMapProductManagerPlugin::DataImport);
		mMainMenu->addAction(mActionDataImport);
		mQGisIface->addToolBarIcon(mActionDataImport);

		mMainMenu->addSeparator();

		// --- 9. 主题管理 ---
		QIcon iconTheme(QStringLiteral(":/map_product_manager_tools/icon_theme_manager.svg"));
		mActionThemeManager = new QAction(iconTheme, tr("主题管理"), this);
		mActionThemeManager->setObjectName(QStringLiteral("mActionThemeManager"));
		mActionThemeManager->setWhatsThis(tr("主题列表、主题设置（添加/删除/刷新主题）、主题内容管理（为数据挂接主题）"));
		connect(mActionThemeManager, &QAction::triggered, this, &CMapProductManagerPlugin::ThemeManager);
		mMainMenu->addAction(mActionThemeManager);
		mQGisIface->addToolBarIcon(mActionThemeManager);

		// 数据目录右键操作桥接动作：放在现有动作之后，避免改变既有回退索引；
		// 不展示在用户菜单中，仅供主程序按配置触发。
		mActionDataCatalogOperation = new QAction(QStringLiteral("成果目录操作"), this);
		mActionDataCatalogOperation->setObjectName(QStringLiteral("mActionDataCatalogOperation"));
		mActionDataCatalogOperation->setVisible(false);
		connect(mActionDataCatalogOperation, &QAction::triggered,
			this, &CMapProductManagerPlugin::DataCatalogOperation);
		mMainMenu->addAction(mActionDataCatalogOperation);
	}

	// 自动尝试使用保存的配置连接数据库
	auto* db = PostgisConnector::instance();
	if (!gCurrentUserSession.isGuest && !db->isConnected())
	{
		QSettings settings("GarMap", "MapProductManager");
		QString host = settings.value("db/host", "localhost").toString();
		int port = settings.value("db/port", 5432).toInt();
		QString dbName = settings.value("db/database", "map_products").toString();
		QString schema = settings.value("db/schema", "public").toString();
		QString user = settings.value("db/user", "YOUR_PASSWORD").toString();
		QString password = settings.value("db/password", "").toString();

		if (!host.isEmpty() && !dbName.isEmpty() && !user.isEmpty())
		{
			if (!schema.isEmpty() && schema != "public")
			{
				db->setSearchPath(schema);
			}
			if (db->connect(host, port, dbName, user, password))
			{
				SchemaManager schemaMgr;
				schemaMgr.initializeSchema();
			}
		}
	}

	updateActions();

	// 确保默认管理员账户存在（处理首次初始化与旧表迁移两种场景）
	if (!gCurrentUserSession.isGuest && db->isConnected())
	{
		ProductDAO dao;
		UserPermission adminPerm = dao.getUserPermission(QStringLiteral("admin"));
		if (adminPerm.id <= 0)
		{
			// 管理员账户不存在，创建默认账户
			dao.addUserWithPassword(QStringLiteral("admin"), QStringLiteral("123456"),
				AccessRole::DatabaseAdmin, QStringLiteral("system"));
		}
		else if (adminPerm.passwordHash.isEmpty())
		{
			// 管理员账户存在但密码为空（旧表迁移遗留），设置初始密码
			dao.changePassword(QStringLiteral("admin"), QStringLiteral("123456"));
		}
	}

	// 同步默认管理员凭据到本地存储（支持离线登录）
	{
		QSettings settings("GarMap", "MapProductManager");
		settings.beginGroup("users/admin");
		if (settings.value("passwordHash").toString().isEmpty())
		{
			settings.setValue("passwordHash", hashPassword("123456"));
			settings.setValue("role", static_cast<int>(AccessRole::DatabaseAdmin));
		}
		settings.endGroup();
	}

	// 初始化时尝试从 config/user_login.ini 自动登录（无配置则保持原逻辑）
	tryAutoLoginFromConfigFile();

}

// ===================== 功能入口（含权限检查） =====================

void CMapProductManagerPlugin::UserLogin()
{
	// 登录使用本地凭据认证，不依赖数据库连接
	LoginDialog loginDlg(mQGisIface->mainWindow());
	if (loginDlg.exec() == QDialog::Accepted)
	{
		gCurrentUserSession = loginDlg.getSession();
	}
	updateActions();
}

void CMapProductManagerPlugin::DbConfig()
{
	if (!requireAccount(mQGisIface))
		return;

	DbConfigDialog dlg(mQGisIface->mainWindow());
	if (dlg.exec() == QDialog::Accepted)
	{
		updateActions();
	}
}

void CMapProductManagerPlugin::ProductStorage()
{
	// 权限检查：数据操作员 或 数据库管理员
	if (!requireAccount(mQGisIface))
		return;
	if (!gCurrentUserSession.canUploadProduct())
	{
		showAccessDenied(tr("成果存储与管理"), mQGisIface);
		return;
	}

	if (mProductStorageDialog)
	{
		mProductStorageDialog->raise();
		mProductStorageDialog->activateWindow();
		return;
	}

	mProductStorageDialog = new ProductStorageDialog(
		nullptr,
		Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint);
	mProductStorageDialog->setQgisInterface(mQGisIface);
	mProductStorageDialog->setAttribute(Qt::WA_DeleteOnClose);
	connect(mProductStorageDialog, &QObject::destroyed, this, [this]()
	{
		mProductStorageDialog = nullptr;
	});
	mProductStorageDialog->setModal(false);
	mProductStorageDialog->show();
}

void CMapProductManagerPlugin::MetadataManager()
{
	QgsMessageLog::logMessage("MetadataManager entry", "MapProduct", Qgis::Info);
	if (!requireAccount(mQGisIface))
		return;
	if (!gCurrentUserSession.canEditMetadata())
	{
		showAccessDenied(tr("元数据管理"), mQGisIface);
		return;
	}

	QgsMessageLog::logMessage("  权限检查通过，开始创建对话框...", "MapProduct", Qgis::Info);
	MetadataManagerDialog* pDlg = new MetadataManagerDialog(nullptr, Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint);
	pDlg->setQgisInterface(mQGisIface);

	QgsMessageLog::logMessage("  对话框创建完成，调用 show()...", "MapProduct", Qgis::Info);
	pDlg->setModal(false);
	pDlg->show();
	QgsMessageLog::logMessage("  show() 调用完成", "MapProduct", Qgis::Info);
}

void CMapProductManagerPlugin::ProductSearch()
{
	// 成果检索与预览：所有登录用户可用
	if (!requireAccount(mQGisIface))
		return;

	ProductSearchDialog* pDlg = new ProductSearchDialog(nullptr, Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint);
	pDlg->setQgisInterface(mQGisIface);
	pDlg->setModal(false);
	pDlg->show();
}

void CMapProductManagerPlugin::VersionDiff()
{
	// 权限检查：数据操作员 或 数据库管理员
	if (!requireAccount(mQGisIface))
		return;
	if (!gCurrentUserSession.canVersionDiff())
	{
		showAccessDenied(tr("版本回溯与差异比对"), mQGisIface);
		return;
	}

	VersionDiffDialog* pDlg = new VersionDiffDialog(nullptr, Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint);
	pDlg->setModal(false);
	pDlg->show();
}

void CMapProductManagerPlugin::AccessControl()
{
	// 权限检查：仅数据库管理员
	if (!requireAccount(mQGisIface))
		return;
	if (!gCurrentUserSession.canManageUsers())
	{
		showAccessDenied(tr("权限管理"), mQGisIface);
		return;
	}

	ProductDAO dao;
	AccessControlDialog* pDlg = new AccessControlDialog(&dao, nullptr, Qt::WindowCloseButtonHint);
	pDlg->setCurrentOperator(gCurrentUserSession.userName);
	pDlg->setModal(false);
	pDlg->show();
}

void CMapProductManagerPlugin::MapService()
{
	// 地图服务管理：无需登录权限
	if (!requireAccount(mQGisIface))
		return;
	MapServiceDialog* pDlg = new MapServiceDialog(nullptr, Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint);
	pDlg->setQgisInterface(mQGisIface);
	pDlg->setModal(false);
	pDlg->show();
}

//void CMapProductManagerPlugin::DynamicStats()
//{
//	// 动态统计：无需登录权限
//	DynamicStatisticsDialog* pDlg = new DynamicStatisticsDialog(mQGisIface->mainWindow());
//	pDlg->setModal(false);
//	pDlg->show();
//}

void CMapProductManagerPlugin::DataImport()
{
	QCoreApplication* app = QCoreApplication::instance();
	const QVariantMap request = app
		? app->property("ltzkProductCatalogRequest").toMap()
		: QVariantMap();
	const QVariant targetDirectoryProperty = app
		? app->property("ltzkProductImportTargetDirId")
		: QVariant();
	const int targetDirectoryId = targetDirectoryProperty.isValid()
		? targetDirectoryProperty.toInt()
		: request.value(QStringLiteral("directoryId")).toInt();
	if (app)
	{
		app->setProperty("ltzkProductCatalogRequest", QVariant());
		app->setProperty("ltzkProductImportTargetDirId", QVariant());
	}

	// 权限检查：数据操作员 或 数据库管理员
	if (!requireAccount(mQGisIface))
		return;
	if (!gCurrentUserSession.canUploadProduct())
	{
		showAccessDenied(tr("批量数据导入"), mQGisIface);
		return;
	}
	if (!syncExternalCatalogConnection(request, mQGisIface))
		return;

	// 数据导入向导：需要数据库连接
	if (!PostgisConnector::instance()->isConnected())
	{
		if (mQGisIface)
		{
			mQGisIface->messageBar()->pushWarning(
				tr("导入失败"),
				tr("数据库未连接，请先配置数据库连接。")
			);
		}
		return;
	}

	DataImportWizard wizard(mQGisIface->mainWindow());
	if (targetDirectoryId > 0)
		wizard.setTargetParentDirId(targetDirectoryId);
	wizard.exec();
}

void CMapProductManagerPlugin::executeProductCatalogRequest(const QVariantMap& request)
{
	QCoreApplication* app = QCoreApplication::instance();
	const QString operation = request.value(QStringLiteral("operation")).toString();
	if (operation == QStringLiteral("import"))
	{
		// 兼容外部调用方：导入仍统一走已有数据导入入口。
		if (app)
		{
			app->setProperty("ltzkProductCatalogRequest", request);
			app->setProperty(
				"ltzkProductImportTargetDirId",
				request.value(QStringLiteral("directoryId")));
		}
		DataImport();
		return;
	}
	if (!requireAccount(mQGisIface))
		return;
	if (!gCurrentUserSession.canUploadProduct())
	{
		showAccessDenied(tr("成果目录操作"), mQGisIface);
		return;
	}
	if (!syncExternalCatalogConnection(request, mQGisIface))
		return;

	QList<int> productIds;
	for (const QVariant& productId : request.value(QStringLiteral("productIds")).toList())
		productIds.append(productId.toInt());

	ProductStorageDialog dialog(mQGisIface ? mQGisIface->mainWindow() : nullptr);
	dialog.setQgisInterface(mQGisIface);
	dialog.executeExternalDirectoryOperation(
		operation,
		request.value(QStringLiteral("directoryId")).toInt(),
		request.value(QStringLiteral("productId")).toInt(),
		productIds);
}

void CMapProductManagerPlugin::DataCatalogOperation()
{
	QCoreApplication* app = QCoreApplication::instance();
	const QVariantMap request = app
		? app->property("ltzkProductCatalogRequest").toMap()
		: QVariantMap();
	if (app)
	{
		app->setProperty("ltzkProductCatalogRequest", QVariant());
		app->setProperty("ltzkProductImportTargetDirId", QVariant());
	}

	executeProductCatalogRequest(request);
}

void CMapProductManagerPlugin::ThemeManager()
{
	// 权限检查：数据操作员 或 数据库管理员
	if (!requireAccount(mQGisIface))
		return;
	if (!gCurrentUserSession.canUploadProduct())
	{
		showAccessDenied(tr("主题管理"), mQGisIface);
		return;
	}

	ThemeManagerDialog* pDlg = new ThemeManagerDialog(mQGisIface, nullptr, Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint);
	pDlg->setModal(false);
	pDlg->show();
}

// ===================== unload =====================
void CMapProductManagerPlugin::unload()
{
	gCurrentUserSession.clear();
	PostgisConnector::instance()->disconnect();

	// 非模态窗口可能仍在事件循环中，必须在 DLL 卸载前同步销毁，
	// 避免窗口的 Qt 元对象、信号槽或析构代码继续访问已卸载的插件。
	if (mProductStorageDialog)
	{
		mProductStorageDialog->setAttribute(Qt::WA_DeleteOnClose, false);
		mProductStorageDialog->close();
		delete mProductStorageDialog.data();
		mProductStorageDialog = nullptr;
	}

	if (mMainMenu)
	{
		mQGisIface->removePluginMenu(tr("&地图成果管理"), mMainMenu->menuAction());
	}

	mQGisIface->removeToolBarIcon(mActionLogin);
	mQGisIface->removeToolBarIcon(mActionDbConfig);
	mQGisIface->removeToolBarIcon(mActionProductStorage);
	mQGisIface->removeToolBarIcon(mActionMetadataManager);
	mQGisIface->removeToolBarIcon(mActionProductSearch);
	mQGisIface->removeToolBarIcon(mActionVersionDiff);
	mQGisIface->removeToolBarIcon(mActionAccessControl);
	mQGisIface->removeToolBarIcon(mActionMapService);
	mQGisIface->removeToolBarIcon(mActionDataImport);
	mQGisIface->removeToolBarIcon(mActionThemeManager);

	delete mActionLogin;
	delete mActionDbConfig;
	delete mActionProductStorage;
	delete mActionMetadataManager;
	delete mActionProductSearch;
	delete mActionVersionDiff;
	delete mActionAccessControl;
	delete mActionMapService;
	//delete mActionDynamicStats;
	delete mActionDataImport;
	delete mActionDataCatalogOperation;
	delete mActionThemeManager;
	delete mMainMenu;

	mActionLogin = nullptr;
	mActionDbConfig = nullptr;
	mActionProductStorage = nullptr;
	mActionMetadataManager = nullptr;
	mActionProductSearch = nullptr;
	mActionVersionDiff = nullptr;
	mActionAccessControl = nullptr;
	mActionMapService = nullptr;
	//mActionDynamicStats = nullptr;
	mActionDataImport = nullptr;
	mActionDataCatalogOperation = nullptr;
	mActionThemeManager = nullptr;
	mMainMenu = nullptr;
}

// ===================== updateActions =====================
void CMapProductManagerPlugin::updateActions()
{
	if (mActionDbConfig) mActionDbConfig->setEnabled(true);
	if (mActionLogin) mActionLogin->setEnabled(true);
	if (mActionProductStorage) mActionProductStorage->setEnabled(true);
	if (mActionMetadataManager) mActionMetadataManager->setEnabled(true);
	if (mActionProductSearch) mActionProductSearch->setEnabled(true);
	if (mActionVersionDiff) mActionVersionDiff->setEnabled(true);
	if (mActionAccessControl) mActionAccessControl->setEnabled(true);
	if (mActionMapService) mActionMapService->setEnabled(true);
	//if (mActionDynamicStats) mActionDynamicStats->setEnabled(true);
	if (mActionDataImport) mActionDataImport->setEnabled(true);
	if (mActionThemeManager) mActionThemeManager->setEnabled(true);
}

// ===================== QGIS插件入口函数 =====================
QGISEXTERN QgisPlugin* classFactory(QgisInterface* qgisInterfacePointer)
{
	return new CMapProductManagerPlugin(qgisInterfacePointer);
}

QGISEXTERN const QString* name()
{
	return &sName;
}

QGISEXTERN const QString* description()
{
	return &sDescription;
}

QGISEXTERN const QString* category()
{
	return &sCategory;
}

QGISEXTERN int type()
{
	return sPluginType;
}

QGISEXTERN const QString* version()
{
	return &sPluginVersion;
}

QGISEXTERN const QString* icon()
{
	return &sPluginIcon;
}

QGISEXTERN void unload(QgisPlugin* pluginPointer)
{
	delete pluginPointer;
}



QString CMapProductManagerPlugin::loadLtzKPlatformQss()
{
	const QString qssPath =
		QDir(QCoreApplication::applicationDirPath())
		.filePath("Resources/styles/ltzk_sci_fi.qss");

	QFile file(qssPath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		return qApp->styleSheet();
	}

	return QString::fromUtf8(file.readAll());
}
