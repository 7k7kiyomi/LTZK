#pragma once

#include <QWidget>

#include "LTZKAuthManager.h"

class QComboBox;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QPlainTextEdit;
class QTableWidget;
class QVBoxLayout;

class LTZKAccessControlDialog final : public QWidget
{
public:
    explicit LTZKAccessControlDialog(QWidget* parent = nullptr);

    void setCurrentOperator(const QString& userName);
    void reloadUsers();
    void refreshScaledMetrics();

private:
    void clearEditor();
    void loadSelectedUser(int row);
    void addUserWithPassword();
    void saveSelectedUser();
    void deleteUserPermission();
    AccessRole selectedRole() const;
    void setStatus(const QString& message, bool isError);

    QTableWidget* mUserTable = nullptr;
    QLineEdit* mUserNameEdit = nullptr;
    QLineEdit* mPasswordEdit = nullptr;
    QComboBox* mRoleCombo = nullptr;
    QPlainTextEdit* mPermissionsEdit = nullptr;
    QPushButton* mAddButton = nullptr;
    QPushButton* mSaveButton = nullptr;
    QPushButton* mDeleteButton = nullptr;
    QLabel* mStatusLabel = nullptr;
    QGridLayout* mEditorLayout = nullptr;
    QHBoxLayout* mButtonLayout = nullptr;
    QVBoxLayout* mRightLayout = nullptr;
    QHBoxLayout* mMainLayout = nullptr;
    QString mCurrentOperator;
};


