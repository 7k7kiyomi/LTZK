#pragma once

#include <QVariantMap>

#include <functional>

class QDialog;
class QObject;

namespace PlatformMenuDatabaseConnectionObserver
{
    using ConnectionAccepted = std::function<void(const QVariantMap&, bool)>;

    bool watch(QDialog* dialog, QObject* receiver, ConnectionAccepted onAccepted);
    bool isConnectionDialogVisible();
}
