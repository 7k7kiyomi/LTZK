#include "LocalProductOperation.h"

#include "ui_LocalMainAreaClip.h"
#include "ui_LocalRangeExport.h"

#include "qgscoordinatereferencesystem.h"
#include "qgscoordinatetransform.h"
#include "qgsfeature.h"
#include "qgsfeatureiterator.h"
#include "qgsfeaturerequest.h"
#include "qgsgeometry.h"
#include "qgsmapcanvas.h"
#include "qgsmapmouseevent.h"
#include "qgsmaptool.h"
#include "qgsmaplayer.h"
#include "qgsproject.h"
#include "qgsrasterfilewriter.h"
#include "qgsrasterlayer.h"
#include "qgsrasterpipe.h"
#include "qgsrubberband.h"
#include "qgsvectordataprovider.h"
#include "qgsvectorfilewriter.h"
#include "qgsvectorlayer.h"
#include "qgsfield.h"
#include "qgsfields.h"
#include "qgswkbtypes.h"
#include "qgsunittypes.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QSet>
#include <QVBoxLayout>
#include <QColor>
#include <QEventLoop>

#include <algorithm>
#include <cmath>
#include <functional>

namespace
{
    struct LocalExportSettings
    {
        QString format;
        QString outputPath;
        QString encoding = QStringLiteral("UTF-8");
        bool overwriteExisting = true;
        bool autoLoad = true;
    };

    struct ExportedOutput
    {
        QString loadUri;
        QString displayName;
        bool raster = false;
    };

    struct ClipOutputTarget
    {
        QString format;
        QString encoding = QStringLiteral("UTF-8");
        QString outputDirectory;
        QString containerPath;
        QString rasterDirectory;
        bool overwriteExisting = true;
        bool containerInitialized = false;
        QSet<QString> layerNames;
    };

    QString safeBaseName(const QString& value)
    {
        QString name = QFileInfo(value.trimmed()).completeBaseName();
        if (name.isEmpty())
        {
            name = value.trimmed();
        }
        if (name.isEmpty())
        {
            name = QStringLiteral("layer");
        }

        const QString forbidden = QStringLiteral("\\/:*?\"<>|");
        for (int index = 0; index < name.size(); ++index)
        {
            const QChar character = name.at(index);
            if (character.unicode() < 0x20 || forbidden.contains(character))
            {
                name[index] = QChar('_');
            }
        }
        while (name.endsWith(QChar('.')) || name.endsWith(QChar(' ')))
        {
            name.chop(1);
        }
        return name.isEmpty() ? QStringLiteral("layer") : name;
    }

    QString sourceName(const QgsMimeDataUtils::Uri& uri)
    {
        QString name = uri.name.trimmed();
        if (name.isEmpty())
        {
            name = QFileInfo(uri.filePath).completeBaseName();
        }
        if (name.isEmpty())
        {
            name = QFileInfo(uri.uri).completeBaseName();
        }
        return safeBaseName(name);
    }

    QString uniquePath(
        const QString& directory,
        const QString& baseName,
        const QString& suffix)
    {
        const QString normalizedSuffix = suffix.startsWith(QChar('.'))
            ? suffix
            : QChar('.') + suffix;
        const QString cleanBase = safeBaseName(baseName);
        QString candidate = QDir(directory).filePath(cleanBase + normalizedSuffix);
        int serial = 2;
        while (QFileInfo::exists(candidate))
        {
            candidate = QDir(directory).filePath(
                QStringLiteral("%1_%2%3")
                    .arg(cleanBase)
                    .arg(serial)
                    .arg(normalizedSuffix));
            ++serial;
        }
        return candidate;
    }

    QString uniqueContainerPath(
        const QString& path,
        const QString& suffix)
    {
        QFileInfo info(path);
        QString directory = info.absolutePath();
        if (directory.isEmpty())
        {
            directory = QDir::currentPath();
        }
        return uniquePath(directory, info.completeBaseName(), suffix);
    }

    QString crsLabel(const QgsCoordinateReferenceSystem& crs)
    {
        if (!crs.isValid())
        {
            return QStringLiteral("未指定（按源数据坐标值处理）");
        }
        const QString authId = crs.authid().trimmed();
        const QString description = crs.description().trimmed();
        if (authId.isEmpty())
        {
            return description;
        }
        return description.isEmpty()
            ? authId
            : QStringLiteral("%1 - %2").arg(authId, description);
    }

    QDoubleSpinBox* createCoordinateSpinBox(QWidget* parent)
    {
        QDoubleSpinBox* spin = new QDoubleSpinBox(parent);
        spin->setRange(-1.0e15, 1.0e15);
        spin->setDecimals(12);
        spin->setSingleStep(1.0);
        spin->setKeyboardTracking(false);
        return spin;
    }

    class LocalExportOptionsWidget final : public QWidget
    {
    public:
        explicit LocalExportOptionsWidget(QWidget* parent = nullptr)
            : QWidget(parent)
        {
            QFormLayout* form = new QFormLayout(this);
            form->setContentsMargins(0, 0, 0, 0);
            form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

            m_format = new QComboBox(this);
            m_format->addItem(
                QStringLiteral("ESRI Shapefile (.shp)"),
                QStringLiteral("shp"));
            m_format->addItem(
                QStringLiteral("GeoPackage (.gpkg)"),
                QStringLiteral("gpkg"));
            m_format->addItem(
                QStringLiteral("GDB (.gdb)"),
                QStringLiteral("gdb"));
            form->addRow(QStringLiteral("输出格式："), m_format);

            QWidget* pathWidget = new QWidget(this);
            QHBoxLayout* pathLayout = new QHBoxLayout(pathWidget);
            pathLayout->setContentsMargins(0, 0, 0, 0);
            m_outputPath = new QLineEdit(pathWidget);
            m_outputPath->setPlaceholderText(QStringLiteral("请选择输出文件夹或文件"));
            QPushButton* browse = new QPushButton(QStringLiteral("浏览…"), pathWidget);
            pathLayout->addWidget(m_outputPath, 1);
            pathLayout->addWidget(browse);
            m_outputLabel = new QLabel(pathWidget);
            form->addRow(m_outputLabel, pathWidget);

            m_autoLoad = new QCheckBox(QStringLiteral("导出后自动加载到地图"), this);
            m_autoLoad->setChecked(true);
            form->addRow(QString(), m_autoLoad);

            connect(
                m_format,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this](int) { updateOutputLabel(); });
            connect(
                browse,
                &QPushButton::clicked,
                this,
                [this]() { browseOutput(); });
            updateOutputLabel();
        }

        QString format() const
        {
            return m_format->currentData().toString();
        }

        QString outputPath() const
        {
            return m_outputPath->text().trimmed();
        }

        bool autoLoad() const
        {
            return m_autoLoad->isChecked();
        }

        void setSuggestedOutput(const QString& path)
        {
            if (m_outputPath->text().trimmed().isEmpty())
            {
                m_outputPath->setText(QDir::toNativeSeparators(path));
            }
        }

    private:
        void updateOutputLabel()
        {
            const bool singleFile = format() == QStringLiteral("gpkg") ||
                format() == QStringLiteral("gdb");
            m_outputLabel->setText(
                singleFile ? QStringLiteral("输出文件：") : QStringLiteral("输出文件夹："));

            const QString current = m_outputPath->text().trimmed();
            if (current.isEmpty())
            {
                return;
            }

            if (singleFile)
            {
                const QString suffix = format() == QStringLiteral("gpkg")
                    ? QStringLiteral(".gpkg")
                    : QStringLiteral(".gdb");
                if (QFileInfo(current).isDir())
                {
                    m_outputPath->setText(
                        QDir(current).filePath(QStringLiteral("local_product_clip") + suffix));
                }
                else if (!current.endsWith(suffix, Qt::CaseInsensitive))
                {
                    const QFileInfo info(current);
                    QString baseName = info.completeBaseName();
                    if (baseName.isEmpty())
                    {
                        baseName = QStringLiteral("local_product_clip");
                    }
                    m_outputPath->setText(
                        info.absolutePath().isEmpty()
                            ? baseName + suffix
                            : QDir(info.absolutePath()).filePath(
                                baseName + suffix));
                }
            }
            else if (!QFileInfo(current).isDir() &&
                     (QFileInfo(current).suffix().compare(
                          QStringLiteral("gpkg"), Qt::CaseInsensitive) == 0 ||
                      QFileInfo(current).suffix().compare(
                          QStringLiteral("gdb"), Qt::CaseInsensitive) == 0))
            {
                m_outputPath->setText(QFileInfo(current).absolutePath());
            }
        }

        void browseOutput()
        {
            const QString current = m_outputPath->text().trimmed();
            if (format() == QStringLiteral("shp"))
            {
                QString start = current;
                if (start.isEmpty() || !QFileInfo(start).isDir())
                {
                    start = QDir::homePath();
                }
                const QString directory = QFileDialog::getExistingDirectory(
                    this,
                    QStringLiteral("选择输出文件夹（每个成果导出为独立文件）"),
                    start);
                if (!directory.isEmpty())
                {
                    m_outputPath->setText(QDir::toNativeSeparators(directory));
                }
                return;
            }

            const QString suffix = format() == QStringLiteral("gpkg")
                ? QStringLiteral(".gpkg")
                : QStringLiteral(".gdb");
            QString suggested = current;
            if (suggested.isEmpty() || QFileInfo(suggested).isDir())
            {
                const QString directory = suggested.isEmpty()
                    ? QDir::homePath()
                    : suggested;
                suggested = QDir(directory).filePath(
                    QStringLiteral("local_product_clip") + suffix);
            }
            else if (!suggested.endsWith(suffix, Qt::CaseInsensitive))
            {
                suggested += suffix;
            }

            const QString path = QFileDialog::getSaveFileName(
                this,
                format() == QStringLiteral("gpkg")
                    ? QStringLiteral("选择输出 GeoPackage 文件")
                    : QStringLiteral("选择输出 GDB 文件"),
                suggested,
                format() == QStringLiteral("gpkg")
                    ? QStringLiteral("GeoPackage (*.gpkg)")
                    : QStringLiteral("文件地理数据库 (*.gdb)"));
            if (!path.isEmpty())
            {
                QString finalPath = path;
                if (!finalPath.endsWith(suffix, Qt::CaseInsensitive))
                {
                    finalPath += suffix;
                }
                m_outputPath->setText(QDir::toNativeSeparators(finalPath));
            }
        }

        QComboBox* m_format = nullptr;
        QLineEdit* m_outputPath = nullptr;
        QLabel* m_outputLabel = nullptr;
        QCheckBox* m_autoLoad = nullptr;
    };

    class LocalRangeDialog final : public QDialog
    {
    public:
        LocalRangeDialog(
            QgsMapCanvas* canvas,
            const QgsCoordinateReferenceSystem& fallbackCrs,
            const QgsRectangle& fallbackExtent,
            QWidget* parent = nullptr)
            : QDialog(parent)
            , m_canvas(canvas)
        {
            setWindowTitle(QStringLiteral("按范围导出"));
            setMinimumWidth(560);

            QVBoxLayout* root = new QVBoxLayout(this);
            QGroupBox* extentGroup = new QGroupBox(QStringLiteral("导出范围"), this);
            QFormLayout* extentForm = new QFormLayout(extentGroup);
            extentForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

            const bool hasCanvasExtent = canvas &&
                !canvas->extent().isNull() && !canvas->extent().isEmpty();
            m_crs = hasCanvasExtent &&
                    canvas->mapSettings().destinationCrs().isValid()
                ? canvas->mapSettings().destinationCrs()
                : fallbackCrs;
            QLabel* crsText = new QLabel(crsLabel(m_crs), extentGroup);
            crsText->setWordWrap(true);
            extentForm->addRow(QStringLiteral("坐标系："), crsText);

            QHBoxLayout* minLayout = new QHBoxLayout();
            m_xMin = createCoordinateSpinBox(extentGroup);
            m_yMin = createCoordinateSpinBox(extentGroup);
            minLayout->addWidget(new QLabel(QStringLiteral("X 最小："), extentGroup));
            minLayout->addWidget(m_xMin, 1);
            minLayout->addWidget(new QLabel(QStringLiteral("Y 最小："), extentGroup));
            minLayout->addWidget(m_yMin, 1);
            extentForm->addRow(QString(), minLayout);

            QHBoxLayout* maxLayout = new QHBoxLayout();
            m_xMax = createCoordinateSpinBox(extentGroup);
            m_yMax = createCoordinateSpinBox(extentGroup);
            maxLayout->addWidget(new QLabel(QStringLiteral("X 最大："), extentGroup));
            maxLayout->addWidget(m_xMax, 1);
            maxLayout->addWidget(new QLabel(QStringLiteral("Y 最大："), extentGroup));
            maxLayout->addWidget(m_yMax, 1);
            extentForm->addRow(QString(), maxLayout);

            QPushButton* useCanvasExtent = new QPushButton(
                QStringLiteral("使用当前地图范围"), extentGroup);
            useCanvasExtent->setEnabled(hasCanvasExtent);
            extentForm->addRow(QString(), useCanvasExtent);
            root->addWidget(extentGroup);

            m_options = new LocalExportOptionsWidget(this);
            root->addWidget(m_options);

            const QString documents = QStandardPaths::writableLocation(
                QStandardPaths::DocumentsLocation);
            m_options->setSuggestedOutput(
                documents.isEmpty() ? QDir::homePath() : documents);

            if (hasCanvasExtent)
            {
                setExtent(canvas->extent());
            }
            else
            {
                setExtent(fallbackExtent);
            }

            QDialogButtonBox* buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                Qt::Horizontal,
                this);
            root->addWidget(buttons);
            connect(
                useCanvasExtent,
                &QPushButton::clicked,
                this,
                [this]()
                {
                    if (m_canvas)
                    {
                        setExtent(m_canvas->extent());
                    }
                });
            connect(
                buttons,
                &QDialogButtonBox::accepted,
                this,
                [this]()
                {
                    if (extent().isEmpty() || !extent().isFinite())
                    {
                        QMessageBox::warning(
                            this,
                            QStringLiteral("按范围导出"),
                            QStringLiteral("请输入有效的矩形范围，且最小值必须小于最大值。"));
                        return;
                    }
                    if (m_options->outputPath().isEmpty())
                    {
                        QMessageBox::warning(
                            this,
                            QStringLiteral("按范围导出"),
                            QStringLiteral("请选择输出文件夹或输出文件。"));
                        return;
                    }
                    accept();
                });
            connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        }

        QgsRectangle extent() const
        {
            return QgsRectangle(
                m_xMin->value(),
                m_yMin->value(),
                m_xMax->value(),
                m_yMax->value());
        }

        QgsCoordinateReferenceSystem extentCrs() const
        {
            return m_crs;
        }

        LocalExportSettings settings() const
        {
            LocalExportSettings result;
            result.format = m_options->format();
            result.outputPath = m_options->outputPath();
            result.autoLoad = m_options->autoLoad();
            return result;
        }

    private:
        void setExtent(const QgsRectangle& rect)
        {
            if (rect.isNull() || rect.isEmpty() || !rect.isFinite())
            {
                return;
            }
            m_xMin->setValue(rect.xMinimum());
            m_yMin->setValue(rect.yMinimum());
            m_xMax->setValue(rect.xMaximum());
            m_yMax->setValue(rect.yMaximum());
        }

        QgsMapCanvas* m_canvas = nullptr;
        QgsCoordinateReferenceSystem m_crs;
        QDoubleSpinBox* m_xMin = nullptr;
        QDoubleSpinBox* m_yMin = nullptr;
        QDoubleSpinBox* m_xMax = nullptr;
        QDoubleSpinBox* m_yMax = nullptr;
        LocalExportOptionsWidget* m_options = nullptr;
    };

    class LocalMainAreaDialog final : public QDialog
    {
    public:
        explicit LocalMainAreaDialog(QWidget* parent = nullptr)
            : QDialog(parent)
        {
            setWindowTitle(QStringLiteral("按制图导出"));
            setMinimumWidth(620);

            QVBoxLayout* root = new QVBoxLayout(this);
            QGroupBox* mainAreaGroup = new QGroupBox(
                QStringLiteral("主区数据与要素"),
                this);
            QFormLayout* form = new QFormLayout(mainAreaGroup);
            form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

            QWidget* pathWidget = new QWidget(mainAreaGroup);
            QHBoxLayout* pathLayout = new QHBoxLayout(pathWidget);
            pathLayout->setContentsMargins(0, 0, 0, 0);
            m_mainAreaPath = new QLineEdit(pathWidget);
            m_mainAreaPath->setPlaceholderText(QStringLiteral("选择主区矢量数据文件"));
            QPushButton* browseMainArea = new QPushButton(QStringLiteral("浏览…"), pathWidget);
            pathLayout->addWidget(m_mainAreaPath, 1);
            pathLayout->addWidget(browseMainArea);
            form->addRow(QStringLiteral("主区文件："), pathWidget);

            m_field = new QComboBox(mainAreaGroup);
            form->addRow(QStringLiteral("标识字段："), m_field);

            m_features = new QListWidget(mainAreaGroup);
            m_features->setSelectionMode(QAbstractItemView::ExtendedSelection);
            m_features->setMinimumHeight(180);
            form->addRow(QStringLiteral("主区要素："), m_features);
            root->addWidget(mainAreaGroup);

            m_options = new LocalExportOptionsWidget(this);
            root->addWidget(m_options);
            const QString documents = QStandardPaths::writableLocation(
                QStandardPaths::DocumentsLocation);
            m_options->setSuggestedOutput(
                documents.isEmpty() ? QDir::homePath() : documents);

            QDialogButtonBox* buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                Qt::Horizontal,
                this);
            root->addWidget(buttons);

            connect(
                browseMainArea,
                &QPushButton::clicked,
                this,
                [this]() { browseMainAreaFile(); });
            connect(
                m_field,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this](int) { reloadFeatures(); });
            connect(
                buttons,
                &QDialogButtonBox::accepted,
                this,
                [this]()
                {
                    if (!m_layer || !m_layer->isValid())
                    {
                        QMessageBox::warning(
                            this,
                            QStringLiteral("按制图导出"),
                            QStringLiteral("请选择有效的主区矢量数据文件。"));
                        return;
                    }
                    if (selectedExtent().isEmpty())
                    {
                        QMessageBox::warning(
                            this,
                            QStringLiteral("按制图导出"),
                            QStringLiteral("请在主区要素列表中选择至少一个要素。"));
                        return;
                    }
                    if (m_options->outputPath().isEmpty())
                    {
                        QMessageBox::warning(
                            this,
                            QStringLiteral("按制图导出"),
                            QStringLiteral("请选择输出文件夹或输出文件。"));
                        return;
                    }
                    accept();
                });
            connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        }

        QgsRectangle selectedExtent() const
        {
            QgsRectangle result;
            if (!m_layer || !m_layer->isValid())
            {
                return result;
            }

            QgsFeatureIds ids;
            for (QListWidgetItem* item : m_features->selectedItems())
            {
                if (!item)
                {
                    continue;
                }
                bool ok = false;
                const qlonglong id = item->data(Qt::UserRole).toString().toLongLong(&ok);
                if (ok)
                {
                    ids.insert(static_cast<QgsFeatureId>(id));
                }
            }
            if (ids.isEmpty())
            {
                return result;
            }

            QgsFeatureRequest request;
            request.setFilterFids(ids);
            QgsFeatureIterator iterator = m_layer->getFeatures(request);
            QgsFeature feature;
            bool first = true;
            while (iterator.nextFeature(feature))
            {
                const QgsRectangle bounds = feature.geometry().boundingBox();
                if (bounds.isEmpty())
                {
                    continue;
                }
                if (first)
                {
                    result = bounds;
                    first = false;
                }
                else
                {
                    result.combineExtentWith(bounds);
                }
            }
            return result;
        }

        QgsCoordinateReferenceSystem extentCrs() const
        {
            return m_layer ? m_layer->crs() : QgsCoordinateReferenceSystem();
        }

        LocalExportSettings settings() const
        {
            LocalExportSettings result;
            result.format = m_options->format();
            result.outputPath = m_options->outputPath();
            result.autoLoad = m_options->autoLoad();
            return result;
        }

    private:
        void browseMainAreaFile()
        {
            const QString path = QFileDialog::getOpenFileName(
                this,
                QStringLiteral("选择主区矢量数据"),
                QDir::homePath(),
                QStringLiteral("矢量数据 (*.shp *.geojson *.json *.gpkg *.sqlite);;所有文件 (*.*)"));
            if (!path.isEmpty())
            {
                m_mainAreaPath->setText(QDir::toNativeSeparators(path));
                loadMainArea();
            }
        }

        void loadMainArea()
        {
            delete m_layer;
            m_layer = nullptr;
            m_field->clear();
            m_features->clear();

            const QString path = m_mainAreaPath->text().trimmed();
            if (path.isEmpty() || !QFileInfo::exists(path))
            {
                return;
            }

            m_layer = new QgsVectorLayer(
                path,
                QFileInfo(path).completeBaseName(),
                QStringLiteral("ogr"));
            if (!m_layer->isValid())
            {
                delete m_layer;
                m_layer = nullptr;
                return;
            }

            for (const QgsField& field : m_layer->fields().toList())
            {
                m_field->addItem(field.name());
            }
            reloadFeatures();
        }

        void reloadFeatures()
        {
            m_features->clear();
            if (!m_layer || !m_layer->isValid())
            {
                return;
            }

            const int fieldIndex = m_field->currentIndex();
            QgsFeatureIterator iterator = m_layer->getFeatures();
            QgsFeature feature;
            while (iterator.nextFeature(feature))
            {
                QString label = QStringLiteral("[%1]").arg(feature.id());
                if (fieldIndex >= 0 && fieldIndex < m_layer->fields().count())
                {
                    const QString value = feature.attribute(fieldIndex).toString().trimmed();
                    if (!value.isEmpty())
                    {
                        label += QStringLiteral(" ") + value;
                    }
                }
                QListWidgetItem* item = new QListWidgetItem(label, m_features);
                item->setData(Qt::UserRole, QString::number(feature.id()));
            }
        }

        QLineEdit* m_mainAreaPath = nullptr;
        QComboBox* m_field = nullptr;
        QListWidget* m_features = nullptr;
        QgsVectorLayer* m_layer = nullptr;
        LocalExportOptionsWidget* m_options = nullptr;
    };

    // 平台侧使用与成果管理插件相同的范围获取界面，但绘制动作由平台自己的
    // QgsMapTool 完成，避免让本地文件依赖插件内部的 MapExtentDrawTool。
    class LocalExtentDrawTool final : public QgsMapTool
    {
    public:
        using FinishCallback = std::function<void(const QgsRectangle&, bool)>;

        LocalExtentDrawTool(QgsMapCanvas* canvas, FinishCallback callback)
            : QgsMapTool(canvas)
            , m_callback(std::move(callback))
        {
            m_rubberBand = new QgsRubberBand(canvas, QgsWkbTypes::PolygonGeometry);
            m_rubberBand->setColor(QColor(30, 120, 220, 220));
            m_rubberBand->setFillColor(QColor(30, 120, 220, 40));
            m_rubberBand->setWidth(2);
            m_rubberBand->setVisible(false);
        }

        ~LocalExtentDrawTool() override
        {
            delete m_rubberBand;
            m_rubberBand = nullptr;
        }

        void canvasPressEvent(QgsMapMouseEvent* event) override
        {
            if (!event || event->button() != Qt::LeftButton || m_finished)
            {
                return;
            }
            m_start = event->mapPoint();
            m_drawing = true;
            updateRubberBand(m_start);
        }

        void canvasMoveEvent(QgsMapMouseEvent* event) override
        {
            if (!event || !m_drawing || m_finished)
            {
                return;
            }
            updateRubberBand(event->mapPoint());
        }

        void canvasReleaseEvent(QgsMapMouseEvent* event) override
        {
            if (!event || event->button() != Qt::LeftButton ||
                !m_drawing || m_finished)
            {
                return;
            }
            m_drawing = false;
            const QgsPointXY end = event->mapPoint();
            const QgsRectangle extent(
                std::min(m_start.x(), end.x()),
                std::min(m_start.y(), end.y()),
                std::max(m_start.x(), end.x()),
                std::max(m_start.y(), end.y()));
            if (extent.isEmpty() || extent.width() <= 0.0 ||
                extent.height() <= 0.0)
            {
                finish(QgsRectangle(), false);
                return;
            }
            finish(extent, true);
        }

        void keyPressEvent(QKeyEvent* event) override
        {
            if (event && event->key() == Qt::Key_Escape)
            {
                finish(QgsRectangle(), false);
                event->accept();
                return;
            }
            QgsMapTool::keyPressEvent(event);
        }

    private:
        void updateRubberBand(const QgsPointXY& end)
        {
            if (!m_rubberBand)
            {
                return;
            }
            const QgsRectangle extent(
                std::min(m_start.x(), end.x()),
                std::min(m_start.y(), end.y()),
                std::max(m_start.x(), end.x()),
                std::max(m_start.y(), end.y()));
            m_rubberBand->setToGeometry(
                QgsGeometry::fromRect(extent),
                QgsCoordinateReferenceSystem());
            m_rubberBand->setVisible(true);
        }

        void finish(const QgsRectangle& extent, bool accepted)
        {
            if (m_finished)
            {
                return;
            }
            m_finished = true;
            if (m_callback)
            {
                m_callback(extent, accepted);
            }
        }

        QgsRubberBand* m_rubberBand = nullptr;
        QgsPointXY m_start;
        FinishCallback m_callback;
        bool m_drawing = false;
        bool m_finished = false;
    };

    bool drawLocalExtent(
        QgsMapCanvas* canvas,
        QgsRectangle* extent,
        QgsCoordinateReferenceSystem* extentCrs,
        QWidget* parent)
    {
        if (!canvas || !extent)
        {
            if (parent)
            {
                QMessageBox::warning(
                    parent,
                    QStringLiteral("按范围导出"),
                    QStringLiteral("当前没有可用的地图画布，无法手动绘制范围。"));
            }
            return false;
        }

        if (parent)
        {
            QMessageBox::information(
                parent,
                QStringLiteral("按范围导出"),
                QStringLiteral(
                    "请在地图上按住鼠标左键拖出矩形范围，松开完成；按 Esc 取消绘制。"));
        }

        const QgsCoordinateReferenceSystem canvasCrs =
            canvas->mapSettings().destinationCrs();
        QgsMapTool* previousTool = canvas->mapTool();
        QgsRectangle drawnExtent;
        bool accepted = false;
        QEventLoop loop;
        LocalExtentDrawTool drawTool(
            canvas,
            [&loop, &drawnExtent, &accepted](
                const QgsRectangle& drawn,
                bool ok)
            {
                drawnExtent = drawn;
                accepted = ok;
                loop.quit();
            });
        canvas->setMapTool(&drawTool);
        loop.exec();
        if (canvas->mapTool() == &drawTool)
        {
            canvas->setMapTool(previousTool);
        }

        if (!accepted || drawnExtent.isEmpty() || !drawnExtent.isFinite())
        {
            return false;
        }
        *extent = drawnExtent;
        if (extentCrs)
        {
            *extentCrs = canvasCrs;
        }
        return true;
    }

    int localExtentPrecision(const QgsRectangle& rect)
    {
        const bool looksLikeLatLon =
            rect.xMinimum() >= -180.0 && rect.xMaximum() <= 180.0 &&
            rect.yMinimum() >= -90.0 && rect.yMaximum() <= 90.0 &&
            rect.width() <= 360.0 && rect.height() <= 180.0;
        return looksLikeLatLon ? 6 : 2;
    }

    void setLocalBoundText(QLineEdit* edit, double value, int precision)
    {
        if (edit)
        {
            edit->setText(QString::number(value, 'f', precision));
        }
    }

    bool localSingleFileFormat(const QString& format)
    {
        return format.contains(QStringLiteral("GeoPackage"), Qt::CaseInsensitive) ||
            format.contains(QStringLiteral("GDB"), Qt::CaseInsensitive);
    }

    QString localSingleFileSuffix(const QString& format)
    {
        return format.contains(QStringLiteral("GeoPackage"), Qt::CaseInsensitive)
            ? QStringLiteral(".gpkg")
            : QStringLiteral(".gdb");
    }

    QString localFormatKey(const QString& format)
    {
        if (format.contains(QStringLiteral("GeoPackage"), Qt::CaseInsensitive))
        {
            return QStringLiteral("gpkg");
        }
        if (format.contains(QStringLiteral("GDB"), Qt::CaseInsensitive))
        {
            return QStringLiteral("gdb");
        }
        return QStringLiteral("shp");
    }

    class LocalRangeExportDialog final : public QDialog
    {
    public:
        LocalRangeExportDialog(
            QgsMapCanvas* canvas,
            const QgsCoordinateReferenceSystem& fallbackCrs,
            const QgsRectangle& fallbackExtent,
            QWidget* parent = nullptr)
            : QDialog(parent)
            , m_canvas(canvas)
        {
            ui.setupUi(this);
            ui.gridLayout_draw->setColumnStretch(0, 1);
            ui.gridLayout_draw->setColumnStretch(1, 1);
            ui.gridLayout_feature->setColumnStretch(0, 0);
            ui.gridLayout_feature->setColumnStretch(1, 1);
            ui.gridLayout_featRange->setColumnStretch(0, 1);
            ui.gridLayout_featRange->setColumnStretch(1, 1);

            const bool hasCanvasExtent = canvas &&
                !canvas->extent().isNull() && !canvas->extent().isEmpty();
            m_extentCrs = hasCanvasExtent &&
                    canvas->mapSettings().destinationCrs().isValid()
                ? canvas->mapSettings().destinationCrs()
                : fallbackCrs;

            {
                const QSignalBlocker blocker(ui.comboBox_layer);
                if (canvas)
                {
                    const QList<QgsMapLayer*>& layers = canvas->layers();
                    for (QgsMapLayer* layer : layers)
                    {
                        if (layer && layer->type() == QgsMapLayerType::VectorLayer)
                        {
                            ui.comboBox_layer->addItem(
                                layer->name(),
                                QVariant::fromValue<void*>(layer));
                        }
                    }
                }
                if (ui.comboBox_layer->count() == 0)
                {
                    ui.comboBox_layer->addItem(QStringLiteral("（无矢量图层）"));
                }
            }

            connect(
                ui.radioButton_inputCoord,
                &QRadioButton::toggled,
                this,
                [this](bool checked)
                {
                    if (checked)
                    {
                        ui.stackedWidget_pages->setCurrentIndex(0);
                    }
                });
            connect(
                ui.radioButton_manualDraw,
                &QRadioButton::toggled,
                this,
                [this](bool checked)
                {
                    if (checked)
                    {
                        ui.stackedWidget_pages->setCurrentIndex(1);
                    }
                });
            connect(
                ui.radioButton_selectFeature,
                &QRadioButton::toggled,
                this,
                [this](bool checked)
                {
                    if (checked)
                    {
                        ui.stackedWidget_pages->setCurrentIndex(2);
                    }
                });
            connect(
                ui.Button_StartDraw,
                &QPushButton::clicked,
                this,
                [this]()
                {
                    m_drawRequested = true;
                    accept();
                });
            connect(
                ui.comboBox_layer,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this](int) { refreshFeatureFieldsAndValues(); });
            connect(
                ui.comboBox_field,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this](int) { refreshFeatureValues(); });
            connect(
                ui.comboBox_value,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this](int) { fetchSelectedFeatureExtent(false); });
            connect(
                ui.Button_OpenLayer,
                &QPushButton::clicked,
                this,
                [this]() { openLocalLayer(); });
            connect(
                ui.Button_GetExtent,
                &QPushButton::clicked,
                this,
                [this]() { fetchSelectedFeatureExtent(true); });
            connect(
                ui.comboBox_format,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this](int) { updateOutputLabel(); });
            connect(
                ui.Button_BrowseOutput,
                &QPushButton::clicked,
                this,
                [this]() { browseOutput(); });
            connect(
                ui.Button_OK,
                &QPushButton::clicked,
                this,
                [this]() { validateAndAccept(); });
            connect(
                ui.Button_Cancel,
                &QPushButton::clicked,
                this,
                &QDialog::reject);

            refreshFeatureFieldsAndValues();
            clearBoundTextEdits();
            setExtent(hasCanvasExtent ? canvas->extent() : fallbackExtent);
            setModeIndex(2);
            updateOutputLabel();
        }

        ~LocalRangeExportDialog() override
        {
            delete m_localLayer;
            m_localLayer = nullptr;
        }

        QgsRectangle exportExtent() const
        {
            return m_extent;
        }

        bool hasExtent() const
        {
            return m_hasExtent;
        }

        QgsCoordinateReferenceSystem exportExtentCrs() const
        {
            return m_extentCrsValid ? m_extentCrs : QgsCoordinateReferenceSystem();
        }

        QString outputPath() const
        {
            return ui.lineEdit_output->text().trimmed();
        }

        QString format() const
        {
            return ui.comboBox_format->currentText();
        }

        LocalExportSettings settings() const
        {
            LocalExportSettings result;
            result.format = localFormatKey(format());
            result.outputPath = outputPath();
            result.autoLoad = ui.checkBox_autoLoad->isChecked();
            return result;
        }

        void setExtent(const QgsRectangle& extent)
        {
            if (extent.isNull() || extent.isEmpty() || !extent.isFinite())
            {
                return;
            }
            m_extent = extent;
            m_hasExtent = true;
            ui.doubleSpinBox_minX->setValue(extent.xMinimum());
            ui.doubleSpinBox_minY->setValue(extent.yMinimum());
            ui.doubleSpinBox_maxX->setValue(extent.xMaximum());
            ui.doubleSpinBox_maxY->setValue(extent.yMaximum());
            fillBoundTextEdits(extent);
        }

        void setExtentCrs(const QgsCoordinateReferenceSystem& crs)
        {
            m_extentCrs = crs;
            m_extentCrsValid = crs.isValid();
        }

        bool drawRequested() const
        {
            return m_drawRequested;
        }

        void resetDrawRequested()
        {
            m_drawRequested = false;
        }

        int modeIndex() const
        {
            if (ui.radioButton_manualDraw->isChecked())
            {
                return 1;
            }
            if (ui.radioButton_selectFeature->isChecked())
            {
                return 2;
            }
            return 0;
        }

        void setModeIndex(int index)
        {
            ui.radioButton_inputCoord->setChecked(index == 0);
            ui.radioButton_manualDraw->setChecked(index == 1);
            ui.radioButton_selectFeature->setChecked(index == 2);
            ui.stackedWidget_pages->setCurrentIndex(qBound(0, index, 2));
        }

        void setAutoLoadAfterExport(bool on)
        {
            ui.checkBox_autoLoad->setChecked(on);
        }

        bool autoLoadAfterExport() const
        {
            return ui.checkBox_autoLoad->isChecked();
        }

    private:
        QgsVectorLayer* currentVectorLayer() const
        {
            void* pointer = ui.comboBox_layer->currentData().value<void*>();
            return pointer
                ? qobject_cast<QgsVectorLayer*>(static_cast<QgsMapLayer*>(pointer))
                : nullptr;
        }

        void refreshFeatureFieldsAndValues()
        {
            const QSignalBlocker blocker(ui.comboBox_field);
            ui.comboBox_field->clear();
            QgsVectorLayer* layer = currentVectorLayer();
            if (layer && layer->isValid())
            {
                for (const QgsField& field : layer->fields().toList())
                {
                    ui.comboBox_field->addItem(field.name());
                }
            }
            refreshFeatureValues();
            clearBoundTextEdits();
        }

        void refreshFeatureValues()
        {
            const QSignalBlocker blocker(ui.comboBox_value);
            ui.comboBox_value->clear();
            QgsVectorLayer* layer = currentVectorLayer();
            const int fieldIndex = ui.comboBox_field->currentIndex();
            if (!layer || !layer->isValid() || fieldIndex < 0 ||
                fieldIndex >= layer->fields().count())
            {
                return;
            }

            QgsFeatureRequest request;
            request.setFlags(QgsFeatureRequest::NoGeometry);
            QgsFeatureIterator iterator = layer->getFeatures(request);
            QgsFeature feature;
            QSet<QString> values;
            while (iterator.nextFeature(feature))
            {
                const QVariant value = feature.attribute(fieldIndex);
                if (!value.isNull())
                {
                    values.insert(value.toString());
                }
            }
            QStringList sortedValues = values.values();
            sortedValues.sort();
            ui.comboBox_value->addItems(sortedValues);
        }

        bool fetchSelectedFeatureExtent(bool warnOnFail)
        {
            QgsVectorLayer* layer = currentVectorLayer();
            const int fieldIndex = ui.comboBox_field->currentIndex();
            const QString value = ui.comboBox_value->currentText().trimmed();
            if (!layer || !layer->isValid())
            {
                if (warnOnFail)
                {
                    QMessageBox::warning(
                        this,
                        QStringLiteral("按范围导出"),
                        QStringLiteral("请选择矢量图层。"));
                }
                return false;
            }
            if (fieldIndex < 0 || value.isEmpty())
            {
                if (warnOnFail)
                {
                    QMessageBox::warning(
                        this,
                        QStringLiteral("按范围导出"),
                        QStringLiteral("请先选择属性字段并输入/选择要素值。"));
                }
                return false;
            }

            QgsFeatureIterator iterator = layer->getFeatures();
            QgsFeature feature;
            while (iterator.nextFeature(feature))
            {
                if (feature.attribute(fieldIndex).toString() != value)
                {
                    continue;
                }
                m_featureGeometry = feature.geometry();
                m_extent = m_featureGeometry.boundingBox();
                m_hasExtent = !m_extent.isEmpty();
                m_extentCrs = layer->crs();
                m_extentCrsValid = m_extentCrs.isValid();
                if (m_hasExtent)
                {
                    fillBoundTextEdits(m_extent);
                }
                return m_hasExtent;
            }

            m_hasExtent = false;
            clearBoundTextEdits();
            if (warnOnFail)
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("按范围导出"),
                    QStringLiteral("未找到与“%1”匹配的要素。").arg(value));
            }
            return false;
        }

        void fillBoundTextEdits(const QgsRectangle& extent)
        {
            const int precision = localExtentPrecision(extent);
            setLocalBoundText(
                ui.lineEdit_drawLatMax, extent.yMaximum(), precision);
            setLocalBoundText(
                ui.lineEdit_drawLatMin, extent.yMinimum(), precision);
            setLocalBoundText(
                ui.lineEdit_drawLonMin, extent.xMinimum(), precision);
            setLocalBoundText(
                ui.lineEdit_drawLonMax, extent.xMaximum(), precision);
            setLocalBoundText(
                ui.lineEdit_featLatMax, extent.yMaximum(), precision);
            setLocalBoundText(
                ui.lineEdit_featLatMin, extent.yMinimum(), precision);
            setLocalBoundText(
                ui.lineEdit_featLonMin, extent.xMinimum(), precision);
            setLocalBoundText(
                ui.lineEdit_featLonMax, extent.xMaximum(), precision);
        }

        void clearBoundTextEdits()
        {
            m_featureGeometry = QgsGeometry();
            ui.lineEdit_drawLatMax->clear();
            ui.lineEdit_drawLatMin->clear();
            ui.lineEdit_drawLonMin->clear();
            ui.lineEdit_drawLonMax->clear();
            ui.lineEdit_featLatMax->clear();
            ui.lineEdit_featLatMin->clear();
            ui.lineEdit_featLonMin->clear();
            ui.lineEdit_featLonMax->clear();
        }

        void openLocalLayer()
        {
            QString start = m_lastOpenDirectory;
            if (start.isEmpty())
            {
                start = QDir::homePath();
            }
            const QString path = QFileDialog::getOpenFileName(
                this,
                QStringLiteral("打开本地矢量数据"),
                start,
                QStringLiteral(
                    "矢量数据 (*.shp *.geojson *.json *.kml *.gpkg *.tab *.sqlite *.dxf *.csv);;所有文件 (*.*)"));
            if (path.isEmpty())
            {
                return;
            }
            m_lastOpenDirectory = QFileInfo(path).absolutePath();
            QgsVectorLayer* layer = new QgsVectorLayer(
                path,
                QFileInfo(path).completeBaseName(),
                QStringLiteral("ogr"));
            if (!layer || !layer->isValid())
            {
                delete layer;
                QMessageBox::warning(
                    this,
                    QStringLiteral("打开本地矢量数据"),
                    QStringLiteral("无法打开矢量数据：\n%1").arg(path));
                return;
            }
            delete m_localLayer;
            m_localLayer = layer;
            for (int index = ui.comboBox_layer->count() - 1; index >= 0; --index)
            {
                if (!ui.comboBox_layer->itemData(index).isValid())
                {
                    ui.comboBox_layer->removeItem(index);
                }
            }
            {
                const QSignalBlocker blocker(ui.comboBox_layer);
                ui.comboBox_layer->insertItem(
                    0,
                    layer->name(),
                    QVariant::fromValue<void*>(layer));
                ui.comboBox_layer->setCurrentIndex(0);
            }
            refreshFeatureFieldsAndValues();
        }

        void updateOutputLabel()
        {
            ui.label_output->setText(
                localSingleFileFormat(format())
                    ? QStringLiteral("输出文件:")
                    : QStringLiteral("输出文件夹:"));
        }

        void browseOutput()
        {
            const QString current = outputPath();
            const QString currentFormat = format();
            if (localSingleFileFormat(currentFormat))
            {
                const bool gpkg = currentFormat.contains(
                    QStringLiteral("GeoPackage"), Qt::CaseInsensitive);
                const QString suffix = localSingleFileSuffix(currentFormat);
                QString suggestion = current;
                if (suggestion.isEmpty())
                {
                    suggestion = QDir::homePath() +
                        QStringLiteral("/clip_export") + suffix;
                }
                else if (QFileInfo(suggestion).isDir())
                {
                    suggestion = QDir(suggestion).filePath(
                        QStringLiteral("clip_export") + suffix);
                }
                else if (!suggestion.endsWith(suffix, Qt::CaseInsensitive))
                {
                    const QFileInfo info(suggestion);
                    suggestion = QDir(info.absolutePath()).filePath(
                        info.completeBaseName() + suffix);
                }
                const QString path = QFileDialog::getSaveFileName(
                    this,
                    gpkg ? QStringLiteral("选择输出 GeoPackage 文件")
                         : QStringLiteral("选择输出 GDB 文件"),
                    suggestion,
                    gpkg ? QStringLiteral("GeoPackage (*.gpkg)")
                         : QStringLiteral("文件地理数据库 (*.gdb)"));
                if (!path.isEmpty())
                {
                    QString finalPath = path;
                    if (!finalPath.endsWith(suffix, Qt::CaseInsensitive))
                    {
                        finalPath += suffix;
                    }
                    ui.lineEdit_output->setText(QDir::toNativeSeparators(finalPath));
                }
                return;
            }

            QString start = current;
            if (start.isEmpty())
            {
                start = QDir::homePath();
            }
            else if (!QFileInfo(start).isDir())
            {
                start = QFileInfo(start).absolutePath();
            }
            const QString directory = QFileDialog::getExistingDirectory(
                this,
                QStringLiteral("选择保存文件夹（每个成果导出为独立 shp）"),
                start);
            if (!directory.isEmpty())
            {
                ui.lineEdit_output->setText(QDir::toNativeSeparators(directory));
            }
        }

        void validateAndAccept()
        {
            if (outputPath().isEmpty())
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("按范围导出"),
                    localSingleFileFormat(format())
                        ? QStringLiteral("请选择输出文件。")
                        : QStringLiteral("请选择输出文件夹。"));
                return;
            }

            switch (ui.stackedWidget_pages->currentIndex())
            {
            case 0:
            {
                const QgsRectangle extent(
                    ui.doubleSpinBox_minX->value(),
                    ui.doubleSpinBox_minY->value(),
                    ui.doubleSpinBox_maxX->value(),
                    ui.doubleSpinBox_maxY->value());
                if (extent.isEmpty())
                {
                    QMessageBox::warning(
                        this,
                        QStringLiteral("按范围导出"),
                        QStringLiteral("坐标范围不合法，请检查最小/最大值。"));
                    return;
                }
                m_extent = extent;
                m_hasExtent = true;
                break;
            }
            case 1:
                if (!m_hasExtent)
                {
                    QMessageBox::warning(
                        this,
                        QStringLiteral("按范围导出"),
                        QStringLiteral("请先在地图上绘制导出范围。"));
                    return;
                }
                break;
            case 2:
                if (!fetchSelectedFeatureExtent(true))
                {
                    return;
                }
                break;
            default:
                return;
            }
            accept();
        }

        Ui::LocalRangeExport ui;
        QgsMapCanvas* m_canvas = nullptr;
        QgsVectorLayer* m_localLayer = nullptr;
        QString m_lastOpenDirectory;
        QgsRectangle m_extent;
        QgsGeometry m_featureGeometry;
        QgsCoordinateReferenceSystem m_extentCrs;
        bool m_extentCrsValid = false;
        bool m_hasExtent = false;
        bool m_drawRequested = false;
    };

    struct LocalIsoPaper
    {
        const char* name;
        double width;
        double height;
    };

    const LocalIsoPaper kLocalIsoPapers[] = {
        {"A0", 1189.0, 841.0},
        {"A1", 841.0, 594.0},
        {"A2", 594.0, 420.0},
        {"A3", 420.0, 297.0},
        {"A4", 297.0, 210.0},
    };

    bool parseLocalPaperSpec(
        const QString& text,
        double* width,
        double* height)
    {
        QString name = text.trimmed();
        int separator = name.indexOf(QChar(0xFF1A));
        if (separator < 0)
        {
            separator = name.indexOf(QChar(':'));
        }
        if (separator >= 0)
        {
            name = name.left(separator).trimmed();
        }
        const QString upperName = name.toUpper();
        for (const LocalIsoPaper& paper : kLocalIsoPapers)
        {
            if (upperName == QLatin1String(paper.name))
            {
                if (width)
                {
                    *width = paper.width;
                }
                if (height)
                {
                    *height = paper.height;
                }
                return true;
            }
        }
        return false;
    }

    double roundLocalScaleToNice(double scale)
    {
        if (scale <= 0.0)
        {
            return 1.0;
        }
        const double magnitude = std::pow(
            10.0,
            std::floor(std::log10(scale)));
        const double mantissa = scale / magnitude;
        double nice = 10.0;
        if (mantissa <= 1.0)
        {
            nice = 1.0;
        }
        else if (mantissa <= 2.0)
        {
            nice = 2.0;
        }
        else if (mantissa <= 5.0)
        {
            nice = 5.0;
        }
        return nice * magnitude;
    }

    class LocalMainAreaClipDialog final : public QDialog
    {
    public:
        explicit LocalMainAreaClipDialog(QWidget* parent = nullptr)
            : QDialog(parent)
        {
            ui.setupUi(this);
            ui.label_customScale->setVisible(false);
            ui.lineEdit_customScale->setVisible(false);
            applyPaperSizeVisibility();
            applyInnerFrameEnabled(false);
            updateOutputLabel();

            connect(
                ui.Button_BrowseMainArea,
                &QPushButton::clicked,
                this,
                [this]() { browseMainArea(); });
            connect(
                ui.comboBox_field,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this](int) { reloadFeatures(); });
            connect(
                ui.comboBox_format,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this](int) { updateOutputLabel(); });
            connect(
                ui.comboBox_paperSize,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this](int) { applyPaperSizeVisibility(); });
            connect(
                ui.checkBox_customScale,
                &QCheckBox::toggled,
                this,
                [this](bool checked) { applyCustomScaleState(checked); });
            connect(
                ui.checkBox_enableInner,
                &QCheckBox::toggled,
                this,
                [this](bool checked) { applyInnerFrameEnabled(checked); });
            connect(
                ui.Button_ComputePaper,
                &QPushButton::clicked,
                this,
                [this]() { computePaperFromExtent(); });
            connect(
                ui.Button_ComputeScale,
                &QPushButton::clicked,
                this,
                [this]() { computeScaleFromExtent(); });
            connect(
                ui.Button_BrowseOutput,
                &QPushButton::clicked,
                this,
                [this]() { browseOutput(); });
            connect(
                ui.Button_Export,
                &QPushButton::clicked,
                this,
                [this]() { validateAndAccept(); });
            connect(
                ui.Button_Cancel,
                &QPushButton::clicked,
                this,
                &QDialog::reject);
        }

        ~LocalMainAreaClipDialog() override
        {
            delete m_mainAreaLayer;
            m_mainAreaLayer = nullptr;
        }

        QString mainAreaPath() const
        {
            return ui.lineEdit_mainArea->text().trimmed();
        }

        QgsRectangle selectedExtent() const
        {
            QgsRectangle result;
            if (!m_mainAreaLayer || !m_mainAreaLayer->isValid())
            {
                return result;
            }

            QgsFeatureIds ids;
            for (QListWidgetItem* item : ui.listWidget_features->selectedItems())
            {
                if (!item)
                {
                    continue;
                }
                bool ok = false;
                const qlonglong id = item->data(Qt::UserRole)
                    .toString().toLongLong(&ok);
                if (ok)
                {
                    ids.insert(static_cast<QgsFeatureId>(id));
                }
            }
            if (ids.isEmpty())
            {
                return result;
            }

            QgsFeatureRequest request;
            request.setFilterFids(ids);
            QgsFeatureIterator iterator = m_mainAreaLayer->getFeatures(request);
            QgsFeature feature;
            bool first = true;
            while (iterator.nextFeature(feature))
            {
                const QgsGeometry geometry = feature.geometry();
                if (geometry.isNull() || geometry.isEmpty())
                {
                    continue;
                }
                const QgsRectangle bounds = geometry.boundingBox();
                if (bounds.isEmpty())
                {
                    continue;
                }
                if (first)
                {
                    result = bounds;
                    first = false;
                }
                else
                {
                    result.combineExtentWith(bounds);
                }
            }
            return result;
        }

        QgsCoordinateReferenceSystem extentCrs() const
        {
            return m_mainAreaLayer
                ? m_mainAreaLayer->crs()
                : QgsCoordinateReferenceSystem();
        }

        LocalExportSettings settings() const
        {
            LocalExportSettings result;
            result.format = localFormatKey(ui.comboBox_format->currentText());
            result.outputPath = ui.lineEdit_output->text().trimmed();
            result.encoding = ui.comboBox_encoding->currentText().trimmed();
            result.overwriteExisting = ui.checkBox_overwrite->isChecked();
            result.autoLoad = ui.checkBox_autoLoad->isChecked();
            return result;
        }

        void setAutoLoadAfterExport(bool on)
        {
            ui.checkBox_autoLoad->setChecked(on);
        }

        bool autoLoadAfterExport() const
        {
            return ui.checkBox_autoLoad->isChecked();
        }

    private:
        void browseMainArea()
        {
            const QString path = QFileDialog::getOpenFileName(
                this,
                QStringLiteral("选择主区矢量数据"),
                QDir::homePath(),
                QStringLiteral(
                    "矢量数据 (*.shp *.geojson *.json *.kml *.gpkg *.tab *.sqlite *.dxf *.csv);;所有文件 (*.*)"));
            if (!path.isEmpty())
            {
                ui.lineEdit_mainArea->setText(QDir::toNativeSeparators(path));
                loadMainAreaFields();
            }
        }

        void loadMainAreaFields()
        {
            delete m_mainAreaLayer;
            m_mainAreaLayer = nullptr;
            ui.comboBox_field->clear();
            ui.listWidget_features->clear();

            const QString path = mainAreaPath();
            if (path.isEmpty() || !QFileInfo::exists(path))
            {
                return;
            }
            QgsVectorLayer* layer = new QgsVectorLayer(
                path,
                QFileInfo(path).completeBaseName(),
                QStringLiteral("ogr"));
            if (!layer || !layer->isValid())
            {
                delete layer;
                return;
            }
            m_mainAreaLayer = layer;
            for (const QgsField& field : layer->fields().toList())
            {
                ui.comboBox_field->addItem(field.name(), field.name());
            }
            reloadFeatures();
        }

        void reloadFeatures()
        {
            ui.listWidget_features->clear();
            if (!m_mainAreaLayer || !m_mainAreaLayer->isValid())
            {
                return;
            }
            const int fieldIndex = m_mainAreaLayer->fields().lookupField(
                ui.comboBox_field->currentText());
            QgsFeatureIterator iterator = m_mainAreaLayer->getFeatures();
            QgsFeature feature;
            int displayIndex = 0;
            while (iterator.nextFeature(feature))
            {
                const QString fid = QString::number(feature.id());
                const QString value = fieldIndex >= 0
                    ? feature.attribute(fieldIndex).toString()
                    : QString();
                const QString label = QStringLiteral("[%1] ").arg(displayIndex) +
                    (value.isEmpty() ? fid : value);
                QListWidgetItem* item = new QListWidgetItem(
                    label,
                    ui.listWidget_features);
                item->setData(Qt::UserRole, fid);
                item->setToolTip(
                    QStringLiteral("fid=%1，标识字段=%2").arg(fid, value));
                ++displayIndex;
            }
        }

        void updateOutputLabel()
        {
            ui.label_output->setText(
                localSingleFileFormat(ui.comboBox_format->currentText())
                    ? QStringLiteral("输出文件：")
                    : QStringLiteral("输出文件夹："));
        }

        void browseOutput()
        {
            const QString format = ui.comboBox_format->currentText();
            const bool toFile = localSingleFileFormat(format);
            const QFileInfo sourceInfo(mainAreaPath());
            QString suggestionBase = sourceInfo.absolutePath();
            if (!suggestionBase.isEmpty())
            {
                suggestionBase += QLatin1String("/");
            }
            suggestionBase += sourceInfo.completeBaseName().isEmpty()
                ? QStringLiteral("clip")
                : sourceInfo.completeBaseName();
            suggestionBase += QStringLiteral("_mainArea");
            const QString current = ui.lineEdit_output->text().trimmed();

            if (toFile)
            {
                const bool gpkg = format.contains(
                    QStringLiteral("GeoPackage"), Qt::CaseInsensitive);
                const QString suffix = localSingleFileSuffix(format);
                QString suggestion = current;
                if (suggestion.isEmpty() || QFileInfo(suggestion).isDir())
                {
                    suggestion = suggestionBase + suffix;
                }
                else if (!suggestion.endsWith(suffix, Qt::CaseInsensitive))
                {
                    const QFileInfo info(suggestion);
                    suggestion = QDir(info.absolutePath()).filePath(
                        info.completeBaseName() + suffix);
                }
                const QString path = QFileDialog::getSaveFileName(
                    this,
                    gpkg ? QStringLiteral("选择输出 GeoPackage 文件")
                         : QStringLiteral("选择输出 GDB 文件"),
                    suggestion,
                    gpkg ? QStringLiteral("GeoPackage (*.gpkg)")
                         : QStringLiteral("文件地理数据库 (*.gdb)"));
                if (!path.isEmpty())
                {
                    QString finalPath = path;
                    if (!finalPath.endsWith(suffix, Qt::CaseInsensitive))
                    {
                        finalPath += suffix;
                    }
                    ui.lineEdit_output->setText(
                        QDir::toNativeSeparators(finalPath));
                }
                return;
            }

            QString start = current;
            if (start.isEmpty())
            {
                start = sourceInfo.absolutePath();
            }
            if (start.isEmpty())
            {
                start = QDir::homePath();
            }
            else if (!QFileInfo(start).isDir())
            {
                start = QFileInfo(start).absolutePath();
            }
            const QString directory = QFileDialog::getExistingDirectory(
                this,
                QStringLiteral("选择保存文件夹（每个成果导出为独立 shp）"),
                start);
            if (!directory.isEmpty())
            {
                ui.lineEdit_output->setText(
                    QDir::toNativeSeparators(directory));
            }
        }

        void applyPaperSizeVisibility()
        {
            const bool custom = ui.comboBox_paperSize->currentText() ==
                QStringLiteral("自定义");
            ui.label_customW->setVisible(custom);
            ui.lineEdit_customW->setVisible(custom);
            ui.label_customH->setVisible(custom);
            ui.lineEdit_customH->setVisible(custom);
        }

        void applyInnerFrameEnabled(bool enabled)
        {
            ui.label_innerW->setEnabled(enabled);
            ui.lineEdit_innerW->setEnabled(enabled);
            ui.label_innerH->setEnabled(enabled);
            ui.lineEdit_innerH->setEnabled(enabled);
        }

        void applyCustomScaleState(bool checked)
        {
            ui.label_customScale->setVisible(checked);
            ui.lineEdit_customScale->setVisible(checked);
            ui.comboBox_standardScale->setEnabled(!checked);
            ui.Button_ComputeScale->setEnabled(!checked);
            if (checked)
            {
                const QString current = ui.comboBox_standardScale->currentText()
                    .trimmed();
                if (!current.isEmpty())
                {
                    ui.lineEdit_customScale->setText(current);
                }
            }
        }

        double paperWidthMm() const
        {
            double width = 0.0;
            double height = 0.0;
            return parseLocalPaperSpec(
                    ui.comboBox_paperSize->currentText(),
                    &width,
                    &height)
                ? width
                : ui.lineEdit_customW->text().toDouble();
        }

        double paperHeightMm() const
        {
            double width = 0.0;
            double height = 0.0;
            return parseLocalPaperSpec(
                    ui.comboBox_paperSize->currentText(),
                    &width,
                    &height)
                ? height
                : ui.lineEdit_customH->text().toDouble();
        }

        double mapScaleDenominator() const
        {
            const QString text = ui.checkBox_customScale->isChecked()
                ? ui.lineEdit_customScale->text()
                : ui.comboBox_standardScale->currentText();
            return text.trimmed().toDouble();
        }

        void computeScaleFromExtent()
        {
            const QgsRectangle extent = selectedExtent();
            if (extent.isEmpty())
            {
                QMessageBox::information(
                    this,
                    QStringLiteral("自动计算比例尺"),
                    QStringLiteral(
                        "请先在要素列表中选中至少一个要素作为主区，再点击自动计算。"));
                return;
            }
            const double width = extent.width();
            const double height = extent.height();
            if (width <= 0.0 || height <= 0.0)
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("自动计算比例尺"),
                    QStringLiteral("主区范围无效，无法计算。"));
                return;
            }
            const double paperWidth =
                std::max(1.0, paperWidthMm() - 20.0) / 1000.0;
            const double paperHeight =
                std::max(1.0, paperHeightMm() - 20.0) / 1000.0;
            const double scale = roundLocalScaleToNice(std::max(
                width / paperWidth,
                height / paperHeight));
            ui.comboBox_standardScale->setCurrentText(
                QString::number(static_cast<qlonglong>(scale)));
            QMessageBox::information(
                this,
                QStringLiteral("自动计算比例尺"),
                QStringLiteral(
                    "主区外包矩形：%1 m × %2 m\n纸幅：%3 mm × %4 mm\n\n推荐标准比例尺：1:%5")
                    .arg(QString::number(width, 'f', 1))
                    .arg(QString::number(height, 'f', 1))
                    .arg(QString::number(paperWidthMm(), 'f', 0))
                    .arg(QString::number(paperHeightMm(), 'f', 0))
                    .arg(QString::number(scale, 'f', 0)));
        }

        void computePaperFromExtent()
        {
            const QgsRectangle extent = selectedExtent();
            if (extent.isEmpty())
            {
                QMessageBox::information(
                    this,
                    QStringLiteral("计算纸张"),
                    QStringLiteral(
                        "请先在要素列表中选中至少一个要素作为主区，再点击计算。"));
                return;
            }
            const double scale = mapScaleDenominator();
            if (scale <= 0.0)
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("计算纸张"),
                    QStringLiteral("比例尺无效，无法计算。"));
                return;
            }

            double groundWidth = extent.width();
            double groundHeight = extent.height();
            if (m_mainAreaLayer && m_mainAreaLayer->crs().isValid())
            {
                const QgsCoordinateReferenceSystem crs = m_mainAreaLayer->crs();
                if (crs.mapUnits() == QgsUnitTypes::DistanceDegrees)
                {
                    QgsCoordinateTransform transform(
                        crs,
                        QgsCoordinateReferenceSystem::fromEpsgId(3857),
                        QgsProject::instance());
                    if (transform.isValid())
                    {
                        try
                        {
                            const QgsRectangle meterExtent =
                                transform.transformBoundingBox(extent);
                            if (!meterExtent.isEmpty())
                            {
                                groundWidth = meterExtent.width();
                                groundHeight = meterExtent.height();
                            }
                        }
                        catch (...)
                        {
                        }
                    }
                }
                else if (crs.mapUnits() == QgsUnitTypes::DistanceFeet)
                {
                    groundWidth *= 0.3048;
                    groundHeight *= 0.3048;
                }
                else if (crs.mapUnits() == QgsUnitTypes::DistanceMiles)
                {
                    groundWidth *= 1609.344;
                    groundHeight *= 1609.344;
                }
                else if (crs.mapUnits() == QgsUnitTypes::DistanceNauticalMiles)
                {
                    groundWidth *= 1852.0;
                    groundHeight *= 1852.0;
                }
            }
            if (groundWidth <= 0.0 || groundHeight <= 0.0)
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("计算纸张"),
                    QStringLiteral("主区范围无效，无法计算。"));
                return;
            }

            const double drawWidth = groundWidth / scale * 1000.0;
            const double drawHeight = groundHeight / scale * 1000.0;
            const bool landscape = drawWidth >= drawHeight;
            ui.comboBox_orient->setCurrentText(
                landscape ? QStringLiteral("横向") : QStringLiteral("纵向"));

            const int paperCount = static_cast<int>(
                sizeof(kLocalIsoPapers) / sizeof(kLocalIsoPapers[0]));
            int chosen = -1;
            for (int index = paperCount - 1; index >= 0; --index)
            {
                const double paperWidth = landscape
                    ? kLocalIsoPapers[index].width
                    : kLocalIsoPapers[index].height;
                const double paperHeight = landscape
                    ? kLocalIsoPapers[index].height
                    : kLocalIsoPapers[index].width;
                if (drawWidth <= paperWidth + 1.0e-9 &&
                    drawHeight <= paperHeight + 1.0e-9)
                {
                    chosen = index;
                    break;
                }
            }

            for (int index = 0; index < 5 &&
                index < ui.comboBox_paperSize->count(); ++index)
            {
                ui.comboBox_paperSize->setItemText(
                    index,
                    QLatin1String(kLocalIsoPapers[index].name));
            }
            if (chosen < 0)
            {
                ui.comboBox_paperSize->setCurrentText(QStringLiteral("自定义"));
                ui.lineEdit_customW->setText(
                    QString::number(drawWidth, 'f', 1));
                ui.lineEdit_customH->setText(
                    QString::number(drawHeight, 'f', 1));
                applyPaperSizeVisibility();
                QMessageBox::information(
                    this,
                    QStringLiteral("计算纸张"),
                    QStringLiteral(
                        "主区绘图尺寸 %1 mm × %2 mm 超出 A0，已按自定义纸张设置。")
                        .arg(QString::number(drawWidth, 'f', 1))
                        .arg(QString::number(drawHeight, 'f', 1)));
                return;
            }

            const QString label = QStringLiteral("%1：%2*%3")
                .arg(QLatin1String(kLocalIsoPapers[chosen].name))
                .arg(QString::number(drawWidth, 'f', 1))
                .arg(QString::number(drawHeight, 'f', 1));
            ui.comboBox_paperSize->setCurrentIndex(chosen);
            ui.comboBox_paperSize->setItemText(chosen, label);
            applyPaperSizeVisibility();
        }

        void validateAndAccept()
        {
            if (!m_mainAreaLayer || !m_mainAreaLayer->isValid())
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("按制图导出"),
                    QStringLiteral("请选择有效的主区矢量数据文件。"));
                return;
            }
            if (selectedExtent().isEmpty())
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("按制图导出"),
                    QStringLiteral("请在主区要素列表中选择至少一个要素。"));
                return;
            }
            if (ui.lineEdit_output->text().trimmed().isEmpty())
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("按制图导出"),
                    localSingleFileFormat(ui.comboBox_format->currentText())
                        ? QStringLiteral("请设置输出文件路径。")
                        : QStringLiteral("请设置输出文件夹路径。"));
                return;
            }
            accept();
        }

        Ui::LocalMainAreaClip ui;
        QgsVectorLayer* m_mainAreaLayer = nullptr;
    };

    // 取得第一个可打开的本地空间源，给范围对话框提供默认 CRS/范围。
    bool firstSourceInfo(
        const QgsMimeDataUtils::UriList& uris,
        QgsCoordinateReferenceSystem* crs,
        QgsRectangle* extent,
        QString* errorMessage)
    {
        for (const QgsMimeDataUtils::Uri& uri : uris)
        {
            bool owner = false;
            QString openError;
            if (uri.layerType == QStringLiteral("vector"))
            {
                QgsVectorLayer* layer = uri.vectorLayer(owner, openError);
                if (layer && layer->isValid())
                {
                    if (crs)
                    {
                        *crs = layer->crs();
                    }
                    if (extent)
                    {
                        *extent = layer->extent();
                    }
                    if (owner)
                    {
                        delete layer;
                    }
                    return true;
                }
                if (owner)
                {
                    delete layer;
                }
            }
            else if (uri.layerType == QStringLiteral("raster"))
            {
                QgsRasterLayer* layer = uri.rasterLayer(owner, openError);
                if (layer && layer->isValid())
                {
                    if (crs)
                    {
                        *crs = layer->crs();
                    }
                    if (extent)
                    {
                        *extent = layer->extent();
                    }
                    if (owner)
                    {
                        delete layer;
                    }
                    return true;
                }
                if (owner)
                {
                    delete layer;
                }
            }
            if (!openError.trimmed().isEmpty() && errorMessage)
            {
                *errorMessage = openError;
            }
        }

        if (errorMessage && errorMessage->isEmpty())
        {
            *errorMessage = QStringLiteral("未找到可打开的本地矢量或栅格数据。 ");
        }
        return false;
    }

    bool transformExtentToSourceCrs(
        const QgsRectangle& extent,
        const QgsCoordinateReferenceSystem& extentCrs,
        const QgsCoordinateReferenceSystem& sourceCrs,
        QgsRectangle* sourceExtent,
        QString* errorMessage)
    {
        if (!sourceExtent)
        {
            return false;
        }
        *sourceExtent = extent;
        if (!extentCrs.isValid() || !sourceCrs.isValid() || extentCrs == sourceCrs)
        {
            return true;
        }

        try
        {
            const QgsCoordinateTransform transform(
                extentCrs,
                sourceCrs,
                QgsProject::instance()->transformContext());
            *sourceExtent = transform.transformBoundingBox(extent);
            if (!sourceExtent->isFinite() || sourceExtent->isEmpty())
            {
                if (errorMessage)
                {
                    *errorMessage = QStringLiteral(
                        "转换后的导出范围无效：%1 -> %2。")
                        .arg(crsLabel(extentCrs), crsLabel(sourceCrs));
                }
                return false;
            }
            return true;
        }
        catch (...)
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral(
                    "无法把导出范围从 %1 转换到源数据坐标系 %2。")
                    .arg(crsLabel(extentCrs), crsLabel(sourceCrs));
            }
            return false;
        }
    }

    bool prepareOutputTarget(
        const LocalExportSettings& settings,
        ClipOutputTarget* target,
        QString* errorMessage)
    {
        if (!target)
        {
            return false;
        }

        target->format = settings.format;
        target->encoding = settings.encoding.trimmed().isEmpty()
            ? QStringLiteral("UTF-8")
            : settings.encoding.trimmed();
        target->overwriteExisting = settings.overwriteExisting;
        QString output = settings.outputPath.trimmed();
        if (settings.format == QStringLiteral("shp"))
        {
            if (output.isEmpty())
            {
                if (errorMessage)
                {
                    *errorMessage = QStringLiteral("未设置有效的输出文件夹。 ");
                }
                return false;
            }
            QFileInfo info(output);
            if (info.exists() && !info.isDir())
            {
                if (errorMessage)
                {
                    *errorMessage = QStringLiteral("Shapefile 输出目标必须是文件夹。 ");
                }
                return false;
            }
            if (!QDir().mkpath(output))
            {
                if (errorMessage)
                {
                    *errorMessage = QStringLiteral("无法创建输出文件夹：%1").arg(output);
                }
                return false;
            }
            target->outputDirectory = QDir(output).absolutePath();
            target->rasterDirectory = target->outputDirectory;
            return true;
        }

        const QString suffix = settings.format == QStringLiteral("gpkg")
            ? QStringLiteral(".gpkg")
            : QStringLiteral(".gdb");
        if (output.isEmpty())
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("未设置有效的输出文件。 ");
            }
            return false;
        }
        if (QFileInfo(output).isDir())
        {
            output = QDir(output).filePath(QStringLiteral("local_product_clip") + suffix);
        }
        if (!output.endsWith(suffix, Qt::CaseInsensitive))
        {
            output += suffix;
        }

        const QFileInfo outputInfo(output);
        const QString parentDirectory = outputInfo.absolutePath();
        if (!QDir().mkpath(parentDirectory))
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("无法创建输出文件所在文件夹：%1")
                    .arg(parentDirectory);
            }
            return false;
        }
        target->containerPath = QFileInfo::exists(output) &&
                !target->overwriteExisting
            ? uniqueContainerPath(output, suffix)
            : output;
        target->rasterDirectory = QDir(parentDirectory).filePath(
            QFileInfo(target->containerPath).completeBaseName() + QStringLiteral("_rasters"));
        if (!QDir().mkpath(target->rasterDirectory))
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("无法创建栅格输出文件夹：%1")
                    .arg(target->rasterDirectory);
            }
            return false;
        }
        return true;
    }

    QString uniqueLayerName(ClipOutputTarget* target, const QString& baseName)
    {
        QString name = safeBaseName(baseName);
        int serial = 2;
        while (target && target->layerNames.contains(name))
        {
            name = QStringLiteral("%1_%2").arg(safeBaseName(baseName)).arg(serial);
            ++serial;
        }
        if (target)
        {
            target->layerNames.insert(name);
        }
        return name;
    }

    bool exportVector(
        const QgsMimeDataUtils::Uri& uri,
        const QgsRectangle& requestedExtent,
        const QgsCoordinateReferenceSystem& requestedCrs,
        ClipOutputTarget* target,
        ExportedOutput* output,
        QString* errorMessage)
    {
        bool owner = false;
        QString openError;
        QgsVectorLayer* source = uri.vectorLayer(owner, openError);
        if (!source || !source->isValid())
        {
            if (owner)
            {
                delete source;
            }
            if (errorMessage)
            {
                *errorMessage = openError.isEmpty()
                    ? QStringLiteral("无法打开本地矢量数据。")
                    : openError;
            }
            return false;
        }

        QgsRectangle sourceExtent;
        if (!transformExtentToSourceCrs(
                requestedExtent,
                requestedCrs,
                source->crs(),
                &sourceExtent,
                errorMessage))
        {
            if (owner)
            {
                delete source;
            }
            return false;
        }
        const QgsRectangle clippedExtent = sourceExtent.intersect(source->extent());
        if (clippedExtent.isEmpty())
        {
            if (owner)
            {
                delete source;
            }
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("导出范围与该矢量数据没有相交要素。 ");
            }
            return false;
        }

        QgsWkbTypes::Type outputType =
            QgsWkbTypes::promoteNonPointTypesToMulti(source->wkbType());
        if (outputType == QgsWkbTypes::Unknown ||
            QgsWkbTypes::geometryType(outputType) == QgsWkbTypes::UnknownGeometry)
        {
            outputType = source->wkbType();
        }
        if (outputType == QgsWkbTypes::Unknown || outputType == QgsWkbTypes::NoGeometry)
        {
            if (owner)
            {
                delete source;
            }
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("该矢量数据没有可裁剪的几何类型。 ");
            }
            return false;
        }

        QString memoryUri = QgsWkbTypes::displayString(outputType);
        if (source->crs().isValid())
        {
            memoryUri += QStringLiteral("?crs=") + source->crs().authid();
        }
        QgsVectorLayer clippedLayer(
            memoryUri,
            sourceName(uri),
            QStringLiteral("memory"));
        if (!clippedLayer.isValid() || !clippedLayer.dataProvider())
        {
            if (owner)
            {
                delete source;
            }
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("无法创建矢量裁剪结果图层。 ");
            }
            return false;
        }
        if (!clippedLayer.dataProvider()->addAttributes(source->fields().toList()))
        {
            if (owner)
            {
                delete source;
            }
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("无法复制矢量属性字段：%1")
                    .arg(clippedLayer.dataProvider()->lastError());
            }
            return false;
        }
        clippedLayer.updateFields();

        const QgsGeometry clipGeometry = QgsGeometry::fromRect(clippedExtent);
        QgsFeatureRequest request;
        request.setFilterRect(clippedExtent);
        QgsFeatureIterator iterator = source->getFeatures(request);
        QgsFeature feature;
        QgsFeatureList outputFeatures;
        while (iterator.nextFeature(feature))
        {
            const QgsGeometry geometry = feature.geometry();
            if (geometry.isNull() || geometry.isEmpty())
            {
                continue;
            }
            const QgsGeometry intersection = geometry.intersection(clipGeometry);
            if (intersection.isNull() || intersection.isEmpty())
            {
                continue;
            }

            const QVector<QgsGeometry> coerced = intersection.coerceToType(outputType);
            if (coerced.isEmpty())
            {
                if (intersection.wkbType() != outputType)
                {
                    continue;
                }
                QgsFeature outputFeature(clippedLayer.fields());
                outputFeature.setAttributes(feature.attributes());
                outputFeature.setGeometry(intersection);
                outputFeatures.append(outputFeature);
                continue;
            }
            for (const QgsGeometry& geometryPart : coerced)
            {
                if (geometryPart.isNull() || geometryPart.isEmpty())
                {
                    continue;
                }
                QgsFeature outputFeature(clippedLayer.fields());
                outputFeature.setAttributes(feature.attributes());
                outputFeature.setGeometry(geometryPart);
                outputFeatures.append(outputFeature);
            }
        }

        if (outputFeatures.isEmpty())
        {
            if (owner)
            {
                delete source;
            }
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("导出范围内没有矢量要素。 ");
            }
            return false;
        }
        if (!clippedLayer.dataProvider()->addFeatures(outputFeatures))
        {
            if (owner)
            {
                delete source;
            }
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("无法写入矢量裁剪结果：%1")
                    .arg(clippedLayer.dataProvider()->lastError());
            }
            return false;
        }

        const QString baseName = sourceName(uri);
        QString destination;
        QString loadUri;
        QString layerName = baseName;
        QgsVectorFileWriter::SaveVectorOptions options;
        options.fileEncoding = target->encoding.isEmpty()
            ? QStringLiteral("UTF-8")
            : target->encoding;
        if (target->format == QStringLiteral("shp"))
        {
            const QString preferred = QDir(target->outputDirectory).filePath(
                safeBaseName(baseName) + QStringLiteral(".shp"));
            destination = target->overwriteExisting
                ? preferred
                : uniquePath(target->outputDirectory, baseName, QStringLiteral(".shp"));
            options.driverName = QStringLiteral("ESRI Shapefile");
            options.layerName = baseName;
            options.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteFile;
            loadUri = destination;
        }
        else
        {
            destination = target->containerPath;
            layerName = uniqueLayerName(target, baseName);
            options.driverName = target->format == QStringLiteral("gpkg")
                ? QStringLiteral("GPKG")
                : QStringLiteral("OpenFileGDB");
            options.layerName = layerName;
            options.actionOnExistingFile = target->containerInitialized
                ? QgsVectorFileWriter::CreateOrOverwriteLayer
                : QgsVectorFileWriter::CreateOrOverwriteFile;
            loadUri = destination + QStringLiteral("|layername=") + layerName;
        }

        QString writeError;
        QString newFileName;
        QString newLayerName;
        const QgsVectorFileWriter::WriterError writeResult =
            QgsVectorFileWriter::writeAsVectorFormatV3(
                &clippedLayer,
                destination,
                QgsProject::instance()->transformContext(),
                options,
                &writeError,
                &newFileName,
                &newLayerName);
        if (writeResult != QgsVectorFileWriter::NoError)
        {
            if (owner)
            {
                delete source;
            }
            if (errorMessage)
            {
                *errorMessage = writeError.isEmpty()
                    ? QStringLiteral("矢量结果写出失败（错误码 %1）。")
                        .arg(static_cast<int>(writeResult))
                    : writeError;
            }
            return false;
        }
        if (target->format != QStringLiteral("shp"))
        {
            target->containerInitialized = true;
            if (!newLayerName.isEmpty())
            {
                layerName = newLayerName;
                loadUri = destination + QStringLiteral("|layername=") + layerName;
            }
        }
        if (output)
        {
            output->loadUri = loadUri;
            output->displayName = baseName;
            output->raster = false;
        }
        if (owner)
        {
            delete source;
        }
        return true;
    }

    bool exportRaster(
        const QgsMimeDataUtils::Uri& uri,
        const QgsRectangle& requestedExtent,
        const QgsCoordinateReferenceSystem& requestedCrs,
        ClipOutputTarget* target,
        ExportedOutput* output,
        QString* errorMessage)
    {
        bool owner = false;
        QString openError;
        QgsRasterLayer* source = uri.rasterLayer(owner, openError);
        if (!source || !source->isValid())
        {
            if (owner)
            {
                delete source;
            }
            if (errorMessage)
            {
                *errorMessage = openError.isEmpty()
                    ? QStringLiteral("无法打开本地栅格数据。")
                    : openError;
            }
            return false;
        }

        QgsRectangle sourceExtent;
        if (!transformExtentToSourceCrs(
                requestedExtent,
                requestedCrs,
                source->crs(),
                &sourceExtent,
                errorMessage))
        {
            if (owner)
            {
                delete source;
            }
            return false;
        }
        const QgsRectangle clippedExtent = sourceExtent.intersect(source->extent());
        if (clippedExtent.isEmpty())
        {
            if (owner)
            {
                delete source;
            }
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("导出范围与该栅格数据没有相交区域。 ");
            }
            return false;
        }

        const double pixelWidth = std::abs(source->rasterUnitsPerPixelX());
        const double pixelHeight = std::abs(source->rasterUnitsPerPixelY());
        const int columns = qMax(
            1,
            static_cast<int>(std::ceil(
                clippedExtent.width() / (pixelWidth > 0.0 ? pixelWidth : 1.0))));
        const int rows = qMax(
            1,
            static_cast<int>(std::ceil(
                clippedExtent.height() / (pixelHeight > 0.0 ? pixelHeight : 1.0))));
        const QString preferred = QDir(target->rasterDirectory).filePath(
            sourceName(uri) + QStringLiteral(".tif"));
        const QString destination = target->overwriteExisting
            ? preferred
            : uniquePath(
                target->rasterDirectory,
                sourceName(uri),
                QStringLiteral(".tif"));

        QgsRasterFileWriter writer(destination);
        writer.setOutputFormat(QStringLiteral("GTiff"));
        const QgsRasterFileWriter::WriterError writeResult = writer.writeRaster(
            source->pipe(),
            columns,
            rows,
            clippedExtent,
            source->crs(),
            QgsProject::instance()->transformContext());
        if (writeResult != QgsRasterFileWriter::NoError)
        {
            if (owner)
            {
                delete source;
            }
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("栅格结果写出失败（错误码 %1）。")
                    .arg(static_cast<int>(writeResult));
            }
            return false;
        }
        if (output)
        {
            output->loadUri = destination;
            output->displayName = sourceName(uri);
            output->raster = true;
        }
        if (owner)
        {
            delete source;
        }
        return true;
    }

    void loadExportedOutput(const ExportedOutput& output)
    {
        QgsMapLayer* layer = nullptr;
        if (output.raster)
        {
            layer = new QgsRasterLayer(
                output.loadUri,
                output.displayName,
                QStringLiteral("gdal"));
        }
        else
        {
            layer = new QgsVectorLayer(
                output.loadUri,
                output.displayName,
                QStringLiteral("ogr"));
        }
        if (!layer || !layer->isValid())
        {
            delete layer;
            return;
        }
        QgsProject::instance()->addMapLayer(layer);
    }

    bool runClipExport(
        const QgsMimeDataUtils::UriList& uris,
        const QgsRectangle& extent,
        const QgsCoordinateReferenceSystem& extentCrs,
        const LocalExportSettings& settings,
        const QString& title,
        QWidget* parent,
        QString* errorMessage)
    {
        ClipOutputTarget target;
        if (!prepareOutputTarget(settings, &target, errorMessage))
        {
            return false;
        }

        int successCount = 0;
        int attemptedCount = 0;
        QStringList successes;
        QStringList failures;
        QList<ExportedOutput> outputs;
        for (const QgsMimeDataUtils::Uri& uri : uris)
        {
            if (uri.layerType != QStringLiteral("vector") &&
                uri.layerType != QStringLiteral("raster"))
            {
                failures.append(
                    QStringLiteral("%1：不是可裁剪的矢量/栅格数据").arg(sourceName(uri)));
                continue;
            }

            ++attemptedCount;
            ExportedOutput exported;
            QString sourceError;
            const bool success = uri.layerType == QStringLiteral("vector")
                ? exportVector(
                    uri,
                    extent,
                    extentCrs,
                    &target,
                    &exported,
                    &sourceError)
                : exportRaster(
                    uri,
                    extent,
                    extentCrs,
                    &target,
                    &exported,
                    &sourceError);
            if (success)
            {
                ++successCount;
                successes.append(exported.displayName);
                outputs.append(exported);
            }
            else
            {
                failures.append(
                    QStringLiteral("%1：%2")
                        .arg(sourceName(uri))
                        .arg(sourceError.isEmpty()
                            ? QStringLiteral("未知错误")
                            : sourceError));
            }
        }

        if (successCount > 0 && settings.autoLoad)
        {
            for (const ExportedOutput& output : outputs)
            {
                loadExportedOutput(output);
            }
        }

        QString summary = QStringLiteral("%1完成：成功 %2/%3 个。")
            .arg(title)
            .arg(successCount)
            .arg(attemptedCount);
        if (!successes.isEmpty())
        {
            summary += QStringLiteral("\n成功：\n") + successes.join(QStringLiteral("\n"));
        }
        if (!failures.isEmpty())
        {
            summary += QStringLiteral("\n失败：\n") + failures.join(QStringLiteral("\n"));
        }
        if (target.format == QStringLiteral("shp"))
        {
            summary += QStringLiteral("\n输出文件夹：%1").arg(target.outputDirectory);
        }
        else
        {
            summary += QStringLiteral("\n输出文件：%1").arg(target.containerPath);
            if (!outputs.isEmpty() &&
                std::any_of(
                    outputs.cbegin(),
                    outputs.cend(),
                    [](const ExportedOutput& output) { return output.raster; }))
            {
                summary += QStringLiteral("\n栅格输出文件夹：%1").arg(target.rasterDirectory);
            }
        }

        if (successCount > 0)
        {
            QMessageBox::information(parent, title, summary);
        }
        else
        {
            if (errorMessage && errorMessage->isEmpty())
            {
                *errorMessage = summary;
            }
            QMessageBox::warning(parent, title, summary);
        }
        return successCount > 0;
    }
}

bool LocalProductOperation::dispatch(
    const QString& operation,
    const QgsMimeDataUtils::UriList& uris,
    QgsMapCanvas* canvas,
    QWidget* parent,
    QString* errorMessage)
{
    if (errorMessage)
    {
        errorMessage->clear();
    }
    if (uris.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("没有选择本地矢量或栅格数据。 ");
        }
        if (parent)
        {
            QMessageBox::information(
                parent,
                QStringLiteral("本地成果操作"),
                QStringLiteral("没有选择可操作的本地矢量或栅格数据。"));
        }
        return false;
    }

    if (operation == QStringLiteral("theme"))
    {
        if (parent)
        {
            QMessageBox::information(
                parent,
                QStringLiteral("添加到主题"),
                QStringLiteral(
                    "本地数据没有数据库目录和产品 ID，平台不能直接写入数据库主题。\n"
                    "如需使用数据库主题，请先将数据导入数据库成果目录。"));
        }
        return true;
    }

    if (operation == QStringLiteral("range"))
    {
        QgsCoordinateReferenceSystem fallbackCrs;
        QgsRectangle fallbackExtent;
        QString sourceError;
        if (!firstSourceInfo(uris, &fallbackCrs, &fallbackExtent, &sourceError))
        {
            if (errorMessage)
            {
                *errorMessage = sourceError;
            }
            if (parent)
            {
                QMessageBox::warning(parent, QStringLiteral("按范围导出"), sourceError);
            }
            return false;
        }

        LocalRangeExportDialog dialog(
            canvas,
            fallbackCrs,
            fallbackExtent,
            parent);
        while (true)
        {
            dialog.resetDrawRequested();
            if (dialog.exec() != QDialog::Accepted)
            {
                return false;
            }
            if (!dialog.drawRequested())
            {
                return runClipExport(
                    uris,
                    dialog.exportExtent(),
                    dialog.exportExtentCrs(),
                    dialog.settings(),
                    QStringLiteral("按范围导出"),
                    parent,
                    errorMessage);
            }

            QgsRectangle drawnExtent;
            QgsCoordinateReferenceSystem drawnCrs;
            if (!drawLocalExtent(
                    canvas,
                    &drawnExtent,
                    &drawnCrs,
                    parent))
            {
                return false;
            }
            dialog.setExtent(drawnExtent);
            dialog.setExtentCrs(drawnCrs);
            dialog.setModeIndex(1);
        }
    }

    if (operation == QStringLiteral("main-area"))
    {
        LocalMainAreaClipDialog dialog(parent);
        if (dialog.exec() != QDialog::Accepted)
        {
            return false;
        }
        return runClipExport(
            uris,
            dialog.selectedExtent(),
            dialog.extentCrs(),
            dialog.settings(),
            QStringLiteral("按制图导出"),
            parent,
            errorMessage);
    }

    if (errorMessage)
    {
        *errorMessage = QStringLiteral("本地数据不支持操作：%1").arg(operation);
    }
    if (parent)
    {
        QMessageBox::warning(
            parent,
            QStringLiteral("本地成果操作"),
            QStringLiteral("本地数据不支持操作：%1").arg(operation));
    }
    return false;
}
