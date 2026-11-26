#include "PlatformMenuDatabaseConnectionObserver.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QObject>
#include <QPointer>
#include <QSpinBox>

namespace
{
    template <typename T>
    T* field(QDialog* dialog, const char* name)
    {
        return dialog->findChild<T*>(QString::fromLatin1(name));
    }
}

bool PlatformMenuDatabaseConnectionObserver::watch(
    QDialog* dialog,
    QObject* receiver,
    ConnectionAccepted onAccepted)
{
    if (!dialog || !receiver || !onAccepted ||
        dialog->objectName() != QStringLiteral("DbConfigDialog") ||
        !field<QLineEdit>(dialog, "mHostEdit") ||
        !field<QSpinBox>(dialog, "mPortSpin") ||
        !field<QLineEdit>(dialog, "mDbNameEdit") ||
        !field<QComboBox>(dialog, "mSchemaCombo") ||
        !field<QLineEdit>(dialog, "mUserEdit") ||
        !field<QLineEdit>(dialog, "mPasswordEdit") ||
        !field<QCheckBox>(dialog, "mSavePwdCheck") ||
        dialog->property("ltzkMenuDatabaseConnectionObserved").toBool())
    {
        return false;
    }

    dialog->setProperty("ltzkMenuDatabaseConnectionObserved", true);
    QPointer<QDialog> guardedDialog(dialog);
    QObject::connect(dialog, &QDialog::accepted, receiver,
        [guardedDialog, onAccepted]()
    {
        if (!guardedDialog)
        {
            return;
        }
        QDialog* acceptedDialog = guardedDialog.data();
        const auto* host = field<QLineEdit>(acceptedDialog, "mHostEdit");
        const auto* port = field<QSpinBox>(acceptedDialog, "mPortSpin");
        const auto* database = field<QLineEdit>(acceptedDialog, "mDbNameEdit");
        const auto* schema = field<QComboBox>(acceptedDialog, "mSchemaCombo");
        const auto* user = field<QLineEdit>(acceptedDialog, "mUserEdit");
        const auto* password = field<QLineEdit>(acceptedDialog, "mPasswordEdit");
        const auto* remember = field<QCheckBox>(acceptedDialog, "mSavePwdCheck");
        if (!host || !port || !database || !schema || !user || !password || !remember ||
            host->text().trimmed().isEmpty() ||
            database->text().trimmed().isEmpty() ||
            user->text().trimmed().isEmpty())
        {
            return;
        }

        QVariantMap parameters;
        parameters.insert(QStringLiteral("host"), host->text().trimmed());
        parameters.insert(QStringLiteral("port"), port->value());
        parameters.insert(QStringLiteral("database"), database->text().trimmed());
        const QString schemaName = schema->currentText().trimmed();
        parameters.insert(
            QStringLiteral("schema"),
            schemaName.isEmpty() ? QStringLiteral("public") : schemaName);
        parameters.insert(QStringLiteral("user"), user->text().trimmed());
        parameters.insert(QStringLiteral("password"), password->text());
        const bool remembered = remember->isChecked();
        parameters.insert(QStringLiteral("savePassword"), remembered);
        onAccepted(parameters, remembered);
    });
    return true;
}

bool PlatformMenuDatabaseConnectionObserver::isConnectionDialogVisible()
{
    for (QWidget* window : QApplication::topLevelWidgets())
    {
        const QDialog* dialog = qobject_cast<QDialog*>(window);
        if (dialog && dialog->isVisible() &&
            dialog->objectName() == QStringLiteral("DbConfigDialog"))
        {
            return true;
        }
    }
    return false;
}
