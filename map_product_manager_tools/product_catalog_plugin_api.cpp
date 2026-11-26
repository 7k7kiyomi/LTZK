#define LTZK_PRODUCT_CATALOG_PLUGIN_BUILD
#include "../product_catalog_plugin_api.h"

#include "map_product_manager_tools.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaObject>
#include <QThread>
#include <QVariantList>
#include <QVariantMap>
#include <QtGlobal>

#include <limits>

namespace
{
    bool buildProductCatalogRequest(
        const LTZKProductCatalogOperationRequest& source,
        QVariantMap& target)
    {
        if (source.apiVersion != LTZK_PRODUCT_CATALOG_API_VERSION ||
            !source.operationUtf8 ||
            source.directoryId < 0 ||
            source.productId < 0)
        {
            return false;
        }

        const QString operation = QString::fromUtf8(source.operationUtf8).trimmed();
        if (operation != QStringLiteral("create-directory") &&
            operation != QStringLiteral("delete-directory") &&
            operation != QStringLiteral("rename-directory") &&
            operation != QStringLiteral("export") &&
            operation != QStringLiteral("range") &&
            operation != QStringLiteral("main-area") &&
            operation != QStringLiteral("theme") &&
            operation != QStringLiteral("map") &&
            operation != QStringLiteral("import"))
        {
            return false;
        }

        if (source.productIdCount > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
            (source.productIdCount > 0 && !source.productIds))
        {
            return false;
        }

        target.insert(QStringLiteral("operation"), operation);
        target.insert(QStringLiteral("directoryId"), source.directoryId);
        target.insert(QStringLiteral("productId"), source.productId);

        QVariantList productIds;
        productIds.reserve(static_cast<int>(source.productIdCount));
        for (uint32_t index = 0; index < source.productIdCount; ++index)
        {
            productIds.append(static_cast<qlonglong>(source.productIds[index]));
        }
        target.insert(QStringLiteral("productIds"), productIds);

        if (source.connectionJsonUtf8 && *source.connectionJsonUtf8)
        {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(
                QByteArray(source.connectionJsonUtf8), &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject())
            {
                return false;
            }
            target.insert(
                QStringLiteral("connection"),
                document.object().toVariantMap());
        }

        return true;
    }
}

extern "C" Q_DECL_EXPORT int ltzk_product_catalog_open_operation(
    const LTZKProductCatalogOperationRequest* request)
{
    if (!request)
    {
        return LTZK_PRODUCT_CATALOG_INVALID_ARGUMENT;
    }

    CMapProductManagerPlugin* plugin = CMapProductManagerPlugin::currentInstance();
    if (!plugin)
    {
        return LTZK_PRODUCT_CATALOG_NOT_INITIALIZED;
    }

    QVariantMap requestMap;
    if (!buildProductCatalogRequest(*request, requestMap))
    {
        const QString operation = request->operationUtf8
            ? QString::fromUtf8(request->operationUtf8).trimmed()
            : QString();
        if (operation == QStringLiteral("range") ||
            operation == QStringLiteral("main-area"))
        {
            return LTZK_PRODUCT_CATALOG_INVALID_ARGUMENT;
        }
        return LTZK_PRODUCT_CATALOG_UNSUPPORTED_OPERATION;
    }

	if (QThread::currentThread() == plugin->thread())
	{
		plugin->executeProductCatalogRequest(requestMap);
		return LTZK_PRODUCT_CATALOG_ACCEPTED;
	}

	// 使用 Qt 元对象中的槽名排队，不把插件 DLL 内的 lambda 代码留在
	// 事件队列中；插件卸载时 Qt 可以安全移除目标对象的待处理调用。
	if (!QMetaObject::invokeMethod(
		plugin,
		"executeProductCatalogRequest",
		Qt::QueuedConnection,
		Q_ARG(QVariantMap, requestMap)))
	{
        return LTZK_PRODUCT_CATALOG_DISPATCH_FAILED;
    }
    return LTZK_PRODUCT_CATALOG_QUEUED;
}
