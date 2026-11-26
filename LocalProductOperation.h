#pragma once

#include <QString>

#include "qgsmimedatautils.h"

class QgsMapCanvas;
class QWidget;

// 平台侧本地成果操作适配器。
//
// 本地文件没有数据库目录 ID / 产品 ID，不能复用成果管理插件的数据库
// ProductStorageDialog。这里保留数据目录控件的 URI 边界，在平台内完成
// 范围裁剪、主区裁剪以及本地操作的明确提示；数据库回调不经过本类。
class LocalProductOperation final
{
public:
    static bool dispatch(
        const QString& operation,
        const QgsMimeDataUtils::UriList& uris,
        QgsMapCanvas* canvas,
        QWidget* parent,
        QString* errorMessage = nullptr);
};
