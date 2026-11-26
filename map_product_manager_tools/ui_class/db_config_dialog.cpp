#include "db_config_dialog.h"
#include "database/postgis_connector.h"
#include "database/schema_manager.h"
#include <QMessageBox>
#include <QSettings>
#include <QApplication>
#include <QPushButton>
#include "qgsgui.h"

DbConfigDialog::DbConfigDialog(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);
	loadSettings();
	setWindowTitle("数据库连接配置");
	setMinimumSize(480, 450);

	// 按钮连接
	connect(ui.mTestBtn, &QPushButton::clicked, this, &DbConfigDialog::onTestConnection);
	connect(ui.mConnectBtn, &QPushButton::clicked, this, &DbConfigDialog::onConnect);

	// 自动检测当前连接状态
	auto* db = PostgisConnector::instance();
	if (db->isConnected())
	{
		setStatus("当前已连接到数据库", "green");
		ui.mConnectBtn->setText("重新连接并初始化");
	}
}

DbConfigDialog::~DbConfigDialog()
{
}

void DbConfigDialog::loadSettings()
{
	QSettings settings("GarMap", "MapProductManager");
	ui.mHostEdit->setText(settings.value("db/host", "localhost").toString());
	ui.mPortSpin->setValue(settings.value("db/port", 5432).toInt());
	ui.mDbNameEdit->setText(settings.value("db/database", "map_products").toString());
	ui.mSchemaCombo->setCurrentText(settings.value("db/schema", "public").toString());
	ui.mUserEdit->setText(settings.value("db/user", "YOUR_PASSWORD").toString());
	ui.mSavePwdCheck->setChecked(settings.value("db/savePassword", false).toBool());

	if (ui.mSavePwdCheck->isChecked())
	{
		ui.mPasswordEdit->setText(settings.value("db/password", "").toString());
	}
}

void DbConfigDialog::saveSettings()
{
	QSettings settings("GarMap", "MapProductManager");
	settings.setValue("db/host", ui.mHostEdit->text().trimmed());
	settings.setValue("db/port", ui.mPortSpin->value());
	settings.setValue("db/database", ui.mDbNameEdit->text().trimmed());
	settings.setValue("db/schema", ui.mSchemaCombo->currentText().trimmed());
	settings.setValue("db/user", ui.mUserEdit->text().trimmed());
	settings.setValue("db/savePassword", ui.mSavePwdCheck->isChecked());

	if (ui.mSavePwdCheck->isChecked())
	{
		settings.setValue("db/password", ui.mPasswordEdit->text());
	}
	else
	{
		settings.remove("db/password");
	}
}

void DbConfigDialog::setStatus(const QString& msg, const QString& color, bool showProgress)
{
	Q_UNUSED(color);
	ui.mStatusLabel->setText(msg);
	ui.mProgressBar->setVisible(showProgress);
	if (showProgress)
	{
		ui.mProgressBar->setValue(0);
	}
	QApplication::processEvents();
}

void DbConfigDialog::onTestConnection()
{
	// 禁用按钮防止重复点击
	ui.mTestBtn->setEnabled(false);
	ui.mConnectBtn->setEnabled(false);

	setStatus("正在测试连接...", "orange", true);

	auto* db = PostgisConnector::instance();

	// 如果已连接，先断开
	if (db->isConnected())
	{
		db->disconnect();
	}

	bool success = db->connect(host(), port(), database(), user(), password());

	if (success)
	{
		setStatus("测试连接成功！PostGIS版本: " + 
			db->executeQueryOne("SELECT PostGIS_Version()").value("postgis_version").toString(), 
			"green");
		db->disconnect(); // 测试后断开
	}
	else
	{
		setStatus("连接失败: " + db->lastError(), "red");
	}

	ui.mTestBtn->setEnabled(true);
	ui.mConnectBtn->setEnabled(true);
}

void DbConfigDialog::onConnect()
{
	// 禁用按钮防止重复点击
	ui.mTestBtn->setEnabled(false);
	ui.mConnectBtn->setEnabled(false);

	auto* db = PostgisConnector::instance();

	// 如果已连接，先断开
	if (db->isConnected())
	{
		db->disconnect();
	}

	// 步骤1: 连接数据库
	setStatus("正在连接数据库...", "orange", true);

	if (!db->connect(host(), port(), database(), user(), password()))
	{
		setStatus("连接失败: " + db->lastError(), "red");
		ui.mTestBtn->setEnabled(true);
		ui.mConnectBtn->setEnabled(true);
		return;
	}

	setStatus("数据库已连接，正在初始化Schema...", "orange", true);

	// 步骤2: 设置搜索路径
	QString schemaName = ui.mSchemaCombo->currentText().trimmed();
	if (!schemaName.isEmpty() && schemaName != "public")
	{
		db->executeNonQuery(QString("CREATE SCHEMA IF NOT EXISTS %1").arg(schemaName));
		db->setSearchPath(schemaName);
	}

	// 步骤3: 初始化Schema
	SchemaManager schemaMgr;
	if (!schemaMgr.initializeSchema())
	{
		QString errDetail = schemaMgr.lastError();
		if (errDetail.isEmpty()) {
			errDetail = "未知错误";
		}
		setStatus("Schema初始化失败: " + errDetail, "red");
		ui.mTestBtn->setEnabled(true);
		ui.mConnectBtn->setEnabled(true);
		return;
	}

	// 成功
	saveSettings();
	setStatus("数据库连接成功，Schema已初始化完成", "green");

	QMessageBox::information(this, "连接成功",
		QString("PostGIS数据库连接成功！\n\n"
				"服务器: %1:%2\n"
				"数据库: %3\n"
				"Schema: %4\n\n"
				"数据库表结构已初始化，可以开始使用地图成果管理功能。")
		.arg(host())
		.arg(port())
		.arg(database())
		.arg(schemaName));

	ui.mTestBtn->setEnabled(true);
	ui.mConnectBtn->setEnabled(true);
	accept();
}

QString DbConfigDialog::host() const { return ui.mHostEdit->text().trimmed(); }
int DbConfigDialog::port() const { return ui.mPortSpin->value(); }
QString DbConfigDialog::database() const { return ui.mDbNameEdit->text().trimmed(); }
QString DbConfigDialog::schema() const { return ui.mSchemaCombo->currentText().trimmed(); }
QString DbConfigDialog::user() const { return ui.mUserEdit->text().trimmed(); }
QString DbConfigDialog::password() const { return ui.mPasswordEdit->text(); }

bool DbConfigDialog::configureAndConnect(QWidget* parent)
{
	DbConfigDialog dlg(parent);
	return dlg.exec() == QDialog::Accepted;
}
