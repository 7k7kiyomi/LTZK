#ifndef MAP_PRODUCT_CLIP_ENGINE_H
#define MAP_PRODUCT_CLIP_ENGINE_H

#include <QList>
#include <QString>
#include <QStringList>
#include <functional>
#include <qgsrectangle.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsgeometry.h>
#include <qgisinterface.h>

#include "product_metadata.h"

// ============================================================
// 成果裁剪导出执行引擎
//
// 支持：
//   1) 按范围（外包矩形，参考「按范围导出」对话框）
//   2) 按主区（任意多边形，参考「按主区裁切导出」对话框）
//
// 对库内产品执行：
//   - 矢量：以 PostGIS 图层为源，逐要素与裁剪多边形求交后写出目标矢量文件
//           （Shapefile / GeoPackage，保持源坐标系与属性）
//   - 栅格：从 BLOB 导出临时原文件，用 GDAL 按裁剪范围外接矩形切出目标
//           GeoTIFF（保持源坐标系与分辨率，无重采样）
//   - 其它类型（CAD/AI/PDF/文档等）：跳过并返回错误信息
//
// 输出约定：
//   - 单产品：直接使用 explicitOutputFile 指定目标文件；
//   - 多产品：explicitOutputFile 留空，在 outputDir 下按 <产品名> 自动命名
//     （矢量 <产品名>.shp / .gpkg，栅格 <产品名>.tif）；
//   - 合并模式（mergeLayersIntoOneFile=true，GPKG/GDB）：所有矢量成果作为
//     独立图层写入 explicitOutputFile 指向的同一个容器文件，图层名取产品名；
//     栅格无法写入矢量容器，仍按 outputDir 输出 <产品名>.tif。
// ============================================================
namespace MapProductClipEngine
{
    struct ClipOptions
    {
        // 裁剪多边形（矩形范围在调用方先转成面）。所在坐标系为 clipCrs；
        // clipCrs 无效时视为与每个源数据的坐标系一致，不进行换算
        QgsGeometry clipGeom;
        QgsCoordinateReferenceSystem clipCrs;

        // 输出目录（多产品导出使用）
        QString outputDir;
        // 单产品导出时直接写该文件（路径带扩展名，如 .shp/.gpkg/.tif）
        QString explicitOutputFile;

        // 合并写入单一矢量容器文件（GPKG/GDB）：为 true 时所有矢量成果都写入
        // explicitOutputFile 指向的同一个文件，每个成果一个图层（图层名=产品名）；
        // 此时 explicitOutputFile 须带正确扩展名（.gpkg/.gdb）
        bool mergeLayersIntoOneFile = false;

        // 矢量输出
        QString vectorDriver = QStringLiteral("ESRI Shapefile"); // "ESRI Shapefile" / "GPKG" / "OpenFileGDB"
        QString fileEncoding = QStringLiteral("UTF-8");           // "UTF-8" / "GBK" / "GB2312"

        bool overwriteExisting = false;   // 已存在同名文件时直接覆盖
        bool autoLoadToMap = false;       // 导出成功后自动加载到地图
        QgisInterface* qgisIface = nullptr;

        // 可选进度回调（在调用方线程内同步调用，界面可据其刷新进度并可取消）：
        //   done    —— 当前成果序号（0 起）
        //   total   —— 成果总数
        //   message —— 当前阶段文字（含成果名、序号与处理细节，可直接展示）
        // 返回 false 表示用户请求取消，引擎将中止处理，未完成的成果按"已取消"处理。
        std::function<bool(int done, int total, const QString& message)> progress;
    };

    struct ClipResult
    {
        QString productName;
        QString filePath;   // 成功后的实际输出文件
        QString note;       // 补充说明（警告等，可为空）
        QString error;      // 空表示成功
        bool ok = false;
    };

    // 按裁剪多边形执行（矩形范围/主区多边形均适用）
    QList<ClipResult> clipProducts(const QList<ProductMetadata>& products,
                                   const ClipOptions& opt);
}

#endif // MAP_PRODUCT_CLIP_ENGINE_H
