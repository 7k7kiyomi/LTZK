#include "LTZKLoginDialog.h"
#include "../LTZKUiScale.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QtGlobal>

namespace
{
    constexpr double kBaseDialogWidth = 500.0;
    constexpr double kBaseDialogHeight = 300.0;

    QString zh(const wchar_t* value)
    {
        return QString::fromWCharArray(value);
    }
}

LTZKLoginDialog::LTZKLoginDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(zh(L"\x7528\x6237\x767B\x5F55"));
    setModal(true);
    const qreal scale = LTZKUiScale::factorForWidget(this);
    setMinimumSize(LTZKUiScale::scaledSize(QSize(440, 260), scale));
    resize(LTZKUiScale::scaledSize(
        QSize(static_cast<int>(kBaseDialogWidth), static_cast<int>(kBaseDialogHeight)),
        scale));
    setSizeGripEnabled(true);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    mTitleLabel = new QLabel(zh(L"\x9647\x56FE\x667A\x63A7\x5E73\x53F0\x7528\x6237\x767B\x5F55"), this);
    mTitleLabel->setObjectName(QStringLiteral("loginTitleLabel"));
    mTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mTitleLabel->setWordWrap(true);
    mTitleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    mTitleLabel->setMinimumHeight(30);

    QLabel* userLabel = new QLabel(zh(L"\x7528\x6237\x540D"), this);
    userLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mUserNameEdit = new QLineEdit(this);
    mUserNameEdit->setObjectName(QStringLiteral("mUserNameEdit"));
    mUserNameEdit->setPlaceholderText(zh(L"\x8BF7\x8F93\x5165\x7528\x6237\x540D"));
    mUserNameEdit->setMinimumHeight(30);
    mUserNameEdit->setMinimumWidth(260);
    mUserNameEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QLabel* passwordLabel = new QLabel(zh(L"\x5BC6\x7801"), this);
    passwordLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mPasswordEdit = new QLineEdit(this);
    mPasswordEdit->setObjectName(QStringLiteral("mPasswordEdit"));
    mPasswordEdit->setEchoMode(QLineEdit::Password);
    mPasswordEdit->setPlaceholderText(zh(L"\x8BF7\x8F93\x5165\x5BC6\x7801"));
    mPasswordEdit->setMinimumHeight(30);
    mPasswordEdit->setMinimumWidth(260);
    mPasswordEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    mStatusLabel = new QLabel(this);
    mStatusLabel->setObjectName(QStringLiteral("mStatusLabel"));
    mStatusLabel->setWordWrap(true);
    mStatusLabel->setMinimumHeight(24);
    mStatusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    mLoginButton = new QPushButton(zh(L"\x767B\x5F55"), this);
    mLoginButton->setObjectName(QStringLiteral("mLoginButton"));
    mLoginButton->setDefault(true);
    mLoginButton->setMinimumHeight(32);
    mLoginButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    mFormLayout = new QGridLayout();
    mFormLayout->setObjectName(QStringLiteral("loginFormLayout"));
    mFormLayout->setHorizontalSpacing(10);
    mFormLayout->setVerticalSpacing(10);
    mFormLayout->setColumnMinimumWidth(0, 62);
    mFormLayout->setColumnStretch(0, 0);
    mFormLayout->setColumnStretch(1, 1);
    mFormLayout->addWidget(userLabel, 0, 0);
    mFormLayout->addWidget(mUserNameEdit, 0, 1);
    mFormLayout->addWidget(passwordLabel, 1, 0);
    mFormLayout->addWidget(mPasswordEdit, 1, 1);

    mButtonLayout = new QHBoxLayout();
    mButtonLayout->setContentsMargins(0, 4, 0, 0);
    mButtonLayout->addStretch();
    mButtonLayout->addWidget(mLoginButton);

    mMainLayout = new QVBoxLayout(this);
    mMainLayout->setContentsMargins(30, 24, 30, 22);
    mMainLayout->setSpacing(12);
    mMainLayout->addWidget(mTitleLabel);
    mMainLayout->addLayout(mFormLayout);
    mMainLayout->addWidget(mStatusLabel);
    mMainLayout->addStretch(1);
    mMainLayout->addLayout(mButtonLayout);

    updateAdaptiveLayout();

    connect(mLoginButton, &QPushButton::clicked, this, [this]() {
        authenticateUser();
    });
    connect(mPasswordEdit, &QLineEdit::returnPressed, this, [this]() {
        authenticateUser();
    });
}

UserSession LTZKLoginDialog::session() const
{
    return mSession;
}

void LTZKLoginDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    updateAdaptiveLayout();
}

double LTZKLoginDialog::uiScale() const
{
    return LTZKUiScale::factorForWidget(this);
}

int LTZKLoginDialog::scaled(int value) const
{
    return LTZKUiScale::scaledInt(value, uiScale());
}

void LTZKLoginDialog::updateAdaptiveLayout()
{
    if (!mMainLayout || !mFormLayout || !mButtonLayout || !mTitleLabel || !mUserNameEdit || !mPasswordEdit || !mLoginButton || !mStatusLabel)
    {
        return;
    }

    setStyleSheet(QStringLiteral(
        "LTZKLoginDialog, QDialog { background: #071827; }"
        "QLabel { color: #e8f8ff; font-size: %1px; }"
        "QLabel#loginTitleLabel { color: #ffffff; font-size: %2px; font-weight: 700; padding-bottom: %3px; }"
        "QLabel#mStatusLabel { color: #ff9d8a; font-size: %4px; }"
        "QLineEdit { background: #06111d; color: #ffffff; border: 1px solid #2ea8df; padding: %5px %6px; font-size: %7px; selection-background-color: #147db5; }"
        "QLineEdit:focus { border: 1px solid #59d6ff; }"
        "QPushButton#mLoginButton { color: #ffffff; background: #09345a; border: 1px solid #2ea8df; padding: %8px %9px; font-size: %10px; font-weight: 600; }"
        "QPushButton#mLoginButton:hover { background: #0e4d7d; border-color: #59d6ff; }")
        .arg(scaled(13))
        .arg(scaled(18))
        .arg(scaled(4))
        .arg(scaled(12))
        .arg(scaled(4))
        .arg(scaled(9))
        .arg(scaled(13))
        .arg(scaled(6))
        .arg(scaled(20))
        .arg(scaled(13)));

    mTitleLabel->setMinimumHeight(scaled(30));
    mUserNameEdit->setMinimumHeight(scaled(30));
    mUserNameEdit->setMinimumWidth(scaled(260));
    mPasswordEdit->setMinimumHeight(scaled(30));
    mPasswordEdit->setMinimumWidth(scaled(260));
    mStatusLabel->setMinimumHeight(scaled(24));
    mLoginButton->setMinimumHeight(scaled(32));

    mFormLayout->setHorizontalSpacing(scaled(10));
    mFormLayout->setVerticalSpacing(scaled(10));
    mFormLayout->setColumnMinimumWidth(0, scaled(62));
    mButtonLayout->setContentsMargins(0, scaled(4), 0, 0);
    mMainLayout->setContentsMargins(scaled(30), scaled(24), scaled(30), scaled(22));
    mMainLayout->setSpacing(scaled(12));
}

void LTZKLoginDialog::authenticateUser()
{
    const QString userName = mUserNameEdit->text().trimmed();
    const QString password = mPasswordEdit->text();

    if (userName.isEmpty())
    {
        mStatusLabel->setText(zh(L"\x8BF7\x8F93\x5165\x7528\x6237\x540D\x3002"));
        mUserNameEdit->setFocus();
        return;
    }

    if (password.isEmpty())
    {
        mStatusLabel->setText(zh(L"\x8BF7\x8F93\x5165\x5BC6\x7801\x3002"));
        mPasswordEdit->setFocus();
        return;
    }

    QString errorMessage;
    UserSession session;
    if (!LTZKAuthManager::instance().authenticateUser(
            userName,
            password,
            &session,
            &errorMessage))
    {
        mStatusLabel->setText(errorMessage);
        mPasswordEdit->selectAll();
        mPasswordEdit->setFocus();
        return;
    }

    mSession = session;
    accept();
}
