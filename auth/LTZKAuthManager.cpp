#include "LTZKAuthManager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QThread>

namespace
{
    QString authConnectionName()
    {
        return QStringLiteral("LTZKAuthPostgisConnection_%1")
            .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16);
    }

    QString zh(const wchar_t* value)
    {
        return QString::fromWCharArray(value);
    }

    QString roleDataOperator()
    {
        return zh(L"\x6570\x636E\x64CD\x4F5C\x5458");
    }

    QString roleDataUser()
    {
        return zh(L"\x6570\x636E\x4F7F\x7528\x8005");
    }

    QString roleDatabaseAdmin()
    {
        return zh(L"\x6570\x636E\x5E93\x7BA1\x7406\x5458");
    }

    QString databaseConfigPath()
    {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList candidates = {
            appDir + QStringLiteral("/config/database.ini"),
            QDir(appDir).absoluteFilePath(QStringLiteral("../config/database.ini")),
            QDir(appDir).absoluteFilePath(QStringLiteral("../../config/database.ini")),
            QDir::currentPath() + QStringLiteral("/config/database.ini")
        };

        for (const QString& candidate : candidates)
        {
            if (QFileInfo::exists(candidate))
            {
                return QFileInfo(candidate).absoluteFilePath();
            }
        }

        return candidates.first();
    }

    QString readDbSetting(
        const QString& key,
        const QVariant& fallback)
    {
        QSettings settings(databaseConfigPath(), QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        const QVariant value = settings.value(key);
        if (value.isValid() && !value.toString().isEmpty())
        {
            return value.toString();
        }

        return fallback.toString();
    }

    int readDbPort()
    {
        QSettings settings(databaseConfigPath(), QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        if (settings.contains("db/port"))
        {
            return settings.value("db/port", 5432).toInt();
        }

        return 5432;
    }

    void bindUserName(QSqlQuery* query, const QString& userName)
    {
        query->bindValue(":user_name", userName);
    }

    UserPermission userPermissionFromQuery(const QSqlQuery& query)
    {
        UserPermission permission;
        permission.id = query.value("id").toInt();
        permission.userName = query.value("user_name").toString();
        permission.role = stringToAccessRole(query.value("role").toString());
        permission.passwordHash = query.value("password_hash").toString();
        permission.permissions = query.value("permissions").toString();
        permission.grantedBy = query.value("granted_by").toString();
        permission.grantedAt = query.value("granted_at").toDateTime();
        return permission;
    }

    void setError(QString* errorMessage, const QString& message)
    {
        if (errorMessage)
        {
            *errorMessage = message;
        }
    }
}

UserSession gCurrentUserSession;

void UserSession::clear()
{
    userName.clear();
    role = AccessRole::DataOperator;
    isLoggedIn = false;
    isGuest = false;
}

bool UserSession::canUseReadOnlyModules() const
{
    return isLoggedIn;
}

bool UserSession::canOperateProductStore() const
{
    return isLoggedIn &&
        (role == AccessRole::DataOperator || role == AccessRole::DatabaseAdmin);
}

bool UserSession::canManageUsers() const
{
    return isLoggedIn && role == AccessRole::DatabaseAdmin;
}

QString accessRoleToString(AccessRole role)
{
    switch (role)
    {
    case AccessRole::DataOperator:
        return roleDataOperator();
    case AccessRole::DataUser:
        return roleDataUser();
    case AccessRole::DatabaseAdmin:
        return roleDatabaseAdmin();
    default:
        return roleDataOperator();
    }
}

AccessRole stringToAccessRole(const QString& value)
{
    if (value == roleDatabaseAdmin())
    {
        return AccessRole::DatabaseAdmin;
    }
    if (value == roleDataUser())
    {
        return AccessRole::DataUser;
    }
    return AccessRole::DataOperator;
}

QString hashPassword(const QString& password)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(
            password.toUtf8(),
            QCryptographicHash::Sha256).toHex());
}

LTZKAuthManager& LTZKAuthManager::instance()
{
    static LTZKAuthManager manager;
    return manager;
}

void LTZKAuthManager::closeCurrentThreadConnection()
{
    const QString name = authConnectionName();
    if (!QSqlDatabase::contains(name))
    {
        return;
    }
    {
        QSqlDatabase db = QSqlDatabase::database(name, false);
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
}

bool LTZKAuthManager::ensureConnectedAndInitialized(QString* errorMessage)
{
    if (!openDatabase(errorMessage))
    {
        return false;
    }

    if (!initializeSchema(errorMessage))
    {
        return false;
    }

    return ensureDefaultAdmin(errorMessage);
}

bool LTZKAuthManager::authenticateUser(
    const QString& userName,
    const QString& password,
    UserSession* session,
    QString* errorMessage)
{
    if (!session)
    {
        setError(errorMessage, zh(L"\x5185\x90E8\x9519\x8BEF\xFF1A\x767B\x5F55\x4F1A\x8BDD\x4E3A\x7A7A\x3002"));
        return false;
    }

    if (!ensureConnectedAndInitialized(errorMessage))
    {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(authConnectionName()));
    query.prepare(
        "SELECT id, user_name, role, password_hash, permissions, granted_by, granted_at "
        "FROM user_permissions "
        "WHERE user_name = :user_name AND password_hash = :password_hash");
    bindUserName(&query, userName);
    query.bindValue(":password_hash", hashPassword(password));

    if (!query.exec())
    {
        setError(errorMessage, query.lastError().text());
        return false;
    }

    if (!query.next())
    {
        session->clear();
        setError(errorMessage, zh(L"\x7528\x6237\x540D\x6216\x5BC6\x7801\x9519\x8BEF\xFF0C\x8BF7\x91CD\x8BD5\x3002"));
        return false;
    }

    const UserPermission permission = userPermissionFromQuery(query);
    session->userName = permission.userName;
    session->role = permission.role;
    session->isLoggedIn = true;
    session->isGuest = false;
    return true;
}

QList<UserPermission> LTZKAuthManager::listUserPermissions(QString* errorMessage)
{
    QList<UserPermission> users;
    if (!ensureConnectedAndInitialized(errorMessage))
    {
        return users;
    }

    QSqlQuery query(QSqlDatabase::database(authConnectionName()));
    if (!query.exec(
            "SELECT id, user_name, role, password_hash, permissions, granted_by, granted_at "
            "FROM user_permissions ORDER BY user_name"))
    {
        setError(errorMessage, query.lastError().text());
        return users;
    }

    while (query.next())
    {
        users.append(userPermissionFromQuery(query));
    }
    return users;
}

UserPermission LTZKAuthManager::getUserPermission(
    const QString& userName,
    QString* errorMessage)
{
    UserPermission permission;
    if (!ensureConnectedAndInitialized(errorMessage))
    {
        return permission;
    }

    QSqlQuery query(QSqlDatabase::database(authConnectionName()));
    query.prepare(
        "SELECT id, user_name, role, password_hash, permissions, granted_by, granted_at "
        "FROM user_permissions WHERE user_name = :user_name");
    bindUserName(&query, userName);

    if (!query.exec())
    {
        setError(errorMessage, query.lastError().text());
        return permission;
    }

    if (query.next())
    {
        permission = userPermissionFromQuery(query);
    }
    return permission;
}

bool LTZKAuthManager::addUserWithPassword(
    const QString& userName,
    const QString& password,
    AccessRole role,
    const QString& grantedBy,
    QString* errorMessage)
{
    if (!ensureConnectedAndInitialized(errorMessage))
    {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(authConnectionName()));
    query.prepare(
        "INSERT INTO user_permissions "
        "(user_name, role, password_hash, granted_by, granted_at) "
        "VALUES (:user_name, :role, :password_hash, :granted_by, CURRENT_TIMESTAMP) "
        "ON CONFLICT (user_name) DO UPDATE SET "
        "role = EXCLUDED.role, "
        "password_hash = EXCLUDED.password_hash, "
        "granted_by = EXCLUDED.granted_by, "
        "granted_at = CURRENT_TIMESTAMP");
    bindUserName(&query, userName);
    query.bindValue(":role", accessRoleToString(role));
    query.bindValue(":password_hash", hashPassword(password));
    query.bindValue(":granted_by", grantedBy);

    if (!query.exec())
    {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    return true;
}

bool LTZKAuthManager::deleteUserPermission(
    const QString& userName,
    QString* errorMessage)
{
    if (!ensureConnectedAndInitialized(errorMessage))
    {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(authConnectionName()));
    query.prepare("DELETE FROM user_permissions WHERE user_name = :user_name");
    bindUserName(&query, userName);

    if (!query.exec())
    {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    return true;
}

bool LTZKAuthManager::changePassword(
    const QString& userName,
    const QString& newPassword,
    QString* errorMessage)
{
    if (!ensureConnectedAndInitialized(errorMessage))
    {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(authConnectionName()));
    query.prepare(
        "UPDATE user_permissions SET password_hash = :password_hash "
        "WHERE user_name = :user_name");
    bindUserName(&query, userName);
    query.bindValue(":password_hash", hashPassword(newPassword));

    if (!query.exec())
    {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool LTZKAuthManager::changeUserRole(
    const QString& userName,
    AccessRole newRole,
    const QString& grantedBy,
    QString* errorMessage)
{
    if (!ensureConnectedAndInitialized(errorMessage))
    {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(authConnectionName()));
    query.prepare(
        "UPDATE user_permissions "
        "SET role = :role, granted_by = :granted_by, granted_at = CURRENT_TIMESTAMP "
        "WHERE user_name = :user_name");
    bindUserName(&query, userName);
    query.bindValue(":role", accessRoleToString(newRole));
    query.bindValue(":granted_by", grantedBy);

    if (!query.exec())
    {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool LTZKAuthManager::updateUserPermissions(
    const QString& userName,
    const QString& permissions,
    QString* errorMessage)
{
    if (!ensureConnectedAndInitialized(errorMessage))
    {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(authConnectionName()));
    query.prepare(
        "UPDATE user_permissions SET permissions = :permissions "
        "WHERE user_name = :user_name");
    bindUserName(&query, userName);
    query.bindValue(":permissions", permissions);

    if (!query.exec())
    {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool LTZKAuthManager::openDatabase(QString* errorMessage)
{
    QSqlDatabase db;
    if (QSqlDatabase::contains(authConnectionName()))
    {
        db = QSqlDatabase::database(authConnectionName());
    }
    else
    {
        db = QSqlDatabase::addDatabase("QPSQL", authConnectionName());
    }

    if (db.isOpen())
    {
        return true;
    }

    const QString host = readDbSetting("db/host", "localhost");
    const int port = readDbPort();
    const QString database = readDbSetting("db/database", "ltzk_platform");
    const QString user = readDbSetting("db/user", "YOUR_PASSWORD");
    const QString password = readDbSetting("db/password", "");

    db.setHostName(host);
    db.setPort(port);
    db.setDatabaseName(database);
    db.setUserName(user);
    db.setPassword(password);

    if (!db.open())
    {
        setError(
            errorMessage,
            zh(L"\x65E0\x6CD5\x8FDE\x63A5\x0020\x0050\x006F\x0073\x0074\x0047\x0049\x0053\x0020\x6570\x636E\x5E93\xFF0C\x8BF7\x68C0\x67E5\x0020") + QDir::toNativeSeparators(databaseConfigPath()) + zh(L"\x0020\x914D\x7F6E\x3002") + db.lastError().text());
        return false;
    }

    const QString schema = readDbSetting("db/schema", "public");
    if (!schema.isEmpty() && schema != QStringLiteral("public"))
    {
        QSqlQuery schemaQuery(db);
        if (!schemaQuery.exec(QStringLiteral("CREATE SCHEMA IF NOT EXISTS \"%1\"").arg(schema)))
        {
            setError(errorMessage, schemaQuery.lastError().text());
            return false;
        }

        QSqlQuery searchPathQuery(db);
        if (!searchPathQuery.exec(QStringLiteral("SET search_path TO \"%1\", public").arg(schema)))
        {
            setError(errorMessage, searchPathQuery.lastError().text());
            return false;
        }
    }

    return true;
}

bool LTZKAuthManager::initializeSchema(QString* errorMessage)
{
    QSqlDatabase db = QSqlDatabase::database(authConnectionName());
    const QStringList schemaSql = {
        "CREATE EXTENSION IF NOT EXISTS postgis",
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS user_permissions ("
            "id SERIAL PRIMARY KEY, "
            "user_name VARCHAR(200) NOT NULL UNIQUE, "
            "role VARCHAR(50) NOT NULL DEFAULT '%1', "
            "password_hash VARCHAR(128) NOT NULL DEFAULT '', "
            "permissions TEXT, "
            "granted_by VARCHAR(200), "
            "granted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
            "CONSTRAINT chk_user_permissions_role CHECK "
            "(role IN ('%1', '%2', '%3'))"
            ")").arg(roleDataOperator(), roleDataUser(), roleDatabaseAdmin()),
        "ALTER TABLE user_permissions ADD COLUMN IF NOT EXISTS password_hash VARCHAR(128) NOT NULL DEFAULT ''",
        "ALTER TABLE user_permissions ADD COLUMN IF NOT EXISTS permissions TEXT",
        "ALTER TABLE user_permissions ADD COLUMN IF NOT EXISTS granted_by VARCHAR(200)",
        "ALTER TABLE user_permissions ADD COLUMN IF NOT EXISTS granted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
    };

    for (const QString& sql : schemaSql)
    {
        QSqlQuery query(db);
        if (!query.exec(sql))
        {
            setError(errorMessage, query.lastError().text());
            return false;
        }
    }

    return true;
}

bool LTZKAuthManager::ensureDefaultAdmin(QString* errorMessage)
{
    QSqlQuery query(QSqlDatabase::database(authConnectionName()));
    query.prepare(
        "SELECT id, user_name, role, password_hash, permissions, granted_by, granted_at "
        "FROM user_permissions WHERE user_name = :user_name");
    query.bindValue(":user_name", QStringLiteral("admin"));

    if (!query.exec())
    {
        setError(errorMessage, query.lastError().text());
        return false;
    }

    if (!query.next())
    {
        QSqlQuery insertQuery(QSqlDatabase::database(authConnectionName()));
        insertQuery.prepare(
            "INSERT INTO user_permissions "
            "(user_name, role, password_hash, granted_by, granted_at) "
            "VALUES (:user_name, :role, :password_hash, :granted_by, CURRENT_TIMESTAMP)");
        insertQuery.bindValue(":user_name", QStringLiteral("admin"));
        insertQuery.bindValue(":role", accessRoleToString(AccessRole::DatabaseAdmin));
        insertQuery.bindValue(":password_hash", hashPassword(QStringLiteral("123456")));
        insertQuery.bindValue(":granted_by", QStringLiteral("system"));
        if (!insertQuery.exec())
        {
            setError(errorMessage, insertQuery.lastError().text());
            return false;
        }
        return true;
    }

    const UserPermission admin = userPermissionFromQuery(query);
    if (admin.passwordHash.isEmpty())
    {
        QSqlQuery passwordQuery(QSqlDatabase::database(authConnectionName()));
        passwordQuery.prepare(
            "UPDATE user_permissions SET password_hash = :password_hash "
            "WHERE user_name = :user_name");
        passwordQuery.bindValue(":password_hash", hashPassword(QStringLiteral("123456")));
        passwordQuery.bindValue(":user_name", QStringLiteral("admin"));
        if (!passwordQuery.exec())
        {
            setError(errorMessage, passwordQuery.lastError().text());
            return false;
        }
    }

    if (admin.role != AccessRole::DatabaseAdmin)
    {
        QSqlQuery roleQuery(QSqlDatabase::database(authConnectionName()));
        roleQuery.prepare(
            "UPDATE user_permissions "
            "SET role = :role, granted_by = :granted_by, granted_at = CURRENT_TIMESTAMP "
            "WHERE user_name = :user_name");
        roleQuery.bindValue(":role", accessRoleToString(AccessRole::DatabaseAdmin));
        roleQuery.bindValue(":granted_by", QStringLiteral("system"));
        roleQuery.bindValue(":user_name", QStringLiteral("admin"));
        if (!roleQuery.exec())
        {
            setError(errorMessage, roleQuery.lastError().text());
            return false;
        }
    }

    return true;
}

