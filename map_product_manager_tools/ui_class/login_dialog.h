#ifndef LOGIN_DIALOG_H
#define LOGIN_DIALOG_H

#include <QDialog>
#include "ui_login_dialog.h"
#include "database/product_metadata.h"

class LoginDialog : public QDialog
{
	Q_OBJECT

public:
	explicit LoginDialog(QWidget* parent = nullptr);
	~LoginDialog() override;

	/** @brief 获取登录成功后的用户会话 */
	const UserSession& getSession() const { return mSession; }

private slots:
	void onLogin();

private:
	/** @brief 确保本地存在默认管理员凭据（首次使用时自动创建） */
	void ensureDefaultCredentials();

	Ui::LoginDialog mUI;
	UserSession mSession;
};

#endif // LOGIN_DIALOG_H
