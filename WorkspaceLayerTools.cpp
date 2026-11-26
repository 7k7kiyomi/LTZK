#include "LTZK.h"
#include "LTZKUiScale.h"
#include "WorkspaceLayerStyleRefresh.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QCursor>
#include <QFileInfo>
#include <QFrame>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QModelIndex>
#include <QMessageBox>
#include <QScreen>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStyle>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolButton>
#include <QToolBar>
#include <QVBoxLayout>
#include <QVector>
#include <QUndoStack>
#include <cmath>
#include <limits>
#include <functional>
#include "auth/LTZKAuthManager.h"

#include "QgsMapCanvas.h"
#include "QgsProject.h"
#include "QgsMapLayer.h"
#include "QgsRasterLayer.h"
#include "QgsVectorLayer.h"
#include "QgsFeature.h"
#include "QgsFeatureRequest.h"
#include "qgsattributedialog.h"
#include "qgsdualview.h"
#include "qgsattributetableview.h"
#include "qgsfield.h"
#include "qgsfieldcalculator.h"
#include "qgsvectordataprovider.h"
#include "qgsvectorlayertools.h"
#include "QgsDistanceArea.h"
#include "QgsGeometry.h"
#include "QgsMapTool.h"
#include "qgsmaptoolidentify.h"
#include "QgsMapMouseEvent.h"
#include "qgsmaptoolcapture.h"
#include "qgsmaptooldigitizefeature.h"
#include "qgsadvanceddigitizingdockwidget.h"
#include "qgsmapsettings.h"
#include "qgsvertexid.h"
#include "qgswkbtypes.h"
#include "qgsscalewidget.h"
#include "qgsprojectionselectiondialog.h"
#include "qgscoordinatetransform.h"
#include "qgsvectorlayerutils.h"
#include "QgsRasterFileWriter.h"
#include "QgsLayerTreeView.h"
#include "QgsLayerTreeModel.h"
#include "QgsLayerTreeGroup.h"
#include "QgsLayerTreeLayer.h"
#include "QgsMapThemeCollection.h"
#include "QgsQueryBuilder.h"
#include "QgsRasterLayerProperties.h"
#include "QgsRubberBand.h"
#include "QgsVectorFileWriter.h"
#include "QgsVectorLayerProperties.h"

namespace
{
    // Bump this when the default toolbar arrangement changes so older saved
    // positions migrate to the new layout once, while newer user layouts persist.
    constexpr int kWorkspaceToolbarStateVersion = 4;

    class WorkspaceVertexEditTool final : public QgsMapTool
    {
    public:
        WorkspaceVertexEditTool(
            QgsMapCanvas* canvas,
            std::function<QgsVectorLayer*()> currentLayer,
            std::function<void(const QString&)> report)
            : QgsMapTool(canvas),
              m_currentLayer(std::move(currentLayer)),
              m_report(std::move(report))
        {
            setToolName(QStringLiteral("编辑几何顶点"));
        }

        QgsMapTool::Flags flags() const override
        {
            return QgsMapTool::EditTool;
        }

        void deactivate() override
        {
            clearPreview();
            QgsMapTool::deactivate();
        }

        void canvasPressEvent(QgsMapMouseEvent* event) override
        {
            if (!event || event->button() != Qt::LeftButton || !canvas())
            {
                return;
            }

            QgsVectorLayer* layer = m_currentLayer ? m_currentLayer() : nullptr;
            if (!layer || !layer->isValid() || !layer->isEditable())
            {
                report(QStringLiteral("请先选择一个处于编辑模式的矢量图层。"));
                return;
            }

            const QgsPointXY mapPoint = event->mapPoint();
            const double mapTolerance = qMax(canvas()->mapUnitsPerPixel() * 8.0, 1e-12);
            const QgsRectangle mapSearchRect(
                mapPoint.x() - mapTolerance, mapPoint.y() - mapTolerance,
                mapPoint.x() + mapTolerance, mapPoint.y() + mapTolerance);
            const QgsRectangle layerSearchRect =
                canvas()->mapSettings().mapToLayerCoordinates(layer, mapSearchRect);
            const QgsPointXY layerPoint =
                canvas()->mapSettings().mapToLayerCoordinates(layer, mapPoint);
            const QgsPointXY layerTolerancePoint = canvas()->mapSettings().mapToLayerCoordinates(
                layer, QgsPointXY(mapPoint.x() + mapTolerance, mapPoint.y()));
            const double toleranceX = layerTolerancePoint.x() - layerPoint.x();
            const QgsPointXY layerTolerancePointY = canvas()->mapSettings().mapToLayerCoordinates(
                layer, QgsPointXY(mapPoint.x(), mapPoint.y() + mapTolerance));
            const double toleranceY = layerTolerancePointY.y() - layerPoint.y();
            const double toleranceSquared = qMax(
                toleranceX * toleranceX + toleranceY * toleranceY, 1e-24);

            double nearestDistanceSquared = std::numeric_limits<double>::max();
            QgsFeature nearestFeature;
            QgsPointXY nearestVertex;
            int nearestVertexIndex = -1;
            QgsFeatureIterator iterator = layer->getFeatures(
                QgsFeatureRequest().setFilterRect(layerSearchRect));
            QgsFeature candidate;
            while (iterator.nextFeature(candidate))
            {
                const QgsGeometry geometry = candidate.geometry();
                if (geometry.isNull() || geometry.isEmpty())
                {
                    continue;
                }

                int vertexIndex = -1;
                int previousVertexIndex = -1;
                int nextVertexIndex = -1;
                double distanceSquared = 0.0;
                const QgsPointXY vertex = geometry.closestVertex(
                    layerPoint, vertexIndex, previousVertexIndex,
                    nextVertexIndex, distanceSquared);
                if (vertexIndex >= 0 && distanceSquared < nearestDistanceSquared)
                {
                    nearestDistanceSquared = distanceSquared;
                    nearestFeature = candidate;
                    nearestVertex = vertex;
                    nearestVertexIndex = vertexIndex;
                }
            }

            if (nearestVertexIndex < 0 || nearestDistanceSquared > toleranceSquared)
            {
                report(QStringLiteral("未找到附近的几何顶点，请靠近顶点后拖动。"));
                return;
            }

            clearPreview();
            m_layer = layer;
            m_featureId = nearestFeature.id();
            m_vertexIndex = nearestVertexIndex;
            m_originalGeometry = nearestFeature.geometry();
            m_previewGeometry = m_originalGeometry;
            m_dragging = true;

            m_previewBand = new QgsRubberBand(
                canvas(), QgsWkbTypes::geometryType(layer->wkbType()));
            m_previewBand->setColor(QColor(255, 150, 30, 220));
            m_previewBand->setWidth(2);
            m_previewBand->setToGeometry(m_previewGeometry, layer);
            m_previewBand->show();
        }

        void canvasMoveEvent(QgsMapMouseEvent* event) override
        {
            if (!m_dragging || !event || !(event->buttons() & Qt::LeftButton)
                || !m_layer || !m_layer->isEditable() || !m_previewBand)
            {
                return;
            }

            const QgsPointXY layerPoint = canvas()->mapSettings().mapToLayerCoordinates(
                m_layer, event->mapPoint());
            m_previewGeometry = m_originalGeometry;
            if (m_previewGeometry.moveVertex(
                    layerPoint.x(), layerPoint.y(), m_vertexIndex))
            {
                m_previewBand->setToGeometry(m_previewGeometry, m_layer);
            }
        }

        void canvasReleaseEvent(QgsMapMouseEvent* event) override
        {
            if (!m_dragging || !event || event->button() != Qt::LeftButton)
            {
                return;
            }

            if (m_layer && m_layer->isEditable()
                && (m_previewGeometry.asWkb() != m_originalGeometry.asWkb()))
            {
                m_layer->beginEditCommand(QStringLiteral("移动几何顶点"));
                if (m_layer->changeGeometry(m_featureId, m_previewGeometry))
                {
                    m_layer->endEditCommand();
                    report(QStringLiteral("几何顶点已移动，可使用撤销恢复。"));
                }
                else
                {
                    m_layer->destroyEditCommand();
                    report(QStringLiteral("顶点修改失败，图层未接受此几何变更。"));
                }
            }

            clearPreview();
        }

    private:
        void report(const QString& message) const
        {
            if (m_report)
            {
                m_report(message);
            }
        }

        void clearPreview()
        {
            m_dragging = false;
            m_layer = nullptr;
            m_featureId = QgsFeatureId();
            m_vertexIndex = -1;
            if (m_previewBand)
            {
                m_previewBand->hide();
                delete m_previewBand;
                m_previewBand = nullptr;
            }
        }

        std::function<QgsVectorLayer*()> m_currentLayer;
        std::function<void(const QString&)> m_report;
        QPointer<QgsVectorLayer> m_layer;
        QgsFeatureId m_featureId;
        int m_vertexIndex = -1;
        QgsGeometry m_originalGeometry;
        QgsGeometry m_previewGeometry;
        QgsRubberBand* m_previewBand = nullptr;
        bool m_dragging = false;
    };

    class WorkspaceFeatureSelectTool final : public QgsMapTool
    {
    public:
        WorkspaceFeatureSelectTool(QgsMapCanvas* canvas, std::function<void(const QString&)> report)
            : QgsMapTool(canvas), m_report(std::move(report))
        {
            setToolName(QStringLiteral("Select feature"));
        }

        void setTreeView(QgsLayerTreeView* view)
        {
            m_treeView = view;
        }

        void canvasReleaseEvent(QgsMapMouseEvent* event) override
        {
            if (!event || event->button() != Qt::LeftButton)
            {
                return;
            }

            QgsVectorLayer* layer = m_treeView
                ? qobject_cast<QgsVectorLayer*>(m_treeView->currentLayer())
                : nullptr;
            if (!layer || !layer->isValid())
            {
                if (m_report)
                {
                    m_report(QStringLiteral("请先在图层管理中选择一个有效的矢量图层。"));
                }
                return;
            }

            const QgsPointXY point = event->mapPoint();
            const double tolerance = qMax(canvas()->mapUnitsPerPixel() * 6.0, 1e-12);
            const QgsRectangle mapSearchExtent(
                point.x() - tolerance, point.y() - tolerance,
                point.x() + tolerance, point.y() + tolerance);
            const QgsRectangle layerSearchExtent = toLayerCoordinates(layer, mapSearchExtent);
            QgsFeatureIterator features = layer->getFeatures(
                QgsFeatureRequest().setFilterRect(layerSearchExtent));
            QgsFeature feature;
            if (features.nextFeature(feature))
            {
                layer->selectByIds(QgsFeatureIds() << feature.id(), Qgis::SelectBehavior::AddToSelection);
                canvas()->refresh();
                if (m_report)
                {
                    m_report(QStringLiteral("已选择要素 %1（%2）。").arg(feature.id()).arg(layer->name()));
                }
            }
            else if (m_report)
            {
                m_report(QStringLiteral("当前位置没有找到“%1”的要素。").arg(layer->name()));
            }
        }

    private:
        QgsLayerTreeView* m_treeView = nullptr;
        std::function<void(const QString&)> m_report;
    };

    class WorkspaceIdentifyTool final : public QgsMapToolIdentify
    {
    public:
        using IdentifiedHandler =
            std::function<void(const QgsMapToolIdentify::IdentifyResult&)>;
        using NoMatchHandler = std::function<void()>;

        WorkspaceIdentifyTool(
            QgsMapCanvas* canvas,
            IdentifiedHandler identifiedHandler,
            NoMatchHandler noMatchHandler)
            : QgsMapToolIdentify(canvas),
              m_identifiedHandler(std::move(identifiedHandler)),
              m_noMatchHandler(std::move(noMatchHandler))
        {
            setToolName(QStringLiteral("Identify visible vector features"));
        }

        void canvasReleaseEvent(QgsMapMouseEvent* event) override
        {
            if (!event || event->button() != Qt::LeftButton || !canvas())
            {
                return;
            }

            QList<QgsMapLayer*> visibleVectorLayers;
            for (QgsMapLayer* mapLayer : canvas()->layers())
            {
                auto* vectorLayer = qobject_cast<QgsVectorLayer*>(mapLayer);
                if (vectorLayer && vectorLayer->isValid())
                {
                    visibleVectorLayers.append(vectorLayer);
                }
            }

            if (visibleVectorLayers.isEmpty())
            {
                reportNoMatch();
                return;
            }

            const QList<QgsMapToolIdentify::IdentifyResult> results =
                identify(
                    event->x(),
                    event->y(),
                    visibleVectorLayers,
                    QgsMapToolIdentify::TopDownStopAtFirst);
            if (results.isEmpty()
                || !results.constFirst().mLayer
                || !results.constFirst().mFeature.isValid())
            {
                reportNoMatch();
                return;
            }

            if (m_identifiedHandler)
            {
                m_identifiedHandler(results.constFirst());
            }
        }

    private:
        void reportNoMatch()
        {
            if (m_noMatchHandler)
            {
                m_noMatchHandler();
            }
        }

        IdentifiedHandler m_identifiedHandler;
        NoMatchHandler m_noMatchHandler;
    };

    class WorkspaceMeasureTool final : public QgsMapTool
    {
    public:
        WorkspaceMeasureTool(
            QgsMapCanvas* canvas,
            QgsMapTool* returnTool,
            bool measureArea,
            std::function<void(const QString&)> report)
            : QgsMapTool(canvas),
              m_returnTool(returnTool),
              m_measureArea(measureArea),
              m_report(std::move(report)),
              m_rubberBand(new QgsRubberBand(
                  canvas,
                  measureArea ? QgsWkbTypes::PolygonGeometry : QgsWkbTypes::LineGeometry))
        {
            m_rubberBand->setWidth(2);
            m_rubberBand->setStrokeColor(QColor(0, 174, 239));
            m_rubberBand->setFillColor(QColor(0, 174, 239, 45));
            m_distanceArea.setSourceCrs(
                canvas->mapSettings().destinationCrs(),
                QgsProject::instance()->transformContext());
            m_distanceArea.setEllipsoid(QgsProject::instance()->ellipsoid());
            setToolName(measureArea ? QStringLiteral("Measure area") : QStringLiteral("Measure distance"));
        }

        void canvasReleaseEvent(QgsMapMouseEvent* event) override
        {
            if (!event)
            {
                return;
            }
            if (event->button() == Qt::RightButton)
            {
                finish();
                return;
            }
            if (event->button() != Qt::LeftButton)
            {
                return;
            }
            m_points.append(event->mapPoint());
            redraw();
            reportMeasurement(false);
        }

        void canvasDoubleClickEvent(QgsMapMouseEvent* event) override
        {
            if (event && event->button() == Qt::LeftButton)
            {
                if (m_points.isEmpty() || m_points.last() != event->mapPoint())
                {
                    m_points.append(event->mapPoint());
                    redraw();
                }
                finish();
            }
        }

        void deactivate() override
        {
            m_points.clear();
            m_rubberBand->reset(m_measureArea ? QgsWkbTypes::PolygonGeometry : QgsWkbTypes::LineGeometry);
            QgsMapTool::deactivate();
        }

    private:
        void redraw()
        {
            m_rubberBand->reset(m_measureArea ? QgsWkbTypes::PolygonGeometry : QgsWkbTypes::LineGeometry);
            for (const QgsPointXY& point : m_points)
            {
                m_rubberBand->addPoint(point);
            }
            if (m_measureArea && m_points.size() >= 3)
            {
                m_rubberBand->addPoint(m_points.first());
            }
        }

        void reportMeasurement(bool finished)
        {
            if (!m_report || m_points.size() < (m_measureArea ? 3 : 2))
            {
                return;
            }
            if (m_measureArea)
            {
                QgsPolygonXY polygon;
                polygon.append(m_points);
                const double area = m_distanceArea.measureArea(QgsGeometry::fromPolygonXY(polygon));
                m_report(QStringLiteral("%1面积：%2")
                    .arg(finished ? QStringLiteral("测量完成，") : QString())
                    .arg(QgsDistanceArea::formatArea(area, 2, m_distanceArea.areaUnits())));
            }
            else
            {
                const double distance = m_distanceArea.measureLine(m_points);
                m_report(QStringLiteral("%1距离：%2")
                    .arg(finished ? QStringLiteral("测量完成，") : QString())
                    .arg(QgsDistanceArea::formatDistance(distance, 2, m_distanceArea.lengthUnits())));
            }
        }

        void finish()
        {
            if (m_points.size() >= (m_measureArea ? 3 : 2))
            {
                reportMeasurement(true);
            }
            else if (m_report)
            {
                m_report(m_measureArea
                    ? QStringLiteral("面积测量至少需要三个点。")
                    : QStringLiteral("距离测量至少需要两个点。"));
            }
            if (canvas() && m_returnTool)
            {
                canvas()->setMapTool(m_returnTool);
            }
        }

        QgsMapTool* m_returnTool = nullptr;
        bool m_measureArea = false;
        std::function<void(const QString&)> m_report;
        QgsRubberBand* m_rubberBand = nullptr;
        QgsDistanceArea m_distanceArea;
        QVector<QgsPointXY> m_points;
    };

    class WorkspaceAttributeTableVectorTools final : public QgsVectorLayerTools
    {
    public:
        WorkspaceAttributeTableVectorTools(QgsMapCanvas* canvas, QWidget* parent)
            : m_canvas(canvas), m_parent(parent)
        {
        }

        bool addFeature(QgsVectorLayer* layer,
            const QgsAttributeMap& defaultValues,
            const QgsGeometry& defaultGeometry,
            QgsFeature* feature,
            QWidget* parentWidget,
            bool showModal,
            bool hideParent) const override
        {
            Q_UNUSED(parentWidget);
            Q_UNUSED(showModal);
            Q_UNUSED(hideParent);
            if (!layer || (!layer->isEditable() && !startEditing(layer)))
            {
                return false;
            }
            QgsFeature added = feature ? *feature : QgsFeature(layer->fields());
            for (auto it = defaultValues.constBegin(); it != defaultValues.constEnd(); ++it)
            {
                added.setAttribute(it.key(), it.value());
            }
            if (!defaultGeometry.isNull())
            {
                added.setGeometry(defaultGeometry);
            }
            added.setValid(true);
            if (!layer->addFeature(added))
            {
                return false;
            }
            if (feature)
            {
                *feature = added;
            }
            if (m_canvas)
            {
                m_canvas->refresh();
            }
            return true;
        }

        bool startEditing(QgsVectorLayer* layer) const override
        {
            return layer && (layer->isEditable() || layer->startEditing());
        }

        bool stopEditing(QgsVectorLayer* layer, bool allowCancel) const override
        {
            if (!layer || !layer->isEditable())
            {
                return true;
            }
            if (allowCancel)
            {
                const QMessageBox::StandardButton answer = QMessageBox::question(
                    m_parent, QStringLiteral("保存修改"),
                    QStringLiteral("是否保存当前图层的修改？"),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                    QMessageBox::Save);
                if (answer == QMessageBox::Cancel)
                {
                    return false;
                }
                if (answer == QMessageBox::Discard)
                {
                    return layer->rollBack();
                }
            }
            return layer->commitChanges();
        }

        bool saveEdits(QgsVectorLayer* layer) const override
        {
            return layer && (!layer->isEditable() || layer->commitChanges(false));
        }

    private:
        QPointer<QgsMapCanvas> m_canvas;
        QPointer<QWidget> m_parent;
    };

    class WorkspaceAttributeTableDialog final : public QDialog
    {
    public:
        WorkspaceAttributeTableDialog(
            QgsVectorLayer* layer,
            QgsMapCanvas* canvas,
            QWidget* parent,
            const QString& initialFilter)
            : QDialog(parent, Qt::Window),
              m_layer(layer),
              m_canvas(canvas),
              m_vectorTools(canvas, this)
        {
            setAttribute(Qt::WA_DeleteOnClose);
            setObjectName(QStringLiteral("workspaceAttributeTableDialog"));
            setWindowTitle(QStringLiteral("属性表 - %1").arg(layer ? layer->name() : QString()));

            QVBoxLayout* layout = new QVBoxLayout(this);
            layout->setContentsMargins(6, 6, 6, 6);
            layout->setSpacing(5);

            QToolBar* toolbar = new QToolBar(this);
            toolbar->setObjectName(QStringLiteral("attributeTableToolbar"));
            toolbar->setMovable(false);
            toolbar->setFloatable(false);
            toolbar->setIconSize(QSize(18, 18));
            toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
            toolbar->setStyleSheet(QStringLiteral(
                "QToolBar#attributeTableToolbar { spacing: 2px; padding: 1px; }"
                "QToolBar#attributeTableToolbar QToolButton { "
                "padding: 2px 4px; min-width: 28px; max-width: 32px; "
                "min-height: 28px; max-height: 32px; }"));
            layout->addWidget(toolbar);

            m_dualView = new QgsDualView(this);
            QgsAttributeEditorContext context;
            context.setMapCanvas(canvas);
            context.setVectorLayerTools(&m_vectorTools);
            m_dualView->init(layer, canvas, QgsFeatureRequest(), context, true, false);
            layout->addWidget(m_dualView, 1);

            QWidget* filterRow = new QWidget(this);
            QHBoxLayout* filterLayout = new QHBoxLayout(filterRow);
            filterLayout->setContentsMargins(0, 0, 0, 0);
            filterLayout->setSpacing(5);

            m_filterMode = new QComboBox(filterRow);
            m_filterMode->setObjectName(QStringLiteral("attributeTableFilterMode"));
            addFilterMode(QStringLiteral("显示全部"), QgsAttributeTableFilterModel::ShowAll);
            addFilterMode(QStringLiteral("仅显示选中要素"), QgsAttributeTableFilterModel::ShowSelected);
            addFilterMode(QStringLiteral("仅显示当前地图范围"), QgsAttributeTableFilterModel::ShowVisible);
            addFilterMode(QStringLiteral("仅显示未保存的修改"), QgsAttributeTableFilterModel::ShowEdited);
            m_filterMode->setToolTip(QStringLiteral("选择属性表中显示的要素范围"));
            filterLayout->addWidget(m_filterMode);

            m_filterExpression = new QLineEdit(filterRow);
            m_filterExpression->setObjectName(QStringLiteral("attributeTableExpressionFilter"));
            m_filterExpression->setPlaceholderText(QStringLiteral("表达式筛选，例如：\"类型\" = '道路'"));
            m_filterExpression->setClearButtonEnabled(true);
            filterLayout->addWidget(m_filterExpression, 1);

            QToolButton* applyFilterButton = new QToolButton(filterRow);
            applyFilterButton->setText(QStringLiteral("应用筛选"));
            applyFilterButton->setToolTip(QStringLiteral("按表达式筛选要素"));
            filterLayout->addWidget(applyFilterButton);
            layout->insertWidget(1, filterRow);

            m_countLabel = new QLabel(this);
            m_countLabel->setObjectName(QStringLiteral("attributeTableFeatureCount"));
            layout->addWidget(m_countLabel);

            QActionGroup* viewGroup = new QActionGroup(toolbar);
            viewGroup->setExclusive(true);
            QAction* tableViewAction = addAction(
                toolbar, QStyle::SP_FileDialogDetailedView,
                QStringLiteral("表格视图"), QStringLiteral("切换到表格视图"));
            QAction* formViewAction = addAction(
                toolbar, QStyle::SP_FileDialogListView,
                QStringLiteral("表单视图"), QStringLiteral("切换到表单视图"));
            for (QAction* action : { tableViewAction, formViewAction })
            {
                action->setCheckable(true);
                viewGroup->addAction(action);
            }
            tableViewAction->setChecked(true);
            connect(tableViewAction, &QAction::triggered, m_dualView,
                [this]() { m_dualView->setView(QgsDualView::AttributeTable); });
            connect(formViewAction, &QAction::triggered, m_dualView,
                [this]() { m_dualView->setView(QgsDualView::AttributeEditor); });

            toolbar->addSeparator();
            m_editAction = addAction(
                toolbar, QStyle::SP_DialogYesButton,
                QStringLiteral("编辑"), QStringLiteral("开始或结束图层编辑"));
            m_editAction->setCheckable(true);
            connect(m_editAction, &QAction::toggled,
                this, [this](bool checked) { toggleEditing(checked); });

            m_saveAction = addAction(
                toolbar, QStyle::SP_DialogSaveButton,
                QStringLiteral("保存修改"), QStringLiteral("将当前编辑保存到数据源"));
            connect(m_saveAction, &QAction::triggered, this, [this]() { saveEdits(); });
            m_rollbackAction = addAction(
                toolbar, QStyle::SP_DialogCancelButton,
                QStringLiteral("撤销修改"), QStringLiteral("撤销当前尚未保存的编辑"));
            connect(m_rollbackAction, &QAction::triggered, this, [this]() { rollbackEdits(true); });

            toolbar->addSeparator();
            m_addFeatureAction = addAction(
                toolbar, QStyle::SP_FileIcon,
                QStringLiteral("添加要素"), QStringLiteral("通过属性表单添加要素"));
            connect(m_addFeatureAction, &QAction::triggered,
                this, [this]() { addFeature(); });
            m_deleteFeaturesAction = addAction(
                toolbar, QStyle::SP_TrashIcon,
                QStringLiteral("删除选中要素"), QStringLiteral("删除当前选中的要素"));
            connect(m_deleteFeaturesAction, &QAction::triggered,
                this, [this]() { deleteSelectedFeatures(); });
            m_addFieldAction = addAction(
                toolbar, QStyle::SP_FileDialogNewFolder,
                QStringLiteral("添加字段"), QStringLiteral("向图层添加一个字段"));
            connect(m_addFieldAction, &QAction::triggered,
                this, [this]() { addField(); });
            m_deleteFieldAction = addAction(
                toolbar, QStyle::SP_DialogDiscardButton,
                QStringLiteral("删除字段"), QStringLiteral("从图层删除一个字段"));
            connect(m_deleteFieldAction, &QAction::triggered,
                this, [this]() { deleteField(); });
            m_fieldCalculatorAction = addAction(
                toolbar, QStyle::SP_ComputerIcon,
                QStringLiteral("字段计算器"), QStringLiteral("打开 QGIS 字段计算器"));
            connect(m_fieldCalculatorAction, &QAction::triggered,
                this, [this]() { openFieldCalculator(); });

            toolbar->addSeparator();
            QAction* selectAllAction = addAction(
                toolbar, QStyle::SP_DialogYesButton,
                QStringLiteral("全选"), QStringLiteral("选中图层中的全部要素"));
            connect(selectAllAction, &QAction::triggered, layer, &QgsVectorLayer::selectAll);
            QAction* invertSelectionAction = addAction(
                toolbar, QStyle::SP_BrowserReload,
                QStringLiteral("反选"), QStringLiteral("反转当前要素选择"));
            connect(invertSelectionAction, &QAction::triggered, layer, &QgsVectorLayer::invertSelection);
            QToolButton* clearSelectionButton = new QToolButton(filterRow);
            clearSelectionButton->setObjectName(
                QStringLiteral("attributeTableClearSelectionButton"));
            clearSelectionButton->setText(QStringLiteral("\u53D6\u6D88\u884C\u9009\u62E9"));
            clearSelectionButton->setToolTip(QStringLiteral(
                "\u6E05\u9664\u5C5E\u6027\u8868\u5F53\u524D\u884C\u9009\u62E9\u548C\u5730\u56FE\u9AD8\u4EAE\uFF0C\u4E0D\u4F1A\u5220\u9664\u8981\u7D20\u3002"));
            clearSelectionButton->setStatusTip(clearSelectionButton->toolTip());
            clearSelectionButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
            filterLayout->addWidget(clearSelectionButton);
            connect(clearSelectionButton, &QToolButton::clicked, this,
                [this]()
                {
                    if (m_layer)
                    {
                        m_layer->removeSelection();
                    }
                    if (m_dualView && m_dualView->tableView())
                    {
                        QgsAttributeTableView* tableView = m_dualView->tableView();
                        tableView->clearSelection();
                        tableView->setCurrentIndex(QModelIndex());
                    }
                });
            QAction* selectByExpressionAction = addAction(
                toolbar, QStyle::SP_FileDialogInfoView,
                QStringLiteral("按表达式选择"), QStringLiteral("使用 QGIS 表达式选择要素"));
            connect(selectByExpressionAction, &QAction::triggered,
                this, [this]() { selectByExpression(); });
            m_selectedOnTopAction = addAction(
                toolbar, QStyle::SP_ArrowUp,
                QStringLiteral("选中要素置顶"), QStringLiteral("将选中要素显示在表格顶部"));
            m_selectedOnTopAction->setCheckable(true);
            connect(m_selectedOnTopAction, &QAction::toggled,
                m_dualView, &QgsDualView::setSelectedOnTop);
            QAction* searchFormAction = addAction(
                toolbar, QStyle::SP_FileDialogContentsView,
                QStringLiteral("表单搜索"), QStringLiteral("在表单视图中按字段值搜索要素"));
            searchFormAction->setCheckable(true);
            connect(searchFormAction, &QAction::toggled, this,
                [this](bool enabled)
                {
                    if (enabled)
                    {
                        m_dualView->setView(QgsDualView::AttributeEditor);
                    }
                    m_dualView->toggleSearchMode(enabled);
                });
            m_multiEditAction = addAction(
                toolbar, QStyle::SP_DialogApplyButton,
                QStringLiteral("批量编辑"), QStringLiteral("同时编辑多个选中要素的属性"));
            m_multiEditAction->setCheckable(true);
            connect(m_multiEditAction, &QAction::toggled,
                m_dualView, &QgsDualView::setMultiEditEnabled);
            QAction* copyRowsAction = addAction(
                toolbar, QStyle::SP_FileDialogContentsView,
                QStringLiteral("复制选中行"), QStringLiteral("复制选中要素的属性到剪贴板"));
            connect(copyRowsAction, &QAction::triggered,
                this, [this]() { copySelectedRows(); });
            toolbar->addSeparator();
            QAction* zoomAction = addAction(
                toolbar, QStyle::SP_ArrowUp,
                QStringLiteral("缩放至选中要素"), QStringLiteral("将地图缩放到选中要素"));
            connect(zoomAction, &QAction::triggered,
                this, [this]() { zoomToSelection(); });
            QAction* panAction = addAction(
                toolbar, QStyle::SP_ArrowBack,
                QStringLiteral("平移至选中要素"), QStringLiteral("将地图平移到选中要素"));
            connect(panAction, &QAction::triggered,
                this, [this]() { panToSelection(); });
            m_reloadAction = addAction(
                toolbar, QStyle::SP_BrowserReload,
                QStringLiteral("重新加载"), QStringLiteral("从数据源重新加载图层"));
            connect(m_reloadAction, &QAction::triggered,
                this, [this]() { reloadLayer(); });
            QAction* conditionalStyleAction = addAction(
                toolbar, QStyle::SP_FileDialogInfoView,
                QStringLiteral("条件样式"), QStringLiteral("配置属性表条件样式"));
            connect(conditionalStyleAction, &QAction::triggered,
                m_dualView, &QgsDualView::openConditionalStyles);

            connect(applyFilterButton, &QToolButton::clicked,
                this, [this]() { applyExpressionFilter(); });
            connect(m_filterExpression, &QLineEdit::returnPressed,
                this, [this]() { applyExpressionFilter(); });
            connect(m_filterMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index)
                {
                    m_dualView->setFilterMode(static_cast<QgsAttributeTableFilterModel::FilterMode>(
                        m_filterMode->itemData(index).toInt()));
                    updateFeatureCount();
                });

            connect(m_dualView, &QgsDualView::filterChanged,
                this, [this]() { updateFeatureCount(); });
            connect(layer, &QgsVectorLayer::selectionChanged,
                this, [this]() { updateActionState(); });
            connect(layer, &QgsVectorLayer::editingStarted,
                this, [this]() { updateActionState(); });
            connect(layer, &QgsVectorLayer::editingStopped,
                this, [this]() { updateActionState(); });
            connect(layer, &QgsVectorLayer::featureAdded,
                this, [this](QgsFeatureId) { updateActionState(); });
            connect(layer, &QgsVectorLayer::featureDeleted,
                this, [this](QgsFeatureId) { updateActionState(); });
            connect(layer, &QgsVectorLayer::attributeValueChanged,
                this, [this](QgsFeatureId, int, const QVariant&)
                {
                    if (m_saveAction) m_saveAction->setEnabled(true);
                    if (m_rollbackAction) m_rollbackAction->setEnabled(true);
                });
            connect(layer, &QgsVectorLayer::geometryChanged,
                this, [this](QgsFeatureId, const QgsGeometry&)
                {
                    if (m_saveAction) m_saveAction->setEnabled(true);
                    if (m_rollbackAction) m_rollbackAction->setEnabled(true);
                });
            connect(layer, &QObject::destroyed, this,
                [this]() { m_layer = nullptr; close(); });

            if (!initialFilter.trimmed().isEmpty())
            {
                m_filterExpression->setText(initialFilter);
                applyExpressionFilter();
            }
            updateActionState();

            QScreen* targetScreen = QGuiApplication::screenAt(QCursor::pos());
            if (!targetScreen && parent)
            {
                targetScreen = parent->screen();
            }
            if (!targetScreen)
            {
                targetScreen = QApplication::primaryScreen();
            }
            const QRect available = targetScreen ? targetScreen->availableGeometry() : QRect();
            const int availableWidth = available.width() > 0 ? available.width() : 1000;
            const int availableHeight = available.height() > 0 ? available.height() : 650;
            setMinimumSize(qMin(availableWidth, 800), qMin(availableHeight, 500));
            resize(qMin(availableWidth, qMax(800, qMin(1100, qRound(availableWidth * 0.78)))),
                qMin(availableHeight, qMax(500, qMin(700, qRound(availableHeight * 0.68)))));
        }

        void setExpressionFilter(const QString& expression)
        {
            m_filterExpression->setText(expression);
            applyExpressionFilter();
        }

    protected:
        void closeEvent(QCloseEvent* event) override
        {
            if (m_layer && m_layer->isEditable() && m_layer->isModified())
            {
                QMessageBox message(QMessageBox::Warning,
                    QStringLiteral("未保存的编辑"),
                    QStringLiteral("图层“%1”有尚未保存的修改。是否保存后关闭属性表？")
                        .arg(m_layer->name()),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                    this);
                message.setButtonText(QMessageBox::Save, QStringLiteral("保存"));
                message.setButtonText(QMessageBox::Discard, QStringLiteral("放弃修改"));
                message.setButtonText(QMessageBox::Cancel, QStringLiteral("取消"));
                const int choice = message.exec();
                if (choice == QMessageBox::Cancel)
                {
                    event->ignore();
                    return;
                }
                if (choice == QMessageBox::Save && !saveEdits())
                {
                    event->ignore();
                    return;
                }
                if (choice == QMessageBox::Discard)
                {
                    m_layer->rollBack();
                }
            }
            QDialog::closeEvent(event);
        }

    private:
        QAction* addAction(QToolBar* toolbar, QStyle::StandardPixmap pixmap,
            const QString& text, const QString& toolTip)
        {
            QAction* action = toolbar->addAction(style()->standardIcon(pixmap), text);
            action->setToolTip(toolTip);
            action->setStatusTip(toolTip);
            return action;
        }

        void addFilterMode(const QString& text, QgsAttributeTableFilterModel::FilterMode mode)
        {
            m_filterMode->addItem(text, static_cast<int>(mode));
        }

        QgsFeatureIds selectedFeatureIds() const
        {
            QgsFeatureIds ids;
            if (!m_layer || !m_dualView)
            {
                return ids;
            }
            const QList<QgsFeatureId> tableIds = m_dualView->tableView()->selectedFeaturesIds();
            for (QgsFeatureId id : tableIds)
            {
                ids.insert(id);
            }
            if (ids.isEmpty())
            {
                ids = m_layer->selectedFeatureIds();
            }
            return ids;
        }

        QList<QgsFeatureId> selectedFeatureIdsInTableOrder() const
        {
            if (!m_layer || !m_dualView)
            {
                return QList<QgsFeatureId>();
            }
            const QList<QgsFeatureId> tableIds = m_dualView->tableView()->selectedFeaturesIds();
            return tableIds.isEmpty() ? m_layer->selectedFeatureIds().values() : tableIds;
        }

        bool ensureEditing()
        {
            if (!m_layer || m_layer->readOnly() || !m_layer->dataProvider()
                || !(m_layer->dataProvider()->capabilities() & QgsVectorDataProvider::EditingCapabilities))
            {
                QMessageBox::information(this, QStringLiteral("图层不可编辑"),
                    QStringLiteral("当前图层的数据源不支持编辑。"));
                return false;
            }
            if (m_layer->isEditable())
            {
                return true;
            }
            if (!m_layer->startEditing())
            {
                QMessageBox::warning(this, QStringLiteral("无法开始编辑"),
                    QStringLiteral("无法为图层“%1”启动编辑。请检查数据源权限和编辑能力。")
                        .arg(m_layer->name()));
                updateActionState();
                return false;
            }
            updateActionState();
            return true;
        }

        bool saveEdits()
        {
            if (!m_layer || !m_layer->isEditable())
            {
                return true;
            }
            if (!m_layer->commitChanges(false))
            {
                const QString errors = m_layer->commitErrors().join(QLatin1Char('\n'));
                QMessageBox::warning(this, QStringLiteral("保存失败"),
                    errors.isEmpty() ? QStringLiteral("图层修改未能保存到数据源。") : errors);
                return false;
            }
            if (m_canvas)
            {
                m_canvas->refresh();
            }
            updateActionState();
            updateFeatureCount();
            return true;
        }

        void rollbackEdits(bool ask, bool stopEditing = false)
        {
            if (!m_layer || !m_layer->isEditable())
            {
                return;
            }
            if (ask && m_layer->isModified())
            {
                const QMessageBox::StandardButton answer = QMessageBox::question(
                    this, QStringLiteral("放弃修改"),
                    QStringLiteral("确定撤销图层中所有尚未保存的修改吗？"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (answer != QMessageBox::Yes)
                {
                    return;
                }
            }
            m_layer->rollBack(stopEditing);
            if (m_canvas)
            {
                m_canvas->refresh();
            }
            updateActionState();
            updateFeatureCount();
        }

        void toggleEditing(bool checked)
        {
            if (!m_layer)
            {
                return;
            }
            if (checked)
            {
                if (!ensureEditing())
                {
                    synchronizeEditAction();
                }
                return;
            }
            if (!m_layer->isEditable())
            {
                updateActionState();
                return;
            }
            if (m_layer->isModified())
            {
                QMessageBox message(QMessageBox::Warning,
                    QStringLiteral("结束编辑"),
                    QStringLiteral("图层有尚未保存的修改。请选择保存、放弃或继续编辑。"),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                    this);
                message.setButtonText(QMessageBox::Save, QStringLiteral("保存"));
                message.setButtonText(QMessageBox::Discard, QStringLiteral("放弃修改"));
                message.setButtonText(QMessageBox::Cancel, QStringLiteral("继续编辑"));
                const int choice = message.exec();
                if (choice == QMessageBox::Save)
                {
                    if (!saveEdits())
                    {
                        synchronizeEditAction();
                    }
                }
                else if (choice == QMessageBox::Discard)
                {
                    rollbackEdits(false, true);
                }
                else
                {
                    synchronizeEditAction();
                }
            }
            else
            {
                m_layer->rollBack();
                updateActionState();
            }
        }

        void synchronizeEditAction()
        {
            if (!m_editAction || !m_layer)
            {
                return;
            }
            const QSignalBlocker blocker(m_editAction);
            m_editAction->setChecked(m_layer->isEditable());
        }

        void updateActionState()
        {
            if (!m_layer)
            {
                return;
            }
            const QgsVectorDataProvider* provider = m_layer->dataProvider();
            const QgsVectorDataProvider::Capabilities capabilities = provider
                ? provider->capabilities()
                : QgsVectorDataProvider::Capabilities();
            const bool canEdit = provider && !m_layer->readOnly()
                && (capabilities & QgsVectorDataProvider::EditingCapabilities);
            if (m_editAction)
            {
                m_editAction->setEnabled(canEdit);
                synchronizeEditAction();
            }
            const bool editable = m_layer->isEditable();
            if (m_saveAction) m_saveAction->setEnabled(editable && m_layer->isModified());
            if (m_rollbackAction) m_rollbackAction->setEnabled(editable && m_layer->isModified());
            if (m_addFeatureAction) m_addFeatureAction->setEnabled(canEdit && (capabilities & QgsVectorDataProvider::AddFeatures));
            if (m_deleteFeaturesAction) m_deleteFeaturesAction->setEnabled(canEdit && editable
                && (capabilities & QgsVectorDataProvider::DeleteFeatures)
                && !selectedFeatureIds().isEmpty());
            if (m_addFieldAction) m_addFieldAction->setEnabled(canEdit && editable
                && (capabilities & QgsVectorDataProvider::AddAttributes));
            if (m_deleteFieldAction) m_deleteFieldAction->setEnabled(canEdit && editable
                && (capabilities & QgsVectorDataProvider::DeleteAttributes));
            if (m_fieldCalculatorAction) m_fieldCalculatorAction->setEnabled(canEdit);
            if (m_multiEditAction) m_multiEditAction->setEnabled(canEdit && editable);
            if (m_reloadAction) m_reloadAction->setEnabled(!editable);
            updateFeatureCount();
        }

        void updateFeatureCount()
        {
            if (!m_countLabel || !m_dualView || !m_layer)
            {
                return;
            }
            m_countLabel->setText(QStringLiteral("显示 %1 条要素 / 图层共 %2 条要素")
                .arg(m_dualView->filteredFeatureCount())
                .arg(m_dualView->featureCount()));
        }

        void applyExpressionFilter()
        {
            if (!m_layer || !m_dualView)
            {
                return;
            }
            const QString text = m_filterExpression->text().trimmed();
            const QgsExpression expression(text.isEmpty() ? QStringLiteral("TRUE") : text);
            if (expression.hasParserError())
            {
                QMessageBox::warning(this, QStringLiteral("筛选表达式错误"), expression.parserErrorString());
                return;
            }
            m_dualView->filterFeatures(expression, m_layer->createExpressionContext());
            updateFeatureCount();
        }

        void selectByExpression()
        {
            if (!m_layer)
            {
                return;
            }
            bool accepted = false;
            const QString text = QInputDialog::getText(this,
                QStringLiteral("按表达式选择"),
                QStringLiteral("输入 QGIS 表达式："),
                QLineEdit::Normal, QString(), &accepted).trimmed();
            if (!accepted || text.isEmpty())
            {
                return;
            }
            const QgsExpression expression(text);
            if (expression.hasParserError())
            {
                QMessageBox::warning(this, QStringLiteral("选择表达式错误"),
                    expression.parserErrorString());
                return;
            }
            QgsFeatureIds ids;
            QgsFeatureIterator iterator = m_layer->getFeatures(
                QgsFeatureRequest().setFilterExpression(text));
            QgsFeature feature;
            while (iterator.nextFeature(feature))
            {
                ids.insert(feature.id());
            }
            m_layer->selectByIds(ids);
        }

        void addFeature()
        {
            if (!ensureEditing())
            {
                return;
            }
            QgsFeature* feature = new QgsFeature(m_layer->fields());
            feature->setValid(true);
            QgsAttributeEditorContext context;
            context.setMapCanvas(m_canvas);
            context.setVectorLayerTools(&m_vectorTools);
            QgsAttributeDialog dialog(m_layer, feature, true, this, true, context);
            dialog.setMode(QgsAttributeEditorContext::AddFeatureMode);
            dialog.setEditCommandMessage(QStringLiteral("通过属性表添加要素"));
            dialog.setWindowTitle(QStringLiteral("添加要素 - %1").arg(m_layer->name()));
            dialog.exec();
            updateFeatureCount();
            updateActionState();
        }

        void deleteSelectedFeatures()
        {
            if (!ensureEditing())
            {
                return;
            }
            const QgsFeatureIds ids = selectedFeatureIds();
            if (ids.isEmpty())
            {
                QMessageBox::information(this, QStringLiteral("删除要素"), QStringLiteral("请先选择要删除的要素。"));
                return;
            }
            const QMessageBox::StandardButton answer = QMessageBox::question(
                this, QStringLiteral("删除要素"),
                QStringLiteral("确定删除选中的 %1 个要素吗？此操作可在保存前撤销。")
                    .arg(ids.size()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes)
            {
                return;
            }
            m_layer->selectByIds(ids);
            if (!m_layer->deleteSelectedFeatures())
            {
                QMessageBox::warning(this, QStringLiteral("删除失败"), QStringLiteral("无法删除当前选中的要素。"));
            }
            updateActionState();
            updateFeatureCount();
            if (m_canvas) m_canvas->refresh();
        }

        void addField()
        {
            if (!ensureEditing())
            {
                return;
            }
            bool accepted = false;
            const QString name = QInputDialog::getText(this,
                QStringLiteral("添加字段"), QStringLiteral("新字段名称："),
                QLineEdit::Normal, QString(), &accepted).trimmed();
            if (!accepted || name.isEmpty())
            {
                return;
            }
            if (m_layer->fields().indexFromName(name) >= 0)
            {
                QMessageBox::warning(this, QStringLiteral("字段已存在"),
                    QStringLiteral("字段“%1”已经存在。请使用其他名称。").arg(name));
                return;
            }
            const QStringList typeNames = {
                QStringLiteral("文本"), QStringLiteral("整数"),
                QStringLiteral("小数"), QStringLiteral("日期")
            };
            const QString selectedType = QInputDialog::getItem(this,
                QStringLiteral("字段类型"), QStringLiteral("选择字段类型："),
                typeNames, 0, false, &accepted);
            if (!accepted)
            {
                return;
            }
            QVariant::Type type = QVariant::String;
            if (selectedType == typeNames.at(1)) type = QVariant::Int;
            else if (selectedType == typeNames.at(2)) type = QVariant::Double;
            else if (selectedType == typeNames.at(3)) type = QVariant::Date;
            if (!m_layer->addAttribute(QgsField(name, type)))
            {
                QMessageBox::warning(this, QStringLiteral("添加字段失败"),
                    QStringLiteral("数据源未能创建字段“%1”。").arg(name));
                return;
            }
            m_dualView->masterModel()->resetModel();
            updateActionState();
        }

        void deleteField()
        {
            if (!ensureEditing())
            {
                return;
            }
            QStringList names;
            for (int index = 0; index < m_layer->fields().count(); ++index)
            {
                const QgsFields::FieldOrigin origin = m_layer->fields().fieldOrigin(index);
                if (origin == QgsFields::OriginProvider || origin == QgsFields::OriginEdit)
                {
                    names.append(m_layer->fields().at(index).name());
                }
            }
            if (names.isEmpty())
            {
                QMessageBox::information(this, QStringLiteral("删除字段"), QStringLiteral("当前没有可删除的字段。"));
                return;
            }
            bool accepted = false;
            const QString name = QInputDialog::getItem(this,
                QStringLiteral("删除字段"), QStringLiteral("选择要删除的字段："),
                names, 0, false, &accepted);
            if (!accepted)
            {
                return;
            }
            const QMessageBox::StandardButton answer = QMessageBox::question(
                this, QStringLiteral("删除字段"),
                QStringLiteral("删除字段“%1”会同时删除该字段的所有属性值，确定继续吗？")
                    .arg(name),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes)
            {
                return;
            }
            const int fieldIndex = m_layer->fields().indexFromName(name);
            if (fieldIndex < 0 || !m_layer->deleteAttribute(fieldIndex))
            {
                QMessageBox::warning(this, QStringLiteral("删除字段失败"),
                    QStringLiteral("数据源未能删除字段“%1”。").arg(name));
                return;
            }
            m_dualView->masterModel()->resetModel();
            updateActionState();
        }

        void openFieldCalculator()
        {
            if (!ensureEditing())
            {
                return;
            }
            QgsFieldCalculator calculator(m_layer, this);
            calculator.setWindowTitle(QStringLiteral("字段计算器 - %1").arg(m_layer->name()));
            calculator.exec();
            m_dualView->masterModel()->resetModel();
            updateActionState();
        }

        void copySelectedRows()
        {
            if (!m_layer)
            {
                return;
            }
            const QList<QgsFeatureId> orderedIds = selectedFeatureIdsInTableOrder();
            QgsFeatureIds ids;
            for (QgsFeatureId id : orderedIds)
            {
                ids.insert(id);
            }
            if (ids.isEmpty())
            {
                QMessageBox::information(this, QStringLiteral("复制要素"), QStringLiteral("请先选择要复制的表格行。"));
                return;
            }
            QMap<QgsFeatureId, QgsAttributes> attributesById;
            QgsFeatureIterator iterator = m_layer->getFeatures(QgsFeatureRequest().setFilterFids(ids));
            QgsFeature feature;
            while (iterator.nextFeature(feature))
            {
                attributesById.insert(feature.id(), feature.attributes());
            }
            QStringList lines;
            QStringList headers;
            for (const QgsField& field : m_layer->fields())
            {
                headers.append(field.name());
            }
            lines.append(headers.join(QLatin1Char('\t')));
            for (QgsFeatureId id : orderedIds)
            {
                if (!attributesById.contains(id))
                {
                    continue;
                }
                QStringList values;
                const QgsAttributes attributes = attributesById.value(id);
                for (const QVariant& value : attributes)
                {
                    QString text = value.toString();
                    text.replace(QLatin1Char('\t'), QLatin1Char(' '));
                    text.replace(QLatin1Char('\r'), QLatin1Char(' '));
                    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
                    values.append(text);
                }
                lines.append(values.join(QLatin1Char('\t')));
            }
            QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
        }

        void zoomToSelection()
        {
            const QgsFeatureIds ids = selectedFeatureIds();
            if (m_canvas && m_layer && !ids.isEmpty())
            {
                m_canvas->zoomToFeatureIds(m_layer, ids);
            }
        }

        void panToSelection()
        {
            const QgsFeatureIds ids = selectedFeatureIds();
            if (m_canvas && m_layer && !ids.isEmpty())
            {
                m_canvas->panToFeatureIds(m_layer, ids, true);
            }
        }

        void reloadLayer()
        {
            if (!m_layer || m_layer->isEditable())
            {
                QMessageBox::information(this, QStringLiteral("重新加载"),
                    QStringLiteral("请先保存或撤销编辑，再重新加载图层。"));
                return;
            }
            m_layer->reload();
            m_dualView->masterModel()->resetModel();
            if (m_canvas) m_canvas->refresh();
            updateFeatureCount();
        }

        QPointer<QgsVectorLayer> m_layer;
        QPointer<QgsMapCanvas> m_canvas;
        WorkspaceAttributeTableVectorTools m_vectorTools;
        QgsDualView* m_dualView = nullptr;
        QComboBox* m_filterMode = nullptr;
        QLineEdit* m_filterExpression = nullptr;
        QLabel* m_countLabel = nullptr;
        QAction* m_editAction = nullptr;
        QAction* m_saveAction = nullptr;
        QAction* m_rollbackAction = nullptr;
        QAction* m_addFeatureAction = nullptr;
        QAction* m_deleteFeaturesAction = nullptr;
        QAction* m_addFieldAction = nullptr;
        QAction* m_deleteFieldAction = nullptr;
        QAction* m_fieldCalculatorAction = nullptr;
        QAction* m_selectedOnTopAction = nullptr;
        QAction* m_reloadAction = nullptr;
        QAction* m_multiEditAction = nullptr;
    };
}

QDialog* LTZK::openAttributeTable(QgsVectorLayer* layer, const QString& filterExpression)
{
    if (!layer || !layer->isValid())
    {
        statusBar()->showMessage(QStringLiteral("请先选择一个有效的矢量图层。"), 4000);
        return nullptr;
    }

    const QString layerId = layer->id();
    QDialog* existingDialog = m_attributeTableDialogs.value(layerId).data();
    if (existingDialog)
    {
        auto* dialog = static_cast<WorkspaceAttributeTableDialog*>(existingDialog);
        if (!filterExpression.trimmed().isEmpty())
        {
            dialog->setExpressionFilter(filterExpression);
        }
        dialog->setWindowState(dialog->windowState() & ~Qt::WindowMinimized);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
        return dialog;
    }

    auto* dialog = new WorkspaceAttributeTableDialog(layer, m_mapCanvas, this, filterExpression);
    m_attributeTableDialogs.insert(layerId, dialog);
    const auto forgetDialog = [this, layerId, dialog]()
    {
        // An older, deferred-deletion dialog must not remove its replacement.
        if (m_attributeTableDialogs.value(layerId).data() == dialog
            || m_attributeTableDialogs.value(layerId).isNull())
        {
            m_attributeTableDialogs.remove(layerId);
        }
    };
    connect(dialog, &QDialog::finished, this, forgetDialog);
    connect(dialog, &QObject::destroyed, this, forgetDialog);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    statusBar()->showMessage(QStringLiteral("已打开图层“%1”的属性表。")
        .arg(layer->name()), 3000);
    return dialog;
}

void LTZK::initWorkspaceMapStatusBar(QVBoxLayout* mapLayout)
{
    if (!mapLayout || !m_mapCanvas || m_mapScaleWidget)
    {
        return;
    }
    statusBar()->setStyleSheet(QStringLiteral("QStatusBar::item { border: none; }"));
    auto* row = new QFrame(statusBar());
    row->setObjectName(QStringLiteral("workspaceMapStatusBar"));
    row->setFrameShape(QFrame::NoFrame);
    row->setStyleSheet(QStringLiteral(
        "#workspaceMapStatusBar, #workspaceMapStatusBar QgsScaleWidget, "
        "#workspaceMapStatusBar QLineEdit, #workspaceMapStatusBar QComboBox { background: transparent; }"
        "#workspaceMapStatusBar QComboBox::drop-down { background: transparent; }"));
    auto* layout = new QHBoxLayout(row);
    const qreal scale = LTZKUiScale::factorForWidget(row);
    const int margin = qMax(3, LTZKUiScale::scaledInt(4, scale));
    layout->setContentsMargins(margin, 0, margin, 0);
    layout->setSpacing(margin * 2);
    auto* coordinateLabel = new QLabel(QStringLiteral("\u5750\u6807"), row);
    layout->addWidget(coordinateLabel);
    m_mapCoordinateEdit = new QLineEdit(QStringLiteral("--, --"), row);
    m_mapCoordinateEdit->setObjectName(QStringLiteral("workspaceMapCoordinate"));
    m_mapCoordinateEdit->setReadOnly(true);
    m_mapCoordinateEdit->setMinimumWidth(row->fontMetrics().horizontalAdvance(
        QStringLiteral("-12345678.00, -12345678.00")) + 18);
    m_mapCoordinateEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    coordinateLabel->setBuddy(m_mapCoordinateEdit);
    layout->addWidget(m_mapCoordinateEdit);
    m_mapCrsLabel = new QLabel(row);
    m_mapCrsLabel->setObjectName(QStringLiteral("workspaceMapCrs"));
    m_mapCrsLabel->setTextFormat(Qt::RichText);
    m_mapCrsLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    m_mapCrsLabel->setOpenExternalLinks(false);
    m_mapCrsLabel->setCursor(Qt::PointingHandCursor);
    connect(m_mapCrsLabel, &QLabel::linkActivated, this, [this]() { showWorkspaceProjectCrsDialog(); });
    layout->addWidget(m_mapCrsLabel);
    auto* scaleLabel = new QLabel(QStringLiteral("\u6BD4\u4F8B"), row);
    layout->addWidget(scaleLabel);
    m_mapScaleWidget = new QgsScaleWidget(row);
    m_mapScaleWidget->setObjectName(QStringLiteral("workspaceMapScale"));
    m_mapScaleWidget->setMapCanvas(m_mapCanvas);
    m_mapScaleWidget->setMinimumWidth(row->fontMetrics().horizontalAdvance(QStringLiteral("1:100000000")) + 35);
    scaleLabel->setBuddy(m_mapScaleWidget);
    m_mapScaleWidget->setToolTip(QStringLiteral("\u8F93\u5165\u6216\u9009\u62E9\u6BD4\u4F8B\u5C3A\u4EE5\u7F29\u653E\u5730\u56FE"));
    layout->addWidget(m_mapScaleWidget);
    statusBar()->addPermanentWidget(row);

    connect(m_mapCanvas, &QgsMapCanvas::scaleChanged, row, [this](double denominator)
    {
        const QSignalBlocker blocker(m_mapScaleWidget);
        m_mapScaleWidget->setScale(denominator);
    });
    connect(m_mapScaleWidget, &QgsScaleWidget::scaleChanged, m_mapCanvas, [this](double denominator)
    {
        if (std::isfinite(denominator) && denominator > 0)
        {
            m_mapCanvas->zoomScale(denominator);
        }
    });
    connect(m_mapCanvas, &QgsMapCanvas::destinationCrsChanged, row, [this]() { updateWorkspaceMapCrs(); });
    connect(QgsProject::instance(), &QgsProject::crsChanged, row, [this]()
    {
        const auto oldCrs = m_mapCanvas->mapSettings().destinationCrs();
        const auto newCrs = QgsProject::instance()->crs();
        if (m_hasInitialMapView && oldCrs.isValid() && newCrs.isValid() && oldCrs != newCrs)
        {
            try
            {
                m_initialMapExtent = QgsCoordinateTransform(oldCrs, newCrs, QgsProject::instance())
                    .transformBoundingBox(m_initialMapExtent);
            }
            catch (const QgsCsException&)
            {
                m_hasInitialMapView = false;
            }
        }
        m_mapCanvas->setDestinationCrs(QgsProject::instance()->crs());
        m_mapCanvas->refresh();
    });
    m_mapScaleWidget->setScale(m_mapCanvas->scale());
    updateWorkspaceMapCrs();
}

void LTZK::updateWorkspaceMapCoordinates(const QgsPointXY& coordinate)
{
    if (!m_mapCoordinateEdit || !m_mapCanvas)
    {
        return;
    }
    const auto crs = m_mapCanvas->mapSettings().destinationCrs();
    if (!crs.isValid() || !std::isfinite(coordinate.x()) || !std::isfinite(coordinate.y()))
    {
        m_mapCoordinateEdit->setText(QStringLiteral("--, --"));
        return;
    }
    const int precision = crs.isGeographic() ? 6 : 2;
    m_mapCoordinateEdit->setText(QStringLiteral("%1, %2")
        .arg(QString::number(coordinate.x(), 'f', precision))
        .arg(QString::number(coordinate.y(), 'f', precision)));
}

void LTZK::updateWorkspaceMapCrs()
{
    if (!m_mapCrsLabel || !m_mapCanvas)
    {
        return;
    }
    const auto crs = m_mapCanvas->mapSettings().destinationCrs();
    const QString crsText = crs.isValid()
        ? (crs.authid().isEmpty() ? QStringLiteral("\u81EA\u5B9A\u4E49 CRS") : crs.authid())
        : QStringLiteral("CRS --");
    m_mapCrsLabel->setText(QStringLiteral("<a href=\"project-crs\" style=\"color: %1; text-decoration: none;\">%2</a>")
        .arg(m_mapCrsLabel->palette().color(QPalette::WindowText).name(), crsText.toHtmlEscaped()));
    m_mapCrsLabel->setToolTip(QStringLiteral("%1\n点击设置工程坐标参考系（CRS）").arg(crs.description()));
    m_mapCoordinateEdit->setToolTip(crs.isGeographic()
        ? QStringLiteral("\u5730\u56FE\u5750\u6807\u7CFB\u4E0B\u7684\u7ECF\u5EA6\u3001\u7EAC\u5EA6\uFF08\u5EA6\uFF09")
        : QStringLiteral("\u5730\u56FE\u5750\u6807\u7CFB\u4E0B\u7684 X\u3001Y \u5750\u6807"));
    m_mapCoordinateEdit->setText(QStringLiteral("--, --"));
    if (m_CurrentCoordinage)
    {
        m_CurrentCoordinage->setText(QStringLiteral("\u7ECF\u5EA6\uFF1A--  \u7EAC\u5EA6\uFF1A--"));
    }
}

void LTZK::showWorkspaceProjectCrsDialog()
{
    if (!m_mapCanvas) return;
    QgsProjectionSelectionDialog dialog(this);
    dialog.setObjectName(QStringLiteral("workspaceProjectCrsDialog"));
    dialog.setWindowTitle(QStringLiteral("工程坐标参考系（CRS）"));
    const qreal uiScale = qMax<qreal>(1.0, LTZKUiScale::factorForWidget(this));
    const QSize available = screen()->availableGeometry().size() - QSize(40, 80);
    dialog.setMinimumSize(QSize(LTZKUiScale::scaledInt(760, uiScale), LTZKUiScale::scaledInt(560, uiScale))
        .boundedTo(available));
    dialog.resize(QSize(LTZKUiScale::scaledInt(900, uiScale), LTZKUiScale::scaledInt(720, uiScale))
        .boundedTo(available));
    dialog.setShowNoProjection(false);
    dialog.setRequireValidSelection();
    const auto projectCrs = QgsProject::instance()->crs();
    dialog.setCrs(projectCrs.isValid() ? projectCrs : m_mapCanvas->mapSettings().destinationCrs());
    if (dialog.exec() != QDialog::Accepted || !dialog.crs().isValid()) return;
    QgsProject::instance()->setCrs(dialog.crs());
    m_mapCanvas->setDestinationCrs(dialog.crs());
    m_mapCanvas->refresh();
    updateWorkspaceMapCrs();
}

void LTZK::initWorkspaceToolbars()
{
    if (!m_mapCanvasToolBarHost)
    {
        return;
    }

    QToolBar* toolbarMenuBar = new QToolBar(m_mapCanvasToolBarHost);
    toolbarMenuBar->setObjectName(QStringLiteral("mapWorkspaceToolbarMenuBar"));
    toolbarMenuBar->setMovable(false);
    toolbarMenuBar->setFloatable(false);
    toolbarMenuBar->setAllowedAreas(Qt::TopToolBarArea);
    toolbarMenuBar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbarMenuBar->setContextMenuPolicy(Qt::PreventContextMenu);
    toolbarMenuBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* toolbarMenuSpacer = new QWidget(toolbarMenuBar);
    toolbarMenuSpacer->setObjectName(QStringLiteral("mapWorkspaceToolbarMenuSpacer"));
    toolbarMenuSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbarMenuBar->addWidget(toolbarMenuSpacer);

    m_mapCanvasToolBarMenuButton = new QToolButton(toolbarMenuBar);
    m_mapCanvasToolBarMenuButton->setObjectName(QStringLiteral("mapWorkspaceToolbarMenuButton"));
    m_mapCanvasToolBarMenuButton->setText(QStringLiteral("工具栏"));
    m_mapCanvasToolBarMenuButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_mapCanvasToolBarMenuButton->setPopupMode(QToolButton::InstantPopup);
    m_mapCanvasToolBarMenuButton->setToolTip(QStringLiteral("显示或隐藏地图工作区工具栏，并恢复默认布局"));
    toolbarMenuBar->addWidget(m_mapCanvasToolBarMenuButton);
    m_mapCanvasToolBarHost->addToolBar(Qt::TopToolBarArea, toolbarMenuBar);

    QMenu* toolbarsMenu = new QMenu(m_mapCanvasToolBarMenuButton);
    toolbarsMenu->setObjectName(QStringLiteral("menuMapWorkspaceToolbars"));
    m_mapCanvasToolBarMenuButton->setMenu(toolbarsMenu);

    const auto createToolbar = [this, toolbarsMenu](const QString& title, const QString& objectName)
    {
        QToolBar* toolbar = new QToolBar(title, m_mapCanvasToolBarHost);
        toolbar->setObjectName(objectName);
        toolbar->setMovable(true);
        toolbar->setFloatable(true);
        toolbar->setAllowedAreas(Qt::AllToolBarAreas);
        toolbar->setIconSize(QSize(22, 22));
        toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_mapCanvasToolBarHost->addToolBar(Qt::TopToolBarArea, toolbar);
        m_workspaceToolBars.append(toolbar);
        toolbarsMenu->addAction(toolbar->toggleViewAction());
        toolbar->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(toolbar, &QToolBar::visibilityChanged, this,
            [this, toolbar](bool visible)
            {
                if (!m_mapWorkspaceToolbarsActive
                    || m_synchronizingWorkspaceToolbarVisibility)
                {
                    return;
                }
                const QString name = toolbar->objectName();
                m_mapWorkspaceToolbarVisibility.insert(name, visible);
                QSettings settings(platformSettingsPath(), QSettings::IniFormat);
                settings.setIniCodec("UTF-8");
                settings.setValue(
                    QStringLiteral("mapWorkspace/toolbars/visible/%1").arg(name), visible);
                settings.sync();
            });
        connect(toolbar, &QWidget::customContextMenuRequested, this,
            [this, toolbar](const QPoint& position)
            {
                QMenu menu(toolbar);
                for (QToolBar* candidate : m_workspaceToolBars)
                {
                    if (candidate)
                    {
                        menu.addAction(candidate->toggleViewAction());
                    }
                }
                menu.exec(toolbar->mapToGlobal(position));
            });
        return toolbar;
    };

    QToolBar* projectToolbar = createToolbar(QStringLiteral("工程与数据"), QStringLiteral("workspaceProjectDataToolBar"));
    projectToolbar->addAction(ui.actionOpenProject);
    projectToolbar->addAction(ui.actionSaveProject);
    projectToolbar->addAction(ui.actionSaveAsProject);
    projectToolbar->addSeparator();
    projectToolbar->addAction(ui.actionDataSourceManager);
    projectToolbar->addAction(ui.actionAddVectorData);
    projectToolbar->addAction(ui.actionAddRasterData);
    projectToolbar->addAction(ui.actionAddWmsLayer);
    projectToolbar->addAction(ui.actionAddWfsLayer);

    QToolBar* navigationToolbar = createToolbar(QStringLiteral("地图浏览"), QStringLiteral("workspaceMapNavigationToolBar"));
    ui.actionZoomIn->setCheckable(true);
    ui.actionZoomOut->setCheckable(true);
    ui.actionPan->setCheckable(true);
    if (m_zoomInTool) m_zoomInTool->setAction(ui.actionZoomIn);
    if (m_zoomOutTool) m_zoomOutTool->setAction(ui.actionZoomOut);
    if (m_panTool) m_panTool->setAction(ui.actionPan);
    navigationToolbar->addAction(ui.actionZoomIn);
    navigationToolbar->addAction(ui.actionZoomOut);
    navigationToolbar->addAction(ui.actionPan);
    navigationToolbar->addSeparator();
    QAction* previousExtent = navigationToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionZoomLast.svg")), QStringLiteral("上一视图"));
    connect(previousExtent, &QAction::triggered, m_mapCanvas, &QgsMapCanvas::zoomToPreviousExtent);
    QAction* nextExtent = navigationToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionZoomNext.svg")), QStringLiteral("下一视图"));
    connect(nextExtent, &QAction::triggered, m_mapCanvas, &QgsMapCanvas::zoomToNextExtent);
    connect(m_mapCanvas, &QgsMapCanvas::zoomLastStatusChanged, previousExtent, &QAction::setEnabled);
    connect(m_mapCanvas, &QgsMapCanvas::zoomNextStatusChanged, nextExtent, &QAction::setEnabled);
    navigationToolbar->addAction(ui.actionZoomActual);
    navigationToolbar->addAction(ui.actionFullExtent);
    QAction* zoomToLayer = navigationToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionZoomToLayer.svg")), QStringLiteral("缩放至当前图层"));
    connect(zoomToLayer, &QAction::triggered, this, &LTZK::zoomToCurrentLayer);
    QAction* zoomToSelection = navigationToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionZoomToSelected.svg")), QStringLiteral("缩放至选中要素"));
    connect(zoomToSelection, &QAction::triggered, this,
        [this]()
        {
            if (m_mapCanvas && m_layerTreeView && m_layerTreeView->currentLayer())
            {
                m_mapCanvas->zoomToSelected(m_layerTreeView->currentLayer());
            }
        });

    QToolBar* queryToolbar = createToolbar(QStringLiteral("查询与测量"), QStringLiteral("workspaceQueryMeasureToolBar"));
    QAction* identifyAction = queryToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionIdentify.svg")), QStringLiteral("识别要素"));
    identifyAction->setCheckable(true);
    m_identifyFeatureAction = identifyAction;
    connect(identifyAction, &QAction::triggered, this, &LTZK::identifyCurrentLayerFeature);
    QAction* distanceAction = queryToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionMeasure.svg")), QStringLiteral("测距"));
    distanceAction->setCheckable(true);
    auto* distanceTool = new WorkspaceMeasureTool(m_mapCanvas, m_panTool, false,
        [this](const QString& message) { statusBar()->showMessage(message, 8000); });
    distanceTool->setAction(distanceAction);
    m_measureDistanceTool = distanceTool;
    queryToolbar->addAction(distanceAction);
    QAction* areaAction = queryToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionMeasureArea.svg")), QStringLiteral("测面"));
    areaAction->setCheckable(true);
    auto* areaTool = new WorkspaceMeasureTool(m_mapCanvas, m_panTool, true,
        [this](const QString& message) { statusBar()->showMessage(message, 8000); });
    areaTool->setAction(areaAction);
    m_measureAreaTool = areaTool;
    queryToolbar->addAction(areaAction);
    QAction* layerProperties = queryToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mIconProperties.svg")), QStringLiteral("图层属性/样式"));
    connect(layerProperties, &QAction::triggered, this, &LTZK::showCurrentLayerProperties);

    QToolBar* selectionToolbar = createToolbar(QStringLiteral("要素选择"), QStringLiteral("workspaceFeatureSelectionToolBar"));
    QAction* selectAction = selectionToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionSelect.svg")), QStringLiteral("点选要素"));
    selectAction->setCheckable(true);
    auto* selectTool = new WorkspaceFeatureSelectTool(m_mapCanvas,
        [this](const QString& message) { statusBar()->showMessage(message, 5000); });
    selectTool->setTreeView(m_layerTreeView);
    selectTool->setAction(selectAction);
    m_selectFeatureTool = selectTool;
    connect(selectAction, &QAction::triggered, this,
        [this, selectTool]()
        {
            if (m_mapCanvas && selectTool)
            {
                m_mapCanvas->setMapTool(selectTool);
                statusBar()->showMessage(
                    QStringLiteral("单击地图画布选择当前矢量图层中的要素。"), 5000);
            }
        });
    selectionToolbar->addAction(selectAction);
    QAction* selectAll = selectionToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionSelectAll.svg")), QStringLiteral("全选要素"));
    connect(selectAll, &QAction::triggered, this,
        [this]()
        {
            if (QgsVectorLayer* layer = m_layerTreeView
                ? qobject_cast<QgsVectorLayer*>(m_layerTreeView->currentLayer()) : nullptr)
            {
                layer->selectAll();
            }
        });
    QAction* clearSelection = selectionToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionDeselectAll.svg")), QStringLiteral("清除选择"));
    connect(clearSelection, &QAction::triggered, this,
        [this]()
        {
            if (QgsVectorLayer* layer = m_layerTreeView
                ? qobject_cast<QgsVectorLayer*>(m_layerTreeView->currentLayer()) : nullptr)
            {
                layer->removeSelection();
            }
        });
    QAction* invertSelection = selectionToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionInvertSelection.svg")), QStringLiteral("反选"));
    connect(invertSelection, &QAction::triggered, this,
        [this]()
        {
            if (QgsVectorLayer* layer = m_layerTreeView
                ? qobject_cast<QgsVectorLayer*>(m_layerTreeView->currentLayer()) : nullptr)
            {
                layer->invertSelection();
            }
        });

    QToolBar* editingToolbar = createToolbar(
        QStringLiteral("要素编辑"), QStringLiteral("workspaceFeatureEditingToolBar"));
    editingToolbar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_editModeAction = editingToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionToggleEditing.svg")),
        QStringLiteral("切换编辑模式"));
    m_editModeAction->setObjectName(QStringLiteral("actionWorkspaceToggleEditing"));
    m_editModeAction->setCheckable(true);
    m_editModeAction->setToolTip(QStringLiteral("为当前矢量图层开始或结束编辑"));
    connect(m_editModeAction, &QAction::toggled, this,
        [this](bool enabled)
        {
            QgsVectorLayer* layer = m_layerTreeView
                ? qobject_cast<QgsVectorLayer*>(m_layerTreeView->currentLayer()) : nullptr;
            setLayerEditing(layer, enabled);
        });

    m_saveLayerEditsAction = editingToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionSaveEdits.svg")),
        QStringLiteral("保存修改"));
    m_saveLayerEditsAction->setObjectName(QStringLiteral("actionWorkspaceSaveEdits"));
    m_saveLayerEditsAction->setToolTip(QStringLiteral("保存当前图层修改并继续编辑"));
    connect(m_saveLayerEditsAction, &QAction::triggered, this,
        [this]()
        {
            QgsVectorLayer* layer = m_layerTreeView
                ? qobject_cast<QgsVectorLayer*>(m_layerTreeView->currentLayer()) : nullptr;
            saveLayerEdits(layer, true);
        });

    m_rollbackLayerEditsAction = editingToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionRollbackEdits.svg")),
        QStringLiteral("回滚修改"));
    m_rollbackLayerEditsAction->setObjectName(QStringLiteral("actionWorkspaceRollbackEdits"));
    m_rollbackLayerEditsAction->setToolTip(QStringLiteral("撤销当前图层尚未保存的全部修改"));
    connect(m_rollbackLayerEditsAction, &QAction::triggered, this,
        [this]()
        {
            QgsVectorLayer* layer = m_layerTreeView
                ? qobject_cast<QgsVectorLayer*>(m_layerTreeView->currentLayer()) : nullptr;
            rollbackLayerEdits(layer, true);
        });

    m_endLayerEditingAction = editingToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionCancelEdits.svg")),
        QStringLiteral("结束编辑"));
    m_endLayerEditingAction->setObjectName(QStringLiteral("actionWorkspaceEndEditing"));
    m_endLayerEditingAction->setToolTip(QStringLiteral("结束当前图层编辑；未保存时可选择保存或放弃"));
    connect(m_endLayerEditingAction, &QAction::triggered, this,
        [this]()
        {
            QgsVectorLayer* layer = m_layerTreeView
                ? qobject_cast<QgsVectorLayer*>(m_layerTreeView->currentLayer()) : nullptr;
            setLayerEditing(layer, false, true);
        });

    auto* currentEditsButton = new QToolButton(editingToolbar);
    currentEditsButton->setObjectName(QStringLiteral("buttonWorkspaceCurrentEdits"));
    currentEditsButton->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/mActionAllEdits.svg")));
    currentEditsButton->setToolTip(QStringLiteral("管理所选图层或全部图层的编辑"));
    currentEditsButton->setPopupMode(QToolButton::InstantPopup);
    auto* currentEditsMenu = new QMenu(currentEditsButton);
    currentEditsButton->setMenu(currentEditsMenu);
    editingToolbar->addWidget(currentEditsButton);

    const auto editingLayers = [this](bool allLayers)
    {
        QList<QgsVectorLayer*> layers;
        QSet<QgsVectorLayer*> seen;
        if (allLayers)
        {
            for (QgsMapLayer* mapLayer : QgsProject::instance()->mapLayers().values())
            {
                if (QgsVectorLayer* vectorLayer = qobject_cast<QgsVectorLayer*>(mapLayer))
                {
                    if (!seen.contains(vectorLayer))
                    {
                        seen.insert(vectorLayer);
                        layers.push_back(vectorLayer);
                    }
                }
            }
        }
        else if (m_layerTreeView)
        {
            for (QgsMapLayer* mapLayer : m_layerTreeView->selectedLayers())
            {
                if (QgsVectorLayer* vectorLayer = qobject_cast<QgsVectorLayer*>(mapLayer))
                {
                    if (!seen.contains(vectorLayer))
                    {
                        seen.insert(vectorLayer);
                        layers.push_back(vectorLayer);
                    }
                }
            }
        }
        return layers;
    };
    const auto addBatchEditActions = [this, currentEditsMenu, editingLayers](
        const QString& scopeLabel, bool allLayers)
    {
        QAction* saveAction = currentEditsMenu->addAction(
            QIcon(QStringLiteral(":/LTZK/Resources/%1")
                .arg(allLayers ? QStringLiteral("mActionSaveAllEdits.svg")
                    : QStringLiteral("mActionSaveEdits.svg"))),
            QStringLiteral("保存%1正在编辑的图层").arg(scopeLabel));
        connect(saveAction, &QAction::triggered, this,
            [this, editingLayers, allLayers]()
            {
                for (QgsVectorLayer* layer : editingLayers(allLayers))
                {
                    if (layer && layer->isEditable() && layer->isModified())
                    {
                        saveLayerEdits(layer, true);
                    }
                }
            });

        QAction* rollbackAction = currentEditsMenu->addAction(
            QIcon(QStringLiteral(":/LTZK/Resources/%1")
                .arg(allLayers ? QStringLiteral("mActionRollbackAllEdits.svg")
                    : QStringLiteral("mActionRollbackEdits.svg"))),
            QStringLiteral("回滚%1正在编辑的图层").arg(scopeLabel));
        connect(rollbackAction, &QAction::triggered, this,
            [this, editingLayers, allLayers]()
            {
                const QList<QgsVectorLayer*> layers = editingLayers(allLayers);
                bool hasChanges = false;
                for (QgsVectorLayer* layer : layers)
                {
                    hasChanges = hasChanges || (layer && layer->isEditable() && layer->isModified());
                }
                if (hasChanges && QMessageBox::question(
                        this, QStringLiteral("回滚编辑"),
                        QStringLiteral("确定回滚所选图层中尚未保存的修改吗？"),
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
                {
                    return;
                }
                for (QgsVectorLayer* layer : layers)
                {
                    if (layer && layer->isEditable())
                    {
                        layer->rollBack(false);
                    }
                }
                if (m_mapCanvas) m_mapCanvas->refresh();
                updateWorkspaceEditingActions();
            });

        QAction* cancelAction = currentEditsMenu->addAction(
            QIcon(QStringLiteral(":/LTZK/Resources/%1")
                .arg(allLayers ? QStringLiteral("mActionCancelAllEdits.svg")
                    : QStringLiteral("mActionCancelEdits.svg"))),
            QStringLiteral("结束%1编辑并放弃修改").arg(scopeLabel));
        connect(cancelAction, &QAction::triggered, this,
            [this, editingLayers, allLayers]()
            {
                const QList<QgsVectorLayer*> layers = editingLayers(allLayers);
                bool hasChanges = false;
                for (QgsVectorLayer* layer : layers)
                {
                    hasChanges = hasChanges || (layer && layer->isEditable() && layer->isModified());
                }
                if (hasChanges && QMessageBox::question(
                        this, QStringLiteral("结束编辑"),
                        QStringLiteral("确定结束所选图层的编辑并放弃尚未保存的修改吗？"),
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
                {
                    return;
                }
                for (QgsVectorLayer* layer : layers)
                {
                    if (layer && layer->isEditable())
                    {
                        layer->rollBack(true);
                    }
                }
                if (m_mapCanvas) m_mapCanvas->refresh();
                updateWorkspaceEditingActions();
            });
    };
    addBatchEditActions(QStringLiteral("所選"), false);
    currentEditsMenu->addSeparator();
    addBatchEditActions(QStringLiteral("全部"), true);

    editingToolbar->addSeparator();
    QActionGroup* editingMapToolGroup = new QActionGroup(editingToolbar);
    editingMapToolGroup->setExclusive(true);
    // QGIS capture tools access this widget when the layer changes and when
    // capturing vertices, even if the advanced digitizing panel is not shown.
    auto* digitizingDock = new QgsAdvancedDigitizingDockWidget(
        m_mapCanvas, m_mapCanvasToolBarHost);
    digitizingDock->setObjectName(QStringLiteral("workspaceAdvancedDigitizingDock"));
    digitizingDock->hide();
    auto createCaptureAction = [this, editingToolbar, editingMapToolGroup, digitizingDock](
        const QString& iconPath, const QString& label, const QString& tip,
        QgsMapToolCapture::CaptureMode mode, QgsMapTool** toolTarget) -> QAction*
    {
        QAction* action = editingToolbar->addAction(QIcon(iconPath), label);
        action->setCheckable(true);
        action->setToolTip(tip);
        editingMapToolGroup->addAction(action);
        auto* tool = new QgsMapToolDigitizeFeature(m_mapCanvas, digitizingDock, mode);
        tool->setAction(action);
        if (toolTarget)
        {
            *toolTarget = tool;
        }
        connect(action, &QAction::triggered, this,
            [this, action, tool]()
            {
                if (!action->isChecked())
                {
                    return;
                }
                QgsVectorLayer* layer = m_layerTreeView
                    ? qobject_cast<QgsVectorLayer*>(m_layerTreeView->currentLayer()) : nullptr;
                if (!layer || !layer->isEditable())
                {
                    QMessageBox::information(this, QStringLiteral("无法绘制要素"),
                        QStringLiteral("请先选择一个可编辑的矢量图层。"));
                    m_mapCanvas->setMapTool(m_panTool);
                    return;
                }
                tool->setLayer(layer);
                m_mapCanvas->setMapTool(tool);
            });
        connect(tool, &QgsMapToolDigitizeFeature::digitizingCompleted, this,
            [this](const QgsFeature& feature)
            {
                QgsVectorLayer* layer = m_layerTreeView
                    ? qobject_cast<QgsVectorLayer*>(m_layerTreeView->currentLayer()) : nullptr;
                addDigitizedFeature(layer, feature);
            });
        return action;
    };

    m_addPointFeatureAction = createCaptureAction(
        QStringLiteral(":/LTZK/Resources/mActionCapturePoint.svg"),
        QStringLiteral("绘制点"), QStringLiteral("在当前点图层中绘制新要素"),
        QgsMapToolCapture::CapturePoint, &m_capturePointTool);
    m_addLineFeatureAction = createCaptureAction(
        QStringLiteral(":/LTZK/Resources/mActionCaptureLine.svg"),
        QStringLiteral("绘制线"), QStringLiteral("在当前线图层中绘制新要素"),
        QgsMapToolCapture::CaptureLine, &m_captureLineTool);
    m_addPolygonFeatureAction = createCaptureAction(
        QStringLiteral(":/LTZK/Resources/mActionCapturePolygon.svg"),
        QStringLiteral("绘制面"), QStringLiteral("在当前面图层中绘制新要素"),
        QgsMapToolCapture::CapturePolygon, &m_capturePolygonTool);

    m_vertexEditAction = editingToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionVertexTool.svg")),
        QStringLiteral("编辑顶点"));
    m_vertexEditAction->setCheckable(true);
    m_vertexEditAction->setToolTip(QStringLiteral("拖动当前图层要素的几何顶点"));
    editingMapToolGroup->addAction(m_vertexEditAction);
    auto* vertexTool = new WorkspaceVertexEditTool(
        m_mapCanvas,
        [this]()
        {
            return m_layerTreeView
                ? qobject_cast<QgsVectorLayer*>(m_layerTreeView->currentLayer()) : nullptr;
        },
        [this](const QString& message) { statusBar()->showMessage(message, 5000); });
    vertexTool->setAction(m_vertexEditAction);
    m_vertexEditTool = vertexTool;
    connect(m_vertexEditAction, &QAction::triggered, this,
        [this, vertexTool, m_vertexEditAction = m_vertexEditAction]()
        {
            if (!m_vertexEditAction->isChecked()) return;
            QgsVectorLayer* layer = m_layerTreeView
                ? qobject_cast<QgsVectorLayer*>(m_layerTreeView->currentLayer()) : nullptr;
            if (!layer || !layer->isEditable())
            {
                QMessageBox::information(this, QStringLiteral("无法编辑顶点"),
                    QStringLiteral("请先选择一个处于编辑模式的矢量图层。"));
                m_mapCanvas->setMapTool(m_panTool);
                return;
            }
            m_mapCanvas->setMapTool(vertexTool);
        });

    m_deleteSelectedFeaturesAction = editingToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionDeleteSelectedFeatures.svg")),
        QStringLiteral("删除所选要素"));
    m_deleteSelectedFeaturesAction->setToolTip(QStringLiteral("从当前图层删除已选中的要素"));
    connect(m_deleteSelectedFeaturesAction, &QAction::triggered, this,
        [this]()
        {
            QgsVectorLayer* layer = m_layerTreeView
                ? qobject_cast<QgsVectorLayer*>(m_layerTreeView->currentLayer()) : nullptr;
            if (!layer || !layer->isEditable() || layer->selectedFeatureCount() == 0)
            {
                return;
            }
            if (QMessageBox::question(this, QStringLiteral("删除所选要素"),
                    QStringLiteral("确定删除当前选中的 %1 个要素吗？")
                        .arg(layer->selectedFeatureCount()),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            {
                return;
            }
            layer->deleteSelectedFeatures();
            if (m_mapCanvas) m_mapCanvas->refresh();
            updateWorkspaceEditingActions();
        });

    m_undoLayerEditAction = editingToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionUndo.svg")), QStringLiteral("撤销"));
    m_undoLayerEditAction->setToolTip(QStringLiteral("撤销当前图层的上一项编辑"));
    connect(m_undoLayerEditAction, &QAction::triggered, this,
        [this]()
        {
            QgsVectorLayer* layer = m_layerTreeView
                ? qobject_cast<QgsVectorLayer*>(m_layerTreeView->currentLayer()) : nullptr;
            if (layer && layer->isEditable() && layer->undoStack()->canUndo())
            {
                layer->undoStack()->undo();
                if (m_mapCanvas) m_mapCanvas->refresh();
            }
        });
    m_redoLayerEditAction = editingToolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionRedo.svg")), QStringLiteral("重做"));
    m_redoLayerEditAction->setToolTip(QStringLiteral("重做当前图层的上一项撤销"));
    connect(m_redoLayerEditAction, &QAction::triggered, this,
        [this]()
        {
            QgsVectorLayer* layer = m_layerTreeView
                ? qobject_cast<QgsVectorLayer*>(m_layerTreeView->currentLayer()) : nullptr;
            if (layer && layer->isEditable() && layer->undoStack()->canRedo())
            {
                layer->undoStack()->redo();
                if (m_mapCanvas) m_mapCanvas->refresh();
            }
        });

    applyDefaultWorkspaceToolbarLayout();

    QSettings settings(platformSettingsPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    for (QToolBar* toolbar : m_workspaceToolBars)
    {
        if (!toolbar)
        {
            continue;
        }
        const bool defaultVisible =
            toolbar->objectName() != QStringLiteral("workspaceFeatureSelectionToolBar");
        m_mapWorkspaceToolbarVisibility.insert(
            toolbar->objectName(),
            settings.value(
                QStringLiteral("mapWorkspace/toolbars/visible/%1").arg(toolbar->objectName()),
                defaultVisible).toBool());
    }

    const QByteArray mapToolbarState = settings.value(
        QStringLiteral("mapWorkspace/toolbars/state")).toByteArray();
    bool shouldPersistDefaultToolbarLayout = false;
    if (!mapToolbarState.isEmpty())
    {
        shouldPersistDefaultToolbarLayout = !m_mapCanvasToolBarHost->restoreState(
            mapToolbarState, kWorkspaceToolbarStateVersion);
    }

    toolbarsMenu->addSeparator();
    QAction* resetLayoutAction = toolbarsMenu->addAction(QStringLiteral("恢复默认工具栏布局"));
    connect(resetLayoutAction, &QAction::triggered, this, &LTZK::resetWorkspaceToolbarLayout);

    const QByteArray geometry = settings.value(QStringLiteral("ui/mainWindowGeometry")).toByteArray();
    if (!geometry.isEmpty())
    {
        restoreGeometry(geometry);
    }
    const QByteArray state = settings.value(QStringLiteral("ui/mainWindowState")).toByteArray();
    if (!state.isEmpty())
    {
        restoreState(state, 1);
    }

    if (m_workspaceStack)
    {
        connect(m_workspaceStack, &QStackedWidget::currentChanged, this,
            [this](int)
            {
                const bool mapWorkspaceIsActive = m_workspaceStack
                    && m_workspaceStack->currentWidget() == m_mapCanvasPage
                    && m_currentBusinessModuleTitle != QStringLiteral("地图服务管理")
                    && gCurrentUserSession.isLoggedIn;
                setWorkspaceToolbarsActive(mapWorkspaceIsActive);
            });
    }
    setWorkspaceToolbarsActive(
        m_workspaceStack && m_workspaceStack->currentWidget() == m_mapCanvasPage
        && m_currentBusinessModuleTitle != QStringLiteral("地图服务管理")
        && gCurrentUserSession.isLoggedIn);
    if (shouldPersistDefaultToolbarLayout)
    {
        saveMapWorkspaceToolbarState();
    }
    updateWorkspaceEditingActions();
}

bool LTZK::layerCanStartEditing(const QgsVectorLayer* layer) const
{
    if (!layer || !layer->isValid() || layer->readOnly())
    {
        return false;
    }
    const QgsVectorDataProvider* provider = layer->dataProvider();
    return provider
        && (provider->capabilities() & QgsVectorDataProvider::EditingCapabilities);
}

bool LTZK::setLayerEditing(QgsVectorLayer* layer, bool enabled, bool confirmDiscard)
{
    if (!layer || !layer->isValid())
    {
        updateWorkspaceEditingActions();
        return false;
    }

    if (enabled)
    {
        if (layer->isEditable())
        {
            updateWorkspaceEditingActions();
            return true;
        }
        if (!layerCanStartEditing(layer))
        {
            QMessageBox::information(this, QStringLiteral("图层不可编辑"),
                QStringLiteral("当前图层为只读图层，或数据源不支持编辑。"));
            updateWorkspaceEditingActions();
            return false;
        }
        if (!layer->startEditing())
        {
            QMessageBox::warning(this, QStringLiteral("无法开始编辑"),
                QStringLiteral("无法开始编辑图层“%1”。请检查数据源权限和编辑能力。")
                    .arg(layer->name()));
            updateWorkspaceEditingActions();
            return false;
        }
        if (m_mapCanvas) m_mapCanvas->refresh();
        updateWorkspaceEditingActions();
        return true;
    }

    if (!layer->isEditable())
    {
        updateWorkspaceEditingActions();
        return true;
    }

    if (layer->isModified() && confirmDiscard)
    {
        QMessageBox message(QMessageBox::Warning,
            QStringLiteral("结束图层编辑"),
            QStringLiteral("图层“%1”有尚未保存的修改。请选择保存、放弃或继续编辑。")
                .arg(layer->name()),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            this);
        message.setButtonText(QMessageBox::Save, QStringLiteral("保存并结束"));
        message.setButtonText(QMessageBox::Discard, QStringLiteral("放弃修改"));
        message.setButtonText(QMessageBox::Cancel, QStringLiteral("继续编辑"));
        const int choice = message.exec();
        if (choice == QMessageBox::Save)
        {
            return saveLayerEdits(layer, false);
        }
        if (choice == QMessageBox::Discard)
        {
            layer->rollBack(true);
        }
        else
        {
            updateWorkspaceEditingActions();
            return false;
        }
    }
    else if (layer->isModified())
    {
        layer->rollBack(true);
    }
    else if (!layer->commitChanges())
    {
        const QString errors = layer->commitErrors().join(QLatin1Char('\n'));
        QMessageBox::warning(this, QStringLiteral("结束编辑失败"),
            errors.isEmpty() ? QStringLiteral("无法结束当前图层的编辑。") : errors);
        updateWorkspaceEditingActions();
        return false;
    }

    if (m_mapCanvas) m_mapCanvas->refresh();
    updateWorkspaceEditingActions();
    return true;
}

bool LTZK::saveLayerEdits(QgsVectorLayer* layer, bool keepEditing)
{
    if (!layer || !layer->isEditable())
    {
        return false;
    }
    if (!layer->isModified())
    {
        if (!keepEditing)
        {
            return setLayerEditing(layer, false, false);
        }
        updateWorkspaceEditingActions();
        return true;
    }
    if (!layer->commitChanges(!keepEditing))
    {
        const QString errors = layer->commitErrors().join(QLatin1Char('\n'));
        QMessageBox::warning(this, QStringLiteral("保存失败"),
            errors.isEmpty() ? QStringLiteral("图层修改未能保存到数据源。") : errors);
        updateWorkspaceEditingActions();
        return false;
    }
    if (m_mapCanvas) m_mapCanvas->refresh();
    updateWorkspaceEditingActions();
    return true;
}

bool LTZK::rollbackLayerEdits(QgsVectorLayer* layer, bool confirm)
{
    if (!layer || !layer->isEditable())
    {
        return false;
    }
    if (confirm && layer->isModified()
        && QMessageBox::question(this, QStringLiteral("回滚修改"),
            QStringLiteral("确定回滚图层“%1”尚未保存的修改吗？")
                .arg(layer->name()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    {
        return false;
    }

    const bool rolledBack = layer->rollBack(false);
    if (!rolledBack)
    {
        QMessageBox::warning(this, QStringLiteral("回滚失败"),
            QStringLiteral("无法回滚图层“%1”的修改。可以尝试结束编辑并重新加载图层。")
                .arg(layer->name()));
    }
    if (m_mapCanvas) m_mapCanvas->refresh();
    updateWorkspaceEditingActions();
    return rolledBack;
}

void LTZK::addDigitizedFeature(QgsVectorLayer* layer, const QgsFeature& capturedFeature)
{
    if (!layer || !layer->isValid() || !layer->isEditable()
        || !layer->dataProvider()
        || !(layer->dataProvider()->capabilities() & QgsVectorDataProvider::AddFeatures))
    {
        QMessageBox::information(this, QStringLiteral("无法添加要素"),
            QStringLiteral("当前图层不支持添加要素，或已退出编辑模式。"));
        return;
    }

    QgsFeature feature = QgsVectorLayerUtils::createFeature(
        layer, capturedFeature.geometry());
    QgsAttributeEditorContext context;
    context.setMapCanvas(m_mapCanvas);
    QgsAttributeDialog form(layer, &feature, false, this, true, context);
    form.setMode(QgsAttributeEditorContext::AddFeatureMode);
    form.setEditCommandMessage(QStringLiteral("添加要素"));
    if (form.exec() == QDialog::Accepted && m_mapCanvas)
    {
        m_mapCanvas->refresh();
    }
    updateWorkspaceEditingActions();
}

void LTZK::updateWorkspaceEditingActions()
{
    QgsVectorLayer* layer = m_layerTreeView
        ? qobject_cast<QgsVectorLayer*>(m_layerTreeView->currentLayer()) : nullptr;

    if (m_workspaceEditingObservedLayer.data() != layer)
    {
        for (const QMetaObject::Connection& connection : m_workspaceEditingConnections)
        {
            disconnect(connection);
        }
        for (const QMetaObject::Connection& connection : m_workspaceUndoConnections)
        {
            disconnect(connection);
        }
        m_workspaceEditingConnections.clear();
        m_workspaceUndoConnections.clear();
        m_workspaceEditingObservedLayer = layer;
        if (layer)
        {
            const auto refresh = [this]() { updateWorkspaceEditingActions(); };
            m_workspaceEditingConnections.append(connect(
                layer, &QgsMapLayer::editingStarted, this, refresh));
            m_workspaceEditingConnections.append(connect(
                layer, &QgsMapLayer::editingStopped, this, refresh));
            m_workspaceEditingConnections.append(connect(
                layer, &QgsVectorLayer::afterCommitChanges, this, refresh));
            m_workspaceEditingConnections.append(connect(
                layer, &QgsVectorLayer::afterRollBack, this, refresh));
            m_workspaceEditingConnections.append(connect(
                layer, &QgsVectorLayer::editCommandEnded, this, refresh));
            m_workspaceEditingConnections.append(connect(
                layer, &QgsVectorLayer::featureAdded, this,
                [refresh](QgsFeatureId) { refresh(); }));
            m_workspaceEditingConnections.append(connect(
                layer, &QgsVectorLayer::featureDeleted, this,
                [refresh](QgsFeatureId) { refresh(); }));
            m_workspaceEditingConnections.append(connect(
                layer, &QgsVectorLayer::selectionChanged, this,
                [refresh]() { refresh(); }));
            m_workspaceEditingConnections.append(connect(
                layer, &QgsVectorLayer::attributeValueChanged, this,
                [refresh](QgsFeatureId, int, const QVariant&) { refresh(); }));
            m_workspaceEditingConnections.append(connect(
                layer, &QgsVectorLayer::geometryChanged, this,
                [refresh](QgsFeatureId, const QgsGeometry&) { refresh(); }));

            if (QUndoStack* undoStack = layer->undoStack())
            {
                m_workspaceUndoConnections.append(connect(
                    undoStack, &QUndoStack::canUndoChanged, this,
                    [refresh](bool) { refresh(); }));
                m_workspaceUndoConnections.append(connect(
                    undoStack, &QUndoStack::canRedoChanged, this,
                    [refresh](bool) { refresh(); }));
                m_workspaceUndoConnections.append(connect(
                    undoStack, &QUndoStack::indexChanged, this,
                    [refresh](int) { refresh(); }));
            }
        }
    }

    const bool editable = layer && layer->isEditable();
    const QgsVectorDataProvider* provider = layer ? layer->dataProvider() : nullptr;
    const QgsVectorDataProvider::Capabilities capabilities = provider
        ? provider->capabilities() : QgsVectorDataProvider::Capabilities();
    const bool canStart = layerCanStartEditing(layer);
    if (m_editModeAction)
    {
        const QSignalBlocker blocker(m_editModeAction);
        m_editModeAction->setEnabled(canStart || editable);
        m_editModeAction->setChecked(editable);
        m_editModeAction->setText(editable
            ? QStringLiteral("结束编辑模式") : QStringLiteral("开始编辑模式"));
        m_editModeAction->setToolTip(editable
            ? QStringLiteral("结束当前矢量图层编辑")
            : QStringLiteral("为当前矢量图层开始编辑"));
    }
    if (m_saveLayerEditsAction)
    {
        m_saveLayerEditsAction->setEnabled(editable && layer->isModified());
    }
    if (m_rollbackLayerEditsAction)
    {
        m_rollbackLayerEditsAction->setEnabled(editable && layer->isModified());
    }
    if (m_endLayerEditingAction)
    {
        m_endLayerEditingAction->setEnabled(editable);
    }

    const QgsWkbTypes::GeometryType geometryType = layer
        ? QgsWkbTypes::geometryType(layer->wkbType())
        : QgsWkbTypes::NullGeometry;
    const bool canAddFeatures = editable && (capabilities & QgsVectorDataProvider::AddFeatures);
    if (m_addPointFeatureAction)
    {
        m_addPointFeatureAction->setEnabled(canAddFeatures
            && geometryType == QgsWkbTypes::PointGeometry);
    }
    if (m_addLineFeatureAction)
    {
        m_addLineFeatureAction->setEnabled(canAddFeatures
            && geometryType == QgsWkbTypes::LineGeometry);
    }
    if (m_addPolygonFeatureAction)
    {
        m_addPolygonFeatureAction->setEnabled(canAddFeatures
            && geometryType == QgsWkbTypes::PolygonGeometry);
    }
    if (m_vertexEditAction)
    {
        m_vertexEditAction->setEnabled(editable
            && geometryType != QgsWkbTypes::NullGeometry
            && (capabilities & QgsVectorDataProvider::ChangeGeometries));
    }
    if (m_deleteSelectedFeaturesAction)
    {
        m_deleteSelectedFeaturesAction->setEnabled(editable
            && layer->selectedFeatureCount() > 0
            && (capabilities & QgsVectorDataProvider::DeleteFeatures));
    }
    QUndoStack* undoStack = layer ? layer->undoStack() : nullptr;
    if (m_undoLayerEditAction)
    {
        m_undoLayerEditAction->setEnabled(editable && undoStack && undoStack->canUndo());
    }
    if (m_redoLayerEditAction)
    {
        m_redoLayerEditAction->setEnabled(editable && undoStack && undoStack->canRedo());
    }
}

void LTZK::setWorkspaceToolbarsActive(bool active)
{
    if (QWidget* row = statusBar()->findChild<QWidget*>(QStringLiteral("workspaceMapStatusBar")))
    {
        row->setVisible(active);
    }
    if (m_mapWorkspaceToolbarsActive && !active)
    {
        saveMapWorkspaceToolbarState();
    }

    m_synchronizingWorkspaceToolbarVisibility = true;
    m_mapWorkspaceToolbarsActive = active;
    if (m_mapCanvasToolBarHost)
    {
        m_mapCanvasToolBarHost->setVisible(active);
    }

    for (QToolBar* toolbar : m_workspaceToolBars)
    {
        if (!toolbar)
        {
            continue;
        }
        const bool defaultVisible =
            toolbar->objectName() != QStringLiteral("workspaceFeatureSelectionToolBar");
        const bool userWantsVisible = m_mapWorkspaceToolbarVisibility.value(
            toolbar->objectName(), defaultVisible);
        toolbar->setVisible(active && userWantsVisible);
    }
    m_synchronizingWorkspaceToolbarVisibility = false;
}

void LTZK::resetWorkspaceToolbarLayout()
{
    if (!m_mapCanvasToolBarHost)
    {
        return;
    }

    m_synchronizingWorkspaceToolbarVisibility = true;
    m_mapWorkspaceToolbarVisibility.clear();
    applyDefaultWorkspaceToolbarLayout();
    for (QToolBar* toolbar : m_workspaceToolBars)
    {
        if (!toolbar)
        {
            continue;
        }
        const bool defaultVisible =
            toolbar->objectName() != QStringLiteral("workspaceFeatureSelectionToolBar");
        m_mapWorkspaceToolbarVisibility.insert(toolbar->objectName(), defaultVisible);
        toolbar->setVisible(m_mapWorkspaceToolbarsActive && defaultVisible);
    }
    m_synchronizingWorkspaceToolbarVisibility = false;

    saveMapWorkspaceToolbarState();
}

void LTZK::applyDefaultWorkspaceToolbarLayout()
{
    if (!m_mapCanvasToolBarHost)
    {
        return;
    }

    QToolBar* toolbarMenuBar = m_mapCanvasToolBarHost->findChild<QToolBar*>(
        QStringLiteral("mapWorkspaceToolbarMenuBar"));
    QToolBar* editingToolbar = nullptr;
    for (QToolBar* toolbar : m_workspaceToolBars)
    {
        if (toolbar && toolbar->objectName() == QStringLiteral("workspaceFeatureEditingToolBar"))
        {
            editingToolbar = toolbar;
            break;
        }
    }

    if (editingToolbar)
    {
        m_mapCanvasToolBarHost->removeToolBarBreak(editingToolbar);
    }
    if (toolbarMenuBar)
    {
        m_mapCanvasToolBarHost->removeToolBarBreak(toolbarMenuBar);
        m_mapCanvasToolBarHost->removeToolBar(toolbarMenuBar);
    }

    for (QToolBar* toolbar : m_workspaceToolBars)
    {
        if (!toolbar)
        {
            continue;
        }
        m_mapCanvasToolBarHost->removeToolBar(toolbar);
    }
    for (QToolBar* toolbar : m_workspaceToolBars)
    {
        if (toolbar && toolbar != editingToolbar)
        {
            m_mapCanvasToolBarHost->addToolBar(Qt::TopToolBarArea, toolbar);
        }
    }
    if (toolbarMenuBar)
    {
        m_mapCanvasToolBarHost->addToolBar(Qt::TopToolBarArea, toolbarMenuBar);
        toolbarMenuBar->show();
    }

    if (editingToolbar)
    {
        m_mapCanvasToolBarHost->addToolBarBreak(Qt::TopToolBarArea);
        m_mapCanvasToolBarHost->addToolBar(Qt::TopToolBarArea, editingToolbar);
    }
}

void LTZK::saveMapWorkspaceToolbarState() const
{
    if (!m_mapCanvasToolBarHost)
    {
        return;
    }

    QSettings settings(platformSettingsPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue(QStringLiteral("mapWorkspace/toolbars/state"),
        m_mapCanvasToolBarHost->saveState(kWorkspaceToolbarStateVersion));
    for (QToolBar* toolbar : m_workspaceToolBars)
    {
        if (!toolbar)
        {
            continue;
        }
        const bool defaultVisible =
            toolbar->objectName() != QStringLiteral("workspaceFeatureSelectionToolBar");
        settings.setValue(
            QStringLiteral("mapWorkspace/toolbars/visible/%1").arg(toolbar->objectName()),
            m_mapWorkspaceToolbarVisibility.value(toolbar->objectName(), defaultVisible));
    }
    settings.sync();
}

void LTZK::saveWorkspaceToolbarState() const
{
    if (m_mapWorkspaceToolbarsActive)
    {
        saveMapWorkspaceToolbarState();
    }

    const QString settingsPath = platformSettingsPath();
    QDir().mkpath(QFileInfo(settingsPath).absolutePath());
    QSettings settings(settingsPath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue(QStringLiteral("ui/mainWindowGeometry"), saveGeometry());
    settings.setValue(QStringLiteral("ui/mainWindowState"), saveState(1));
    settings.sync();
}

void LTZK::showCurrentLayerProperties()
{
    showLayerProperties(m_layerTreeView ? m_layerTreeView->currentLayer() : m_curMapLayer);
}

void LTZK::showLayerProperties(QgsMapLayer* layer)
{
    if (!layer || !layer->isValid())
    {
        statusBar()->showMessage(QStringLiteral("请先选择一个有效图层。"), 4000);
        return;
    }
    if (QgsVectorLayer* vectorLayer = qobject_cast<QgsVectorLayer*>(layer))
    {
        QgsVectorLayerProperties dialog(m_mapCanvas, m_messageBar, vectorLayer, this);
        connectLayerStyleRefresh(&dialog, m_mapCanvas);
        dialog.setWindowTitle(QStringLiteral("图层属性 - %1").arg(layer->name()));
        dialog.setCurrentPage(QStringLiteral("symbology"));
        dialog.exec();
    }
    else if (qobject_cast<QgsRasterLayer*>(layer))
    {
        QgsRasterLayerProperties dialog(layer, m_mapCanvas, this);
        connectLayerStyleRefresh(&dialog, m_mapCanvas);
        dialog.setWindowTitle(QStringLiteral("图层属性 - %1").arg(layer->name()));
        dialog.setCurrentPage(QStringLiteral("symbology"));
        dialog.exec();
    }
    else
    {
        statusBar()->showMessage(QStringLiteral("该图层类型暂不支持属性设置。"), 4000);
    }
}

void LTZK::zoomToCurrentLayer()
{
    QgsMapLayer* layer = m_layerTreeView ? m_layerTreeView->currentLayer() : nullptr;
    if (!m_mapCanvas || !layer || !layer->isValid())
    {
        statusBar()->showMessage(QStringLiteral("请先选择一个有效图层。"), 4000);
        return;
    }
    QgsRectangle extent = m_mapCanvas->mapSettings().layerExtentToOutputExtent(layer, layer->extent());
    if (extent.isEmpty())
    {
        statusBar()->showMessage(QStringLiteral("当前图层没有可定位的范围。"), 4000);
        return;
    }
    m_mapCanvas->setExtent(extent);
    m_mapCanvas->refresh();
}

void LTZK::identifyCurrentLayerFeature()
{
    if (!m_mapCanvas)
    {
        if (m_identifyFeatureAction)
        {
            m_identifyFeatureAction->setChecked(false);
        }
        statusBar()->showMessage(QStringLiteral("\u65E0\u6548\u7684\u5730\u56FE\u753B\u5E03\u3002"), 4000);
        return;
    }

    auto* identifyTool = dynamic_cast<WorkspaceIdentifyTool*>(m_identifyFeatureTool);
    if (!identifyTool)
    {
        identifyTool = new WorkspaceIdentifyTool(
            m_mapCanvas,
            [this](const QgsMapToolIdentify::IdentifyResult& result)
            {
                auto* identifiedLayer = qobject_cast<QgsVectorLayer*>(result.mLayer);
                if (!identifiedLayer || !identifiedLayer->isValid())
                {
                    return;
                }

                const QgsFields fields = identifiedLayer->fields();
                const QgsAttributes attributes = result.mFeature.attributes();
                QStringList rows;
                for (int i = 0; i < attributes.size() && i < fields.size() && i < 16; ++i)
                {
                    rows.append(QStringLiteral("%1: %2")
                        .arg(fields.at(i).name(), attributes.at(i).toString()));
                }

                QMessageBox::information(
                    this,
                    QStringLiteral("\u8981\u7D20\u8BC6\u522B - %1 (%2)")
                        .arg(identifiedLayer->name())
                        .arg(result.mFeature.id()),
                    rows.isEmpty()
                        ? QStringLiteral("\u627E\u5230\u8981\u7D20\uFF0C\u4F46\u6CA1\u6709\u53EF\u663E\u793A\u7684\u5C5E\u6027\u3002")
                        : rows.join(QLatin1Char('\n')));
            },
            [this]()
            {
                statusBar()->showMessage(
                    QStringLiteral("\u70B9\u51FB\u4F4D\u7F6E\u7684\u53EF\u89C1\u77E2\u91CF\u56FE\u5C42\u4E2D\u6CA1\u6709\u627E\u5230\u8981\u7D20\u3002"),
                    4000);
            });
        m_identifyFeatureTool = identifyTool;
    }

    identifyTool->setAction(m_identifyFeatureAction);
    m_mapCanvas->setMapTool(identifyTool);
    statusBar()->showMessage(
        QStringLiteral("\u5728\u5730\u56FE\u4E0A\u70B9\u51FB\u53EF\u89C1\u77E2\u91CF\u8981\u7D20\u8FDB\u884C\u8BC6\u522B\uFF1B\u6309 Esc \u9000\u51FA\u3002"),
        5000);
}



void LTZK::refreshLayerLegendMapFilter()
{
    if (!m_layerTreeView || !m_layerTreeView->layerTreeModel() || !m_mapCanvas)
    {
        return;
    }
    QgsLayerTreeModel* model = m_layerTreeView->layerTreeModel();
    if (!m_layerLegendMapFilterEnabled && !m_layerLegendExpressionFilterEnabled)
    {
        model->setLegendFilter(nullptr);
        model->setLegendFilterByMap(nullptr);
        return;
    }
    model->setLegendFilter(&m_mapCanvas->mapSettings(), m_layerLegendMapFilterEnabled,
        QgsGeometry(), m_layerLegendExpressionFilterEnabled);
}

void LTZK::populateMapThemeMenu(QMenu* menu)
{
    if (!menu)
    {
        return;
    }
    menu->clear();
    QAction* createAction = menu->addAction(QStringLiteral("新建地图主题…"));
    connect(createAction, &QAction::triggered, this, &LTZK::createMapTheme);
    QgsMapThemeCollection* collection = QgsProject::instance()->mapThemeCollection();
    if (!collection)
    {
        return;
    }
    const QStringList themes = collection->mapThemes();
    if (themes.isEmpty())
    {
        QAction* emptyAction = menu->addAction(QStringLiteral("暂无已保存主题"));
        emptyAction->setEnabled(false);
        return;
    }
    menu->addSeparator();
    for (const QString& themeName : themes)
    {
        QAction* themeAction = menu->addAction(themeName);
        themeAction->setCheckable(true);
        themeAction->setChecked(themeName == m_activeMapThemeName);
        connect(themeAction, &QAction::triggered, this,
            [this, themeName]() { applyMapTheme(themeName); });
    }
    QMenu* updateMenu = menu->addMenu(QStringLiteral("更新主题"));
    QMenu* renameMenu = menu->addMenu(QStringLiteral("重命名主题"));
    QMenu* removeMenu = menu->addMenu(QStringLiteral("删除主题"));
    for (const QString& themeName : themes)
    {
        QAction* updateAction = updateMenu->addAction(themeName);
        connect(updateAction, &QAction::triggered, this,
            [this, themeName]() { updateMapTheme(themeName); });
        QAction* renameAction = renameMenu->addAction(themeName);
        connect(renameAction, &QAction::triggered, this,
            [this, themeName]() { renameMapTheme(themeName); });
        QAction* removeAction = removeMenu->addAction(themeName);
        connect(removeAction, &QAction::triggered, this,
            [this, themeName]() { removeMapTheme(themeName); });
    }
}

void LTZK::applyMapTheme(const QString& name)
{
    QgsMapThemeCollection* collection = QgsProject::instance()->mapThemeCollection();
    if (!collection || !collection->hasMapTheme(name) || !m_layerTreeView || !m_mapCanvas)
    {
        return;
    }
    collection->applyTheme(name, QgsProject::instance()->layerTreeRoot(), m_layerTreeView->layerTreeModel());
    m_activeMapThemeName = name;
    m_mapCanvas->refresh();
    statusBar()->showMessage(QStringLiteral("已应用地图主题“%1”。").arg(name), 4000);
}

void LTZK::createMapTheme()
{
    if (!m_layerTreeView)
    {
        return;
    }
    bool accepted = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("新建地图主题"),
        QStringLiteral("主题名称："), QLineEdit::Normal, QString(), &accepted).trimmed();
    if (!accepted || name.isEmpty())
    {
        return;
    }
    QgsMapThemeCollection* collection = QgsProject::instance()->mapThemeCollection();
    if (!collection || collection->hasMapTheme(name))
    {
        QMessageBox::warning(this, QStringLiteral("无法新建主题"), QStringLiteral("主题名称已存在或工程主题集不可用。"));
        return;
    }
    collection->insert(name, QgsMapThemeCollection::createThemeFromCurrentState(
        QgsProject::instance()->layerTreeRoot(), m_layerTreeView->layerTreeModel()));
    m_activeMapThemeName = name;
    statusBar()->showMessage(QStringLiteral("已保存地图主题“%1”。").arg(name), 4000);
}

void LTZK::updateMapTheme(const QString& name)
{
    if (!m_layerTreeView)
    {
        return;
    }
    if (QgsMapThemeCollection* collection = QgsProject::instance()->mapThemeCollection())
    {
        collection->update(name, QgsMapThemeCollection::createThemeFromCurrentState(
            QgsProject::instance()->layerTreeRoot(), m_layerTreeView->layerTreeModel()));
        statusBar()->showMessage(QStringLiteral("已更新地图主题“%1”。").arg(name), 4000);
    }
}

void LTZK::renameMapTheme(const QString& name)
{
    bool accepted = false;
    const QString newName = QInputDialog::getText(this, QStringLiteral("重命名地图主题"),
        QStringLiteral("新名称："), QLineEdit::Normal, name, &accepted).trimmed();
    if (!accepted || newName.isEmpty() || newName == name)
    {
        return;
    }
    if (QgsMapThemeCollection* collection = QgsProject::instance()->mapThemeCollection())
    {
        if (collection->renameMapTheme(name, newName) && m_activeMapThemeName == name)
        {
            m_activeMapThemeName = newName;
        }
    }
}

void LTZK::removeMapTheme(const QString& name)
{
    if (QMessageBox::question(this, QStringLiteral("删除地图主题"),
        QStringLiteral("确定删除地图主题“%1”吗？").arg(name)) != QMessageBox::Yes)
    {
        return;
    }
    if (QgsMapThemeCollection* collection = QgsProject::instance()->mapThemeCollection())
    {
        collection->removeMapTheme(name);
        if (m_activeMapThemeName == name)
        {
            m_activeMapThemeName.clear();
        }
    }
}
