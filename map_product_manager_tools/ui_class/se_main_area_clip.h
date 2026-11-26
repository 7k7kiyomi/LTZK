#ifndef SE_MAIN_AREA_CLIP_H
#define SE_MAIN_AREA_CLIP_H

#include <QDialog>
#include <QStringList>
#include <qgsrectangle.h>

#include "ui_main_area_clip.h"

class QgsVectorLayer;
class QgsCoordinateReferenceSystem;

// ============================================================
// 「按主区裁切导出」对话框（.ui 表单版）
//
// 对应纯 C++ 版 CSE_MainAreaExportDialog（se_data_list_export.h），
// 界面控件与行为完全一致，供外部系统集成调用：
//
//   CSE_MainAreaClipDialogUi dlg(parent);
//   dlg.setSourcePath(srcPath);          // 源数据路径（用于输出路径建议）
//   dlg.setAutoLoadAfterExport(true);
//   if (dlg.exec() == QDialog::Accepted)
//   {
//       QString     path = dlg.mainAreaPath();        // 主区数据文件
//       QStringList ids  = dlg.selectedFeatureIds();  // 选中要素 fid 列表
//       QString     out  = dlg.outputFilePath();      // 输出文件
//       double      scale = dlg.mapScaleDenominator();// 生效比例尺分母
//       ...
//       // 计算裁剪范围（主区所选要素合并外包矩形）：
//       QgsCoordinateReferenceSystem crs;
//       QgsRectangle clipRect = computeMainAreaClipRect(path, ids, &crs);
//       // ... 用 clipRect（crs 为主区数据坐标系）执行裁剪导出
//   }
// ============================================================
class CSE_MainAreaClipDialogUi : public QDialog
{
    Q_OBJECT

public:
    explicit CSE_MainAreaClipDialogUi(QWidget* parent = nullptr);
    ~CSE_MainAreaClipDialogUi() override;

    // 设置或获取源数据路径（可指向单文件或目录，用于输出文件路径建议）
    void setSourcePath(const QString& path) { d_srcPath = path; }
    QString sourcePath() const { return d_srcPath; }

    // 主区数据
    QString mainAreaPath() const;
    QString mainAreaField() const;
    QStringList selectedFeatureIds() const;

    // 输出设置
    // 输出路径：Shapefile 格式时为"保存文件夹"（每个成果一个 shp）；
    // GeoPackage/GDB 格式时为"保存文件"（所有成果写入该单一文件，各占一个图层）
    QString outputFilePath() const;
    QString encoding() const;               // "UTF-8" / "GBK" / "GB2312"
    QString format() const;                 // "ESRI Shapefile (.shp)" / "GeoPackage (.gpkg)" / "GDB (.gdb)"
    bool overwriteExisting() const;

    // 制图参数
    QString paperSize() const;              // "A2" / "自定义" 等（"计算"按钮可能写入"A0：893.0*654.0"组合文本）
    QString paperOrient() const;            // "自动" / "纵向" / "横向"
    QString standardScale() const;          // 生效比例尺字符串，如 "10000"
    bool    useCustomScale() const;
    double  mapScaleDenominator() const;
    double  paperWidthMm() const;           // 当前生效的纸张宽度（mm）
    double  paperHeightMm() const;          // 当前生效的纸张高度（mm）
    double  innerMapFrameWidthMm() const;   // 未启用内图廓时返回 0
    double  innerMapFrameHeightMm() const;  // 未启用内图廓时返回 0
    bool    enableInnerFrame() const;

    // 是否在导出后自动加载结果到地图（默认 true）
    void setAutoLoadAfterExport(bool on);
    bool autoLoadAfterExport() const;

private slots:
    void on_Button_BrowseMainArea_clicked();
    void on_comboBox_field_currentIndexChanged(int index);
    void on_Button_BrowseOutput_clicked();
    void on_comboBox_format_currentIndexChanged(int index);
    void on_comboBox_paperSize_currentIndexChanged(int index);
    void on_Button_ComputePaper_clicked();
    void on_Button_ComputeScale_clicked();
    void on_checkBox_customScale_toggled(bool checked);
    void on_checkBox_enableInner_toggled(bool checked);
    void on_Button_Export_clicked();
    void on_Button_Cancel_clicked();

private:
    Ui_SeMainAreaClipDialog ui;

    QString d_srcPath;
    // 缓存主区 layer，以便标识字段切换时无须重新读取整个 shp
    QgsVectorLayer* m_pMainAreaLayer = nullptr;

    void applyPaperSizeVisibility();        // "自定义"纸张时显示宽/高输入框
    void applyInnerFrameEnabled(bool enabled);
    void loadMainAreaFields();              // 选择主区文件后加载字段与要素列表
    void reloadFeatures();                  // 标识字段切换时重新填充要素列表
    QgsRectangle selectedMainAreaRect() const;  // 已选要素的合并外包矩形（主区 layer 原始 CRS）
    void computeScaleFromExtent();          // "自动计算"比例尺
    void computePaperFromExtent();          // "计算"纸张方向与大小
};

// 从主区矢量读取所选要素的合并外包矩形，作为统一裁剪范围。
// mainAreaPath: 主区 shapefile 路径；featureIds: 选中的要素 ID 字符串列表。
// outCrs（可选）：返回矩形所在的主区 layer 原始 CRS，供裁剪时把范围转换到源数据 CRS。
// 失败（路径无效/图层打不开）返回空矩形（isEmpty()）。
QgsRectangle computeMainAreaClipRect(const QString& mainAreaPath,
    const QStringList& featureIds, QgsCoordinateReferenceSystem* outCrs = nullptr);

#endif // SE_MAIN_AREA_CLIP_H
