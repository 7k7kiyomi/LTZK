#include "PlatformMenuDatabaseConnectionObserver.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QVariantMap>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QDialog dialog;
    dialog.setObjectName(QStringLiteral("DbConfigDialog"));

    QLineEdit host(&dialog);
    host.setObjectName(QStringLiteral("mHostEdit"));
    host.setText(QStringLiteral("127.0.0.1"));
    QSpinBox port(&dialog);
    port.setObjectName(QStringLiteral("mPortSpin"));
    port.setValue(5432);
    QLineEdit database(&dialog);
    database.setObjectName(QStringLiteral("mDbNameEdit"));
    database.setText(QStringLiteral("database_b"));
    QComboBox schema(&dialog);
    schema.setObjectName(QStringLiteral("mSchemaCombo"));
    schema.setEditable(true);
    schema.setCurrentText(QStringLiteral("public"));
    QLineEdit user(&dialog);
    user.setObjectName(QStringLiteral("mUserEdit"));
    user.setText(QStringLiteral("YOUR_PASSWORD"));
    QLineEdit password(&dialog);
    password.setObjectName(QStringLiteral("mPasswordEdit"));
    password.setText(QStringLiteral("session-only-secret"));
    QCheckBox remember(&dialog);
    remember.setObjectName(QStringLiteral("mSavePwdCheck"));
    remember.setChecked(false);

    dialog.setAttribute(Qt::WA_DontShowOnScreen);
    dialog.show();
    app.processEvents();
    if (!PlatformMenuDatabaseConnectionObserver::isConnectionDialogVisible())
    {
        return 5;
    }

    QObject receiver;
    QVariantMap observed;
    bool observedRemember = true;
    int count = 0;
    if (!PlatformMenuDatabaseConnectionObserver::watch(
            &dialog, &receiver,
            [&](const QVariantMap& values, bool remembered) {
                observed = values;
                observedRemember = remembered;
                ++count;
            }))
    {
        return 1;
    }
    if (PlatformMenuDatabaseConnectionObserver::watch(
            &dialog, &receiver,
            [&](const QVariantMap&, bool) { ++count; }))
    {
        return 3;
    }

    dialog.reject();
    if (count != 0)
    {
        return 4;
    }
    if (PlatformMenuDatabaseConnectionObserver::isConnectionDialogVisible())
    {
        return 6;
    }
    dialog.accept();
    if (count != 1 || observedRemember ||
        observed.value(QStringLiteral("database")).toString() != QStringLiteral("database_b") ||
        observed.value(QStringLiteral("password")).toString() != QStringLiteral("session-only-secret") ||
        !observed.contains(QStringLiteral("savePassword")) ||
        observed.value(QStringLiteral("savePassword")).toBool())
    {
        return 2;
    }
    remember.setChecked(true);
    dialog.accept();
    if (count != 2 || !observedRemember ||
        !observed.value(QStringLiteral("savePassword")).toBool())
    {
        return 7;
    }
    return 0;
}
