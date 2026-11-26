#include "login_dialog.h"
#include <QMessageBox>
#include <QKeyEvent>
#include <QSettings>

LoginDialog::LoginDialog(QWidget* parent)
	: QDialog(parent)
{
	mUI.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setFixedSize(800, 560);

	connect(mUI.mLoginBtn, &QPushButton::clicked, this, &LoginDialog::onLogin);
	connect(mUI.mCancelBtn, &QPushButton::clicked, this, &QDialog::reject);

	// 允许回车键触发登录
	mUI.mPasswordEdit->installEventFilter(this);

	// 首次使用时自动创建默认管理员凭据
	ensureDefaultCredentials();
}

LoginDialog::~LoginDialog()
{
}

void LoginDialog::ensureDefaultCredentials()
{
	QSettings settings("GarMap", "MapProductManager");
	settings.beginGroup("users");
	QStringList users = settings.childGroups();
	if (users.isEmpty())
	{
		// 尚无本地用户，创建默认管理员账户
		settings.beginGroup("admin");
		settings.setValue("passwordHash", hashPassword("123456"));
		settings.setValue("role", static_cast<int>(AccessRole::DatabaseAdmin));
		settings.endGroup();
	}
	settings.endGroup();
}

void LoginDialog::onLogin()
{
	QString userName = mUI.mUserNameEdit->text().trimmed();
	QString password = mUI.mPasswordEdit->text();

	if (userName.isEmpty())
	{
		mUI.mStatusLabel->setText(QStringLiteral("请输入用户名"));
		mUI.mUserNameEdit->setFocus();
		return;
	}
	if (password.isEmpty())
	{
		mUI.mStatusLabel->setText(QStringLiteral("请输入密码"));
		mUI.mPasswordEdit->setFocus();
		return;
	}

	// 本地认证：从 QSettings 读取用户凭据（不依赖数据库连接）
	QSettings settings("GarMap", "MapProductManager");
	settings.beginGroup("users");
	settings.beginGroup(userName);

	QString storedHash = settings.value("passwordHash").toString();
	int roleInt = settings.value("role", static_cast<int>(AccessRole::DataOperator)).toInt();

	settings.endGroup();
	settings.endGroup();

	if (storedHash.isEmpty() || storedHash != hashPassword(password))
	{
		mUI.mStatusLabel->setText(QStringLiteral("用户名或密码错误，请重试"));
		mUI.mPasswordEdit->selectAll();
		mUI.mPasswordEdit->setFocus();
		return;
	}

	// 登录成功，填充会话
	mSession.userName = userName;
	mSession.role = static_cast<AccessRole>(roleInt);
	mSession.isLoggedIn = true;

	{
		QMessageBox box(QMessageBox::Information,
			QStringLiteral("登录成功"),
			QStringLiteral("登录成功"),
			QMessageBox::Ok,
			this);
		box.setButtonText(QMessageBox::Ok, QStringLiteral("确定"));
		box.exec();
	}
	accept();
}
