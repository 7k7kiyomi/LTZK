#pragma once

#include <QCoreApplication>
#include <QVariantMap>

namespace LTZKSessionBridge
{
    inline constexpr const char* kPropertyName = "ltzkSharedUserSession";

    inline QVariantMap read()
    {
        const QCoreApplication* app = QCoreApplication::instance();
        return app ? app->property(kPropertyName).toMap() : QVariantMap();
    }

    inline void write(const QVariantMap& session)
    {
        if (QCoreApplication* app = QCoreApplication::instance())
        {
            app->setProperty(kPropertyName, session);
        }
    }
}
