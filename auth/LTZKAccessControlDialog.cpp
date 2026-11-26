#include "LTZKAccessControlDialog.h"

#include "../LTZKUiScale.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace
{
    QString zh(const wchar_t* value)
    {
        return QString::fromWCharArray(value);
    }

    QTableWidgetItem* readOnlyItem(const QString& text)
    {
        QTableWidgetItem* item = new QTableWidgetItem(text);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        return item;
    }
}

LTZKAccessControlDialog::LTZKAccessControlDialog(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("accessControlEditor"));

    mUserTable = new QTableWidget(this);
    mUserTable->setColumnCount(4);
    mUserTable->setHorizontalHeaderLabels(QStringList()
        << zh(L"\x7528\x6237\x540D")
        << zh(L"\x89D2\x8272")
        << zh(L"\x6388\x6743\x4EBA")
        << zh(L"\x6388\x6743\x65F6\x95F4"));
    mUserTable->horizontalHeader()->setStretchLastSection(true);
    mUserTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mUserTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mUserTable->horizontalHeader()->setHighlightSections(false);
    mUserTable->verticalHeader()->setHighlightSections(false);
    mUserTable->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);

    mUserNameEdit = new QLineEdit(this);
    mUserNameEdit->setPlaceholderText(zh(L"\x8F93\x5165\x7528\x6237\x540D\xFF08\x4E0D\x53EF\x91CD\x590D\xFF09"));

    mPasswordEdit = new QLineEdit(this);
    mPasswordEdit->setEchoMode(QLineEdit::Password);
    mPasswordEdit->setPlaceholderText(zh(L"\x65B0\x7528\x6237\x5FC5\x586B\xFF1B\x4FEE\x6539\x65F6\x7559\x7A7A\x8868\x793A\x4E0D\x6539\x5BC6\x7801"));

    mRoleCombo = new QComboBox(this);
    mRoleCombo->addItem(accessRoleToString(AccessRole::DataOperator));
    mRoleCombo->addItem(accessRoleToString(AccessRole::DataUser));
    mRoleCombo->addItem(accessRoleToString(AccessRole::DatabaseAdmin));

    mPermissionsEdit = new QPlainTextEdit(this);
    mPermissionsEdit->setPlaceholderText(zh(L"\x6269\x5C55\x6743\x9650\xFF08\x004A\x0053\x004F\x004E\xFF0C\x53EF\x9009\xFF09"));

    mAddButton = new QPushButton(zh(L"\x6DFB\x52A0\x7528\x6237"), this);
    mSaveButton = new QPushButton(zh(L"\x4FDD\x5B58\x6743\x9650"), this);
    mDeleteButton = new QPushButton(zh(L"\x5220\x9664\x7528\x6237"), this);

    mStatusLabel = new QLabel(this);
    mStatusLabel->setWordWrap(true);

    mEditorLayout = new QGridLayout();
    mEditorLayout->addWidget(new QLabel(zh(L"\x7528\x6237\x540D"), this), 0, 0);
    mEditorLayout->addWidget(mUserNameEdit, 0, 1);
    mEditorLayout->addWidget(new QLabel(zh(L"\x89D2\x8272"), this), 1, 0);
    mEditorLayout->addWidget(mRoleCombo, 1, 1);
    mEditorLayout->addWidget(new QLabel(zh(L"\x5BC6\x7801"), this), 2, 0);
    mEditorLayout->addWidget(mPasswordEdit, 2, 1);
    mEditorLayout->addWidget(new QLabel(zh(L"\x6269\x5C55\x6743\x9650"), this), 3, 0);
    mEditorLayout->addWidget(mPermissionsEdit, 3, 1);

    mButtonLayout = new QHBoxLayout();
    mButtonLayout->addWidget(mAddButton);
    mButtonLayout->addWidget(mSaveButton);
    mButtonLayout->addWidget(mDeleteButton);
    mButtonLayout->addStretch();

    mRightLayout = new QVBoxLayout();
    mRightLayout->addLayout(mEditorLayout);
    mRightLayout->addLayout(mButtonLayout);
    mRightLayout->addWidget(mStatusLabel);
    mRightLayout->addStretch();

    mMainLayout = new QHBoxLayout(this);
    mMainLayout->addWidget(mUserTable, 2);
    mMainLayout->addLayout(mRightLayout, 1);
    refreshScaledMetrics();

    connect(mUserTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        loadSelectedUser(row);
    });
    connect(mAddButton, &QPushButton::clicked, this, [this]() {
        addUserWithPassword();
    });
    connect(mSaveButton, &QPushButton::clicked, this, [this]() {
        saveSelectedUser();
    });
    connect(mDeleteButton, &QPushButton::clicked, this, [this]() {
        deleteUserPermission();
    });
}

void LTZKAccessControlDialog::refreshScaledMetrics()
{
    const qreal uiScale = LTZKUiScale::factorForWidget(this);
    mPermissionsEdit->setMaximumHeight(LTZKUiScale::scaledInt(90, uiScale));
    mMainLayout->setContentsMargins(LTZKUiScale::scaledMargins(
        QMargins(18, 18, 18, 18),
        uiScale));
    mMainLayout->setSpacing(LTZKUiScale::scaledInt(6, uiScale));
    mEditorLayout->setHorizontalSpacing(LTZKUiScale::scaledInt(6, uiScale));
    mEditorLayout->setVerticalSpacing(LTZKUiScale::scaledInt(6, uiScale));
    mButtonLayout->setSpacing(LTZKUiScale::scaledInt(6, uiScale));
    mRightLayout->setSpacing(LTZKUiScale::scaledInt(6, uiScale));
    mUserTable->verticalHeader()->setMinimumSectionSize(
        LTZKUiScale::scaledInt(24, uiScale));
    mUserTable->verticalHeader()->setDefaultSectionSize(
        LTZKUiScale::scaledInt(30, uiScale));
    updateGeometry();
}

void LTZKAccessControlDialog::setCurrentOperator(const QString& userName)
{
    mCurrentOperator = userName;
}

void LTZKAccessControlDialog::reloadUsers()
{
    QString errorMessage;
    const QList<UserPermission> users = LTZKAuthManager::instance().listUserPermissions(&errorMessage);
    if (!errorMessage.isEmpty())
    {
        setStatus(errorMessage, true);
    }

    mUserTable->setRowCount(users.size());
    for (int row = 0; row < users.size(); ++row)
    {
        const UserPermission& user = users.at(row);
        mUserTable->setItem(row, 0, readOnlyItem(user.userName));
        mUserTable->setItem(row, 1, readOnlyItem(accessRoleToString(user.role)));
        mUserTable->setItem(row, 2, readOnlyItem(user.grantedBy));
        mUserTable->setItem(row, 3, readOnlyItem(user.grantedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
    }

    clearEditor();
}

void LTZKAccessControlDialog::clearEditor()
{
    mUserNameEdit->clear();
    mPasswordEdit->clear();
    mRoleCombo->setCurrentText(accessRoleToString(AccessRole::DataOperator));
    mPermissionsEdit->clear();
}

void LTZKAccessControlDialog::loadSelectedUser(int row)
{
    QTableWidgetItem* nameItem = mUserTable->item(row, 0);
    if (!nameItem)
    {
        return;
    }

    QString errorMessage;
    const UserPermission user = LTZKAuthManager::instance().getUserPermission(nameItem->text(), &errorMessage);
    if (user.id <= 0)
    {
        setStatus(errorMessage.isEmpty() ? zh(L"\x672A\x627E\x5230\x7528\x6237\x3002") : errorMessage, true);
        return;
    }

    mUserNameEdit->setText(user.userName);
    mRoleCombo->setCurrentText(accessRoleToString(user.role));
    mPasswordEdit->clear();
    mPermissionsEdit->setPlainText(user.permissions);
}

void LTZKAccessControlDialog::addUserWithPassword()
{
    const QString userName = mUserNameEdit->text().trimmed();
    const QString password = mPasswordEdit->text();
    if (userName.isEmpty() || password.isEmpty())
    {
        setStatus(zh(L"\x65B0\x5EFA\x7528\x6237\x9700\x8981\x586B\x5199\x7528\x6237\x540D\x548C\x521D\x59CB\x5BC6\x7801\x3002"), true);
        return;
    }

    QString errorMessage;
    LTZKAuthManager& manager = LTZKAuthManager::instance();
    if (!manager.addUserWithPassword(userName, password, selectedRole(), mCurrentOperator, &errorMessage))
    {
        setStatus(errorMessage, true);
        return;
    }

    manager.updateUserPermissions(userName, mPermissionsEdit->toPlainText().trimmed(), &errorMessage);
    reloadUsers();
    setStatus(zh(L"\x7528\x6237\x5DF2\x6DFB\x52A0\x3002"), false);
}

void LTZKAccessControlDialog::saveSelectedUser()
{
    const QString userName = mUserNameEdit->text().trimmed();
    if (userName.isEmpty())
    {
        setStatus(zh(L"\x8BF7\x5148\x9009\x62E9\x6216\x8F93\x5165\x7528\x6237\x540D\x3002"), true);
        return;
    }

    if (userName == mCurrentOperator && selectedRole() != AccessRole::DatabaseAdmin)
    {
        setStatus(zh(L"\x4E0D\x80FD\x5C06\x5F53\x524D\x7BA1\x7406\x5458\x964D\x6743\x3002"), true);
        return;
    }

    QString errorMessage;
    LTZKAuthManager& manager = LTZKAuthManager::instance();
    if (!mPasswordEdit->text().isEmpty() && !manager.changePassword(userName, mPasswordEdit->text(), &errorMessage))
    {
        setStatus(errorMessage, true);
        return;
    }

    if (!manager.changeUserRole(userName, selectedRole(), mCurrentOperator, &errorMessage))
    {
        setStatus(errorMessage, true);
        return;
    }

    if (!manager.updateUserPermissions(userName, mPermissionsEdit->toPlainText().trimmed(), &errorMessage))
    {
        setStatus(errorMessage, true);
        return;
    }

    reloadUsers();
    setStatus(zh(L"\x7528\x6237\x6743\x9650\x5DF2\x4FDD\x5B58\x3002"), false);
}

void LTZKAccessControlDialog::deleteUserPermission()
{
    const QString userName = mUserNameEdit->text().trimmed();
    if (userName.isEmpty())
    {
        setStatus(zh(L"\x8BF7\x5148\x9009\x62E9\x8981\x5220\x9664\x7684\x7528\x6237\x3002"), true);
        return;
    }

    if (userName == mCurrentOperator)
    {
        setStatus(zh(L"\x4E0D\x80FD\x5220\x9664\x5F53\x524D\x767B\x5F55\x7528\x6237\x3002"), true);
        return;
    }

    if (QMessageBox::question(
            this,
            zh(L"\x786E\x8BA4\x5220\x9664"),
            zh(L"\x786E\x5B9A\x8981\x5220\x9664\x8BE5\x7528\x6237\x5417\xFF1F")) != QMessageBox::Yes)
    {
        return;
    }

    QString errorMessage;
    if (!LTZKAuthManager::instance().deleteUserPermission(userName, &errorMessage))
    {
        setStatus(errorMessage, true);
        return;
    }

    reloadUsers();
    setStatus(zh(L"\x7528\x6237\x5DF2\x5220\x9664\x3002"), false);
}

AccessRole LTZKAccessControlDialog::selectedRole() const
{
    return stringToAccessRole(mRoleCombo->currentText());
}

void LTZKAccessControlDialog::setStatus(const QString& message, bool isError)
{
    mStatusLabel->setStyleSheet(isError ? QStringLiteral("color: #c0392b;") : QStringLiteral("color: #1e8449;"));
    mStatusLabel->setText(message);
}




