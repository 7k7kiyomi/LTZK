#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

enum class AccessRole
{
    DataOperator = 1,
    DataUser = 2,
    DatabaseAdmin = 3
};

struct UserPermission
{
    int id = -1;
    QString userName;
    AccessRole role = AccessRole::DataOperator;
    QString passwordHash;
    QString permissions;
    QString grantedBy;
    QDateTime grantedAt;
};

struct UserSession
{
    QString userName;
    AccessRole role = AccessRole::DataOperator;
    bool isLoggedIn = false;
    bool isGuest = false;

    void clear();
    bool canUseReadOnlyModules() const;
    bool canOperateProductStore() const;
    bool canManageUsers() const;
};

QString accessRoleToString(AccessRole role);
AccessRole stringToAccessRole(const QString& value);
QString hashPassword(const QString& password);

extern UserSession gCurrentUserSession;

class LTZKAuthManager
{
public:
    static LTZKAuthManager& instance();
    static void closeCurrentThreadConnection();

    bool ensureConnectedAndInitialized(QString* errorMessage = nullptr);
    bool authenticateUser(
        const QString& userName,
        const QString& password,
        UserSession* session,
        QString* errorMessage = nullptr);

    QList<UserPermission> listUserPermissions(QString* errorMessage = nullptr);
    UserPermission getUserPermission(
        const QString& userName,
        QString* errorMessage = nullptr);

    bool addUserWithPassword(
        const QString& userName,
        const QString& password,
        AccessRole role,
        const QString& grantedBy,
        QString* errorMessage = nullptr);

    bool deleteUserPermission(
        const QString& userName,
        QString* errorMessage = nullptr);

    bool changePassword(
        const QString& userName,
        const QString& newPassword,
        QString* errorMessage = nullptr);

    bool changeUserRole(
        const QString& userName,
        AccessRole newRole,
        const QString& grantedBy,
        QString* errorMessage = nullptr);

    bool updateUserPermissions(
        const QString& userName,
        const QString& permissions,
        QString* errorMessage = nullptr);

private:
    LTZKAuthManager() = default;

    bool openDatabase(QString* errorMessage);
    bool initializeSchema(QString* errorMessage);
    bool ensureDefaultAdmin(QString* errorMessage);
};
