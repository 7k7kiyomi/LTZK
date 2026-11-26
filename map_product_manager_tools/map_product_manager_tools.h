#ifndef MAP_PRODUCT_MANAGER_TOOLS_PLUGIN_H
#define MAP_PRODUCT_MANAGER_TOOLS_PLUGIN_H

#include "qgisplugin.h"
#include <QMenu>
#include <QPointer>
#include <QVariantMap>

class QAction;
class QgisInterface;
class ProductStorageDialog;

class CMapProductManagerPlugin : public QObject, public QgisPlugin
{
	Q_OBJECT

public:
	explicit CMapProductManagerPlugin(QgisInterface* qgisInterface);
	~CMapProductManagerPlugin() override;

	//! 返回当前已由 QGIS 创建的插件实例，供稳定 C API 转发到 GUI 对象。
	static CMapProductManagerPlugin* currentInstance();

private:




public slots:
	//! 初始化Gui
	void initGui() override;

	//! 用户登录
	void UserLogin();

	//! 数据库连接配置
	void DbConfig();

	//! 成果存储与管理（上传、元数据提取、版本管理）
	void ProductStorage();

	//! 元数据管理（自动提取+人工补录、多级目录与标签）
	void MetadataManager();

	//! 成果检索与预览（多条件检索、缩略图预览、结果导出）
	void ProductSearch();

	//! 版本回溯与差异比对
	void VersionDiff();

	//! 权限管理
	void AccessControl();

	//! 地图服务管理（台账、统计、报表）
	void MapService();

	//! 动态统计（按年/月/季度灵活统计，导出Excel/PDF/PNG）
	//void DynamicStats();

	//! 数据导入（向导式导入，支持矢量/栅格/制图文件）
	void DataImport();

	//! 数据目录右键操作桥接（复用成果存储与管理的现有业务实现）
	void DataCatalogOperation();

	//! 执行主程序传入的成果目录操作请求（复用现有对话框与业务实现）。
	void executeProductCatalogRequest(const QVariantMap& request);

	//! 主题管理（主题列表、主题设置、主题内容管理）
	void ThemeManager();

	//! 卸载插件
	void unload() override;



private:
	//! QGIS接口指针
	QgisInterface* mQGisIface = nullptr;

	//! 主菜单
	QMenu* mMainMenu = nullptr;

	//! 菜单Actions
	QAction* mActionLogin = nullptr;
	QAction* mActionDbConfig = nullptr;
	QAction* mActionProductStorage = nullptr;
	QAction* mActionMetadataManager = nullptr;
	QAction* mActionProductSearch = nullptr;
	QAction* mActionVersionDiff = nullptr;
	QAction* mActionAccessControl = nullptr;
	QAction* mActionMapService = nullptr;
	//QAction* mActionDynamicStats = nullptr;
	QAction* mActionDataImport = nullptr;
	QAction* mActionDataCatalogOperation = nullptr;
	QAction* mActionThemeManager = nullptr;
	//! 非模态成果存储窗口，确保插件卸载前被关闭并销毁。
	QPointer<ProductStorageDialog> mProductStorageDialog;
	static CMapProductManagerPlugin* s_instance;

private:
	// 加载LTZK平台样式表
	QString loadLtzKPlatformQss();

private slots:
	void updateActions();
};

#endif // MAP_PRODUCT_MANAGER_TOOLS_PLUGIN_H
