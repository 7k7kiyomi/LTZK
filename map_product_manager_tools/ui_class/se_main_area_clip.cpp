/*--------------SE---------------*/
#include "se_main_area_clip.h"

/*--------------QT---------------*/
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QListWidget>
#include <QListWidgetItem>
#include <cmath>
#include <algorithm>

/*--------------QGIS---------------*/
#include <qgsvectorlayer.h>
#include <qgsfields.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeatureid.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsproject.h>
#include <qgsunittypes.h>


// ISO A 系列纸张（宽=长边、高=短边，单位 mm）
struct SeIsoPaper { const char* name; double w; double h; };
static const SeIsoPaper kSeIsoPapers[] = {
    { "A0", 1189, 841 },
    { "A1", 841, 594 },
    { "A2", 594, 420 },
    { "A3", 420, 297 },
    { "A4", 297, 210 },
};

// 解析纸张规格文本："A0" / "A0：893.0*654.0" / "A1：677.0*496.0"。
// 带组合文本时只取纸张系列前缀，后缀为展示用的绘图尺寸。
// 返回 true 且填充 w/h（宽=长边、高=短边）；"自定义"或未知文本返回 false
static bool seParsePaperSpec(const QString& text, double& w, double& h)
{
    QString sz = text.trimmed();
    int sep = sz.indexOf(QStringLiteral("："));
    if (sep < 0) sep = sz.indexOf(QStringLiteral(":"));
    if (sep >= 0) sz = sz.left(sep).trimmed();
    const QString upper = sz.toUpper();
    for (const SeIsoPaper& p : kSeIsoPapers)
        if (upper == QLatin1String(p.name)) { w = p.w; h = p.h; return true; }
    return false;
}

// 把比例尺分母向上取整为"整比例尺"（1/2/5 * 10^n 系列），保证无小数且接近计算值
static double roundScaleToNice(double scale)
{
    if (scale <= 0) return 1;
    double mag = std::pow(10.0, std::floor(std::log10(scale)));
    double m = scale / mag; // 1<=m<10
    double nice;
    if (m <= 1.0) nice = 1.0;
    else if (m <= 2.0) nice = 2.0;
    else if (m <= 5.0) nice = 5.0;
    else nice = 10.0;
    return nice * mag;
}


CSE_MainAreaClipDialogUi::CSE_MainAreaClipDialogUi(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    // 注意：按钮/下拉框信号已由 setupUi() 中的 connectSlotsByName 自动连接
    //（on_Button_BrowseMainArea_clicked / on_comboBox_field_currentIndexChanged 等），
    // 不需要手动 connect，否则会造成槽函数被调用两次。

    // 初始状态与纯 C++ 版 buildUi() 后一致：
    // 1) 自定义比例尺输入框默认隐藏（勾选"使用自定义"后显示）
    ui.label_customScale->setVisible(false);
    ui.lineEdit_customScale->setVisible(false);
    // 2) 纸张默认 A2 → 自定义宽/高隐藏
    applyPaperSizeVisibility();
    // 3) 未启用内图廓 → 宽/高输入框置灰
    applyInnerFrameEnabled(false);
    // 4) 默认格式为 Shapefile → 输出为"文件夹"，标签随之联动
    on_comboBox_format_currentIndexChanged(ui.comboBox_format->currentIndex());
}

CSE_MainAreaClipDialogUi::~CSE_MainAreaClipDialogUi()
{
    if (m_pMainAreaLayer) delete m_pMainAreaLayer;
}

void CSE_MainAreaClipDialogUi::on_Button_BrowseMainArea_clicked()
{
    QString f = QFileDialog::getOpenFileName(this, tr("选择主区矢量数据"), QString(),
        tr("矢量数据 (*.shp *.geojson *.json);;所有文件 (*.*)"));
    if (!f.isEmpty())
    {
        ui.lineEdit_mainArea->setText(f);
        loadMainAreaFields();
    }
}

void CSE_MainAreaClipDialogUi::loadMainAreaFields()
{
    ui.comboBox_field->blockSignals(true);
    ui.comboBox_field->clear();
    ui.listWidget_features->clear();
    // 清掉旧缓存
    if (m_pMainAreaLayer)
    {
        delete m_pMainAreaLayer;
        m_pMainAreaLayer = nullptr;
    }
    QString path = ui.lineEdit_mainArea->text();
    if (path.isEmpty() || !QFileInfo::exists(path))
    {
        ui.comboBox_field->blockSignals(false);
        return;
    }
    QgsVectorLayer* pLayer = new QgsVectorLayer(path, QFileInfo(path).completeBaseName(), "ogr");
    if (!pLayer || !pLayer->isValid())
    {
        if (pLayer) delete pLayer;
        ui.comboBox_field->blockSignals(false);
        return;
    }
    const QgsFields flds = pLayer->fields();
    for (int i = 0; i < flds.count(); ++i)
        ui.comboBox_field->addItem(flds[i].name(), flds[i].name());

    // 把 layer 缓存为成员，标识字段切换时重新填充列表，不用每次打开 shp
    m_pMainAreaLayer = pLayer;

    ui.comboBox_field->blockSignals(false);
    reloadFeatures();
}

void CSE_MainAreaClipDialogUi::reloadFeatures()
{
    ui.listWidget_features->clear();
    if (!m_pMainAreaLayer || !m_pMainAreaLayer->isValid())
        return;

    const QString fieldName = ui.comboBox_field->currentText();
    if (fieldName.isEmpty())
        return;

    // 在图层字段中查找当前标识字段对应的索引
    int idxField = m_pMainAreaLayer->fields().lookupField(fieldName);
    if (idxField < 0)
        return;

    // 枚举全部要素，按当前字段构造显示文本：
    //   [序号] <fieldValue>   （首行）
    //   如果 fid 转 QString 后和上面不同，作为副标题（tooltip）
    QgsFeatureIterator it = m_pMainAreaLayer->getFeatures();
    QgsFeature feat;
    int dispIdx = 0;
    while (it.nextFeature(feat))
    {
        QString fid = QString::number(feat.id());
        QString name = feat.attribute(idxField).toString();
        QString label = tr("[%1] ").arg(dispIdx) + (name.isEmpty() ? fid : name);
        QListWidgetItem* item = new QListWidgetItem(label, ui.listWidget_features);
        item->setData(Qt::UserRole, fid);
        item->setToolTip(tr("fid=%1，标识字段=%2").arg(fid).arg(name));
        ++dispIdx;
    }
}

void CSE_MainAreaClipDialogUi::on_comboBox_field_currentIndexChanged(int /*index*/)
{
    reloadFeatures();
}

// 格式切换时联动输出标签：SHP 输出到文件夹，GPKG/GDB 输出到单一文件
void CSE_MainAreaClipDialogUi::on_comboBox_format_currentIndexChanged(int /*index*/)
{
    const QString fmt = ui.comboBox_format->currentText();
    const bool toFile = fmt.contains(QStringLiteral("GeoPackage"), Qt::CaseInsensitive)
                        || fmt.contains(QStringLiteral("GDB"), Qt::CaseInsensitive);
    ui.label_output->setText(toFile ? tr("输出文件：") : tr("输出文件夹："));
}

void CSE_MainAreaClipDialogUi::on_Button_BrowseOutput_clicked()
{
    const QString fmt = ui.comboBox_format->currentText();
    const bool toFile = fmt.contains(QStringLiteral("GeoPackage"), Qt::CaseInsensitive)
                        || fmt.contains(QStringLiteral("GDB"), Qt::CaseInsensitive);

    // 建议路径：<源文件目录>/<源文件名>_mainArea
    const QFileInfo src(d_srcPath);
    const QString baseDir = src.absolutePath();
    QString suggestBase = baseDir;
    if (!suggestBase.isEmpty()) suggestBase += QLatin1String("/");
    suggestBase += src.completeBaseName().isEmpty()
        ? QStringLiteral("clip") : src.completeBaseName();
    suggestBase += QStringLiteral("_mainArea");
    const QString cur = ui.lineEdit_output->text().trimmed();

    if (toFile)
    {
        // GPKG / GDB：选择保存文件，所有裁切成果写入该单一文件
        const bool isGpkg = fmt.contains(QStringLiteral("GeoPackage"), Qt::CaseInsensitive);
        const QString suffix = isGpkg ? QStringLiteral(".gpkg") : QStringLiteral(".gdb");
        QString suggest = cur;
        if (suggest.isEmpty() || QFileInfo(suggest).isDir())
            suggest = suggestBase + suffix;
        else if (!suggest.endsWith(suffix, Qt::CaseInsensitive))
            suggest = QFileInfo(suggest).absolutePath() + "/"
                      + QFileInfo(suggest).completeBaseName() + suffix;

        const QString f = QFileDialog::getSaveFileName(this,
            isGpkg ? tr("选择输出 GeoPackage 文件") : tr("选择输出 GDB 文件"),
            suggest,
            isGpkg ? tr("GeoPackage (*.gpkg)") : tr("文件地理数据库 (*.gdb)"));
        if (f.isEmpty()) return;
        QString finalFile = f;
        if (!finalFile.endsWith(suffix, Qt::CaseInsensitive)) finalFile += suffix;
        ui.lineEdit_output->setText(QDir::toNativeSeparators(finalFile));
    }
    else
    {
        // SHP：选择保存文件夹，每个成果在该目录下导出为独立 shp
        QString startDir = cur;
        if (startDir.isEmpty()) startDir = baseDir;
        else if (!QFileInfo(startDir).isDir()) startDir = QFileInfo(startDir).absolutePath();
        if (startDir.isEmpty()) startDir = QDir::homePath();

        const QString dir = QFileDialog::getExistingDirectory(this,
            tr("选择保存文件夹（每个成果导出为独立 shp）"), startDir);
        if (!dir.isEmpty())
            ui.lineEdit_output->setText(QDir::toNativeSeparators(dir));
    }
}

void CSE_MainAreaClipDialogUi::applyPaperSizeVisibility()
{
    bool custom = (ui.comboBox_paperSize->currentText() == tr("自定义"));
    ui.label_customW->setVisible(custom);
    ui.lineEdit_customW->setVisible(custom);
    ui.label_customH->setVisible(custom);
    ui.lineEdit_customH->setVisible(custom);
}

void CSE_MainAreaClipDialogUi::on_comboBox_paperSize_currentIndexChanged(int /*index*/)
{
    applyPaperSizeVisibility();
}

void CSE_MainAreaClipDialogUi::applyInnerFrameEnabled(bool enabled)
{
    ui.label_innerW->setEnabled(enabled);
    ui.lineEdit_innerW->setEnabled(enabled);
    ui.label_innerH->setEnabled(enabled);
    ui.lineEdit_innerH->setEnabled(enabled);
}

void CSE_MainAreaClipDialogUi::on_checkBox_enableInner_toggled(bool checked)
{
    applyInnerFrameEnabled(checked);
}

void CSE_MainAreaClipDialogUi::on_checkBox_customScale_toggled(bool checked)
{
    // 勾选"使用自定义"：显示自定义输入框、禁用标准下拉框与自动计算；否则反之
    ui.label_customScale->setVisible(checked);
    ui.lineEdit_customScale->setVisible(checked);
    ui.comboBox_standardScale->setEnabled(!checked);
    ui.Button_ComputeScale->setEnabled(!checked);
    if (checked)
    {
        // 用当前标准值填充自定义输入框，方便微调
        QString cur = ui.comboBox_standardScale->currentText().trimmed();
        if (cur.isEmpty()) cur = ui.lineEdit_customScale->text();
        if (!cur.isEmpty()) ui.lineEdit_customScale->setText(cur);
    }
}

// 当前已选要素的合并外包矩形（在主区 layer 的原始 CRS 下）
QgsRectangle CSE_MainAreaClipDialogUi::selectedMainAreaRect() const
{
    QgsRectangle rect;
    if (!m_pMainAreaLayer || !m_pMainAreaLayer->isValid())
        return rect;
    const int n = ui.listWidget_features->count();
    QgsFeatureIds idSet;
    for (int i = 0; i < n; ++i)
    {
        QListWidgetItem* item = ui.listWidget_features->item(i);
        if (item && item->isSelected())
        {
            bool ok = false;
            long long v = item->data(Qt::UserRole).toString().toLongLong(&ok);
            if (ok) idSet.insert(v);
        }
    }
    if (idSet.isEmpty())
        return rect;

    QgsFeatureRequest req;
    req.setFilterFids(idSet);
    QgsFeatureIterator it = m_pMainAreaLayer->getFeatures(req);
    QgsFeature feat;
    bool first = true;
    while (it.nextFeature(feat))
    {
        QgsGeometry g = feat.geometry();
        if (g.isNull()) continue;
        QgsRectangle b = g.boundingBox();
        if (b.isEmpty()) continue;
        if (first) { rect = b; first = false; }
        else rect.combineExtentWith(b);
    }
    return rect;
}

// 当前生效的纸张宽/高(mm)。A0~A4 固定（兼容"计算"按钮写入的"A0：893.0*654.0"组合文本，
// 取纸张系列前缀对应的 ISO 尺寸）；"自定义"读输入框
double CSE_MainAreaClipDialogUi::paperWidthMm() const
{
    double w, h;
    if (seParsePaperSpec(paperSize(), w, h)) return w;
    return ui.lineEdit_customW->text().toDouble();
}

double CSE_MainAreaClipDialogUi::paperHeightMm() const
{
    double w, h;
    if (seParsePaperSpec(paperSize(), w, h)) return h;
    return ui.lineEdit_customH->text().toDouble();
}

// 依据当前纸张 + 选中主区外包矩形自动计算比例尺，写回标准下拉框
void CSE_MainAreaClipDialogUi::computeScaleFromExtent()
{
    QgsRectangle rect = selectedMainAreaRect();
    if (rect.isEmpty())
    {
        QMessageBox::information(this, tr("自动计算比例尺"),
            tr("请先在要素列表中选中至少一个要素作为主区，再点击自动计算。"));
        return;
    }
    const double W = rect.width();  // 地面宽度（主区 CRS 单位，通常为米）
    const double H = rect.height(); // 地面高度
    if (W <= 0 || H <= 0)
    {
        QMessageBox::warning(this, tr("自动计算比例尺"), tr("主区范围无效，无法计算。"));
        return;
    }

    // 纸幅（mm）换算为米，预留页边距（四周各留 10mm）
    double paperW_m = std::max(1.0, paperWidthMm() - 20.0) / 1000.0;
    double paperH_m = std::max(1.0, paperHeightMm() - 20.0) / 1000.0;

    // 所需比例尺分母 = 地面尺寸 / 纸面尺寸（取两个方向较大者）
    double scaleW = W / paperW_m;
    double scaleH = H / paperH_m;
    double scale = roundScaleToNice(std::max(scaleW, scaleH));

    ui.comboBox_standardScale->setCurrentText(QString::number(static_cast<long long>(scale)));
    QMessageBox::information(this, tr("自动计算比例尺"),
        tr("主区外包矩形：%1 m × %2 m\n纸幅：%3 mm × %4 mm\n\n推荐标准比例尺：1:%5")
        .arg(QString::number(W, 'f', 1))
        .arg(QString::number(H, 'f', 1))
        .arg(QString::number(paperWidthMm(), 'f', 0))
        .arg(QString::number(paperHeightMm(), 'f', 0))
        .arg(QString::number(scale, 'f', 0)));
}

void CSE_MainAreaClipDialogUi::on_Button_ComputeScale_clicked()
{
    computeScaleFromExtent();
}

// "计算"按钮：依据主区外包矩形 + 当前比例尺，自动确定纸张方向与大小
// 绘图尺寸(mm) = 地面尺寸(米) / 比例尺 × 1000
// 方向：宽 ≥ 高 → 横向，否则纵向
// 纸张：选能容纳绘图尺寸的最小 ISO A 系列，combo 文本置为 "A0：893.0*654.0"
//（纸张类型后追加绘图尺寸）；超过 A0 时落"自定义"并填绘图尺寸
void CSE_MainAreaClipDialogUi::computePaperFromExtent()
{
    QgsRectangle rect = selectedMainAreaRect();
    if (rect.isEmpty())
    {
        QMessageBox::information(this, tr("计算纸张"),
            tr("请先在要素列表中选中至少一个要素作为主区，再点击计算。"));
        return;
    }
    const double scale = mapScaleDenominator();
    if (scale <= 0)
    {
        QMessageBox::warning(this, tr("计算纸张"), tr("比例尺无效，无法计算。"));
        return;
    }

    // 主区外包矩形 → 地面尺寸（米）。主区数据通常为平面投影（米）；
    // 经纬度时转换到 Web 墨卡托（米）兜底，英尺/英里/海里换算为米
    double groundW = rect.width();
    double groundH = rect.height();
    if (m_pMainAreaLayer && m_pMainAreaLayer->crs().isValid())
    {
        const QgsCoordinateReferenceSystem crs = m_pMainAreaLayer->crs();
        if (crs.mapUnits() == QgsUnitTypes::DistanceDegrees)
        {
            QgsCoordinateTransform tr(crs,
                QgsCoordinateReferenceSystem::fromEpsgId(3857), QgsProject::instance());
            if (tr.isValid())
            {
                QgsRectangle mRect;
                try { mRect = tr.transformBoundingBox(rect); }
                catch (...) { }
                if (!mRect.isEmpty()) { groundW = mRect.width(); groundH = mRect.height(); }
            }
        }
        else if (crs.mapUnits() == QgsUnitTypes::DistanceFeet)
        { groundW *= 0.3048; groundH *= 0.3048; }
        else if (crs.mapUnits() == QgsUnitTypes::DistanceMiles)
        { groundW *= 1609.344; groundH *= 1609.344; }
        else if (crs.mapUnits() == QgsUnitTypes::DistanceNauticalMiles)
        { groundW *= 1852.0; groundH *= 1852.0; }
    }
    if (groundW <= 0 || groundH <= 0)
    {
        QMessageBox::warning(this, tr("计算纸张"), tr("主区范围无效，无法计算。"));
        return;
    }

    const double drawW = groundW / scale * 1000.0; // 绘图宽(mm)
    const double drawH = groundH / scale * 1000.0; // 绘图高(mm)

    // 方向：宽 ≥ 高 → 横向
    const bool landscape = (drawW >= drawH);
    ui.comboBox_orient->setCurrentText(landscape ? tr("横向") : tr("纵向"));

    // 先把 5 个标准纸张项文本恢复为纯名称，清掉上次"计算"写入的组合文本
    for (int i = 0; i < 5 && i < ui.comboBox_paperSize->count(); ++i)
        ui.comboBox_paperSize->setItemText(i, QLatin1String(kSeIsoPapers[i].name));

    // 选能容纳绘图尺寸的最小 ISO A 系列：从 A4（最小）往 A0（最大）找，
    // 第一个能容纳的即最小适配纸张（若从 A0 起找会永远命中 A0）
    const int paperCount = int(sizeof(kSeIsoPapers) / sizeof(kSeIsoPapers[0]));
    int chosen = -1;
    for (int i = paperCount - 1; i >= 0; --i)
    {
        double pw = landscape ? kSeIsoPapers[i].w : kSeIsoPapers[i].h; // 可用宽
        double ph = landscape ? kSeIsoPapers[i].h : kSeIsoPapers[i].w; // 可用高
        if (drawW <= pw + 1e-9 && drawH <= ph + 1e-9) { chosen = i; break; }
    }

    if (chosen < 0)
    {
        // 超过 A0：落"自定义"并填绘图尺寸
        ui.comboBox_paperSize->setCurrentText(tr("自定义"));
        ui.lineEdit_customW->setText(QString::number(drawW, 'f', 1));
        ui.lineEdit_customH->setText(QString::number(drawH, 'f', 1));
        applyPaperSizeVisibility();
        QMessageBox::information(this, tr("计算纸张"),
            tr("主区绘图尺寸 %1 mm × %2 mm 超出 A0，已按自定义纸张设置。")
            .arg(QString::number(drawW, 'f', 1)).arg(QString::number(drawH, 'f', 1)));
        return;
    }

    // 纸张类型后追加绘图尺寸，如 "A0：893.0*654.0"
    const QString label = QString("%1：%2*%3")
        .arg(QLatin1String(kSeIsoPapers[chosen].name))
        .arg(QString::number(drawW, 'f', 1))
        .arg(QString::number(drawH, 'f', 1));
    ui.comboBox_paperSize->setCurrentIndex(chosen);
    ui.comboBox_paperSize->setItemText(chosen, label);
    applyPaperSizeVisibility();
}

void CSE_MainAreaClipDialogUi::on_Button_ComputePaper_clicked()
{
    computePaperFromExtent();
}

void CSE_MainAreaClipDialogUi::on_Button_Export_clicked()
{
    if (ui.lineEdit_mainArea->text().isEmpty())
    {
        QMessageBox::warning(this, tr("按主区裁切导出"), tr("请先选择主区数据。"));
        return;
    }
    if (ui.listWidget_features->selectedItems().isEmpty())
    {
        QMessageBox::warning(this, tr("按主区裁切导出"), tr("请至少选择一个要素作为主区。"));
        return;
    }
    if (ui.lineEdit_output->text().isEmpty())
    {
        const QString fmt = ui.comboBox_format->currentText();
        const bool toFile = fmt.contains(QStringLiteral("GeoPackage"), Qt::CaseInsensitive)
                            || fmt.contains(QStringLiteral("GDB"), Qt::CaseInsensitive);
        QMessageBox::warning(this, tr("按主区裁切导出"),
            toFile ? tr("请设置输出文件路径。") : tr("请设置输出文件夹路径。"));
        return;
    }
    accept();
}

void CSE_MainAreaClipDialogUi::on_Button_Cancel_clicked()
{
    reject();
}

QString    CSE_MainAreaClipDialogUi::mainAreaPath() const { return ui.lineEdit_mainArea->text(); }
QString    CSE_MainAreaClipDialogUi::mainAreaField() const { return ui.comboBox_field->currentText(); }
QStringList CSE_MainAreaClipDialogUi::selectedFeatureIds() const
{
    QStringList ids;
    for (QListWidgetItem* it : ui.listWidget_features->selectedItems())
        ids << it->data(Qt::UserRole).toString();
    return ids;
}
QString CSE_MainAreaClipDialogUi::outputFilePath() const { return ui.lineEdit_output->text(); }
QString CSE_MainAreaClipDialogUi::encoding() const { return ui.comboBox_encoding->currentText(); }
QString CSE_MainAreaClipDialogUi::format() const { return ui.comboBox_format->currentText(); }
bool    CSE_MainAreaClipDialogUi::overwriteExisting() const { return ui.checkBox_overwrite->isChecked(); }
QString CSE_MainAreaClipDialogUi::paperSize() const { return ui.comboBox_paperSize->currentText(); }
QString CSE_MainAreaClipDialogUi::paperOrient() const { return ui.comboBox_orient->currentText(); }
bool    CSE_MainAreaClipDialogUi::useCustomScale() const { return ui.checkBox_customScale->isChecked(); }

// 生效比例尺字符串：勾选"使用自定义"读自定义输入框，否则读标准下拉框（含手填值）
QString CSE_MainAreaClipDialogUi::standardScale() const
{
    if (useCustomScale())
        return ui.lineEdit_customScale->text().trimmed();
    return ui.comboBox_standardScale->currentText().trimmed();
}

double CSE_MainAreaClipDialogUi::mapScaleDenominator() const
{
    return standardScale().toDouble();
}

// 内图廓尺寸：仅当启用时返回输入值，否则返回 0
double CSE_MainAreaClipDialogUi::innerMapFrameWidthMm() const
{
    if (!enableInnerFrame()) return 0;
    return ui.lineEdit_innerW->text().toDouble();
}
double CSE_MainAreaClipDialogUi::innerMapFrameHeightMm() const
{
    if (!enableInnerFrame()) return 0;
    return ui.lineEdit_innerH->text().toDouble();
}
bool CSE_MainAreaClipDialogUi::enableInnerFrame() const { return ui.checkBox_enableInner->isChecked(); }

void CSE_MainAreaClipDialogUi::setAutoLoadAfterExport(bool on)
{
    ui.checkBox_autoLoad->setChecked(on);
}

// 是否在导出后自动加载结果到地图（默认 true）
bool CSE_MainAreaClipDialogUi::autoLoadAfterExport() const
{
    return ui.checkBox_autoLoad->isChecked();
}


// ============================================================
// 从主区矢量读取所选要素的合并外包矩形，作为统一裁剪范围
// ============================================================
QgsRectangle computeMainAreaClipRect(const QString& mainAreaPath,
    const QStringList& featureIds, QgsCoordinateReferenceSystem* outCrs)
{
    QgsRectangle rect;
    if (mainAreaPath.isEmpty() || !QFileInfo::exists(mainAreaPath)) return rect;
    QgsVectorLayer* pLayer = new QgsVectorLayer(mainAreaPath,
        QFileInfo(mainAreaPath).completeBaseName(), "ogr");
    if (!pLayer || !pLayer->isValid())
    {
        if (pLayer) delete pLayer;
        return rect;
    }

    // 将字符串 ID 集合转换为 long long 集合用于 feature 请求
    QgsFeatureIds idSet;
    for (const QString& s : featureIds)
    {
        bool ok = false;
        long long v = s.toLongLong(&ok);
        if (ok) idSet.insert(v);
    }

    QgsFeatureRequest req;
    if (!idSet.isEmpty())
        req.setFilterFids(idSet);

    QgsFeatureIterator it = pLayer->getFeatures(req);
    QgsFeature feat;
    bool first = true;
    while (it.nextFeature(feat))
    {
        QgsGeometry g = feat.geometry();
        if (g.isNull()) continue;
        QgsRectangle b = g.boundingBox();
        if (b.isEmpty()) continue;
        if (first) { rect = b; first = false; }
        else rect.combineExtentWith(b);
    }
    // 输出矩形所在的主区 layer 原始 CRS，供裁剪时把范围正确转换到源数据 CRS
    if (outCrs && pLayer->crs().isValid())
        *outCrs = pLayer->crs();
    pLayer->deleteLater();
    return rect;
}
