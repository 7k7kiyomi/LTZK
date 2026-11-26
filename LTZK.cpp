#include "LTZK.h"
#include "LTZKUiScale.h"
#include "LTZKNativeDialogStyle.h"
#include "LTZKSeriesTableTypography.h"
#include "LTZKPluginManager.h"
#include "product_catalog_plugin_api.h"
#include "DataCatalogWidget.h"
#include "qgsbrowserguimodel.h"
#include "qgsdatasourcemanagerdialog.h"
#include "PlatformMenuDatabaseConnectionObserver.h"
#include "LocalProductOperation.h"
#include "PluginBindingConfig.h"
#include "auth/LTZKAccessControlDialog.h"
#include "auth/LTZKAuthManager.h"
#include "auth/LTZKLoginDialog.h"
#include "auth/LTZKSessionBridge.h"
#include "QFileDialog.h"
#include "QApplication.h"
#include "QgsProject.h"
#include <QgsMapCanvas.h>
#include <qgsmapmouseevent.h>
#include "qgsmapsettings.h"
#include "qgsrectangle.h"
#include "QgsVectorLayer.h"
#include "QMessageBox.h"
#include "QgsRasterLayer.h"
#include "qgsbrightnesscontrastfilter.h"
#include "qgshuesaturationfilter.h"
#include "QgsLayerTreeView.h"
#include "QgsLayerTreeModel.h"
#include "qgslayertreeregistrybridge.h"
#include "QgsLayerTreeViewDefaultActions.h"
#include "qgsmapthemecollection.h"
#include "qgsmaptoolidentifyfeature.h"
#include "qgsdistancearea.h"
#include "qgsrubberband.h"
#include "qgsquerybuilder.h"
#include "qgsvectorlayerproperties.h"
#include "qgsrasterlayerproperties.h"
#include "qgscollapsiblegroupbox.h"
#include "QgsApplication.h"
#include "QgsGui.h"
#include "QgsLayerTreeViewDefaultActions.h"
#include "QgsDockWidget.h"
#include "QgsSourceFieldsProperties.h"
#include "qgsmessagelog.h"
#include "qgsmessagebar.h"
#include <QAction>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QIcon>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QPainter>
#include <QLinearGradient>
#include <QSvgRenderer>
#include <QDebug>
#include <QPainterPath>
#include <QSize>
#include <QVector>
#include <cmath>
#include <QPixmap>
#include <QPolygon>
#include <QBrush>
#include <QRegion>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QColor>
#include <QColorDialog>
#include <QStackedWidget>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPalette>
#include <QRegularExpression>
#include <QPushButton>
#include <QTableView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeView>
#include <QTreeWidgetItem>
#include <QLabel>
#include <QTreeWidget>
#include <QTabWidget>
#include <QTabBar>
#include <QPlainTextEdit>
#include <QDateTime>
#include <QTimer>
#include <QThread>
#include <QGroupBox>
#include <QFormLayout>
#include <QDir>
#include <QSet>
#include <QSplitter>
#include <QScreen>
#include <QToolTip>
#include <QWindow>
#include <QMargins>
#include <QScrollArea>
#include <QScrollBar>
#include <QResizeEvent>
#include <QEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QStyle>
#include <QProxyStyle>
#include <QStyledItemDelegate>
#include <QPointer>
#include <memory>
#include <QSignalBlocker>
#include <QPersistentModelIndex>
#include <functional>

#include "QgsVectorFileWriter.h"
#include "QgsRasterFileWriter.h"
#include "qgsrasterpipe.h"
#include "QgsVectorLayerEditBuffer.h"
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolButton>
#include <QToolBar>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QWheelEvent>
#include <QFileInfo>
#include <QFile>
#include <QFileSystemWatcher>
#include <QSettings>
#include <QTextStream>
#include <QMimeData>
#include <QUrl>
#include <QPoint>
#include <QtGlobal>
#include <QtMath>
#include "qgscoordinatereferencesystem.h"
#include "qgslayertree.h"
#include "qgslayertreegroup.h"
#include "qgslayertreelayer.h"
#include "qgsfeaturerequest.h"
#include "qgsfillsymbol.h"
#include "qgslinesymbol.h"
#include "qgsmarkersymbol.h"
#include "qgssinglesymbolrenderer.h"
#include "qgspallabeling.h"
#include "qgsvectorlayerlabeling.h"
#include "qgstextformat.h"
#include "qgstextbuffersettings.h"



namespace
{
    QgsLayerTreeGroup* asLayerTreeGroupNode(QgsLayerTreeNode* node)
    {
        return QgsLayerTree::isGroup(node)
            ? QgsLayerTree::toGroup(node)
            : nullptr;
    }

    QgsLayerTreeLayer* asLayerTreeLayerNode(QgsLayerTreeNode* node)
    {
        return QgsLayerTree::isLayer(node)
            ? QgsLayerTree::toLayer(node)
            : nullptr;
    }

    class WorkspaceFeatureSelectTool final : public QgsMapTool
    {
    public:
        WorkspaceFeatureSelectTool(QgsMapCanvas* canvas, std::function<void(const QString&)> report)
            : QgsMapTool(canvas), m_report(std::move(report))
        {
            setToolName(QStringLiteral("Select feature"));
        }

        void canvasReleaseEvent(QgsMapMouseEvent* event) override
        {
            if (!event || event->button() != Qt::LeftButton)
            {
                return;
            }

            QgsLayerTreeView* treeView = m_treeView;
            QgsVectorLayer* layer = treeView
                ? qobject_cast<QgsVectorLayer*>(treeView->currentLayer())
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
            const QgsRectangle searchExtent(
                point.x() - tolerance,
                point.y() - tolerance,
                point.x() + tolerance,
                point.y() + tolerance);
            QgsFeatureIterator features = layer->getFeatures(
                QgsFeatureRequest().setFilterRect(searchExtent));
            QgsFeature feature;
            if (features.nextFeature(feature))
            {
                layer->selectByIds(QgsFeatureIds() << feature.id(), Qgis::SelectBehavior::AddToSelection);
                canvas()->refresh();
                if (m_report)
                {
                    m_report(QStringLiteral("已选择要素 %1（%2）。")
                        .arg(feature.id())
                        .arg(layer->name()));
                }
            }
            else if (m_report)
            {
                m_report(QStringLiteral("当前位置没有找到“%1”的要素。")
                    .arg(layer->name()));
            }
        }

        void setTreeView(QgsLayerTreeView* treeView)
        {
            m_treeView = treeView;
        }

    private:
        QgsLayerTreeView* m_treeView = nullptr;
        std::function<void(const QString&)> m_report;
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

            QString value;
            if (m_measureArea)
            {
                QgsPolygonXY polygon;
                polygon.append(m_points);
                const double area = m_distanceArea.measureArea(QgsGeometry::fromPolygonXY(polygon));
                value = QgsDistanceArea::formatArea(area, 2, m_distanceArea.areaUnits());
                m_report(QStringLiteral("%1面积：%2")
                    .arg(finished ? QStringLiteral("测量完成，") : QString())
                    .arg(value));
            }
            else
            {
                const double distance = m_distanceArea.measureLine(m_points);
                value = QgsDistanceArea::formatDistance(distance, 2, m_distanceArea.lengthUnits());
                m_report(QStringLiteral("%1距离：%2")
                    .arg(finished ? QStringLiteral("测量完成，") : QString())
                    .arg(value));
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

    // 1920x1080 基准界面中经过确认的卡片实际视觉尺寸。
    constexpr int kHomeModuleCardBaseWidth = 462;
    constexpr int kHomeModuleCardBaseHeight = 234;
    // 1920x1080 最大化窗口扣除标题栏和状态栏后的首页设计画布。
    constexpr int kHomeCanvasBaseWidth = 1920;
    constexpr int kHomeCanvasBaseHeight = 1020;
    // These are the final visual dimensions measured from the user's
    // external-monitor reference screenshots, not the pre-scaling values
    // used by older responsive layouts.
    constexpr int kHomeLoginPanelBaseWidth = 540;
    constexpr int kHomeLoginPanelBaseHeight = 390;
    constexpr int kHomeUtilityButtonWidth = 172;
    constexpr int kHomeUtilityButtonHeight = 54;
    constexpr int kHomeUtilityButtonFontSize = 16;
    constexpr int kHomeGridHorizontalSpacing = 80;
    constexpr int kHomeGridVerticalSpacing = 74;
    constexpr int kHomeGridToUtilityGap = 45;
    constexpr int kHomeUtilityBottomGap = 20;
    constexpr int kQualityResultPostActionScanIntervalMs = 2000;
    constexpr int kMaxQualityResultPostActionScanAttempts = 300;
    constexpr const char* kPluginEmbeddedControlDelegateProperty = "ltzkPluginEmbeddedControlDelegate";

    bool hasWorkspaceTopBarThemeBackground(const QString& moduleTitle)
    {
        return moduleTitle == QStringLiteral("\u5730\u56FE\u6210\u679C\u7BA1\u7406")
            || moduleTitle == QStringLiteral("\u5730\u56FE\u667A\u80FD\u5236\u56FE")
            || moduleTitle == QStringLiteral("\u5730\u56FE\u6210\u679C\u8D28\u68C0")
            || moduleTitle == QStringLiteral("\u5730\u56FE\u670D\u52A1\u7BA1\u7406");
    }

    struct WorkspaceTopBarBackgroundPalette
    {
        QColor backgroundStart;
        QColor backgroundEnd;
    };

    WorkspaceTopBarBackgroundPalette workspaceTopBarBackgroundPalette(const QString& themeKey)
    {
        const WorkspaceTopBarBackgroundPalette sciFiPalette = {
            QColor(QStringLiteral("#07598C")),
            QColor(QStringLiteral("#06245A"))
        };

        if (themeKey == QStringLiteral("white"))
        {
            return {
                QColor(QStringLiteral("#FFFFFF")),
                QColor(QStringLiteral("#F1F5F9"))
            };
        }
        if (themeKey == QStringLiteral("black"))
        {
            return {
                QColor(QStringLiteral("#101010")),
                QColor(QStringLiteral("#1A1A1A"))
            };
        }
        if (themeKey == QStringLiteral("field-notes"))
        {
            return {
                QColor(QStringLiteral("#3D5540")),
                QColor(QStringLiteral("#465B47"))
            };
        }
        if (themeKey == QStringLiteral("steel-warm-white"))
        {
            return {
                QColor(QStringLiteral("#34464B")),
                QColor(QStringLiteral("#2F474A"))
            };
        }
        if (themeKey == QStringLiteral("sci-fi"))
        {
            return sciFiPalette;
        }

        return sciFiPalette;
    }

    class WorkspaceTopBarGradientScrollArea final : public QScrollArea
    {
    public:
        explicit WorkspaceTopBarGradientScrollArea(QWidget* parent = nullptr)
            : QScrollArea(parent)
        {
            connect(
                horizontalScrollBar(),
                &QScrollBar::rangeChanged,
                this,
                [this](int, int)
                {
                    scheduleHeightAdjustment();
                });
        }

        void setBaseContentHeight(int height)
        {
            m_baseContentHeight = qMax(1, height);
            scheduleHeightAdjustment();
        }

    protected:
        void resizeEvent(QResizeEvent* event) override
        {
            QScrollArea::resizeEvent(event);
            scheduleHeightAdjustment();
        }

        void wheelEvent(QWheelEvent* event) override
        {
            if (!event)
            {
                return;
            }

            const QPoint angleDelta = event->angleDelta();
            const QPoint pixelDelta = event->pixelDelta();
            if (angleDelta.y() != 0 || pixelDelta.y() != 0)
            {
                // The top bar is a command strip, not a vertically scrollable
                // document.  Consume vertical wheel input so a hidden
                // scrollbar can never move the buttons out of view.
                event->accept();
                return;
            }
            QScrollArea::wheelEvent(event);
        }

        void paintEvent(QPaintEvent* event) override
        {
            QScrollArea::paintEvent(event);
            if (!viewport()->property("topBarThemeBackgroundActive").toBool())
            {
                return;
            }

            QPainter painter(viewport());
            painter.setRenderHint(QPainter::Antialiasing, true);

            const qreal widthValue = qMax<qreal>(1.0, viewport()->width());
            const QString themeKey = viewport()->property("topBarThemeKey").toString();
            const WorkspaceTopBarBackgroundPalette palette = workspaceTopBarBackgroundPalette(themeKey);

            QLinearGradient backgroundGradient(
                QPointF(0.0, 0.0),
                QPointF(widthValue, 0.0));
            backgroundGradient.setColorAt(0.0, palette.backgroundStart);
            backgroundGradient.setColorAt(1.0, palette.backgroundEnd);
            painter.fillRect(viewport()->rect(), backgroundGradient);
        }

    private:
        void scheduleHeightAdjustment()
        {
            if (m_heightAdjustmentPending)
            {
                return;
            }

            m_heightAdjustmentPending = true;
            QTimer::singleShot(
                0,
                this,
                [this]()
                {
                    m_heightAdjustmentPending = false;
                    if (m_baseContentHeight <= 0 || !horizontalScrollBar())
                    {
                        return;
                    }

                    const int scrollBarHeight = horizontalScrollBar()->maximum() > 0
                        ? horizontalScrollBar()->sizeHint().height()
                        : 0;
                    const int desiredHeight = m_baseContentHeight + scrollBarHeight;
                    if (height() != desiredHeight)
                    {
                        setFixedHeight(desiredHeight);
                    }
                });
        }

        int m_baseContentHeight = 0;
        bool m_heightAdjustmentPending = false;
    };

    class LTZKLayerTreeModel final : public QgsLayerTreeModel
    {
    public:
        explicit LTZKLayerTreeModel(QgsLayerTree* rootNode, QObject* parent = nullptr)
            : QgsLayerTreeModel(rootNode, parent)
        {
        }

        QVariant data(const QModelIndex& index, int role) const override
        {
            if (role == Qt::CheckStateRole)
            {
                QgsLayerTreeNode* node = index2node(index);
                if (node && QgsLayerTree::isGroup(node))
                {
                    QgsLayerTreeGroup* group = asLayerTreeGroupNode(node);
                    if (group)
                    {
                        if (group->isItemVisibilityCheckedRecursive())
                        {
                            return Qt::Checked;
                        }
                        if (group->isItemVisibilityUncheckedRecursive())
                        {
                            return Qt::Unchecked;
                        }
                        return Qt::PartiallyChecked;
                    }
                }
            }

            return QgsLayerTreeModel::data(index, role);
        }

        bool setData(
            const QModelIndex& index,
            const QVariant& value,
            int role) override
        {
            QgsLayerTreeNode* node = nullptr;
            if (role == Qt::CheckStateRole)
            {
                node = index2node(index);
            }

            QgsLayerTreeGroup* group = nullptr;
            if (node && QgsLayerTree::isGroup(node))
            {
                group = asLayerTreeGroupNode(node);
            }

            const bool checked = value.toInt() == static_cast<int>(Qt::Checked);
            const bool result = QgsLayerTreeModel::setData(index, value, role);
            if (!result || !group)
            {
                return result;
            }

            const bool descendantsMatch = checked
                ? group->isItemVisibilityCheckedRecursive()
                : group->isItemVisibilityUncheckedRecursive();
            if (!descendantsMatch)
            {
                group->setItemVisibilityCheckedRecursive(checked);
            }

            return result;
        }
    };

    Qt::CheckState layerTreeNodeCheckState(
        QgsLayerTreeNode* node,
        const QgsLayerTreeModel* model)
    {
        if (!node || !model)
        {
            return Qt::Unchecked;
        }

        const QModelIndex index = model->node2index(node);
        const QVariant state = model->data(index, Qt::CheckStateRole);
        if (!state.isValid())
        {
            return Qt::Unchecked;
        }

        return static_cast<Qt::CheckState>(state.toInt());
    }

    QList<QgsLayerTreeNode*> collectCheckedLayerTreeNodes(
        QgsLayerTreeGroup* root,
        const QgsLayerTreeModel* model)
    {
        QList<QgsLayerTreeNode*> checkedNodes;
        if (!root || !model)
        {
            return checkedNodes;
        }

        const auto visit = [&](
            const auto& self,
            QgsLayerTreeNode* node) -> void
        {
            if (!node)
            {
                return;
            }

            const Qt::CheckState checkState = layerTreeNodeCheckState(node, model);
            if (QgsLayerTree::isGroup(node))
            {
                if (checkState == Qt::Checked)
                {
                    checkedNodes.append(node);
                    return;
                }

                if (checkState == Qt::PartiallyChecked ||
                    checkState == Qt::Unchecked)
                {
                    for (QgsLayerTreeNode* child : node->children())
                    {
                        self(self, child);
                    }
                }
                return;
            }

            if (QgsLayerTree::isLayer(node) && checkState == Qt::Checked)
            {
                checkedNodes.append(node);
            }
        };

        for (QgsLayerTreeNode* child : root->children())
        {
            visit(visit, child);
        }

        return checkedNodes;
    }

    QList<QgsLayerTreeNode*> mergeLayerTreeRemovalTargets(
        QgsLayerTreeView* view,
        QgsLayerTreeNode* contextNode)
    {
        QList<QgsLayerTreeNode*> candidates;
        QgsLayerTree* root = QgsProject::instance()->layerTreeRoot();
        if (!view || !root)
        {
            return candidates;
        }

        if (QgsLayerTreeModel* model = view->layerTreeModel())
        {
            candidates.append(collectCheckedLayerTreeNodes(root, model));
        }
        candidates.append(view->selectedNodes(true));
        if (contextNode)
        {
            candidates.append(contextNode);
        }

        QList<QgsLayerTreeNode*> uniqueCandidates;
        QSet<QgsLayerTreeNode*> candidateSet;
        for (QgsLayerTreeNode* node : candidates)
        {
            if (!node || node == root || candidateSet.contains(node))
            {
                continue;
            }

            candidateSet.insert(node);
            uniqueCandidates.append(node);
        }

        QList<QgsLayerTreeNode*> topLevelTargets;
        for (QgsLayerTreeNode* node : uniqueCandidates)
        {
            bool coveredByAncestor = false;
            for (QgsLayerTreeNode* parent = node->parent();
                 parent;
                 parent = parent->parent())
            {
                if (candidateSet.contains(parent))
                {
                    coveredByAncestor = true;
                    break;
                }
            }

            if (!coveredByAncestor)
            {
                topLevelTargets.append(node);
            }
        }

        return topLevelTargets;
    }

    QList<QgsLayerTreeNode*> selectedLayerTreeRemovalTargets(
        QgsLayerTreeView* view,
        QgsLayerTreeNode* contextNode = nullptr)
    {
        if (!view)
        {
            return {};
        }

        QList<QgsLayerTreeNode*> candidates = view->selectedNodes(true);
        if (contextNode)
        {
            candidates.append(contextNode);
        }

        QSet<QgsLayerTreeNode*> candidateSet;
        QList<QgsLayerTreeNode*> uniqueCandidates;
        for (QgsLayerTreeNode* node : candidates)
        {
            if (node && node != QgsProject::instance()->layerTreeRoot()
                && !candidateSet.contains(node))
            {
                candidateSet.insert(node);
                uniqueCandidates.append(node);
            }
        }

        QList<QgsLayerTreeNode*> topLevelTargets;
        for (QgsLayerTreeNode* node : uniqueCandidates)
        {
            bool coveredByAncestor = false;
            for (QgsLayerTreeNode* parent = node->parent(); parent; parent = parent->parent())
            {
                if (candidateSet.contains(parent))
                {
                    coveredByAncestor = true;
                    break;
                }
            }
            if (!coveredByAncestor)
            {
                topLevelTargets.append(node);
            }
        }
        return topLevelTargets;
    }

    bool confirmLayerTreeRemoval(
        QWidget* parent,
        const QList<QgsLayerTreeNode*>& nodes)
    {
        int groupCount = 0;
        int layerCount = 0;
        QStringList names;
        for (QgsLayerTreeNode* node : nodes)
        {
            if (!node)
            {
                continue;
            }

            if (QgsLayerTree::isGroup(node))
            {
                ++groupCount;
            }
            else if (QgsLayerTree::isLayer(node))
            {
                ++layerCount;
            }

            if (names.size() < 8)
            {
                names.append(node->name());
            }
        }

        const int totalCount = groupCount + layerCount;
        if (totalCount == 0)
        {
            return false;
        }

        QStringList messageLines;
        messageLines.append(
            QStringLiteral(
                "确定删除整棵图层树中已勾选/选中的 %1 项吗？")
                .arg(totalCount));
        messageLines.append(
            QStringLiteral("组：%1，图层：%2")
                .arg(groupCount)
                .arg(layerCount));
        if (!names.isEmpty())
        {
            messageLines.append(
                QStringLiteral("项目：%1")
                    .arg(names.join(QStringLiteral("、"))));
        }
        messageLines.append(
            QStringLiteral(
                "本操作只从当前工程图层树移除节点，不会删除本地文件或数据库源数据。"));

        const QMessageBox::StandardButton answer = QMessageBox::question(
            parent,
            QStringLiteral("确认删除图层树节点"),
            messageLines.join(QChar('\n')),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        return answer == QMessageBox::Yes;
    }

    void suppressPluginTableEmbeddedWidgetTexts(QAbstractItemView* view);

    void localizeColorDialog(QColorDialog* dialog)
    {
        if (!dialog)
        {
            return;
        }

        auto localizedText = [](const QString& text) -> QString
        {
            QString key = text.trimmed();
            key.remove(QChar('&'));
            while (key.endsWith(QChar(':')) || key.endsWith(QStringLiteral("：")))
            {
                key.chop(1);
                key = key.trimmed();
            }

            if (key == QStringLiteral("Basic colors")) return QStringLiteral("基础颜色");
            if (key == QStringLiteral("Custom colors")) return QStringLiteral("自定义颜色");
            if (key == QStringLiteral("Pick Screen Color")) return QStringLiteral("拾取屏幕颜色");
            if (key == QStringLiteral("Add to Custom Colors")) return QStringLiteral("添加到自定义颜色");
            if (key == QStringLiteral("Hue")) return QStringLiteral("色调：");
            if (key == QStringLiteral("Sat")) return QStringLiteral("饱和度：");
            if (key == QStringLiteral("Val")) return QStringLiteral("亮度：");
            if (key == QStringLiteral("Red")) return QStringLiteral("红色：");
            if (key == QStringLiteral("Green")) return QStringLiteral("绿色：");
            if (key == QStringLiteral("Blue")) return QStringLiteral("蓝色：");
            if (key == QStringLiteral("HTML")) return QStringLiteral("十六进制：");
            if (key == QStringLiteral("OK")) return QStringLiteral("确定");
            if (key == QStringLiteral("Cancel")) return QStringLiteral("取消");
            return QString();
        };

        const QList<QLabel*> labels = dialog->findChildren<QLabel*>();
        for (QLabel* label : labels)
        {
            const QString translated = localizedText(label->text());
            if (!translated.isEmpty())
            {
                label->setText(translated);
            }
        }

        const QList<QAbstractButton*> buttons = dialog->findChildren<QAbstractButton*>();
        for (QAbstractButton* button : buttons)
        {
            const QString translated = localizedText(button->text());
            if (!translated.isEmpty())
            {
                button->setText(translated);
            }
        }

        const QList<QDialogButtonBox*> buttonBoxes = dialog->findChildren<QDialogButtonBox*>();
        for (QDialogButtonBox* buttonBox : buttonBoxes)
        {
            if (QAbstractButton* okButton = buttonBox->button(QDialogButtonBox::Ok))
            {
                okButton->setText(QStringLiteral("确定"));
            }
            if (QAbstractButton* cancelButton = buttonBox->button(QDialogButtonBox::Cancel))
            {
                cancelButton->setText(QStringLiteral("取消"));
            }
        }
    }

    QColor chooseLocalizedColor(const QColor& initialColor, QWidget* parent, const QString& title, const QString& styleSheet)
    {
        QColorDialog dialog(initialColor, parent);
        dialog.setWindowTitle(title);
        dialog.setOption(QColorDialog::DontUseNativeDialog, true);
        dialog.setStyleSheet(styleSheet);
        localizeColorDialog(&dialog);
        QTimer::singleShot(0, &dialog, [&dialog]() {
            localizeColorDialog(&dialog);
            });

        if (dialog.exec() != QDialog::Accepted)
        {
            return QColor();
        }
        return dialog.selectedColor();
    }


    bool isComboBoxInternalLineEdit(QLineEdit* lineEdit)
    {
        for (QWidget* parent = lineEdit; parent; parent = parent->parentWidget())
        {
            if (qobject_cast<QComboBox*>(parent))
            {
                return true;
            }
        }
        return false;
    }

    bool isDrawingLayoutTreeEmbeddedColumn(QAbstractItemView* view, const QModelIndex& index)
    {
        return view
            && index.isValid()
            && view->objectName() == QStringLiteral("treeWidget_csv")
            && index.column() >= 1
            && index.column() <= 3;
    }

    void ensureDrawingLayoutTreeColumnWidths(QWidget* widget)
    {
        if (!widget)
        {
            return;
        }

        const QList<QTreeWidget*> trees = widget->findChildren<QTreeWidget*>();
        for (QTreeWidget* tree : trees)
        {
            if (!tree
                || tree->objectName() != QStringLiteral("treeWidget_csv")
                || tree->columnCount() != 6)
            {
                continue;
            }

            QHeaderView* header = tree->header();
            if (!header)
            {
                continue;
            }

            header->setStretchLastSection(false);
            for (int column = 0; column < tree->columnCount(); ++column)
            {
                header->setSectionResizeMode(column, QHeaderView::Interactive);
            }

            tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            const int minimumColumnWidths[] = { 105, 150, 180, 245, 120, 160 };
            for (int column = 0; column < tree->columnCount(); ++column)
            {
                tree->setColumnWidth(column, qMax(tree->columnWidth(column), minimumColumnWidths[column]));
            }

            tree->updateGeometry();
            if (tree->viewport())
            {
                tree->viewport()->update();
            }
        }
    }

    class PluginEmbeddedControlDelegate final : public QStyledItemDelegate
    {
    public:
        explicit PluginEmbeddedControlDelegate(QAbstractItemView* view, QAbstractItemDelegate* sourceDelegate)
            : QStyledItemDelegate(view)
            , m_view(view)
            , m_sourceDelegate(sourceDelegate)
        {
            setProperty(kPluginEmbeddedControlDelegateProperty, true);
        }

        void suppressTextForIndex(const QModelIndex& index)
        {
            if (!index.isValid())
            {
                return;
            }

            const QPersistentModelIndex persistentIndex(index);
            if (!m_suppressedTextIndexes.contains(persistentIndex))
            {
                m_suppressedTextIndexes.append(persistentIndex);
            }
        }

        void paint(QPainter* painter, const QStyleOptionViewItem& sourceOption, const QModelIndex& index) const override
        {
            const bool suppressText = m_view && (m_view->indexWidget(index) || isDrawingLayoutTreeEmbeddedColumn(m_view, index) || m_suppressedTextIndexes.contains(QPersistentModelIndex(index)));
            if (!suppressText && m_sourceDelegate && m_sourceDelegate.data() != this)
            {
                m_sourceDelegate->paint(painter, sourceOption, index);
                return;
            }

            QStyleOptionViewItem option(sourceOption);
            initStyleOption(&option, index);
            if (suppressText)
            {
                option.text.clear();
            }
            QStyle* viewStyle = option.widget ? option.widget->style() : QApplication::style();
            viewStyle->drawControl(QStyle::CE_ItemViewItem, &option, painter, option.widget);
        }


    private:
        QPointer<QAbstractItemView> m_view;
        QPointer<QAbstractItemDelegate> m_sourceDelegate;
        QList<QPersistentModelIndex> m_suppressedTextIndexes;
    };

    PluginEmbeddedControlDelegate* pluginEmbeddedControlDelegate(QAbstractItemView* view)
    {
        if (!view)
        {
            return nullptr;
        }

        QAbstractItemDelegate* delegate = view->itemDelegate();
        if (delegate && delegate->property(kPluginEmbeddedControlDelegateProperty).toBool())
        {
            return dynamic_cast<PluginEmbeddedControlDelegate*>(delegate);
        }
        PluginEmbeddedControlDelegate* pluginDelegate = new PluginEmbeddedControlDelegate(view, delegate);
        view->setItemDelegate(pluginDelegate);
        return pluginDelegate;
    }

    void ensurePluginEmbeddedControlDelegate(QAbstractItemView* view)
    {
        if (!view)
        {
            return;
        }

        pluginEmbeddedControlDelegate(view);
    }

    void suppressPluginEmbeddedControlItemText(QAbstractItemView* view, const QModelIndex& index)
    {
        if (!view || !index.isValid())
        {
            return;
        }

        if (PluginEmbeddedControlDelegate* delegate = pluginEmbeddedControlDelegate(view))
        {
            delegate->suppressTextForIndex(index);
        }
    }

    void clearPluginTableWidgetCellItemDisplayText(QTableWidget* tableWidget, int row, int column)
    {
        if (!tableWidget)
        {
            return;
        }

        QTableWidgetItem* item = tableWidget->item(row, column);
        if (!item)
        {
            return;
        }

        item->setData(Qt::DisplayRole, QString());
        item->setData(Qt::EditRole, QString());
    }

    void clearPluginTableWidgetIndexItemDisplayText(QAbstractItemView* view, const QModelIndex& index)
    {
        if (!view || !index.isValid())
        {
            return;
        }

        QTableWidget* tableWidget = qobject_cast<QTableWidget*>(view);
        if (!tableWidget)
        {
            return;
        }

        clearPluginTableWidgetCellItemDisplayText(tableWidget, index.row(), index.column());
    }

    QModelIndex indexForEmbeddedWidget(QAbstractItemView* view, QWidget* embeddedWidget)
    {
        if (!view || !embeddedWidget || !embeddedWidget->parentWidget() || !view->viewport())
        {
            return QModelIndex();
        }

        const QPoint viewportPoint = embeddedWidget->parentWidget()->mapTo(view->viewport(), embeddedWidget->geometry().center());
        QModelIndex index = view->indexAt(viewportPoint);
        if (index.isValid())
        {
            return index;
        }

        QAbstractItemModel* model = view->model();
        if (!model)
        {
            return QModelIndex();
        }

        const QRect embeddedRect(embeddedWidget->parentWidget()->mapTo(view->viewport(), embeddedWidget->geometry().topLeft()), embeddedWidget->size());
        const int rowLimit = qMin(model->rowCount(), 200);
        const int columnLimit = qMin(model->columnCount(), 50);
        int bestArea = 0;
        QModelIndex bestIndex;
        for (int row = 0; row < rowLimit; ++row)
        {
            for (int column = 0; column < columnLimit; ++column)
            {
                QModelIndex candidate = model->index(row, column);
                if (!candidate.isValid())
                {
                    continue;
                }
                const QRect overlap = view->visualRect(candidate).intersected(embeddedRect);
                const int area = overlap.width() * overlap.height();
                if (area > bestArea)
                {
                    bestArea = area;
                    bestIndex = candidate;
                }
            }
        }
        return bestIndex;
    }

    QModelIndex treeModelIndexForItem(QTreeWidget* tree, QTreeWidgetItem* item, int column)
    {
        if (!tree || !item || !tree->model())
        {
            return QModelIndex();
        }

        QList<int> rowPath;
        for (QTreeWidgetItem* current = item; current; current = current->parent())
        {
            QTreeWidgetItem* parent = current->parent();
            const int row = parent ? parent->indexOfChild(current) : tree->indexOfTopLevelItem(current);
            if (row < 0)
            {
                return QModelIndex();
            }
            rowPath.prepend(row);
        }

        QModelIndex parentIndex;
        for (int depth = 0; depth < rowPath.size(); ++depth)
        {
            const int indexColumn = depth == rowPath.size() - 1 ? column : 0;
            parentIndex = tree->model()->index(rowPath.at(depth), indexColumn, parentIndex);
            if (!parentIndex.isValid())
            {
                return QModelIndex();
            }
        }
        return parentIndex;
    }

    void suppressDrawingLayoutTreeEmbeddedItemText(QTreeWidget* tree)
    {
        if (!tree)
        {
            return;
        }

        const QBrush transparentTextBrush(QColor(255, 255, 255, 0));
        std::function<void(QTreeWidgetItem*)> suppressItem = [&](QTreeWidgetItem* item)
        {
            if (!item)
            {
                return;
            }

            for (int column = 0; column < tree->columnCount(); ++column)
            {
                if (tree->itemWidget(item, column))
                {
                    const QModelIndex index = treeModelIndexForItem(tree, item, column);
                    suppressPluginEmbeddedControlItemText(tree, index);
                    item->setForeground(column, transparentTextBrush);
                }
            }
            for (int childIndex = 0; childIndex < item->childCount(); ++childIndex)
            {
                suppressItem(item->child(childIndex));
            }
        };

        for (int topLevelIndex = 0; topLevelIndex < tree->topLevelItemCount(); ++topLevelIndex)
        {
            suppressItem(tree->topLevelItem(topLevelIndex));
        }
        if (tree->viewport())
        {
            tree->viewport()->update();
        }
    }
    bool isDrawingLayoutTreeCombo(QComboBox* combo)
    {
        for (QWidget* parent = combo ? combo->parentWidget() : nullptr; parent; parent = parent->parentWidget())
        {
            if (QTreeWidget* tree = qobject_cast<QTreeWidget*>(parent))
            {
                return tree->objectName() == QStringLiteral("treeWidget_csv");
            }
        }
        return false;
    }

    void polishDrawingLayoutTreeEmbeddedCombos(LTZK* owner, QWidget* widget, bool isDarkTheme, bool isBlackTheme, QComboBox* onlyCombo = nullptr)
    {
        if (!owner || !widget)
        {
            return;
        }

        const QString diagnosticLogPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("log/combo_arrow_runtime.log"));
        QDir diagnosticLogDir(QFileInfo(diagnosticLogPath).absolutePath());
        diagnosticLogDir.mkpath(QStringLiteral("."));
        auto appendDiagnostic = [&diagnosticLogPath](const QString& message)
        {
            QFile logFile(diagnosticLogPath);
            if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            {
                QTextStream diagnosticStream(&logFile);
                diagnosticStream << QDateTime::currentDateTime().toString(Qt::ISODate)
                    << QStringLiteral(" ") << message << QLatin1Char('\n');
            }
        };
        appendDiagnostic(QStringLiteral("polish combo window=%1 object=%2")
            .arg(widget->windowTitle(), widget->objectName()));

        const QString comboBaseColor = isDarkTheme
            ? (isBlackTheme ? QStringLiteral("#050505") : QStringLiteral("#06111d"))
            : QStringLiteral("#f8fafc");
        const QString comboBorderColor = isDarkTheme
            ? (isBlackTheme ? QStringLiteral("#f59e0b") : QStringLiteral("rgba(46, 168, 223, 150)"))
            : QStringLiteral("#93c5fd");
        const QString comboDropDownColor = isDarkTheme
            ? (isBlackTheme ? QStringLiteral("#181818") : QStringLiteral("#0b2b45"))
            : QStringLiteral("#e2e8f0");
        const QString comboTextColor = isDarkTheme
            ? QStringLiteral("#FFFFFF")
            : QStringLiteral("#0f172a");
        const QString comboSelectionColor = isDarkTheme
            ? (isBlackTheme ? QStringLiteral("#7c2d12") : QStringLiteral("#147db5"))
            : QStringLiteral("#bfdbfe");
        const QColor comboTextPaletteColor = isDarkTheme
            ? QColor(255, 255, 255)
            : QColor(15, 23, 42);

        const QString comboStyleTemplate = QStringLiteral(
            "QComboBox { color: %4; background: %1; border: 1px solid %2; padding: 2px 30px 2px 6px; }"
            "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 24px; border-left: 1px solid %2; background: %3; }"
            "QComboBox QLineEdit { color: %4; background: %1; border: 0; selection-background-color: %5; }"
            "QComboBox QAbstractItemView { color: %4; background: %1; selection-background-color: %5; selection-color: %4; }")
            .arg(comboBaseColor)
            .arg(comboBorderColor)
            .arg(comboDropDownColor)
            .arg(comboTextColor)
            .arg(comboSelectionColor);
        const QString comboLineEditStyle = QStringLiteral(
            "QLineEdit { color: %1; background: %2; border: 0; padding: 0 4px; min-height: 0px; max-height: 16777215px; selection-background-color: %3; }")
            .arg(comboTextColor)
            .arg(comboBaseColor)
            .arg(comboSelectionColor);
        auto polishCombo = [owner, isDarkTheme, isBlackTheme, &comboStyleTemplate, &comboLineEditStyle, &comboTextPaletteColor, &appendDiagnostic](QComboBox* combo, const QFont& font)
        {
            if (!combo)
            {
                return;
            }
            if (!isDrawingLayoutTreeCombo(combo))
            {
                return;
            }

            const QString comboThemeKey = !isDarkTheme
                ? QStringLiteral("white")
                : (isBlackTheme ? QStringLiteral("black") : QStringLiteral("sci-fi"));
            if (combo->property("ltzkComboPolishedTheme").toString() == comboThemeKey)
            {
                return;
            }
            const QString preservedComboDisplayText = combo->currentText();
            combo->setProperty("ltzkComboPolishedTheme", comboThemeKey);
            combo->setEditable(true);
            combo->setFont(font);
            combo->setAttribute(Qt::WA_StyledBackground, true);
            combo->setAutoFillBackground(true);

            if (!combo->property("ltzkDrawingLayoutComboArrowTarget").toBool())
            {
                combo->setProperty("ltzkDrawingLayoutComboArrowTarget", true);
                combo->installEventFilter(owner);
            }
            const QString comboArrowResource = isDarkTheme
                ? QStringLiteral(":/LTZK/Resources/combo_arrow_dark.svg")
                : QStringLiteral(":/LTZK/Resources/combo_arrow_light.svg");
            combo->setStyleSheet(LTZKUiScale::scaleStyleSheetPixels(
                comboStyleTemplate,
                LTZKUiScale::factorForWidget(combo)));
            QLabel* arrowLabel = combo->findChild<QLabel*>(QStringLiteral("ltzkDrawingLayoutComboArrow"), Qt::FindDirectChildrenOnly);
            if (!arrowLabel)
            {
                arrowLabel = new QLabel(combo);
                arrowLabel->setObjectName(QStringLiteral("ltzkDrawingLayoutComboArrow"));
                arrowLabel->setProperty("ltzkDrawingLayoutComboArrow", true);
                arrowLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                arrowLabel->setScaledContents(true);
                arrowLabel->setStyleSheet(QStringLiteral("QLabel#ltzkDrawingLayoutComboArrow { background: transparent; border: 0; }"));
            }
            const qreal comboScale = LTZKUiScale::factorForWidget(combo);
            const int arrowSize = LTZKUiScale::scaledInt(14, comboScale);
            const int arrowInset = LTZKUiScale::scaledInt(6, comboScale);
            static QPixmap darkArrowPixmap;
            static QPixmap lightArrowPixmap;
            static bool darkArrowAttempted = false;
            static bool lightArrowAttempted = false;
            QPixmap& arrowPixmap = isDarkTheme ? darkArrowPixmap : lightArrowPixmap;
            bool& arrowAttempted = isDarkTheme ? darkArrowAttempted : lightArrowAttempted;
            if (!arrowAttempted)
            {
                QFile resourceFile(comboArrowResource);
                QPixmap decodedPixmap(comboArrowResource);
                QSvgRenderer renderer(comboArrowResource);
                const bool resourceExists = resourceFile.exists();
                const bool pixmapNull = decodedPixmap.isNull();
                const bool rendererValid = renderer.isValid();
                arrowPixmap = QPixmap(arrowSize, arrowSize);
                arrowPixmap.fill(Qt::transparent);
                if (rendererValid)
                {
                    QPainter painter(&arrowPixmap);
                    renderer.render(&painter, QRectF(0, 0, arrowSize, arrowSize));
                }
                else
                {
                    arrowPixmap = decodedPixmap;
                }
                qDebug() << "drawing-layout combo arrow path =" << comboArrowResource
                    << "resource exists =" << resourceExists
                    << "pixmap null =" << pixmapNull
                    << "renderer valid =" << rendererValid
                    << "plugin paths =" << QCoreApplication::libraryPaths();
                appendDiagnostic(QStringLiteral("combo path=%1 resource exists=%2 pixmap null=%3 renderer valid=%4")
                    .arg(comboArrowResource)
                    .arg(resourceExists ? QStringLiteral("true") : QStringLiteral("false"))
                    .arg(pixmapNull ? QStringLiteral("true") : QStringLiteral("false"))
                    .arg(rendererValid ? QStringLiteral("true") : QStringLiteral("false")));
                appendDiagnostic(QStringLiteral("plugin paths=%1")
                    .arg(QCoreApplication::libraryPaths().join(QStringLiteral("|"))));
                arrowAttempted = true;
            }
            arrowLabel->setPixmap(arrowPixmap);
            arrowLabel->setGeometry(qMax(0, combo->width() - arrowSize - arrowInset), qMax(0, (combo->height() - arrowSize) / 2), arrowSize, arrowSize);
            arrowLabel->raise();
            arrowLabel->show();
            QPalette palette = combo->palette();
            palette.setColor(QPalette::ButtonText, comboTextPaletteColor);
            palette.setColor(QPalette::WindowText, comboTextPaletteColor);
            palette.setColor(QPalette::Text, comboTextPaletteColor);
            combo->setPalette(palette);
            if (combo->lineEdit())
            {

                combo->lineEdit()->setReadOnly(true);
                combo->lineEdit()->setFrame(false);
                combo->lineEdit()->setFont(font);
                combo->lineEdit()->setTextMargins(0, 0, LTZKUiScale::scaledInt(24, comboScale), 0);
                combo->lineEdit()->setAttribute(Qt::WA_StyledBackground, true);
                combo->lineEdit()->setAutoFillBackground(true);
                combo->lineEdit()->setStyleSheet(LTZKUiScale::scaleStyleSheetPixels(
                    comboLineEditStyle,
                    comboScale));
                QPalette lineEditPalette = combo->lineEdit()->palette();
                lineEditPalette.setColor(QPalette::Text, comboTextPaletteColor);
                lineEditPalette.setColor(QPalette::WindowText, comboTextPaletteColor);
                combo->lineEdit()->setPalette(lineEditPalette);
                if (!preservedComboDisplayText.isEmpty())
                {
                    combo->lineEdit()->setText(preservedComboDisplayText);
                }
            }
            arrowLabel->setGeometry(qMax(0, combo->width() - arrowSize - arrowInset), qMax(0, (combo->height() - arrowSize) / 2), arrowSize, arrowSize);
            arrowLabel->raise();
            arrowLabel->show();
            if (combo->view())
            {
                combo->view()->setAttribute(Qt::WA_StyledBackground, true);
                combo->view()->setAutoFillBackground(true);
                if (combo->view()->viewport())
                {
                    combo->view()->viewport()->setAttribute(Qt::WA_StyledBackground, true);
                    combo->view()->viewport()->setAutoFillBackground(true);
                }
            }
            combo->update();
        };

        const QList<QComboBox*> combos = onlyCombo ? (QList<QComboBox*>() << onlyCombo) : widget->findChildren<QComboBox*>();
        for (QComboBox* combo : combos)
        {
            polishCombo(combo, onlyCombo ? combo->font() : widget->font());
        }

        appendDiagnostic(QStringLiteral("combo count=%1").arg(combos.size()));
        ensureDrawingLayoutTreeColumnWidths(widget);

        if (onlyCombo)
        {
            return;
        }

        const QList<QAbstractItemView*> views = widget->findChildren<QAbstractItemView*>();
        for (QAbstractItemView* view : views)
        {
            if (!view)
            {
                continue;
            }

            ensurePluginEmbeddedControlDelegate(view);
            suppressPluginTableEmbeddedWidgetTexts(view);
            if (QTreeWidget* tree = qobject_cast<QTreeWidget*>(view))
            {
                if (tree->objectName() == QStringLiteral("treeWidget_csv"))
                {
                    suppressDrawingLayoutTreeEmbeddedItemText(tree);
                }
            }


        }
    }    void suppressPluginTableEmbeddedWidgetTexts(QAbstractItemView* view)
    {
        if (!view || !view->model())
        {
            return;
        }

        ensurePluginEmbeddedControlDelegate(view);
        QTableWidget* tableWidget = qobject_cast<QTableWidget*>(view);
        QAbstractItemModel* model = view->model();
        const int rowLimit = qMin(model->rowCount(), 200);
        const int columnLimit = qMin(model->columnCount(), 50);
        for (int row = 0; row < rowLimit; ++row)
        {
            for (int column = 0; column < columnLimit; ++column)
            {
                QModelIndex index = model->index(row, column);
                if (index.isValid() && view->indexWidget(index))
                {
                    if (tableWidget)
                    {
                        QWidget* cellWidget = tableWidget->cellWidget(row, column);
                        if (cellWidget)
                        {
                            clearPluginTableWidgetCellItemDisplayText(tableWidget, row, column);
                        }
                    }
                    suppressPluginEmbeddedControlItemText(view, index);
                }
            }
        }

        QWidget* viewport = view->viewport();
        if (!viewport)
        {
            return;
        }
        const QList<QWidget*> embeddedWidgets = viewport->findChildren<QWidget*>();
        for (QWidget* embeddedWidget : embeddedWidgets)
        {
            if (!embeddedWidget || !embeddedWidget->isVisible())
            {
                continue;
            }
            const QModelIndex index = indexForEmbeddedWidget(view, embeddedWidget);
            if (index.isValid())
            {
                clearPluginTableWidgetIndexItemDisplayText(view, index);
                suppressPluginEmbeddedControlItemText(view, index);
            }
        }
        viewport->update();
    }
    void hidePluginCellLabels(QWidget* widget, QAbstractItemView* view, const QModelIndex& index)
    {
        if (!widget || !view || !index.isValid())
        {
            return;
        }

        const QRect cellRect = view->visualRect(index).adjusted(0, -6, 0, 6);
        const QList<QLabel*> cellLabels = widget->findChildren<QLabel*>();
        for (QLabel* label : cellLabels)
        {
            if (!label || label->text().trimmed().isEmpty())
            {
                continue;
            }

            bool belongsToView = false;
            for (QWidget* parent = label; parent; parent = parent->parentWidget())
            {
                if (parent == view)
                {
                    belongsToView = true;
                    break;
                }
            }
            if (!belongsToView)
            {
                continue;
            }

            const QRect labelRect(label->mapTo(view->viewport(), QPoint(0, 0)), label->size());
            if (labelRect.intersects(cellRect))
            {
                label->setVisible(false);
            }
        }
    }

    qreal visualScaleForWidget(const QWidget* widget)
    {
        return LTZKUiScale::factorForWidget(widget);
    }
    int scaledInt(const QWidget* widget, int value)
    {
        return LTZKUiScale::scaledInt(value, visualScaleForWidget(widget));
    }

    QSize scaledSize(const QWidget* widget, int width, int height)
    {
        return LTZKUiScale::scaledSize(
            QSize(width, height),
            visualScaleForWidget(widget));
    }

    QSize homeModuleCardSize(const QWidget* widget)
    {
        return scaledSize(widget, kHomeModuleCardBaseWidth, kHomeModuleCardBaseHeight);
    }
    QMargins scaledMargins(const QWidget* widget, int left, int top, int right, int bottom)
    {
        return LTZKUiScale::scaledMargins(
            QMargins(left, top, right, bottom),
            visualScaleForWidget(widget));
    }
    int topBarScaledInt(const QWidget* widget, int value)
    {
        return scaledInt(widget, value);
    }

    int splitterHandleWidth(const QWidget* widget)
    {
        return scaledInt(widget, 6);
    }

    int splitterHandleRailWidth(const QWidget* widget)
    {
        return splitterHandleWidth(widget);
    }

    QSize splitterTriangleButtonSize(const QWidget* widget)
    {
        const qreal equilateralAltitudeRatio = 0.8660254037844386;
        const int triangleWidth = splitterHandleWidth(widget);
        const int triangleHeight = qCeil(triangleWidth / equilateralAltitudeRatio);
        return QSize(triangleWidth, triangleHeight);
    }

    QPolygonF trianglePolygon(const QRect& rect, Qt::ArrowType direction)
    {
        const QRectF bounds(0.0, 0.0, rect.width(), rect.height());
        const QPointF center = bounds.center();
        // For an equilateral triangle, the altitude is side * sqrt(3) / 2.
        // Keeping the altitude and side derived from the available bounds
        // prevents the narrow splitter button from becoming a tall arrow.
        const qreal equilateralAltitudeRatio = 0.8660254037844386;
        const qreal horizontalSide = qMin(bounds.height(), bounds.width() / equilateralAltitudeRatio);
        const qreal horizontalAltitude = horizontalSide * equilateralAltitudeRatio;
        const qreal horizontalHalfSide = horizontalSide * 0.5;
        const qreal verticalSide = qMin(bounds.width(), bounds.height() / equilateralAltitudeRatio);
        const qreal verticalAltitude = verticalSide * equilateralAltitudeRatio;
        const qreal verticalHalfSide = verticalSide * 0.5;
        switch (direction)
        {
        case Qt::LeftArrow:
            return QPolygonF() << QPointF(bounds.left(), center.y())
                << QPointF(bounds.left() + horizontalAltitude, center.y() - horizontalHalfSide)
                << QPointF(bounds.left() + horizontalAltitude, center.y() + horizontalHalfSide);
        case Qt::RightArrow:
            return QPolygonF() << QPointF(bounds.right(), center.y())
                << QPointF(bounds.right() - horizontalAltitude, center.y() - horizontalHalfSide)
                << QPointF(bounds.right() - horizontalAltitude, center.y() + horizontalHalfSide);
        case Qt::UpArrow:
            return QPolygonF() << QPointF(center.x(), bounds.top())
                << QPointF(center.x() - verticalHalfSide, bounds.top() + verticalAltitude)
                << QPointF(center.x() + verticalHalfSide, bounds.top() + verticalAltitude);
        case Qt::DownArrow:
        default:
            return QPolygonF() << QPointF(center.x(), bounds.bottom())
                << QPointF(center.x() - verticalHalfSide, bounds.bottom() - verticalAltitude)
                << QPointF(center.x() + verticalHalfSide, bounds.bottom() - verticalAltitude);
        }
    }

    class SplitterTriangleButton final : public QToolButton
    {
    public:
        explicit SplitterTriangleButton(Qt::ArrowType direction, QWidget* parent = nullptr)
            : QToolButton(parent)
            , m_direction(direction)
        {
            setProperty("panelCollapseButton", true);
            setArrowType(Qt::NoArrow);
            setAutoRaise(true);
            setCursor(Qt::PointingHandCursor);
            setFocusPolicy(Qt::NoFocus);
            setAttribute(Qt::WA_TranslucentBackground, true);
            setAttribute(Qt::WA_NoSystemBackground, true);
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        }

        void setTriangleDirection(Qt::ArrowType direction)
        {
            if (m_direction == direction)
            {
                return;
            }

            m_direction = direction;
            update();
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing, true);
            QPolygonF triangle = trianglePolygon(rect(), m_direction);
            const qreal outlineWidth = qMax<qreal>(0.5, width() * 0.15);
            const QColor fill = isDown()
                ? QColor(255, 178, 56, 235)
                : (underMouse() ? QColor(74, 231, 255, 235) : QColor(27, 201, 255, 210));
            painter.setPen(QPen(QColor(200, 251, 255, 230), outlineWidth));
            painter.setBrush(fill);
            painter.drawPolygon(triangle);
        }

        void enterEvent(QEvent* event) override
        {
            QToolButton::enterEvent(event);
            update();
        }

        void leaveEvent(QEvent* event) override
        {
            QToolButton::leaveEvent(event);
            update();
        }

        bool hitButton(const QPoint& pos) const override
        {
            return trianglePolygon(rect(), m_direction).containsPoint(pos, Qt::OddEvenFill);
        }

    private:
        Qt::ArrowType m_direction = Qt::NoArrow;
    };


    void setTopBarToolButtonResponsiveMetrics(QToolButton* button, const QWidget* scaleWidget)
    {
        if (!button)
        {
            return;
        }
        QFont topBarFont = button->font();
        topBarFont.setPixelSize(topBarScaledInt(scaleWidget, 13));
        button->setFont(topBarFont);
        button->setIconSize(QSize(topBarScaledInt(scaleWidget, 30), topBarScaledInt(scaleWidget, 30)));
        button->setMinimumSize(QSize(topBarScaledInt(scaleWidget, 105), topBarScaledInt(scaleWidget, 84)));
        button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    }

    void setWorkspacePluginToolButtonCompactMetrics(QToolButton* button, const QWidget* scaleWidget)
    {
        if (!button)
        {
            return;
        }
        setTopBarToolButtonResponsiveMetrics(button, scaleWidget);
        button->setIconSize(QSize(topBarScaledInt(scaleWidget, 34), topBarScaledInt(scaleWidget, 34)));
        button->setMinimumWidth(topBarScaledInt(scaleWidget, 100));
    }

    class ResponsiveHomeModuleButton final : public QToolButton
    {
    public:
        ResponsiveHomeModuleButton(
            const QString& moduleTitle,
            const QString& moduleCode,
            const QString& subtitle,
            const QString& objectName,
            const QIcon& icon,
            QWidget* parent = nullptr)
            : QToolButton(parent)
            , m_icon(icon)
        {
            setObjectName(objectName);
            setProperty("moduleButton", true);
            setProperty("homeModuleButton", true);
            setText(QString());
            setToolTip(moduleTitle + QStringLiteral(" - ") + subtitle);
            setAccessibleName(moduleCode + QStringLiteral(" ") + moduleTitle + QStringLiteral(" ") + subtitle);
            setIcon(QIcon());
            setToolButtonStyle(Qt::ToolButtonIconOnly);
            m_scaledCardSize = homeModuleCardSize(this);
            setFixedSize(m_scaledCardSize);
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

            m_cardLayout = new QVBoxLayout(this);
            m_cardLayout->setContentsMargins(0, 0, 0, 0);
            m_cardLayout->setSpacing(0);

            m_iconLabel = new QLabel(this);
            m_iconLabel->setAlignment(Qt::AlignCenter);
            m_iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

            m_codeLabel = new QLabel(moduleCode, this);
            m_codeLabel->setAlignment(Qt::AlignCenter);
            m_codeLabel->setStyleSheet(QStringLiteral("color: #2AD4FF; background: transparent; border: none;"));
            m_codeLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

            m_titleLabel = new QLabel(moduleTitle, this);
            m_titleLabel->setAlignment(Qt::AlignCenter);
            m_titleLabel->setWordWrap(true);
            m_titleLabel->setStyleSheet(QStringLiteral("color: #F4FBFF; background: transparent; border: none;"));
            m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

            m_subtitleLabel = new QLabel(subtitle, this);
            m_subtitleLabel->setAlignment(Qt::AlignCenter);
            m_subtitleLabel->setWordWrap(true);
            m_subtitleLabel->setStyleSheet(QStringLiteral("color: #8FB7C9; background: transparent; border: none;"));
            m_subtitleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

            m_cardLayout->addWidget(m_iconLabel);
            m_cardLayout->addWidget(m_codeLabel);
            m_cardLayout->addWidget(m_titleLabel);
            m_cardLayout->addWidget(m_subtitleLabel);
            m_cardLayout->setStretch(0, 5);
            m_cardLayout->setStretch(1, 1);
            m_cardLayout->setStretch(2, 2);
            m_cardLayout->setStretch(3, 1);

            m_shadowEffect = new QGraphicsDropShadowEffect(this);
            m_shadowEffect->setColor(QColor(0, 0, 0, 125));
            setGraphicsEffect(m_shadowEffect);

            applyTheme(QStringLiteral("sci-fi"));
        }

        QSize sizeHint() const override
        {
            return m_scaledCardSize.isValid() ? m_scaledCardSize : homeModuleCardSize(this);
        }

        QSize minimumSizeHint() const override
        {
            return sizeHint();
        }

        void applyScaledCardSize(const QSize& size)
        {
            if (!size.isValid() || size.isEmpty() || size == m_scaledCardSize)
            {
                return;
            }

            m_scaledCardSize = size;
            setFixedSize(m_scaledCardSize);
            updateGeometry();
            updateScaledLayout();
        }

        void applyTheme(const QString& themeKey)
        {
            const QString normalized = themeKey.trimmed().toLower();
            m_themeKey = normalized;
            if (normalized == QStringLiteral("white"))
            {
                m_codeLabel->setStyleSheet(QStringLiteral("color: #2563eb; background: transparent; border: none;"));
                m_titleLabel->setStyleSheet(QStringLiteral("color: #0f172a; background: transparent; border: none;"));
                m_subtitleLabel->setStyleSheet(QStringLiteral("color: #475569; background: transparent; border: none;"));
            }
            else if (normalized == QStringLiteral("black"))
            {
                m_codeLabel->setStyleSheet(QStringLiteral("color: #f59e0b; background: transparent; border: none;"));
                m_titleLabel->setStyleSheet(QStringLiteral("color: #fafafa; background: transparent; border: none;"));
                m_subtitleLabel->setStyleSheet(QStringLiteral("color: #a3a3a3; background: transparent; border: none;"));
            }
            else if (normalized == QStringLiteral("field-notes"))
            {
                m_codeLabel->setStyleSheet(QStringLiteral("color: #B86F4F; background: transparent; border: none;"));
                m_titleLabel->setStyleSheet(QStringLiteral("color: #3D5540; background: transparent; border: none;"));
                m_subtitleLabel->setStyleSheet(QStringLiteral("color: #66725D; background: transparent; border: none;"));
            }
            else if (normalized == QStringLiteral("steel-warm-white"))
            {
                m_codeLabel->setStyleSheet(QStringLiteral("color: #668B8C; background: transparent; border: none;"));
                m_titleLabel->setStyleSheet(QStringLiteral("color: #2F5960; background: transparent; border: none;"));
                m_subtitleLabel->setStyleSheet(QStringLiteral("color: #687775; background: transparent; border: none;"));
            }
            else
            {
                m_codeLabel->setStyleSheet(QStringLiteral("color: #2AD4FF; background: transparent; border: none;"));
                m_titleLabel->setStyleSheet(QStringLiteral("color: #F4FBFF; background: transparent; border: none;"));
                m_subtitleLabel->setStyleSheet(QStringLiteral("color: #8FB7C9; background: transparent; border: none;"));
            }
            applyScaledCardSize(homeModuleCardSize(this));
            updateScaledLayout();
        }
    protected:
        void resizeEvent(QResizeEvent* event) override
        {
            QToolButton::resizeEvent(event);
            updateScaledLayout();
        }

    private:
        void updateScaledLayout()
        {
            const int horizontalMargin = scaledInt(this, 18);
            const int topMargin = scaledInt(this, 13);
            const int bottomMargin = scaledInt(this, 16);
            m_cardLayout->setContentsMargins(horizontalMargin, topMargin, horizontalMargin, bottomMargin);
            m_cardLayout->setSpacing(scaledInt(this, 3));

            if (m_shadowEffect)
            {
                m_shadowEffect->setBlurRadius(scaledInt(this, 20));
                m_shadowEffect->setOffset(0, scaledInt(this, 6));
            }

            const QSize iconSize = scaledSize(this, 286, 103);
            m_iconLabel->setFixedHeight(scaledInt(this, 103));
            setThemeAwareIconPixmap(iconSize);

            QFont font = m_codeLabel->font();
            font.setFamily(QStringLiteral("Microsoft YaHei UI"));
            font.setPixelSize(scaledInt(this, 18));
            font.setWeight(QFont::Bold);
            m_codeLabel->setFont(font);
            m_codeLabel->setMinimumHeight(scaledInt(this, 20));

            font = m_titleLabel->font();
            font.setFamily(QStringLiteral("Microsoft YaHei UI"));
            font.setPixelSize(scaledInt(this, 22));
            font.setWeight(QFont::ExtraBold);
            m_titleLabel->setFont(font);
            m_titleLabel->setMinimumHeight(scaledInt(this, 36));

            font = m_subtitleLabel->font();
            font.setFamily(QStringLiteral("Microsoft YaHei UI"));
            font.setPixelSize(scaledInt(this, 12));
            font.setWeight(QFont::Medium);
            m_subtitleLabel->setFont(font);
            m_subtitleLabel->setMinimumHeight(scaledInt(this, 20));
        }

        QPixmap normalizedIconPixmap(const QPixmap& pixmap) const
        {
            if (pixmap.isNull())
            {
                return pixmap;
            }

            qreal dpr = pixmap.devicePixelRatio();
            if (!qIsFinite(dpr) || dpr <= 0.0)
            {
                dpr = 1.0;
            }

            const QSize logicalSize(
                qMax(1, qRound(pixmap.width() / dpr)),
                qMax(1, qRound(pixmap.height() / dpr)));
            QPixmap normalized = pixmap;
            if (normalized.size() != logicalSize)
            {
                normalized = normalized.scaled(
                    logicalSize,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation);
            }
            // The destination QLabel uses logical coordinates. Normalize the
            // source before centering so a high-DPI pixmap cannot be placed
            // with physical-pixel dimensions and appear in the upper-left.
            normalized.setDevicePixelRatio(1.0);
            return normalized;
        }

        void setThemeAwareIconPixmap(const QSize& iconSize)
        {
            const QPixmap originalPixmap = normalizedIconPixmap(m_icon.pixmap(iconSize));
            const bool isFieldNotes = m_themeKey == QStringLiteral("field-notes");
            const bool isSteelWarmWhite = m_themeKey == QStringLiteral("steel-warm-white");
            if (!isFieldNotes && !isSteelWarmWhite)
            {
                m_iconLabel->setPixmap(originalPixmap);
                return;
            }

            QPixmap themedPixmap(iconSize);
            themedPixmap.fill(Qt::transparent);
            QPainter painter(&themedPixmap);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(Qt::NoPen);
            painter.setBrush(isFieldNotes
                ? QColor(184, 199, 175, 58)
                : QColor(184, 207, 203, 54));
            const qreal haloHorizontalInset = scaledInt(this, 8);
            const qreal haloVerticalInset = scaledInt(this, 6);
            const QRectF haloRect = QRectF(themedPixmap.rect()).adjusted(
                haloHorizontalInset,
                haloVerticalInset,
                -haloHorizontalInset,
                -haloVerticalInset);
            painter.drawEllipse(haloRect);
            painter.setOpacity(0.84);
            const QPoint iconPosition(
                (themedPixmap.width() - originalPixmap.width()) / 2,
                (themedPixmap.height() - originalPixmap.height()) / 2);
            painter.drawPixmap(iconPosition, originalPixmap);
            m_iconLabel->setPixmap(themedPixmap);
        }

        QIcon m_icon;
        QString m_themeKey = QStringLiteral("sci-fi");
        QVBoxLayout* m_cardLayout = nullptr;
        QLabel* m_iconLabel = nullptr;
        QLabel* m_codeLabel = nullptr;
        QLabel* m_titleLabel = nullptr;
        QLabel* m_subtitleLabel = nullptr;
        QGraphicsDropShadowEffect* m_shadowEffect = nullptr;
        QSize m_scaledCardSize;
    };

    class HomeLoginPage final : public QWidget
    {
    public:
        explicit HomeLoginPage(QWidget* parent = nullptr)
            : QWidget(parent)
        {
        }

        void refreshScaledMetrics()
        {
            QFrame* panel = findChild<QFrame*>(QStringLiteral("homeLoginPanel"));
            if (!panel)
            {
                return;
            }

            panel->setFixedSize(scaledSize(this, kHomeLoginPanelBaseWidth, kHomeLoginPanelBaseHeight));
            if (QGraphicsDropShadowEffect* shadow =
                    qobject_cast<QGraphicsDropShadowEffect*>(panel->graphicsEffect()))
            {
                shadow->setBlurRadius(scaledInt(panel, 24));
                shadow->setOffset(0, scaledInt(panel, 8));
            }

            if (QVBoxLayout* panelLayout = qobject_cast<QVBoxLayout*>(panel->layout()))
            {
                panelLayout->setContentsMargins(scaledMargins(panel, 40, 24, 40, 22));
                panelLayout->setSpacing(scaledInt(panel, 10));
            }

            if (QLabel* title = panel->findChild<QLabel*>(QStringLiteral("homeLoginTitle")))
            {
                QFont font = title->font();
                font.setFamily(QStringLiteral("Microsoft YaHei UI"));
                font.setPixelSize(scaledInt(panel, 54));
                font.setWeight(QFont::Black);
                title->setFont(font);
                title->setMinimumHeight(scaledInt(panel, 64));
            }
            if (QFrame* scanLine = panel->findChild<QFrame*>(QStringLiteral("homeLoginScanLine")))
            {
                scanLine->setFixedHeight(scaledInt(panel, 3));
            }

            if (QGridLayout* formLayout = panel->findChild<QGridLayout*>(QStringLiteral("homeLoginFormLayout")))
            {
                formLayout->setContentsMargins(scaledMargins(panel, 0, 8, 0, 0));
                formLayout->setHorizontalSpacing(scaledInt(panel, 12));
                formLayout->setVerticalSpacing(scaledInt(panel, 10));
                formLayout->setColumnMinimumWidth(0, scaledInt(panel, 60));
            }

            const QStringList editNames = {
                QStringLiteral("homeLoginUserNameEdit"),
                QStringLiteral("homeLoginPasswordEdit")
            };
            for (const QString& name : editNames)
            {
                if (QLineEdit* edit = panel->findChild<QLineEdit*>(name))
                {
                    edit->setMinimumHeight(scaledInt(panel, 36));
                }
            }
            if (QLabel* status = panel->findChild<QLabel*>(QStringLiteral("homeLoginStatusLabel")))
            {
                status->setMinimumHeight(scaledInt(panel, 20));
            }

            const QStringList buttonNames = {
                QStringLiteral("homeLoginButton"),
                QStringLiteral("homeLoginResetButton"),
                QStringLiteral("homeLoginGuestButton")
            };
            for (const QString& name : buttonNames)
            {
                if (QPushButton* button = panel->findChild<QPushButton*>(name))
                {
                    button->setMinimumHeight(scaledInt(panel, 42));
                }
            }
            if (QHBoxLayout* buttonLayout = panel->findChild<QHBoxLayout*>(QStringLiteral("homeLoginButtonLayout")))
            {
                buttonLayout->setContentsMargins(scaledMargins(panel, 0, 4, 0, 0));
                buttonLayout->setSpacing(scaledInt(panel, 12));
            }

            if (QFrame* ambientBand = findChild<QFrame*>(QStringLiteral("homeLoginAmbientBand")))
            {
                ambientBand->setFixedHeight(scaledInt(this, 38));
                ambientBand->setMaximumWidth(scaledInt(this, 1280));
            }
            if (QFrame* telemetry = findChild<QFrame*>(QStringLiteral("homeLoginLowerTelemetry")))
            {
                telemetry->setFixedHeight(scaledInt(this, 26));
                telemetry->setMaximumWidth(scaledInt(this, 1180));
            }

            if (QVBoxLayout* centerLayout = qobject_cast<QVBoxLayout*>(layout()))
            {
                if (centerLayout->count() > 4)
                {
                    if (QSpacerItem* upperGap = centerLayout->itemAt(2)->spacerItem())
                    {
                        upperGap->changeSize(0, scaledInt(this, 20), QSizePolicy::Minimum, QSizePolicy::Fixed);
                    }
                    if (QSpacerItem* lowerGap = centerLayout->itemAt(4)->spacerItem())
                    {
                        lowerGap->changeSize(0, scaledInt(this, 22), QSizePolicy::Minimum, QSizePolicy::Fixed);
                    }
                }
                centerLayout->invalidate();
            }
        }
    };

    class HomeModulesPanel final : public QWidget
    {
    public:
        explicit HomeModulesPanel(QWidget* parent = nullptr)
            : QWidget(parent)
        {
        }

        void registerModuleButton(QToolButton* button)
        {
            if (!button)
            {
                return;
            }

            m_moduleButtons.append(button);
            updateModuleCardSizes();
        }

        void configureScaleAwareLayout(
            QVBoxLayout* stageLayout,
            QGridLayout* gridLayout,
            QHBoxLayout* utilityLayout,
            QSpacerItem* introGap,
            QSpacerItem* utilityGap,
            QSpacerItem* bottomGap)
        {
            m_stageLayout = stageLayout;
            m_gridLayout = gridLayout;
            m_utilityLayout = utilityLayout;
            m_introGap = introGap;
            m_utilityGap = utilityGap;
            m_bottomGap = bottomGap;
            refreshScaledMetrics();
        }

        void refreshScaledMetrics()
        {
            if (m_stageLayout)
            {
                m_stageLayout->setSpacing(scaledInt(this, 10));
            }
            if (m_gridLayout)
            {
                m_gridLayout->setHorizontalSpacing(scaledInt(this, kHomeGridHorizontalSpacing));
                m_gridLayout->setVerticalSpacing(scaledInt(this, kHomeGridVerticalSpacing));
            }
            if (m_utilityLayout)
            {
                m_utilityLayout->setSpacing(scaledInt(this, 16));
            }
            if (m_introGap)
            {
                m_introGap->changeSize(0, scaledInt(this, 10), QSizePolicy::Minimum, QSizePolicy::Fixed);
            }
            if (m_utilityGap)
            {
                m_utilityGap->changeSize(0, scaledInt(this, kHomeGridToUtilityGap), QSizePolicy::Minimum, QSizePolicy::Fixed);
            }
            if (m_bottomGap)
            {
                m_bottomGap->changeSize(0, scaledInt(this, kHomeUtilityBottomGap), QSizePolicy::Minimum, QSizePolicy::Fixed);
            }
            updateModuleCardSizes();
            if (m_stageLayout)
            {
                m_stageLayout->invalidate();
            }
        }

    protected:
        void resizeEvent(QResizeEvent* event) override
        {
            QWidget::resizeEvent(event);
            updateModuleCardSizes();
        }

    private:
        void updateModuleCardSizes()
        {
            if (m_moduleButtons.isEmpty())
            {
                return;
            }

            const QSize cardSize = homeModuleCardSize(this);
            for (QToolButton* button : m_moduleButtons)
            {
                if (!button)
                {
                    continue;
                }

                if (ResponsiveHomeModuleButton* moduleButton = dynamic_cast<ResponsiveHomeModuleButton*>(button))
                {
                    moduleButton->applyScaledCardSize(cardSize);
                }
            }
        }

        QList<QToolButton*> m_moduleButtons;
        QVBoxLayout* m_stageLayout = nullptr;
        QGridLayout* m_gridLayout = nullptr;
        QHBoxLayout* m_utilityLayout = nullptr;
        QSpacerItem* m_introGap = nullptr;
        QSpacerItem* m_utilityGap = nullptr;
        QSpacerItem* m_bottomGap = nullptr;
    };
    class SciFiHomePage final : public QWidget
    {
    public:
        explicit SciFiHomePage(QWidget* parent = nullptr)
            : QWidget(parent)
            , m_background(QPixmap(QStringLiteral(":/LTZK/Resources/home_login_background.png")))
            , m_whiteBackground(QPixmap(QStringLiteral(":/LTZK/Resources/home_white_background.png")))
            , m_blackBackground(QPixmap(QStringLiteral(":/LTZK/Resources/home_black_background.png")))
        {
            setAttribute(Qt::WA_OpaquePaintEvent, true);
        }

    protected:
        void paintEvent(QPaintEvent* event) override
        {
            Q_UNUSED(event);

            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

            const QString themeKey = property("platformTheme").toString();
            QLinearGradient base(rect().topLeft(), rect().bottomRight());
            if (themeKey == QStringLiteral("white"))
            {
                base.setColorAt(0.0, QColor(248, 250, 252));
                base.setColorAt(0.52, QColor(229, 236, 246));
                base.setColorAt(1.0, QColor(219, 228, 240));
            }
            else if (themeKey == QStringLiteral("black"))
            {
                base.setColorAt(0.0, QColor(5, 5, 5));
                base.setColorAt(0.55, QColor(16, 16, 16));
                base.setColorAt(1.0, QColor(0, 0, 0));
            }
            else if (themeKey == QStringLiteral("field-notes"))
            {
                base.setColorAt(0.0, QColor(244, 235, 215));
                base.setColorAt(0.55, QColor(233, 223, 200));
                base.setColorAt(1.0, QColor(216, 197, 157));
            }
            else if (themeKey == QStringLiteral("steel-warm-white"))
            {
                base.setColorAt(0.0, QColor(244, 242, 235));
                base.setColorAt(0.55, QColor(228, 231, 227));
                base.setColorAt(1.0, QColor(213, 222, 218));
            }
            else
            {
                base.setColorAt(0.0, QColor(8, 28, 46));
                base.setColorAt(0.50, QColor(12, 40, 64));
                base.setColorAt(1.0, QColor(5, 18, 31));
            }
            painter.fillRect(rect(), base);

            if (!m_background.isNull()
                && themeKey != QStringLiteral("field-notes")
                && themeKey != QStringLiteral("steel-warm-white"))
            {
                const QPixmap& background =
                    themeKey == QStringLiteral("white") ? m_whiteBackground :
                    themeKey == QStringLiteral("black") ? m_blackBackground :
                    m_background;
                QSize scaledSize = background.size();
                scaledSize.scale(size(), Qt::KeepAspectRatioByExpanding);
                const QPixmap scaledBackground =
                    background.scaled(
                        scaledSize,
                        Qt::KeepAspectRatioByExpanding,
                        Qt::SmoothTransformation);
                const QPoint offset(
                    (width() - scaledBackground.width()) / 2,
                    (height() - scaledBackground.height()) / 2);

                qreal backgroundOpacity = 0.68;
                if (themeKey == QStringLiteral("white"))
                {
                    backgroundOpacity = 0.52;
                }
                else if (themeKey == QStringLiteral("black"))
                {
                    backgroundOpacity = 0.58;
                }
                painter.setOpacity(backgroundOpacity);
                painter.drawPixmap(offset, scaledBackground);
                painter.setOpacity(1.0);
            }

            QLinearGradient readability(rect().topLeft(), rect().bottomRight());
            if (themeKey == QStringLiteral("white"))
            {
                readability.setColorAt(0.0, QColor(255, 255, 255, 65));
                readability.setColorAt(0.46, QColor(245, 249, 255, 82));
                readability.setColorAt(1.0, QColor(235, 242, 252, 98));
            }
            else if (themeKey == QStringLiteral("black"))
            {
                readability.setColorAt(0.0, QColor(0, 0, 0, 55));
                readability.setColorAt(0.46, QColor(0, 0, 0, 78));
                readability.setColorAt(1.0, QColor(0, 0, 0, 108));
            }
            else if (themeKey == QStringLiteral("field-notes"))
            {
                readability.setColorAt(0.0, QColor(255, 248, 236, 55));
                readability.setColorAt(0.46, QColor(255, 248, 236, 75));
                readability.setColorAt(1.0, QColor(238, 223, 194, 92));
            }
            else if (themeKey == QStringLiteral("steel-warm-white"))
            {
                readability.setColorAt(0.0, QColor(255, 255, 255, 55));
                readability.setColorAt(0.46, QColor(255, 255, 255, 72));
                readability.setColorAt(1.0, QColor(215, 224, 220, 92));
            }
            else
            {
                readability.setColorAt(0.0, QColor(4, 18, 31, 58));
                readability.setColorAt(0.46, QColor(3, 16, 28, 76));
                readability.setColorAt(1.0, QColor(2, 13, 24, 96));
            }
            painter.fillRect(rect(), readability);
        }

    private:
        const QPixmap m_background;
        const QPixmap m_whiteBackground;
        const QPixmap m_blackBackground;
    };

    class SciFiHomeHeader final : public QWidget
    {
    public:
        explicit SciFiHomeHeader(
            const QString& title,
            QWidget* parent = nullptr)
            : QWidget(parent)
            , m_title(title)
        {
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            refreshScaledMetrics();
        }

        void refreshScaledMetrics()
        {
            setMinimumHeight(scaledInt(this, 150));
            setMaximumHeight(scaledInt(this, 162));
            updateGeometry();
            update();
        }

        QSize sizeHint() const override
        {
            return scaledSize(this, 1200, 156);
        }

    protected:
        void paintEvent(QPaintEvent* event) override
        {
            Q_UNUSED(event);

            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing, true);

            const qreal width = this->width();
            const qreal height = this->height();
            const qreal uiScale = visualScaleForWidget(this);
            const auto scaledReal = [uiScale](qreal value)
            {
                return value * uiScale;
            };
            const QString themeKey = property("platformTheme").toString();
            const bool isWhiteTheme = themeKey == QStringLiteral("white");
            const bool isBlackTheme = themeKey == QStringLiteral("black");

            QColor headerBarLeftColor(6, 38, 91, 245);
            QColor headerBarQuarterColor(7, 52, 117, 245);
            QColor headerBarCenterColor(8, 61, 133, 248);
            QColor headerDividerColor(51, 175, 236, 115);
            QColor centerPanelShadowColor(0, 13, 45, 105);
            QColor centerPanelTopColor(18, 113, 188, 232);
            QColor centerPanelUpperColor(12, 96, 173, 220);
            QColor centerPanelLowerColor(8, 72, 143, 205);
            QColor centerPanelBottomColor(5, 46, 104, 188);
            QColor centerPanelBorderColor(87, 205, 255, 118);
            QColor glassEdgeColor(95, 211, 255, 8);
            QColor glassQuarterColor(95, 211, 255, 28);
            QColor glassCenterColor(125, 226, 255, 58);
            QColor topGlassStartColor(150, 230, 255, 52);
            QColor topGlassEndColor(150, 230, 255, 0);
            QColor titleShadowColor(0, 15, 45, 150);
            QColor titleTextColor("#F5FBFF");

            if (isWhiteTheme)
            {
                headerBarLeftColor = QColor(255, 255, 255, 245);
                headerBarQuarterColor = QColor(241, 245, 249, 246);
                headerBarCenterColor = QColor(226, 232, 240, 248);
                headerDividerColor = QColor(37, 99, 235, 110);
                centerPanelShadowColor = QColor(100, 116, 139, 70);
                centerPanelTopColor = QColor(255, 255, 255, 246);
                centerPanelUpperColor = QColor(248, 250, 252, 242);
                centerPanelLowerColor = QColor(239, 246, 255, 236);
                centerPanelBottomColor = QColor(219, 234, 254, 228);
                centerPanelBorderColor = QColor(37, 99, 235, 120);
                glassEdgeColor = QColor(37, 99, 235, 8);
                glassQuarterColor = QColor(37, 99, 235, 22);
                glassCenterColor = QColor(147, 197, 253, 48);
                topGlassStartColor = QColor(255, 255, 255, 82);
                topGlassEndColor = QColor(255, 255, 255, 0);
                titleShadowColor = QColor(255, 255, 255, 65);
                titleTextColor = QColor(15, 23, 42);
            }
            else if (isBlackTheme)
            {
                headerBarLeftColor = QColor(8, 8, 8, 245);
                headerBarQuarterColor = QColor(17, 17, 17, 246);
                headerBarCenterColor = QColor(28, 28, 28, 248);
                headerDividerColor = QColor(245, 158, 11, 125);
                centerPanelShadowColor = QColor(0, 0, 0, 150);
                centerPanelTopColor = QColor(38, 38, 38, 238);
                centerPanelUpperColor = QColor(30, 30, 30, 232);
                centerPanelLowerColor = QColor(20, 20, 20, 222);
                centerPanelBottomColor = QColor(10, 10, 10, 214);
                centerPanelBorderColor = QColor(245, 158, 11, 120);
                glassEdgeColor = QColor(245, 158, 11, 8);
                glassQuarterColor = QColor(245, 158, 11, 22);
                glassCenterColor = QColor(251, 191, 36, 42);
                topGlassStartColor = QColor(255, 255, 255, 22);
                topGlassEndColor = QColor(255, 255, 255, 0);
                titleShadowColor = QColor(0, 0, 0, 180);
                titleTextColor = QColor(250, 250, 250);
            }
            else if (themeKey == QStringLiteral("field-notes"))
            {
                headerBarLeftColor = QColor(61, 85, 64, 245);
                headerBarQuarterColor = QColor(82, 100, 67, 245);
                headerBarCenterColor = QColor(108, 121, 86, 248);
                headerDividerColor = QColor(184, 111, 79, 130);
                centerPanelShadowColor = QColor(62, 77, 61, 70);
                centerPanelTopColor = QColor(244, 235, 215, 246);
                centerPanelUpperColor = QColor(241, 229, 205, 242);
                centerPanelLowerColor = QColor(228, 223, 200, 236);
                centerPanelBottomColor = QColor(216, 197, 157, 228);
                centerPanelBorderColor = QColor(185, 185, 146, 150);
                glassEdgeColor = QColor(255, 255, 255, 8);
                glassQuarterColor = QColor(255, 255, 255, 22);
                glassCenterColor = QColor(255, 248, 236, 40);
                topGlassStartColor = QColor(255, 255, 255, 58);
                topGlassEndColor = QColor(255, 255, 255, 0);
                titleShadowColor = QColor(62, 77, 61, 55);
                titleTextColor = QColor(61, 85, 64);
            }
            else if (themeKey == QStringLiteral("steel-warm-white"))
            {
                headerBarLeftColor = QColor(52, 70, 75, 245);
                headerBarQuarterColor = QColor(67, 87, 91, 245);
                headerBarCenterColor = QColor(78, 98, 102, 248);
                headerDividerColor = QColor(102, 139, 140, 135);
                centerPanelShadowColor = QColor(45, 61, 64, 70);
                centerPanelTopColor = QColor(250, 249, 244, 246);
                centerPanelUpperColor = QColor(244, 242, 235, 242);
                centerPanelLowerColor = QColor(231, 236, 232, 236);
                centerPanelBottomColor = QColor(213, 222, 218, 228);
                centerPanelBorderColor = QColor(170, 182, 179, 150);
                glassEdgeColor = QColor(255, 255, 255, 8);
                glassQuarterColor = QColor(255, 255, 255, 20);
                glassCenterColor = QColor(198, 216, 210, 40);
                topGlassStartColor = QColor(255, 255, 255, 65);
                topGlassEndColor = QColor(255, 255, 255, 0);
                titleShadowColor = QColor(45, 61, 64, 50);
                titleTextColor = QColor(47, 89, 96);
            }

            const qreal bandHeight = qMin<qreal>(height, scaledReal(52.0));
            QLinearGradient barGradient(0, 0, width, 0);
            barGradient.setColorAt(0.00, headerBarLeftColor);
            barGradient.setColorAt(0.28, headerBarQuarterColor);
            barGradient.setColorAt(0.50, headerBarCenterColor);
            barGradient.setColorAt(0.72, headerBarQuarterColor);
            barGradient.setColorAt(1.00, headerBarLeftColor);
            painter.fillRect(QRectF(0, 0, width, bandHeight), barGradient);

            painter.setPen(QPen(headerDividerColor, scaledReal(1.0)));
            painter.drawLine(
                QPointF(0, bandHeight - scaledReal(1.0)),
                QPointF(width, bandHeight - scaledReal(1.0)));

            const qreal topWidth = qMin<qreal>(width, scaledReal(1220.0));
            const qreal bottomWidth = topWidth * 0.78;
            const qreal centerX = width / 2.0;
            const qreal topLeft = centerX - topWidth / 2.0;
            const qreal topRight = centerX + topWidth / 2.0;
            const qreal bottomLeft = centerX - bottomWidth / 2.0;
            const qreal bottomRight = centerX + bottomWidth / 2.0;
            const qreal bottomY = height - scaledReal(5.0);
            const qreal cornerRadius = scaledReal(28.0);

            QPainterPath centerPanel;
            centerPanel.moveTo(topLeft, 0);
            centerPanel.lineTo(topRight, 0);
            centerPanel.lineTo(bottomRight, bottomY - cornerRadius);
            centerPanel.quadTo(bottomRight, bottomY, bottomRight - cornerRadius, bottomY);
            centerPanel.lineTo(bottomLeft + cornerRadius, bottomY);
            centerPanel.quadTo(bottomLeft, bottomY, bottomLeft, bottomY - cornerRadius);
            centerPanel.lineTo(topLeft, 0);
            centerPanel.closeSubpath();

            painter.save();
            painter.translate(0, scaledReal(4.0));
            painter.setPen(Qt::NoPen);
            painter.setBrush(centerPanelShadowColor);
            painter.drawPath(centerPanel);
            painter.restore();

            QLinearGradient centerGradient(0, 0, 0, bottomY);
            centerGradient.setColorAt(0.00, centerPanelTopColor);
            centerGradient.setColorAt(0.35, centerPanelUpperColor);
            centerGradient.setColorAt(0.72, centerPanelLowerColor);
            centerGradient.setColorAt(1.00, centerPanelBottomColor);
            painter.setBrush(centerGradient);
            painter.setPen(QPen(centerPanelBorderColor, scaledReal(1.2)));
            painter.drawPath(centerPanel);

            painter.save();
            painter.setClipPath(centerPanel);
            QLinearGradient glassGradient(topLeft, 0, topRight, 0);
            glassGradient.setColorAt(0.00, glassEdgeColor);
            glassGradient.setColorAt(0.28, glassQuarterColor);
            glassGradient.setColorAt(0.50, glassCenterColor);
            glassGradient.setColorAt(0.72, glassQuarterColor);
            glassGradient.setColorAt(1.00, glassEdgeColor);
            painter.fillRect(centerPanel.boundingRect(), glassGradient);

            QLinearGradient topGlass(0, 0, 0, bottomY * 0.48);
            topGlass.setColorAt(0.00, topGlassStartColor);
            topGlass.setColorAt(1.00, topGlassEndColor);
            painter.fillRect(centerPanel.boundingRect(), topGlass);
            painter.restore();

            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(centerPanelBorderColor, scaledReal(1.1)));
            painter.drawPath(centerPanel);

            QFont titleFont(QStringLiteral("Microsoft YaHei UI"));
            titleFont.setPixelSize(scaledInt(this, 66));
            titleFont.setWeight(QFont::Bold);
            painter.setFont(titleFont);

            const QRectF titleRect(bottomLeft, scaledReal(18.0), bottomWidth, scaledReal(96.0));
            painter.setPen(titleShadowColor);
            painter.drawText(titleRect.translated(0, scaledReal(2.0)), Qt::AlignCenter, m_title);
            painter.setPen(titleTextColor);
            painter.drawText(titleRect, Qt::AlignCenter, m_title);
        }

    private:
        QString m_title;
    };
    QStringList pluginBindingConfigPathCandidates()
    {
        const QString appDir = QCoreApplication::applicationDirPath();
        return {
            appDir + QStringLiteral("/config/plugin_bindings.json"),
            appDir + QStringLiteral("/../config/plugin_bindings.json"),
            appDir + QStringLiteral("/../../config/plugin_bindings.json")
        };
    }

    QString pluginBindingConfigPath()
    {
        const QStringList candidates = pluginBindingConfigPathCandidates();
        for (const QString& path : candidates)
        {
            if (QFileInfo(path).exists())
            {
                return QFileInfo(path).absoluteFilePath();
            }
        }

        return candidates.isEmpty() ? QString() : candidates.first();
    }

    bool loadPluginBindings(PluginBindingConfig* config, QString* errorMessage)
    {
        return loadPluginBindingConfig(pluginBindingConfigPath(), config, errorMessage);
    }

    QStringList pluginDllPathCandidates(const QString& dllName)
    {
        const QString appDir = QCoreApplication::applicationDirPath();
        return {
            appDir + QStringLiteral("/plugins/") + dllName,
            appDir + QStringLiteral("/../plugins/") + dllName,
            appDir + QStringLiteral("/") + dllName
        };
    }

    QString resolvePluginDllPath(const QString& dllName)
    {
        const QStringList candidates = pluginDllPathCandidates(dllName);
        for (const QString& path : candidates)
        {
            if (QFileInfo(path).exists())
            {
                return QFileInfo(path).absoluteFilePath();
            }
        }

        return candidates.isEmpty() ? dllName : candidates.first();
    }

    QString pluginActionCacheKey(const QString& pluginPath)
    {
        QString normalizedPath = QFileInfo(pluginPath).canonicalFilePath();
        if (normalizedPath.isEmpty())
        {
            normalizedPath = QFileInfo(pluginPath).absoluteFilePath();
        }
        return QDir::cleanPath(normalizedPath).toLower();
    }

    bool buildProductCatalogApiRequest(
        const QVariantMap& source,
        QByteArray& operationUtf8,
        QVector<int32_t>& productIds,
        QByteArray& connectionJsonUtf8,
        LTZKProductCatalogOperationRequest& target)
    {
        const QString operation = source.value(QStringLiteral("operation")).toString().trimmed();
        if (operation.isEmpty())
        {
            return false;
        }

        operationUtf8 = operation.toUtf8();
        const QVariantList sourceProductIds = source.value(QStringLiteral("productIds")).toList();
        productIds.clear();
        productIds.reserve(sourceProductIds.size());
        for (const QVariant& productId : sourceProductIds)
        {
            productIds.append(static_cast<int32_t>(productId.toInt()));
        }

        connectionJsonUtf8.clear();
        const QVariantMap connection = source.value(QStringLiteral("connection")).toMap();
        if (!connection.isEmpty())
        {
            connectionJsonUtf8 = QJsonDocument(
                QJsonObject::fromVariantMap(connection)).toJson(QJsonDocument::Compact);
        }

        target = LTZKProductCatalogOperationRequest{};
        target.apiVersion = LTZK_PRODUCT_CATALOG_API_VERSION;
        target.operationUtf8 = operationUtf8.constData();
        target.directoryId = static_cast<int32_t>(source.value(QStringLiteral("directoryId")).toInt());
        target.productId = static_cast<int32_t>(source.value(QStringLiteral("productId")).toInt());
        target.productIds = productIds.isEmpty() ? nullptr : productIds.constData();
        target.productIdCount = static_cast<uint32_t>(productIds.size());
        target.connectionJsonUtf8 = connectionJsonUtf8.isEmpty()
            ? nullptr
            : connectionJsonUtf8.constData();
        return true;
    }

    QString productManagerAutoLoginPath()
    {
        const QString appDir = QCoreApplication::applicationDirPath();
        return QDir::cleanPath(
            QDir(appDir).absoluteFilePath(QStringLiteral("../config/user_login.ini")));
    }

    QString topBarButtonText(QString text)
    {
        text.remove(QChar('&'));
        text = text.trimmed();

        constexpr int kMaxCharsPerTopBarLine = 7;
        if (text.size() <= kMaxCharsPerTopBarLine)
        {
            return text;
        }

        QStringList lines;
        for (int index = 0; index < text.size(); index += kMaxCharsPerTopBarLine)
        {
            lines << text.mid(index, kMaxCharsPerTopBarLine);
        }

        return lines.join(QChar('\n'));
    }
    QIcon configuredIcon(const QString& iconPath)
    {
        if (iconPath.isEmpty())
        {
            return QIcon();
        }

        const QString normalized = QString(iconPath).replace(QStringLiteral("\\"), QStringLiteral("/"));
        if (normalized.startsWith(QStringLiteral(":/")))
        {
            return QIcon(normalized);
        }

        const QIcon resourceIcon(QStringLiteral(":/LTZK/") + normalized);
        if (!resourceIcon.isNull())
        {
            return resourceIcon;
        }

        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList candidates = {
            appDir + QStringLiteral("/") + normalized,
            appDir + QStringLiteral("/../") + normalized
        };
        for (const QString& path : candidates)
        {
            if (QFileInfo(path).exists())
            {
                return QIcon(path);
            }
        }

        return QIcon();
    }

    struct PluginIconPalette
    {
        QString primary;
        QString secondary;
        QString accent;
    };

    PluginIconPalette pluginIconPaletteForTheme(const QString& themeKey)
    {
        if (themeKey == QStringLiteral("white"))
        {
            return { QStringLiteral("#2563EB"), QStringLiteral("#0F766E"), QStringLiteral("#D97706") };
        }
        if (themeKey == QStringLiteral("black"))
        {
            return { QStringLiteral("#60A5FA"), QStringLiteral("#61DDAA"), QStringLiteral("#F59E0B") };
        }
        if (themeKey == QStringLiteral("field-notes"))
        {
            return { QStringLiteral("#C5E987"), QStringLiteral("#6FD8D0"), QStringLiteral("#FFC45E") };
        }
        if (themeKey == QStringLiteral("steel-warm-white"))
        {
            return { QStringLiteral("#4E9FA3"), QStringLiteral("#80B8AA"), QStringLiteral("#D38A63") };
        }

        return { QStringLiteral("#1BC9FF"), QStringLiteral("#61DDAA"), QStringLiteral("#FFB238") };
    }

    QIcon renderThemeAwareSvgIcon(const QString& iconPath, const QString& themeKey)
    {
        const QString normalized = QString(iconPath).replace(QStringLiteral("\\"), QStringLiteral("/"));
        const QString resourcePath = normalized.startsWith(QStringLiteral(":/"))
            ? normalized
            : QStringLiteral(":/LTZK/") + normalized;

        QFile svgFile(resourcePath);
        if (!svgFile.open(QIODevice::ReadOnly))
        {
            return QIcon();
        }

        QByteArray svgData = svgFile.readAll();
        const PluginIconPalette palette = pluginIconPaletteForTheme(themeKey);
        svgData.replace(QByteArrayLiteral("{{PRIMARY}}"), palette.primary.toUtf8());
        svgData.replace(QByteArrayLiteral("{{SECONDARY}}"), palette.secondary.toUtf8());
        svgData.replace(QByteArrayLiteral("{{ACCENT}}"), palette.accent.toUtf8());

        QSvgRenderer renderer;
        if (!renderer.load(svgData))
        {
            return QIcon();
        }

        QIcon icon;
        const QList<int> iconSizes = { 16, 20, 22, 24, 32, 34, 48 };
        for (const int size : iconSizes)
        {
            QPixmap pixmap(size, size);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            renderer.render(&painter);
            painter.end();
            icon.addPixmap(pixmap);
        }

        return icon;
    }

    QIcon themeAwareConfiguredIcon(const QString& iconPath, const QString& themeKey)
    {
        const QString normalized = QString(iconPath).replace(QStringLiteral("\\"), QStringLiteral("/"));
        if (!normalized.contains(QStringLiteral("icon_intelligent_")))
        {
            return configuredIcon(iconPath);
        }

        const QIcon themedIcon = renderThemeAwareSvgIcon(iconPath, themeKey);
        if (!themedIcon.isNull())
        {
            return themedIcon;
        }

        static QSet<QString> warnedIconFallbacks;
        const QString warningKey = normalized + QStringLiteral("|") + themeKey;
        if (!warnedIconFallbacks.contains(warningKey))
        {
            warnedIconFallbacks.insert(warningKey);
            qWarning().noquote() << QStringLiteral("Theme-aware plugin icon failed; using configured fallback: %1").arg(iconPath);
        }
        return configuredIcon(iconPath);
    }

    struct TopBarHoverPalette
    {
        QColor border;
        QColor topHighlight;
        QColor shadow;
        QColor rail;
    };

    TopBarHoverPalette topBarHoverPalette(const QString& themeKey)
    {
        if (themeKey == QStringLiteral("white"))
        {
            return {
                QColor(QStringLiteral("#2563eb")),
                QColor(QStringLiteral("#ffffff")),
                QColor(30, 58, 138, 175),
                QColor(QStringLiteral("#1d4ed8"))
            };
        }
        if (themeKey == QStringLiteral("black"))
        {
            return {
                QColor(QStringLiteral("#f59e0b")),
                QColor(QStringLiteral("#fff7ed")),
                QColor(120, 53, 15, 225),
                QColor(QStringLiteral("#b45309"))
            };
        }
        if (themeKey == QStringLiteral("field-notes"))
        {
            return {
                QColor(QStringLiteral("#B86F4F")),
                QColor(QStringLiteral("#FFF8EC")),
                QColor(61, 85, 64, 195),
                QColor(QStringLiteral("#9A5539"))
            };
        }
        if (themeKey == QStringLiteral("steel-warm-white"))
        {
            return {
                QColor(QStringLiteral("#668B8C")),
                QColor(QStringLiteral("#FFFFFF")),
                QColor(48, 75, 77, 190),
                QColor(QStringLiteral("#416264"))
            };
        }

        return {
            QColor(QStringLiteral("#50dfff")),
            QColor(QStringLiteral("#e4fbff")),
            QColor(6, 63, 97, 235),
            QColor(QStringLiteral("#2ac5ff"))
        };
    }

    class TopBarFloatingHoverEffect final : public QObject
    {
    public:
        explicit TopBarFloatingHoverEffect(QToolButton* button)
            : QObject(button)
            , m_button(button)
            , m_shadow(new QGraphicsDropShadowEffect(button))
            , m_blurAnimation(new QPropertyAnimation(m_shadow, "blurRadius", this))
            , m_offsetAnimation(new QPropertyAnimation(m_shadow, "offset", this))
        {
            setObjectName(QStringLiteral("ltzkTopBarFloatingHoverEffect"));
            m_button->setProperty("topBarRaised", false);
            m_button->setAttribute(Qt::WA_Hover, true);
            m_shadow->setBlurRadius(4.0);
            m_shadow->setOffset(0.0, 2.0);
            m_shadow->setColor(QColor(0, 0, 0, 45));
            m_button->setGraphicsEffect(m_shadow);
            m_button->installEventFilter(this);

            for (QPropertyAnimation* animation : { m_blurAnimation, m_offsetAnimation })
            {
                animation->setDuration(150);
                animation->setEasingCurve(QEasingCurve::OutCubic);
            }
        }

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (watched == m_button)
            {
                if (event->type() == QEvent::Enter)
                {
                    setRaised(true);
                }
                else if (event->type() == QEvent::Leave)
                {
                    setRaised(false);
                }
            }
            return QObject::eventFilter(watched, event);
        }

    private:
        void setRaised(bool raised)
        {
            if (!m_button || m_button->property("topBarRaised").toBool() == raised)
            {
                return;
            }

            m_button->setProperty("topBarRaised", raised);
            const TopBarHoverPalette palette = topBarHoverPalette(
                m_button->property("platformTheme").toString());
            m_shadow->setColor(raised ? palette.shadow : QColor(0, 0, 0, 45));

            m_blurAnimation->stop();
            m_blurAnimation->setStartValue(m_shadow->blurRadius());
            m_blurAnimation->setEndValue(raised ? 20.0 : 4.0);
            m_blurAnimation->start();

            m_offsetAnimation->stop();
            m_offsetAnimation->setStartValue(m_shadow->offset());
            m_offsetAnimation->setEndValue(QPointF(0.0, raised ? 7.0 : 2.0));
            m_offsetAnimation->start();

            if (m_button->style())
            {
                m_button->style()->unpolish(m_button);
                m_button->style()->polish(m_button);
            }
            m_button->update();
        }

        QPointer<QToolButton> m_button;
        QGraphicsDropShadowEffect* m_shadow = nullptr;
        QPropertyAnimation* m_blurAnimation = nullptr;
        QPropertyAnimation* m_offsetAnimation = nullptr;
    };

    void installTopBarFloatingHoverEffect(QToolButton* button)
    {
        if (!button || button->findChild<QObject*>(QStringLiteral("ltzkTopBarFloatingHoverEffect")))
        {
            return;
        }

        new TopBarFloatingHoverEffect(button);
    }

    void refreshTopBarFloatingHoverStyles(QWidget* root, const QString& themeKey)
    {
        if (!root)
        {
            return;
        }

        const TopBarHoverPalette palette = topBarHoverPalette(themeKey);
        for (QToolButton* button : root->findChildren<QToolButton*>())
        {
            if (!button->property("topBarTool").toBool())
            {
                continue;
            }

            button->setProperty("platformTheme", themeKey);
            if (auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(button->graphicsEffect()))
            {
                shadow->setColor(button->property("topBarRaised").toBool()
                    ? palette.shadow
                    : QColor(0, 0, 0, 45));
            }
            if (button->style())
            {
                button->style()->unpolish(button);
                button->style()->polish(button);
            }
            button->update();
        }
    }

    class ConfiguredMenuIconStyle final : public QProxyStyle
    {
    public:
        explicit ConfiguredMenuIconStyle(QStyle* baseStyle)
            : QProxyStyle(baseStyle)
        {
        }

        int pixelMetric(
            PixelMetric metric,
            const QStyleOption* option = nullptr,
            const QWidget* widget = nullptr) const override
        {
            if (metric == PM_SmallIconSize
                && qobject_cast<const QMenu*>(widget)
                && widget->property("ltzkConfiguredMenuIconSize").toInt() == 22)
            {
                return 22;
            }
            return QProxyStyle::pixelMetric(metric, option, widget);
        }
    };

    void installConfiguredMenuIconStyle()
    {
        static bool installed = false;
        if (installed || !QApplication::style())
        {
            return;
        }

        QStyle* baseStyle = QApplication::style();
        QApplication::setStyle(new ConfiguredMenuIconStyle(baseStyle));
        installed = true;
    }

    void setConfiguredMenuIconSize(QMenu* menu)
    {
        if (!menu)
        {
            return;
        }

        installConfiguredMenuIconStyle();
        menu->setProperty("ltzkConfiguredMenuIconSize", 22);
        menu->ensurePolished();
    }

    QList<QAction*> actionsAddedByPlugin(const QList<QAction*>& before, const QList<QAction*>& after)
    {
        QList<QAction*> added;
        for (QAction* action : after)
        {
            if (action && !before.contains(action) && !added.contains(action))
            {
                added.append(action);
            }
        }
        return added;
    }

    void applyConfiguredPluginActionIcons(
        const QVector<PluginActionIcon>& actionIcons,
        const QList<QAction*>& pluginActions,
        const QString& themeKey = QString())
    {
        for (const PluginActionIcon& actionIcon : actionIcons)
        {
            if (!actionIcon.enabled)
            {
                continue;
            }

            QAction* action = nullptr;
            for (QAction* candidate : pluginActions)
            {
                if (!candidate)
                {
                    continue;
                }

                bool matched = !actionIcon.actionKeywords.isEmpty();
                for (const QString& keyword : actionIcon.actionKeywords)
                {
                    if (!candidate->text().contains(keyword))
                    {
                        matched = false;
                        break;
                    }
                }

                if (matched)
                {
                    action = candidate;
                    break;
                }
            }

            if (!action && actionIcon.fallbackActionIndex >= 0 && actionIcon.fallbackActionIndex < pluginActions.size())
            {
                action = pluginActions[actionIcon.fallbackActionIndex];
            }

            const QIcon icon = themeAwareConfiguredIcon(actionIcon.icon, themeKey);
            if (action && !icon.isNull())
            {
                action->setIcon(icon);
            }
        }
    }

    QString messageLogLevelText(int level)
    {
        switch (level)
        {
        case Qgis::Info:
            return QStringLiteral("Info");
        case Qgis::Warning:
            return QStringLiteral("Warning");
        case Qgis::Critical:
            return QStringLiteral("Critical");
        case Qgis::Success:
            return QStringLiteral("Success");
        default:
            return QStringLiteral("Log");
        }
    }

    void logLtzKMessage(const QString& message, Qgis::MessageLevel level = Qgis::Info)
    {
        QgsMessageLog::logMessage(message, QStringLiteral("LTZK"), level, false);
    }

    void centerDialogOnParentOrScreen(QDialog& dialog, QWidget* parent)
    {
        QRect targetGeometry;
        if (parent && parent->isVisible() && !parent->frameGeometry().isEmpty())
        {
            targetGeometry = parent->frameGeometry();
        }
        else if (QScreen* screen = parent ? parent->screen() : QApplication::primaryScreen())
        {
            targetGeometry = screen->availableGeometry();
        }

        if (targetGeometry.isEmpty())
        {
            return;
        }

        dialog.move(targetGeometry.center() - dialog.rect().center());
    }

    void startupTrace(const QString& message)
    {
        // Detailed startup tracing is diagnostic and must not block normal UI work.
        static const bool enabled = qEnvironmentVariableIntValue("LTZK_STARTUP_TRACE") != 0;
        if (!enabled)
        {
            return;
        }
        QFile file(QCoreApplication::applicationDirPath() + QStringLiteral("/ltzk_startup_trace.log"));
        if (file.open(QIODevice::Append | QIODevice::Text))
        {
            QTextStream out(&file);
            out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << ' ' << message << '\n';
        }
    }

    QString localGansuLabelField(const QgsVectorLayer* layer)
    {
        if (!layer)
        {
            return QString();
        }

        const QStringList candidates = {
            QStringLiteral("\u4e61"),
            QStringLiteral("\u53bf"),
            QStringLiteral("\u5e02"),
            QStringLiteral("\u7701"),
            QStringLiteral("name"),
            QStringLiteral("NAME"),
            QStringLiteral("Name"),
            QStringLiteral("NAME_CHN"),
            QStringLiteral("XZQMC"),
            QStringLiteral("MC"),
            QStringLiteral("\u540d\u79f0"),
            QStringLiteral("\u5730\u540d")
        };

        for (const QString& candidate : candidates)
        {
            if (layer->fields().indexOf(candidate) >= 0)
            {
                return candidate;
            }
        }

        return QString();
    }

    void applyLocalGansuBasemapLabels(QgsVectorLayer* layer, bool isLightTheme)
    {
        if (!layer)
        {
            return;
        }

        const QString labelField = localGansuLabelField(layer);
        if (labelField.isEmpty())
        {
            layer->setLabelsEnabled(false);
            return;
        }

        QgsTextBufferSettings bufferSettings;
        bufferSettings.setEnabled(true);
        bufferSettings.setSize(1.2);
        bufferSettings.setColor(isLightTheme ? QColor(255, 255, 255, 225) : QColor(2, 10, 20, 220));

        QgsTextFormat textFormat;
        QFont labelFont(QStringLiteral("Microsoft YaHei UI"));
        labelFont.setBold(true);
        textFormat.setFont(labelFont);
        textFormat.setSize(9.5);
        textFormat.setColor(isLightTheme ? QColor(15, 23, 42, 235) : QColor(255, 255, 255, 235));
        textFormat.setBuffer(bufferSettings);

        QgsPalLayerSettings labelSettings;
        labelSettings.fieldName = labelField;
        labelSettings.isExpression = false;
        labelSettings.placement = Qgis::LabelPlacement::OverPoint;
        labelSettings.setFormat(textFormat);

        layer->setLabeling(new QgsVectorLayerSimpleLabeling(labelSettings));
        layer->setLabelsEnabled(true);
    }
    void appendUniquePath(QStringList& paths, const QString& path)
    {
        const QString cleanPath = QDir::cleanPath(path);
        if (cleanPath.isEmpty())
        {
            return;
        }

        if (!paths.contains(cleanPath, Qt::CaseInsensitive))
        {
            paths << cleanPath;
        }
    }

    QStringList localShpBasemapDirectories()
    {
        const QStringList folderNames = {
            QStringLiteral("local_shp")
        };

        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList baseDirs = {
            appDir,
            QDir(appDir).absoluteFilePath(QStringLiteral("..")),
            QDir(appDir).absoluteFilePath(QStringLiteral("../..")),
            QDir::currentPath()
        };

        QStringList directories;
        for (const QString& baseDir : baseDirs)
        {
            const QDir dir(baseDir);
            for (const QString& folderName : folderNames)
            {
                const QString candidate = dir.absoluteFilePath(folderName);
                if (QFileInfo(candidate).isDir())
                {
                    appendUniquePath(directories, QFileInfo(candidate).absoluteFilePath());
                }
            }
        }

        return directories;
    }

    QString localGansuShpBasemapPath()
    {
        QStringList shpFiles;
        const QStringList searchRoots = localShpBasemapDirectories();
        for (const QString& root : searchRoots)
        {
            QDirIterator it(root, QStringList() << QStringLiteral("*.shp"), QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext())
            {
                shpFiles << QFileInfo(it.next()).absoluteFilePath();
            }
        }

        shpFiles.sort(Qt::CaseInsensitive);
        return shpFiles.isEmpty() ? QString() : shpFiles.first();
    }
    double lonToWebMercatorX(double lon)
    {
        return lon * 20037508.342789244 / 180.0;
    }

    double latToWebMercatorY(double lat)
    {
        const double clampedLat = qBound(-85.05112878, lat, 85.05112878);
        return qLn(qTan(M_PI / 4.0 + qDegreesToRadians(clampedLat) / 2.0)) * 6378137.0;
    }

    QgsRectangle gansuWebMercatorExtent()
    {
        return QgsRectangle(
            lonToWebMercatorX(103.25),
            latToWebMercatorY(32.1),
            lonToWebMercatorX(109.7),
            latToWebMercatorY(42.95));
    }

    QgsPointXY gansuWebMercatorCenter()
    {
        return QgsPointXY(
            lonToWebMercatorX(103.8263),
            latToWebMercatorY(36.0594));
    }

    bool isSystemBasemapLayer(QgsMapLayer* layer)
    {
        return layer && layer->customProperty(QStringLiteral("ltzk/systemBasemap")).toBool();
    }

    QString canonicalOrAbsolutePath(const QString& path)
    {
        const QFileInfo info(path);
        const QString canonicalPath = info.canonicalFilePath();
        return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
    }

    QString localPathFromLayerSource(const QString& source)
    {
        QString path = source;
        const int pipeIndex = path.indexOf(QLatin1Char('|'));
        if (pipeIndex >= 0)
        {
            path = path.left(pipeIndex);
        }

        if (path.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive))
        {
            const QString localFile = QUrl(path).toLocalFile();
            if (!localFile.isEmpty())
            {
                path = localFile;
            }
        }

        return QFileInfo(path).absoluteFilePath();
    }

    QString normalizedQualityResultUiText(const QString& text)
    {
        QString normalized = text;
        normalized.remove(QLatin1Char('&'));
        return normalized.simplified().toLower();
    }

    bool isQualityResultOutputLabel(const QString& labelText)
    {
        const QString normalized = normalizedQualityResultUiText(labelText);
        return normalized.contains(QStringLiteral("结果输出目录"))
            || normalized.contains(QStringLiteral("输出结果路径"))
            || normalized.contains(QStringLiteral("结果输出路径"))
            || normalized.contains(QStringLiteral("输出路径"))
            || normalized.contains(QStringLiteral("输出目录"))
            || normalized.contains(QStringLiteral("result output path"))
            || normalized.contains(QStringLiteral("result output directory"))
            || normalized.contains(QStringLiteral("output path"))
            || normalized.contains(QStringLiteral("output directory"));
    }

    bool isQualityResultOutputLabelForLineEdit(QLabel* label, QLineEdit* lineEdit)
    {
        if (!label || !lineEdit || !isQualityResultOutputLabel(label->text()))
        {
            return false;
        }

        if (label->buddy() == lineEdit)
        {
            return true;
        }

        const QPoint labelCenter = label->mapToGlobal(label->rect().center());
        const QPoint lineEditCenter = lineEdit->mapToGlobal(lineEdit->rect().center());
        const int rowTolerance = qMax(label->height(), lineEdit->height()) + 8;
        return qAbs(labelCenter.y() - lineEditCenter.y()) <= rowTolerance
            && labelCenter.x() <= lineEditCenter.x();
    }

    bool isQualityResultOutputPathControl(QLineEdit* lineEdit, QWidget* pluginWindow)
    {
        if (!lineEdit || !pluginWindow || !lineEdit->isVisible())
        {
            return false;
        }

        const QString controlName = lineEdit->objectName().toLower();
        if (controlName.contains(QStringLiteral("output")))
        {
            return true;
        }

        for (QLabel* label : pluginWindow->findChildren<QLabel*>())
        {
            if (isQualityResultOutputLabelForLineEdit(label, lineEdit))
            {
                return true;
            }
        }

        return false;
    }

    bool isWaterTaskIntermediate(const QString& path)
    {
        const QStringList parts = QDir::fromNativeSeparators(path).split(
            QLatin1Char('/'), Qt::SkipEmptyParts);
        for (const QString& part : parts) {
            if (part.startsWith(QStringLiteral(".water_task_results_"), Qt::CaseInsensitive))
                return true;
        }
        return false;
    }

    bool isQualityResultShpPath(const QString& shpPath)
    {
        const QFileInfo info(shpPath);
        return !isWaterTaskIntermediate(shpPath) && info.isFile()
            && info.suffix().compare(QStringLiteral("shp"), Qt::CaseInsensitive) == 0;
    }

    bool isDataCatalogVisibleModuleTitle(const QString& moduleTitle)
    {
        return moduleTitle == QStringLiteral("地图成果管理")
            || moduleTitle == QStringLiteral("地图智能制图");
    }

    bool isResultModuleTitle(const QString& moduleTitle)
    {
        return moduleTitle == QStringLiteral("地图成果质检")
            || moduleTitle == QStringLiteral("地图智能制图");
    }

    QString businessModuleMenuTitle(const QString& moduleTitle)
    {
        return moduleTitle == QStringLiteral("地图智能制图")
            ? QStringLiteral("智能制图")
            : moduleTitle;
    }

    QString businessModuleDisplayTitle(const QString& menuTitle)
    {
        return menuTitle == QStringLiteral("智能制图")
            ? QStringLiteral("地图智能制图")
            : menuTitle;
    }

    constexpr int kMaxQualityResultFeatureRowsPerLayer = 1000;
    constexpr int kMaxQualityResultScanFilesPerRoot = 2000;
    constexpr int kMaxQualityErrorResultSearchAncestorLevels = 4;
    constexpr int kMaxQualityErrorResultSearchDepth = 6;

    void appendUniqueDir(QStringList& dirs, const QString& path)
    {
        const QFileInfo info(path);
        if (!info.exists() || !info.isDir())
        {
            return;
        }

        const QString normalized = canonicalOrAbsolutePath(path);
        for (const QString& existing : std::as_const(dirs))
        {
            if (existing.compare(normalized, Qt::CaseInsensitive) == 0)
            {
                return;
            }
        }
        dirs << normalized;
    }

    QString qualityErrorResultFolderName()
    {
        return QStringLiteral("\u8D28\u68C0\u9519\u8BEF\u7ED3\u679C");
    }

    void collectDirectoriesNamed(
        const QString& basePath,
        const QString& targetName,
        int depth,
        int maxDepth,
        QStringList& result)
    {
        const QFileInfo baseInfo(basePath);
        if (!baseInfo.isDir())
        {
            return;
        }

        if (baseInfo.fileName().compare(targetName, Qt::CaseInsensitive) == 0)
        {
            appendUniqueDir(result, baseInfo.absoluteFilePath());
            return;
        }

        if (depth >= maxDepth)
        {
            return;
        }

        const QDir directory(baseInfo.absoluteFilePath());
        const QFileInfoList children = directory.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable | QDir::NoSymLinks,
            QDir::Name);
        for (const QFileInfo& child : children)
        {
            collectDirectoriesNamed(
                child.absoluteFilePath(), targetName, depth + 1, maxDepth, result);
        }
    }

    QString commonDirectoryAncestor(const QString& firstPath, const QString& secondPath)
    {
        QString current = canonicalOrAbsolutePath(firstPath);
        const QString target = canonicalOrAbsolutePath(secondPath);
        if (current.isEmpty() || target.isEmpty())
        {
            return QString();
        }

        while (!current.isEmpty())
        {
            const QString normalizedCurrent = QDir::fromNativeSeparators(current);
            const QString normalizedTarget = QDir::fromNativeSeparators(target);
            if (normalizedTarget.compare(normalizedCurrent, Qt::CaseInsensitive) == 0
                || normalizedTarget.startsWith(
                    normalizedCurrent + QLatin1Char('/'), Qt::CaseInsensitive))
            {
                return normalizedCurrent;
            }

            const QString parent = canonicalOrAbsolutePath(
                QFileInfo(current).dir().absolutePath());
            if (parent.isEmpty() || parent.compare(current, Qt::CaseInsensitive) == 0)
            {
                break;
            }
            current = parent;
        }

        return QString();
    }

    // Theme QSS resources: Resources/styles/ltzk_white.qss, Resources/styles/ltzk_black.qss
    QString platformThemeQssFileName(const QString& themeKey)
    {
        if (themeKey == QStringLiteral("white"))
        {
            return QStringLiteral("ltzk_white.qss");
        }
        if (themeKey == QStringLiteral("black"))
        {
            return QStringLiteral("ltzk_black.qss");
        }
        if (themeKey == QStringLiteral("field-notes"))
        {
            return QStringLiteral("ltzk_field_notes.qss");
        }
        if (themeKey == QStringLiteral("steel-warm-white"))
        {
            return QStringLiteral("ltzk_steel_warm_white.qss");
        }
        return QStringLiteral("ltzk_sci_fi.qss");
    }

    QString platformThemeQssResourcePath(const QString& themeKey)
    {
        return QStringLiteral(":/LTZK/Resources/styles/") + platformThemeQssFileName(themeKey);
    }

    QString platformThemeQssPath(const QString& themeKey)
    {
        return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("Resources/styles/") + platformThemeQssFileName(themeKey));
    }

    QString fallbackPlatformThemeQss(const QString& themeKey)
    {
        QFile resourceFile(platformThemeQssResourcePath(themeKey));
        if (resourceFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            const QString qss = QString::fromUtf8(resourceFile.readAll());
            if (!qss.trimmed().isEmpty())
            {
                return qss;
            }
        }

        QFile sciFiResourceFile(QStringLiteral(":/LTZK/Resources/styles/ltzk_sci_fi.qss"));
        if (sciFiResourceFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            const QString qss = QString::fromUtf8(sciFiResourceFile.readAll());
            if (!qss.trimmed().isEmpty())
            {
                return qss;
            }
        }

        return QStringLiteral(
            "QDialog, QMessageBox { background: #071729; color: #e9fbff; }"
            "QWidget { color: #e9fbff; selection-background-color: #ffb238; selection-color: #06111f; }"
            "QMainWindow, #sciFiWorkspacePage { background: #03101d; }"
            "QPushButton, QToolButton { color: #f8feff; background: #0a345d; border: 1px solid #1bc9ff; }"
            "QLineEdit, QTextEdit, QPlainTextEdit { color: #f6fdff; background: #06111d; border: 1px solid #2ea8df; }");
    }

    QString loadPlatformThemeQss(const QString& themeKey)
    {
        QFile qssFile(platformThemeQssPath(themeKey));
        if (qssFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            const QString qss = QString::fromUtf8(qssFile.readAll());
            if (!qss.trimmed().isEmpty())
            {
                return qss;
            }
        }

        return fallbackPlatformThemeQss(themeKey);
    }

    QString platformSciFiQssPath()
    {
        return platformThemeQssPath(QStringLiteral("sci-fi"));
    }

    QString fallbackPlatformSciFiQss()
    {
        return fallbackPlatformThemeQss(QStringLiteral("sci-fi"));
    }

    QString loadPlatformSciFiQss()
    {
        return loadPlatformThemeQss(QStringLiteral("sci-fi"));
    }

    QString sciFiApplicationStyleSheet()
    {
        return loadPlatformSciFiQss();
    }

    QAction* findConfiguredPluginAction(const QList<QAction*>& pluginActions, const QStringList& keywords, int fallbackIndex)
    {
        for (QAction* action : pluginActions)
        {
            if (!action)
            {
                continue;
            }

            const QString actionText = action->text();
            bool matched = true;
            for (const QString& keyword : keywords)
            {
                if (!actionText.contains(keyword))
                {
                    matched = false;
                    break;
                }
            }

            if (matched)
            {
                return action;
            }
        }

        if (fallbackIndex >= 0 && fallbackIndex < pluginActions.size())
        {
            return pluginActions[fallbackIndex];
        }

        return nullptr;
    }

    QAction* findProductCatalogOperationAction(const QList<QAction*>& pluginActions)
    {
        const QString expectedObjectName = QStringLiteral("mActionDataCatalogOperation");
        const QString expectedText = QStringLiteral("成果目录操作");
        for (QAction* action : pluginActions)
        {
            if (!action || action->isSeparator())
            {
                continue;
            }

            if (action->objectName() == expectedObjectName
                || action->text().trimmed() == expectedText)
            {
                return action;
            }
        }

        return nullptr;
    }

    QAction* findProductDataImportAction(const QList<QAction*>& actions)
    {
        for (QAction* action : actions)
        {
            if (action && !action->isSeparator() &&
                action->objectName() == QStringLiteral("mActionDataImport"))
            {
                return action;
            }
        }
        for (QAction* action : actions)
        {
            if (!action || action->isSeparator() ||
                action->objectName().startsWith(QStringLiteral("pluginFeature_")))
            {
                continue;
            }
            QString actionText = action->text();
            actionText.remove(QLatin1Char('&'));
            if (actionText.trimmed() == QStringLiteral("数据导入"))
            {
                return action;
            }
        }
        return nullptr;
    }

    QList<QAction*> configuredPluginActionsFromCandidates(const QVector<PluginActionIcon>& actionIcons, const QList<QAction*>& candidates)
    {
        QList<QAction*> executableActions;
        for (QAction* action : candidates)
        {
            if (!action || action->isSeparator())
            {
                continue;
            }

            if (action->objectName().startsWith(QStringLiteral("pluginFeature_")))
            {
                continue;
            }

            if (!executableActions.contains(action))
            {
                executableActions.append(action);
            }
        }

        QList<QAction*> configuredActions;
        for (const PluginActionIcon& actionIcon : actionIcons)
        {
            if (!actionIcon.enabled)
            {
                continue;
            }

            QAction* action = nullptr;
            if (!actionIcon.actionKeywords.isEmpty())
            {
                action = findConfiguredPluginAction(
                    executableActions,
                    actionIcon.actionKeywords,
                    actionIcon.fallbackActionIndex);
            }

            if (!action && actionIcon.fallbackActionIndex >= 0 && actionIcon.fallbackActionIndex < executableActions.size())
            {
                action = executableActions[actionIcon.fallbackActionIndex];
            }

            if (action && !configuredActions.contains(action))
            {
                configuredActions.append(action);
            }
        }

        return configuredActions;
    }

    void collectPluginExecutableActions(QMenu* menu, QList<QAction*>& actions)
    {
        if (!menu)
        {
            return;
        }

        for (QAction* action : menu->actions())
        {
            if (!action || action->isSeparator())
            {
                continue;
            }

            if (QMenu* childMenu = action->menu())
            {
                collectPluginExecutableActions(childMenu, actions);
                continue;
            }

            if (!actions.contains(action))
            {
                actions.append(action);
            }
        }
    }

    QList<QAction*> collectPluginExecutableActions(QMenu* menu, QToolBar* toolbar)
    {
        QList<QAction*> actions;
        collectPluginExecutableActions(menu, actions);

        if (toolbar)
        {
            for (QAction* action : toolbar->actions())
            {
                if (action && !action->isSeparator() && !actions.contains(action))
                {
                    actions.append(action);
                }
            }
        }

        return actions;
    }

    QList<QAction*> collectPluginExecutableActions(QMenuBar* menuBar, QMenu* pluginMenu, QToolBar* toolbar)
    {
        QList<QAction*> actions;
        if (menuBar)
        {
            for (QAction* menuAction : menuBar->actions())
            {
                QMenu* topLevelMenu = menuAction ? menuAction->menu() : nullptr;
                if (topLevelMenu)
                {
                    collectPluginExecutableActions(topLevelMenu, actions);
                }
            }
        }

        collectPluginExecutableActions(pluginMenu, actions);
        if (toolbar)
        {
            for (QAction* action : toolbar->actions())
            {
                if (action && !action->isSeparator() && !actions.contains(action))
                {
                    actions.append(action);
                }
            }
        }

        return actions;
    }

    QMenu* ensureConfiguredMenuPath(LTZK* app, const QStringList& menuPath)
    {
        if (!app || menuPath.isEmpty())
        {
            return nullptr;
        }

        QMenu* currentMenu = app->ensureMenu(menuPath.first());
        if (currentMenu)
        {
            setConfiguredMenuIconSize(currentMenu);
        }
        for (int i = 1; i < menuPath.size() && currentMenu; ++i)
        {
            QMenu* nextMenu = nullptr;
            for (QAction* action : currentMenu->actions())
            {
                if (action && action->menu() && action->menu()->title() == menuPath[i])
                {
                    nextMenu = action->menu();
                    break;
                }
            }

            if (!nextMenu)
            {
                nextMenu = currentMenu->addMenu(menuPath[i]);
            }
            setConfiguredMenuIconSize(nextMenu);
            currentMenu = nextMenu;
        }

        return currentMenu;
    }
}

LTZK::LTZK(QWidget* parent)
    : QMainWindow(parent)
{
    // 先初始化 UI，后续 ui.xxx 控件才能安全接入 initLayerTreeView()。
    ui.setupUi(this);
    installConfiguredMenuIconStyle();
    qApp->installEventFilter(this);
    startupTrace(QStringLiteral("LTZK ctor begin"));
    loadSavedPlatformTheme();
    m_curMapLayer = nullptr;
    // 初始化当前图层指针。
    m_zoomInTool = nullptr;
    m_zoomOutTool = nullptr;
    // 初始化地图工具指针。
    m_panTool = nullptr;
    // 创建消息栏。
    m_layerTreeView = nullptr;
    // 创建地图画布。
    m_layerTreeCanvasBridge = nullptr;
    // 按保存的平台风格初始化地图画布背景。
    m_mapCanvas = new QgsMapCanvas(this);
    connect(m_mapCanvas, &QgsMapCanvas::contextMenuAboutToShow, this, [this](QMenu* menu, QgsMapMouseEvent* event) {
        Q_UNUSED(event);
        if (!menu || !m_mapCanvas)
        {
            return;
        }

        menu->clear();
        const QList<QgsMapLayer*> canvasLayers = m_mapCanvas->layers();
        if (!visibleLayersContainShp(canvasLayers, true))
        {
            return;
        }

        QAction* actionChooseCanvasColor = menu->addAction(QStringLiteral("选择画布颜色"));
        connect(actionChooseCanvasColor, &QAction::triggered, this, &LTZK::chooseMapCanvasColor);
        });
    applyWorkspaceShpCanvasBackground(false);
    m_mapCanvas->setCachingEnabled(true);
    m_mapCanvas->setParallelRenderingEnabled(true);
    m_mapCanvas->setPreviewJobsEnabled(false);
    m_mapCanvas->setMapUpdateInterval(250);

    // 初始化图层树视图。
    initLayerTreeView(); // 接入画布与图层树。

    // 初始化缩放与平移工具。
    // 默认地图交互工具稍后设置为平移。
    startupTrace(
        QStringLiteral(
            "before QgsMapToolZoom in"));

    m_zoomInTool =
        new QgsMapToolZoom(
            m_mapCanvas,
            false);

    startupTrace(
        QStringLiteral(
            "after QgsMapToolZoom in"));

    startupTrace(
        QStringLiteral(
            "before QgsMapToolZoom out"));

    m_zoomOutTool =
        new QgsMapToolZoom(
            m_mapCanvas,
            true);

    startupTrace(
        QStringLiteral(
            "after QgsMapToolZoom out"));

    startupTrace(
        QStringLiteral(
            "before QgsMapToolPan"));

    m_panTool =
        new QgsMapToolPan(
            m_mapCanvas);

    startupTrace(
        QStringLiteral(
            "after QgsMapToolPan"));

    startupTrace(
        QStringLiteral(
            "before setMapTool"));

    m_mapCanvas->setMapTool(
        m_panTool);

    startupTrace(
        QStringLiteral(
            "after setMapTool"));

    startupTrace(
        QStringLiteral(
            "before initSciFiWorkspace"));

    initSciFiWorkspace();
    initWorkspaceToolbars();

    startupTrace(
        QStringLiteral(
            "after initSciFiWorkspace"));
    // 先预加载本地 SHP 底图，再执行其它延迟刷新，避免进入页面后底图才慢慢出现。
    QTimer::singleShot(0, this, &LTZK::loadLocalGansuShpBasemap);
    QTimer::singleShot(0, this, &LTZK::setDefaultPanMapTool);
    connect(QgsProject::instance(), &QgsProject::layersAdded, this, [this](const QList<QgsMapLayer*>& addedLayers) {
        const bool monitorActive = !m_qualityResultMonitoringModuleTitle.isEmpty()
            && m_qualityResultMonitoringModuleTitle == m_currentBusinessModuleTitle
            && isResultModuleTitle(m_currentBusinessModuleTitle);
        if (!monitorActive)
        {
            return;
        }

        int candidateCount = 0;
        for (QgsMapLayer* addedLayer : addedLayers)
        {
            QgsVectorLayer* vectorLayer = qobject_cast<QgsVectorLayer*>(addedLayer);
            if (!vectorLayer || !vectorLayer->isValid()
                || vectorLayer->customProperty(QStringLiteral("ltzk/qualityResultShp")).toBool())
            {
                continue;
            }

            const QString sourcePath = localPathFromLayerSource(vectorLayer->source());
            if (QFileInfo(sourcePath).suffix().compare(QStringLiteral("shp"), Qt::CaseInsensitive) == 0
                && isQualityResultLayerInSelectedOutputDirs(sourcePath))
            {
                ++candidateCount;
            }
        }

        if (candidateCount > 0)
        {
            statusBar()->showMessage(
                QStringLiteral("检测到 %1 个检查结果 shp，等待检查任务完成后登记。").arg(candidateCount),
                5000);
        }
        });
    m_qualityResultScanTimer = new QTimer(this);
    m_qualityResultScanTimer->setSingleShot(true);
    m_qualityResultScanTimer->setInterval(500);
    connect(m_qualityResultScanTimer, &QTimer::timeout, this, [this]() {
        if (!m_qualityResultScanReady)
        {
            return;
        }

        refreshQualityResultOutputDirectories();
        const int loadedCount = syncQualityResultShpLayers();
        const int regroupedCount = regroupQualityResultLayersBySourcePath();
        removeMapQualitySeriesIntermediateResultNodes();
        if (loadedCount > 0 || regroupedCount > 0)
        {
            statusBar()->showMessage(QStringLiteral("\u5DF2\u540C\u6B65 %1 \u4E2A\u68C0\u67E5\u7ED3\u679C shp \u56FE\u5C42\u3002")
                .arg(loadedCount + regroupedCount), 5000);
        }

        const bool monitoringActive = !m_qualityResultMonitoringModuleTitle.isEmpty()
            && m_qualityResultMonitoringModuleTitle == m_currentBusinessModuleTitle
            && isResultModuleTitle(m_currentBusinessModuleTitle);
        if (m_qualityResultPostActionScanActive
            && m_qualityResultScanReady
            && monitoringActive
            && m_qualityResultPostActionScanAttempts < kMaxQualityResultPostActionScanAttempts)
        {
            ++m_qualityResultPostActionScanAttempts;
            m_qualityResultScanTimer->start(kQualityResultPostActionScanIntervalMs);
        }
        else
        {
            m_qualityResultPostActionScanActive = false;
            m_qualityResultScanReady = false;
            m_qualityResultPostActionScanAttempts = 0;
        }
        });
    m_qualityResultOutputWatcher = new QFileSystemWatcher(this);
    connect(m_qualityResultOutputWatcher, &QFileSystemWatcher::directoryChanged,
        this, &LTZK::handleQualityResultDirectoryChanged);

    //QgisApp* app = QgisApp::instance();
    //if (app == nullptr)
    //{
    //	app = new QgisApp();
    //	app->setObjectName(QStringLiteral("QgisApp"));
    //}
    setWindowTitle(QStringLiteral("陇图智控平台"));

    setAcceptDrops(true);


    m_CurrentCoordinage = new QLabel(QStringLiteral("\u7ECF\u5EA6\uFF1A--  \u7EAC\u5EA6\uFF1A--"), this);
    m_CurrentCoordinage->setObjectName(QStringLiteral("applicationGeographicCoordinate"));
    m_CurrentCoordinage->setMinimumWidth(scaledInt(this, 260));
    m_CurrentCoordinage->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    this->statusBar()->addPermanentWidget(m_CurrentCoordinage); // coordinate display on the right side of the status bar

    m_messageBar = new QgsMessageBar(this);
    m_messageBar->setObjectName(QStringLiteral("pluginMessageBar"));
    m_messageBar->hide();
    initMessageLogDock();

    connect(m_mapCanvas, &QgsMapCanvas::xyCoordinates, this, &LTZK::mouseCoordinateChanged);



    // 连接属性表和字段管理器动作。
    connect(ui.actionOpenAttrTable, &QAction::triggered, this, &LTZK::on_actionOpenAttrTable_triggered);

    connect(ui.actionOpenFields, &QAction::triggered, this, &LTZK::on_actionOpenFields_triggered);

    m_pluginMenu = new QMenu(QStringLiteral("\u63D2\u4EF6"), this);
    m_pluginMenu->setObjectName(QStringLiteral("pluginMenu"));

    QMenu* mapGeneralizationMenu = menuBar()->addMenu(QStringLiteral("\u667A\u80FD\u5236\u56FE"));
    mapGeneralizationMenu->setObjectName(QStringLiteral("menuMapGeneralization"));

    QMenu* mapQualityMenu = menuBar()->addMenu(QStringLiteral("\u5730\u56FE\u6210\u679C\u8D28\u68C0"));
    mapQualityMenu->setObjectName(QStringLiteral("menuMapQuality"));

    QMenu* mapAchievementManagementMenu = menuBar()->addMenu(QStringLiteral("\u5730\u56FE\u6210\u679C\u7BA1\u7406"));
    mapAchievementManagementMenu->setObjectName(QStringLiteral("menuMapAchievementManagement"));

    QMenu* mapServiceManagementMenu = menuBar()->addMenu(QStringLiteral("\u5730\u56FE\u670D\u52A1\u7BA1\u7406"));
    mapServiceManagementMenu->setObjectName(QStringLiteral("menuMapServiceManagement"));

    m_pluginToolBar = addToolBar(QStringLiteral("\u63D2\u4EF6"));
    m_pluginToolBar->setObjectName(QStringLiteral("pluginToolBar"));
    // 插件管理器负责按配置加载业务插件并绑定功能菜单。
    m_pluginToolBar->hide();
    m_pluginManager = new LTZKPluginManager(this);
    startupTrace(QStringLiteral("before plugin manager"));

    QString pluginConfigError;
    if (loadPluginBindings(&m_pluginBindingConfig, &pluginConfigError))
    {
        bindPluginFeatureActions(m_pluginBindingConfig);
    }
    else
    {
        logLtzKMessage(QStringLiteral("Plugin binding config load failed: %1").arg(pluginConfigError), Qgis::Warning);
        QMessageBox::warning(this, QStringLiteral("\u63D2\u4EF6\u914D\u7F6E\u52A0\u8F7D\u5931\u8D25"), pluginConfigError);
    }
    refreshModuleActionsFromMenus();
    publishCurrentUserSession();
    refreshAuthUiState();
    QTimer::singleShot(0, this, &LTZK::bindUiScaleToCurrentScreen);
    QTimer::singleShot(0, this, &LTZK::showStartupLogin);
    startupTrace(QStringLiteral("before initialized log"));
}

LTZK::~LTZK()
{
    // Destroy tracked children while the registry captured by their callbacks
    // still exists (QObject destroys remaining children after our members).
    const auto attributeDialogs = findChildren<QDialog*>(QStringLiteral("workspaceAttributeTableDialog"));
    for (auto* dialog : attributeDialogs)
    {
        delete dialog;
    }
    m_attributeTableDialogs.clear();
    delete m_dataSourceManager.data();
    m_dataSourceManager = nullptr;
    clearUserLog();
    qApp->removeEventFilter(this);
    destroyAllPluginFeatureWindows();
    delete m_pluginManager;
    m_pluginManager = nullptr;

    // 这里不主动删除插件管理器，交给 Qt 父子对象生命周期处理。
    if (m_layerTreeCanvasBridge) {
        delete m_layerTreeCanvasBridge;
        m_layerTreeCanvasBridge = nullptr;
    }

    // m_pluginManager 设置了 this 父对象，Qt 会在窗口析构时自动 delete。
    m_layerTreeView = nullptr;
    m_layerTreePanelWidget = nullptr;
}

void LTZK::bindUiScaleToCurrentScreen()
{
    QWindow* mainWindowHandle = windowHandle();
    if (!mainWindowHandle)
    {
        QTimer::singleShot(0, this, &LTZK::bindUiScaleToCurrentScreen);
        return;
    }

    if (m_uiScaleScreenChangedConnection)
    {
        disconnect(m_uiScaleScreenChangedConnection);
    }
    m_uiScaleScreenChangedConnection = connect(
        mainWindowHandle,
        &QWindow::screenChanged,
        this,
        [this](QScreen*)
        {
            bindUiScaleToCurrentScreen();
        });

    if (m_uiScaleGeometryChangedConnection)
    {
        disconnect(m_uiScaleGeometryChangedConnection);
    }
    if (m_uiScaleDpiChangedConnection)
    {
        disconnect(m_uiScaleDpiChangedConnection);
    }

    m_uiScaleScreen = mainWindowHandle->screen();
    if (m_uiScaleScreen)
    {
        m_uiScaleGeometryChangedConnection = connect(
            m_uiScaleScreen.data(),
            &QScreen::geometryChanged,
            this,
            [this](const QRect&)
            {
                refreshUiScale();
            });
        m_uiScaleDpiChangedConnection = connect(
            m_uiScaleScreen.data(),
            &QScreen::logicalDotsPerInchChanged,
            this,
            [this](qreal)
            {
                refreshUiScale();
            });
    }

    refreshUiScale();
}

void LTZK::refreshUiScale()
{
    const qreal scale = LTZKUiScale::factorForScreen(
        m_uiScaleScreen ? m_uiScaleScreen.data() : screen());
    if (qAbs(scale - m_lastUiScale) < 0.001)
    {
        return;
    }
    const qreal previousScale = m_lastUiScale;
    m_lastUiScale = scale;

    const QVariant baseFontValue = qApp->property("ltzkBaseApplicationFont");
    if (baseFontValue.isValid() && baseFontValue.canConvert<QFont>())
    {
        qApp->setFont(LTZKUiScale::scaledFont(baseFontValue.value<QFont>(), scale));
    }
    if (m_CurrentCoordinage)
    {
        m_CurrentCoordinage->setMinimumWidth(scaledInt(this, 260));
    }
    if (m_messageLogDock && m_messageLogDock->isVisible())
    {
        const int dockHeight = previousScale > 0.0
            ? qMax(1, qRound(m_messageLogDock->height() * scale / previousScale))
            : scaledInt(this, 180);
        resizeDocks(
            QList<QDockWidget*>() << m_messageLogDock,
            QList<int>() << dockHeight,
            Qt::Vertical);
    }

    refreshPlatformThemeStyles();
    refreshHomePageScaledMetrics();
    refreshWorkspaceScaledMetrics();

    for (QWidget* widget : QApplication::topLevelWidgets())
    {
        if (!widget)
        {
            continue;
        }
        if (widget->layout())
        {
            widget->layout()->invalidate();
            widget->layout()->activate();
        }
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->updateGeometry();
        widget->update();
    }
}

void LTZK::refreshHomePageScaledMetrics()
{
    if (!m_homePage)
    {
        return;
    }

    const qreal screenScale = LTZKUiScale::factorForScreen(
        m_uiScaleScreen ? m_uiScaleScreen.data() : screen());
    // The external-monitor screenshot defines the control scale.  The home
    // shell itself fills the current page width so the background and header
    // do not become a narrow centered strip on a 16:10 laptop.
    const QRect viewportRect = m_homePage->contentsRect();
    qreal targetScale = screenScale;
    const qreal viewportHeightScale = viewportRect.height() > 0
        ? viewportRect.height() / static_cast<qreal>(kHomeCanvasBaseHeight)
        : screenScale;
    if (qIsFinite(viewportHeightScale) && viewportHeightScale > 0.0)
    {
        targetScale = qMin(screenScale, viewportHeightScale);
    }
    if (!qIsFinite(targetScale) || targetScale <= 0.0)
    {
        targetScale = 1.0;
    }

    QWidget* canvas =
        m_homePage->findChild<QWidget*>(QStringLiteral("homeDesignCanvas"));
    if (!canvas)
    {
        return;
    }

    QSize targetCanvasSize = LTZKUiScale::scaledSize(
        QSize(kHomeCanvasBaseWidth, kHomeCanvasBaseHeight),
        targetScale);
    if (viewportRect.width() > 0)
    {
        targetCanvasSize.setWidth(viewportRect.width());
    }
    const bool scaleChanged = qAbs(targetScale - m_lastHomeScale) >= 0.0001
        || canvas->size() != targetCanvasSize;
    if (scaleChanged)
    {
        m_lastHomeScale = targetScale;
        m_homePage->setProperty(LTZKUiScale::kScaleOverrideProperty, targetScale);
        canvas->setFixedSize(targetCanvasSize);
    }
    canvas->move(
        viewportRect.left(),
        viewportRect.top() + qMax(0, (viewportRect.height() - canvas->height()) / 2));

    if (!scaleChanged)
    {
        return;
    }

    if (QVBoxLayout* rootLayout =
            m_homePage->findChild<QVBoxLayout*>(QStringLiteral("homeRootLayout")))
    {
        rootLayout->setSpacing(scaledInt(m_homePage, 4));
    }
    QWidget* homeHeaderWidget =
        m_homePage->findChild<QWidget*>(QStringLiteral("homeHeader"));
    if (SciFiHomeHeader* header = dynamic_cast<SciFiHomeHeader*>(homeHeaderWidget))
    {
        header->refreshScaledMetrics();
    }
    if (QFrame* stage = m_homePage->findChild<QFrame*>(QStringLiteral("homeStage")))
    {
        if (QVBoxLayout* stageLayout =
                stage->findChild<QVBoxLayout*>(QStringLiteral("homeStageLayout")))
        {
            stageLayout->setContentsMargins(scaledMargins(stage, 34, 4, 34, 0));
            stageLayout->setSpacing(scaledInt(stage, 10));
        }
        if (QHBoxLayout* themeLayout =
                stage->findChild<QHBoxLayout*>(QStringLiteral("homeThemeLayout")))
        {
            themeLayout->setContentsMargins(0, 0, 0, scaledInt(stage, 4));
        }
        if (QLabel* resultHint =
                stage->findChild<QLabel*>(QStringLiteral("qualityResultLayerHint")))
        {
            resultHint->setMinimumHeight(qMax(34, scaledInt(stage, 38)));
        }
    }
    if (m_homeThemeButton)
    {
        m_homeThemeButton->setMinimumHeight(scaledInt(m_homePage, 38));
    }
    if (HomeLoginPage* homeLoginPage = dynamic_cast<HomeLoginPage*>(m_homeLoginPage))
    {
        homeLoginPage->refreshScaledMetrics();
    }
    if (HomeModulesPanel* homeModulesPanel = dynamic_cast<HomeModulesPanel*>(m_homeModulesPage))
    {
        homeModulesPanel->refreshScaledMetrics();
    }
    updateHomeUtilityButtonMetrics();

    m_homePage->setStyleSheet(
        LTZKUiScale::scaleStyleSheetPixels(
            loadPlatformThemeQss(platformThemeKey(m_currentTheme)),
            targetScale));

    if (canvas->layout())
    {
        canvas->layout()->invalidate();
        canvas->layout()->activate();
    }
    m_homePage->updateGeometry();
    m_homePage->update();
}

void LTZK::refreshWorkspaceScaledMetrics()
{
    if (!m_mapCanvasPage)
    {
        return;
    }

    QFrame* topBar = m_mapCanvasPage->findChild<QFrame*>(QStringLiteral("dashboardTopBar"));
    QWidget* topActionStrip =
        m_mapCanvasPage->findChild<QWidget*>(QStringLiteral("workspaceTopActionStrip"));
    if (topBar)
    {
        if (QHBoxLayout* topLayout = qobject_cast<QHBoxLayout*>(topBar->layout()))
        {
            topLayout->setContentsMargins(scaledMargins(topBar, 18, 8, 18, 8));
            topLayout->setSpacing(scaledInt(topBar, 12));
        }
    }
    if (m_activeModuleTitle)
    {
        if (QGraphicsDropShadowEffect* glow =
                qobject_cast<QGraphicsDropShadowEffect*>(m_activeModuleTitle->graphicsEffect()))
        {
            glow->setBlurRadius(scaledInt(m_activeModuleTitle, 14));
        }
    }
    if (topActionStrip)
    {
        if (QHBoxLayout* actionLayout = qobject_cast<QHBoxLayout*>(topActionStrip->layout()))
        {
            actionLayout->setSpacing(scaledInt(topActionStrip, 6));
        }
        int contentHeight = topBarScaledInt(topActionStrip, 84);
        const QList<QToolButton*> topBarButtons =
            topActionStrip->findChildren<QToolButton*>();
        for (QToolButton* button : topBarButtons)
        {
            if (button->property("topBarTool").toBool())
            {
                setTopBarToolButtonResponsiveMetrics(button, topActionStrip);
                contentHeight = qMax(contentHeight, button->sizeHint().height());
            }
        }

        if (WorkspaceTopBarGradientScrollArea* scrollArea =
                topActionStrip->findChild<WorkspaceTopBarGradientScrollArea*>(
                    QStringLiteral("workspacePluginActionScrollArea")))
        {
            scrollArea->setMinimumWidth(scaledInt(topActionStrip, 720));
            scrollArea->setBaseContentHeight(contentHeight);
        }
        else if (QScrollArea* scrollArea =
                     topActionStrip->findChild<QScrollArea*>(
                         QStringLiteral("workspacePluginActionScrollArea")))
        {
            scrollArea->setMinimumWidth(scaledInt(topActionStrip, 720));
            scrollArea->setFixedHeight(contentHeight);
        }
        if (QWidget* scrollContent =
                topActionStrip->findChild<QWidget*>(QStringLiteral("workspaceTopActionScrollContent")))
        {
            scrollContent->setFixedHeight(contentHeight);
            if (QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(scrollContent->layout()))
            {
                layout->setSpacing(scaledInt(topActionStrip, 6));
            }
        }
        const QList<QWidget*> pluginStrips =
            topActionStrip->findChildren<QWidget*>(QStringLiteral("workspacePluginActionStrip"));
        for (QWidget* pluginStrip : pluginStrips)
        {
            if (pluginStrip == topActionStrip)
            {
                continue;
            }
            pluginStrip->setFixedHeight(contentHeight);
            if (QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(pluginStrip->layout()))
            {
                layout->setSpacing(scaledInt(topActionStrip, 6));
            }
        }
    }

    if (QSplitter* splitter =
            m_mapCanvasPage->findChild<QSplitter*>(QStringLiteral("workspaceSplitter")))
    {
        splitter->setContentsMargins(scaledMargins(m_mapCanvasPage, 12, 12, 12, 12));
        splitter->setHandleWidth(splitterHandleWidth(m_mapCanvasPage));
    }
    if (QFrame* layerPanel =
            m_mapCanvasPage->findChild<QFrame*>(QStringLiteral("workspaceLayerManagerPanel")))
    {
        if (layerPanel->maximumWidth() != 0)
        {
            layerPanel->setMinimumWidth(scaledInt(m_mapCanvasPage, 260));
        }
        if (QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(layerPanel->layout()))
        {
            layout->setContentsMargins(scaledMargins(layerPanel, 8, 8, 8, 8));
        }
    }
    if (QFrame* mapPanel =
            m_mapCanvasPage->findChild<QFrame*>(QStringLiteral("workspaceMapPanel")))
    {
        if (QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(mapPanel->layout()))
        {
            layout->setContentsMargins(scaledMargins(mapPanel, 4, 4, 4, 4));
        }
    }
    QToolButton* collapseWidget =
        m_mapCanvasPage->findChild<QToolButton*>(QStringLiteral("workspaceLayerManagerCollapseButton"));
    if (SplitterTriangleButton* collapseButton =
            dynamic_cast<SplitterTriangleButton*>(collapseWidget))
    {
        collapseButton->setFixedSize(splitterTriangleButtonSize(m_mapCanvasPage));
    }
    if (QFrame* handleRail =
            m_mapCanvasPage->findChild<QFrame*>(QStringLiteral("workspaceLayerManagerHandleRail")))
    {
        handleRail->setFixedWidth(splitterHandleRailWidth(m_mapCanvasPage));
    }

    if (m_accessControlPage)
    {
        if (QFrame* accessTopBar =
                m_accessControlPage->findChild<QFrame*>(QStringLiteral("accessControlTopBar")))
        {
            if (QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(accessTopBar->layout()))
            {
                layout->setContentsMargins(scaledMargins(accessTopBar, 18, 10, 18, 10));
                layout->setSpacing(scaledInt(accessTopBar, 12));
            }
            const QStringList buttonNames = {
                QStringLiteral("accessControlBackButton"),
                QStringLiteral("accessControlLogoutButton")
            };
            for (const QString& name : buttonNames)
            {
                if (QToolButton* button = accessTopBar->findChild<QToolButton*>(name))
                {
                    button->setIconSize(scaledSize(accessTopBar, 20, 20));
                }
            }
            if (QLabel* title =
                    accessTopBar->findChild<QLabel*>(QStringLiteral("dashboardTitle")))
            {
                if (QGraphicsDropShadowEffect* glow =
                        qobject_cast<QGraphicsDropShadowEffect*>(title->graphicsEffect()))
                {
                    glow->setBlurRadius(scaledInt(title, 14));
                }
            }
        }
        if (m_accessControlWidget)
        {
            m_accessControlWidget->refreshScaledMetrics();
        }
    }
}

void LTZK::initMessageLogDock()
{
    m_messageLogDock = new QgsDockWidget(QStringLiteral("\u65E5\u5FD7\u8F93\u51FA"), this);
    m_messageLogDock->setObjectName(QStringLiteral("messageLogDock"));
    m_messageLogDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_messageLogDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);

    QWidget* logPanel = new QWidget(m_messageLogDock);
    QVBoxLayout* logLayout = new QVBoxLayout(logPanel);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->setSpacing(0);

    m_messageLogTabs = new QTabWidget(logPanel);
    m_messageLogTabs->setObjectName(QStringLiteral("messageLogTabs"));
    m_messageLogTabs->setTabsClosable(false);
    logLayout->addWidget(m_messageLogTabs);
    messageLogTextEdit(QString());

    m_messageLogDock->setWidget(logPanel);

    m_messageLogToggleAction = new QAction(this);
    m_messageLogToggleAction->setText(QString::fromUtf8("\xF0\x9F\x92\xAC"));
    m_messageLogToggleAction->setToolTip(QStringLiteral("\u663E\u793A/\u9690\u85CF QGIS \u65E5\u5FD7\u8F93\u51FA"));
    m_messageLogToggleAction->setCheckable(true);
    m_messageLogToggleAction->setChecked(false);

    m_messageLogToggleButton = new QToolButton(this);
    m_messageLogToggleButton->setObjectName(QStringLiteral("messageLogToggleButton"));
    m_messageLogToggleButton->setDefaultAction(m_messageLogToggleAction);
    m_messageLogToggleButton->setAutoRaise(true);
    m_messageLogToggleButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    statusBar()->addWidget(m_messageLogToggleButton);

    m_messageLogDock->setToggleVisibilityAction(m_messageLogToggleAction);
    addDockWidget(Qt::BottomDockWidgetArea, m_messageLogDock);
    m_messageLogDock->setFloating(false);
    m_messageLogDock->hide();
    resizeDocks(QList<QDockWidget*>() << m_messageLogDock, QList<int>() << 180, Qt::Vertical);

    if (QgsApplication::messageLog())
    {
        connect(QgsApplication::messageLog(),
            static_cast<void (QgsMessageLog::*)(const QString&, const QString&, Qgis::MessageLevel)>(&QgsMessageLog::messageReceived),
            this,
            [this](const QString& message, const QString& tag, Qgis::MessageLevel level)
            {
                appendMessageLogEntry(message, tag, static_cast<int>(level));
            });
    }
}

QPlainTextEdit* LTZK::messageLogTextEdit(const QString& tag)
{
    const QString tabTitle = tag.trimmed().isEmpty() ? QStringLiteral("\u5E38\u89C4") : tag.trimmed();
    for (int i = 0; i < m_messageLogTabs->count(); ++i)
    {
        if (m_messageLogTabs->tabText(i) == tabTitle)
        {
            return qobject_cast<QPlainTextEdit*>(m_messageLogTabs->widget(i));
        }
    }

    QPlainTextEdit* edit = new QPlainTextEdit(m_messageLogTabs);
    edit->setReadOnly(true);
    edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    edit->setPlaceholderText(QStringLiteral("Message log empty"));
    m_messageLogTabs->addTab(edit, tabTitle);
    return edit;
}

void LTZK::appendMessageLogEntry(const QString& message, const QString& tag, int level)
{
    QPlainTextEdit* edit = messageLogTextEdit(tag);
    const QString line = QStringLiteral("[%1] [%2] %3")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
        .arg(messageLogLevelText(level))
        .arg(message);
    edit->appendPlainText(line);
    m_messageLogTabs->setCurrentWidget(edit);
}

QString LTZK::platformStyleSheet() const
{
    // Theme files are shared by many QGIS child windows. Avoid rereading and
    // rescaling the same stylesheet for every window and polish event.
    const QString themeKey = platformThemeKey(m_currentTheme);
    const qreal scale = LTZKUiScale::factorForWidget(this);
    const QFileInfo source(platformThemeQssPath(themeKey));
    static QString cachedTheme;
    static QString cachedStyle;
    static QDateTime cachedModified;
    static qint64 cachedSize = -1;
    static qreal cachedScale = -1;
    if (cachedTheme != themeKey || !qFuzzyCompare(cachedScale, scale)
        || cachedModified != source.lastModified() || cachedSize != source.size()
        || cachedStyle.isEmpty())
    {
        QString style = loadPlatformThemeQss(themeKey);
        style += LTZKNativeDialogStyle::embeddedEditorStyleSheet();
        // Native header rules are shared application-wide. The property selector
        // limits them to explicitly prepared QGIS/plugin controls.
        style += LTZKNativeDialogStyle::nativeHeaderStyleSheet(1.0);
        cachedStyle = LTZKUiScale::scaleStyleSheetPixels(style, scale);
        cachedTheme = themeKey;
        cachedScale = scale;
        cachedModified = source.lastModified();
        cachedSize = source.size();
    }
    return cachedStyle;
}

QString LTZK::platformThemeKey(PlatformTheme theme) const
{
    switch (theme)
    {
    case PlatformTheme::White:
        return QStringLiteral("white");
    case PlatformTheme::Black:
        return QStringLiteral("black");
    case PlatformTheme::SciFi:
        return QStringLiteral("sci-fi");
    case PlatformTheme::FieldNotes:
        return QStringLiteral("field-notes");
    case PlatformTheme::SteelWarmWhite:
        return QStringLiteral("steel-warm-white");
    default:
        return QStringLiteral("sci-fi");
    }
}

QString LTZK::platformThemeLabel(PlatformTheme theme) const
{
    switch (theme)
    {
    case PlatformTheme::White:
        return QStringLiteral("白色系");
    case PlatformTheme::Black:
        return QStringLiteral("黑色系");
    case PlatformTheme::SciFi:
        return QStringLiteral("科幻系");
    case PlatformTheme::FieldNotes:
        return QStringLiteral("绿色系");
    case PlatformTheme::SteelWarmWhite:
        return QStringLiteral("灰色系");
    default:
        return QStringLiteral("科幻系");
    }
}

LTZK::PlatformTheme LTZK::platformThemeFromKey(const QString& key) const
{
    const QString normalized = key.trimmed().toLower();
    if (normalized == QStringLiteral("white"))
    {
        return PlatformTheme::White;
    }
    if (normalized == QStringLiteral("black"))
    {
        return PlatformTheme::Black;
    }
    if (normalized == QStringLiteral("field-notes"))
    {
        return PlatformTheme::FieldNotes;
    }
    if (normalized == QStringLiteral("steel-warm-white"))
    {
        return PlatformTheme::SteelWarmWhite;
    }
    return PlatformTheme::SciFi;
}

bool LTZK::isLightPlatformTheme() const
{
    return m_currentTheme == PlatformTheme::White
        || m_currentTheme == PlatformTheme::FieldNotes
        || m_currentTheme == PlatformTheme::SteelWarmWhite;
}

void LTZK::loadSavedPlatformTheme()
{
    QSettings settings(platformSettingsPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    m_currentTheme = platformThemeFromKey(settings.value(QStringLiteral("platform/theme"), QStringLiteral("sci-fi")).toString());
}

void LTZK::savePlatformTheme() const
{
    const QString path = platformSettingsPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSettings settings(platformSettingsPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue(QStringLiteral("platform/theme"), platformThemeKey(m_currentTheme));
    settings.sync();
}

void LTZK::setPlatformTheme(PlatformTheme theme)
{
    if (m_currentTheme == theme)
    {
        refreshPlatformThemeStyles();
        return;
    }

    m_currentTheme = theme;
    savePlatformTheme();
    refreshPlatformThemeStyles();
    statusBar()->showMessage(QStringLiteral("平台风格已切换为%1。").arg(platformThemeLabel(m_currentTheme)), 3000);
}

void LTZK::showPlatformThemeDialog()
{
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("platformThemeDialog"));
    dialog.setWindowTitle(QStringLiteral("风格选择"));
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setModal(true);
    dialog.setMinimumSize(scaledSize(this, 460, 340));
    dialog.resize(scaledSize(this, 460, 340));
    dialog.setStyleSheet(platformStyleSheet());

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(scaledMargins(&dialog, 22, 20, 22, 20));
    layout->setSpacing(scaledInt(&dialog, 12));
    layout->setAlignment(Qt::AlignCenter);

    const QList<PlatformTheme> themes = {
        PlatformTheme::White,
        PlatformTheme::Black,
        PlatformTheme::SciFi,
        PlatformTheme::FieldNotes,
        PlatformTheme::SteelWarmWhite
    };

    for (PlatformTheme theme : themes)
    {
        QToolButton* optionButton = new QToolButton(&dialog);
        optionButton->setProperty("themeOptionButton", true);
        optionButton->setCheckable(true);
        optionButton->setChecked(theme == m_currentTheme);
        optionButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
        optionButton->setText(platformThemeLabel(theme) + (theme == m_currentTheme ? QStringLiteral("（当前风格）") : QString()));
        optionButton->setCursor(Qt::PointingHandCursor);
        optionButton->setMinimumWidth(scaledInt(&dialog, 300));
        optionButton->setMinimumHeight(scaledInt(&dialog, 68));
        optionButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        connect(optionButton, &QToolButton::clicked, this, [this, theme, &dialog]() {
            setPlatformTheme(theme);
            dialog.accept();
        });
        layout->addWidget(optionButton, 0, Qt::AlignHCenter);
    }

    dialog.exec();
}

void LTZK::refreshPlatformThemeStyles()
{
    const QString style = platformStyleSheet();
    const QString themeKey = platformThemeKey(m_currentTheme);
    qApp->setStyleSheet(style);
    QToolTip::setFont(qApp->font());
    setStyleSheet(style);
    refreshTopBarFloatingHoverStyles(this, themeKey);

    const QList<QWidget*> themedPages = {
        m_homePage,
        m_homeLoginPage,
        m_homeModulesPage,
        m_mapCanvasPage,
        m_accessControlPage
    };

    for (QWidget* page : themedPages)
    {
        if (!page)
        {
            continue;
        }
        page->setProperty("platformTheme", themeKey);
        if (page == m_homePage)
        {
            const qreal homeScale = LTZKUiScale::factorForWidget(page);
            page->setStyleSheet(
                LTZKUiScale::scaleStyleSheetPixels(
                    loadPlatformThemeQss(themeKey),
                    homeScale));
        }
        else if (page == m_homeLoginPage || page == m_homeModulesPage)
        {
            // These pages are part of the one homepage design canvas. They
            // must inherit its target-scaled stylesheet instead of creating
            // a second screen-scaled QSS layer.
            page->setStyleSheet(QString());
        }
        else if (page == m_accessControlPage)
        {
            const QString accessControlBaseStyleSheet =
                page->property("ltzkAccessControlBaseStyleSheet").toString();
            page->setStyleSheet(
                style
                + (themeKey == QStringLiteral("sci-fi")
                    ? QLatin1Char('\n')
                        + LTZKUiScale::scaleStyleSheetPixels(
                            accessControlBaseStyleSheet,
                            LTZKUiScale::factorForWidget(page))
                    : QString()));
        }
        else
        {
            page->setStyleSheet(style);
        }
        for (QWidget* child : page->findChildren<QWidget*>())
        {
            child->setProperty("platformTheme", themeKey);
            if (ResponsiveHomeModuleButton* moduleButton = dynamic_cast<ResponsiveHomeModuleButton*>(child))
            {
                moduleButton->applyTheme(themeKey);
            }
            if (SciFiHomeHeader* header = dynamic_cast<SciFiHomeHeader*>(child))
            {
                header->refreshScaledMetrics();
            }
        }
        page->style()->unpolish(page);
        page->style()->polish(page);
        page->update();
    }

    for (QgsMapLayer* layer : QgsProject::instance()->mapLayers())
    {
        QgsVectorLayer* vectorLayer = qobject_cast<QgsVectorLayer*>(layer);
        if (vectorLayer && vectorLayer->customProperty(QStringLiteral("ltzk/systemBasemap")).toBool())
        {
            styleLocalGansuBasemapLayer(vectorLayer);
        }
    }
    applyWorkspaceShpCanvasBackground(m_workspaceUsesLightShpCanvas);
    if (m_mapCanvas)
    {
        m_mapCanvas->refresh();
    }

    if (m_homeThemeButton)
    {
        m_homeThemeButton->setText(QStringLiteral("平台风格：%1").arg(platformThemeLabel(m_currentTheme)));
        m_homeThemeButton->setStyleSheet(QString());
    }

    for (QWidget* widget : QApplication::topLevelWidgets())
    {
        if (widget && widget != this)
        {
            widget->setProperty("ltzkWindowInitialized", false);
        }
    }
    if (m_workspaceContentStack && m_workspaceContentStack->serviceDialog())
    {
        m_workspaceContentStack->serviceDialog()->setProperty("ltzkWindowInitialized", false);
    }
    applyPlatformStyleToTopLevelWidgets();
    updateHomeUtilityButtonMetrics();
    refreshConfiguredPluginActionIcons();
    updateWorkspaceTopBarThemeBackground(m_currentBusinessModuleTitle);
}
void LTZK::initSciFiWorkspace()
{
    startupTrace(
        QStringLiteral(
            "initSciFiWorkspace begin"));

    loadSavedPlatformTheme();

    /*
     * 将全局样式应用到顶层弹窗。
     *
     * qApp->setStyleSheet() 不会立即刷新所有已创建窗口，
     * QgsMapCanvas 和 QgsLayerTreeView 也需要重新 polish。
     * 所以这里遍历顶层窗口并强制刷新样式。
     *
     * 只处理当前应用窗口，避免影响外部宿主。
     * 这是科幻主题切换后的补充刷新。
     */
    qApp->setStyleSheet(platformStyleSheet());

    startupTrace(
        QStringLiteral(
            "before workspace stack"));

    m_workspaceStack =
        new QStackedWidget(this);

    startupTrace(
        QStringLiteral(
            "after workspace stack"));

    m_workspaceStack->setObjectName(
        QStringLiteral(
            "workspaceStack"));

    startupTrace(
        QStringLiteral(
            "before build home page"));

    m_homePage =
        buildSciFiHomePage();
    if (m_homePage)
    {
        m_homePage->installEventFilter(this);
    }

    startupTrace(
        QStringLiteral(
            "after build home page"));

    startupTrace(
        QStringLiteral(
            "before build workspace page"));

    m_mapCanvasPage =
        buildSciFiWorkspacePage();

    startupTrace(
        QStringLiteral(
            "after build workspace page"));

    startupTrace(
        QStringLiteral(
            "before build access control page"));

    m_accessControlPage =
        buildAccessControlPage();

    startupTrace(
        QStringLiteral(
            "after build access control page"));

    startupTrace(
        QStringLiteral(
            "before add pages"));

    if (m_homePage)
    {
        m_workspaceStack->addWidget(
            m_homePage);
    }

    if (m_mapCanvasPage)
    {
        m_workspaceStack->addWidget(
            m_mapCanvasPage);
    }

    if (m_accessControlPage)
    {
        m_workspaceStack->addWidget(
            m_accessControlPage);
    }

    startupTrace(
        QStringLiteral(
            "after add pages"));

    if (m_homePage)
    {
        m_workspaceStack->setCurrentWidget(
            m_homePage);
    }

    startupTrace(
        QStringLiteral(
            "before setCentralWidget"));

    setCentralWidget(
        m_workspaceStack);

    startupTrace(
        QStringLiteral(
            "after setCentralWidget"));

    menuBar()->hide();

    if (ui.mainToolBar)
    {
        ui.mainToolBar->hide();
    }

    if (ui.LayerTreeControl)
    {
        ui.LayerTreeControl->hide();
    }

    /*
     * 初始化科幻工作台后需要立即把样式同步给 qApp。
     */
    setStyleSheet(platformStyleSheet());
    refreshPlatformThemeStyles();
    refreshHomePageScaledMetrics();
    refreshWorkspaceScaledMetrics();

    startupTrace(
        QStringLiteral(
            "initSciFiWorkspace end"));
}
QWidget* LTZK::buildSciFiHomePage()
{
    QWidget* page = new SciFiHomePage(this);
    page->setObjectName(QStringLiteral("sciFiHomePage"));
    const QString homeBaseStyleSheet = QStringLiteral(
        // 首页整体深色径向背景，强调中心业务入口。
        "#sciFiHomePage { "
        "background: qlineargradient("
        "x1:0, y1:0, x2:1, y2:1, "
        "stop:0 #0B1C2E, "
        "stop:0.48 #102A43, "
        "stop:1 #071521); "
        "}"

        // 首页舞台透明，保留整体背景层次。
        "#homeStage { "
        "background: transparent; "
        "border: 0; "
        "}"

        "#homeContentStack { "
        "background: transparent; "
        "border: 0; "
        "}"

        "#homeLoginPage, #homeModulesPage { "
        "background: transparent; "
        "}"

        "#homeLoginAmbientBand { "
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "stop:0 rgba(42, 212, 255, 0), "
        "stop:0.16 rgba(42, 212, 255, 58), "
        "stop:0.5 rgba(255, 178, 56, 74), "
        "stop:0.84 rgba(42, 212, 255, 58), "
        "stop:1 rgba(42, 212, 255, 0)); "
        "border-top: 1px solid rgba(89, 214, 255, 90); "
        "border-bottom: 1px solid rgba(89, 214, 255, 70); "
        "}"

        "#homeLoginLowerTelemetry { "
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "stop:0 rgba(7, 28, 49, 0), "
        "stop:0.2 rgba(8, 59, 88, 132), "
        "stop:0.5 rgba(6, 29, 48, 170), "
        "stop:0.8 rgba(8, 59, 88, 132), "
        "stop:1 rgba(7, 28, 49, 0)); "
        "border: 1px solid rgba(46, 168, 223, 86); "
        "border-left: 0; "
        "border-right: 0; "
        "}"
        "#homeLoginPanel { "
        "background: qlineargradient("
        "x1:0, y1:0, x2:0, y2:1, "
        "stop:0 rgba(18, 49, 78, 246), "
        "stop:0.55 rgba(14, 40, 64, 248), "
        "stop:1 rgba(11, 33, 54, 252)); "
        "border: 1px solid rgba(83, 166, 205, 135); "
        "border-top: 3px solid #2AD4FF; "
        "border-radius: 10px; "
        "}"

        "QLabel#homeLoginTitle { "
        "color: #F4FBFF; "
        "font-family: \"Microsoft YaHei UI\"; "
        "font-size: 54px; "
        "font-weight: 900; "
        "min-height: 78px; "
        "background: transparent; "
        "}"
        "#homeLoginStatusLabel { "
        "color: #ffb199; "
        "font-family: \"Microsoft YaHei UI\"; "
        "font-size: 15px; "
        "font-weight: 500; "
        "background: transparent; "
        "}"

        "#homeLoginScanLine { "
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "stop:0 rgba(42, 212, 255, 0), "
        "stop:0.22 rgba(42, 212, 255, 170), "
        "stop:0.5 rgba(255, 178, 56, 210), "
        "stop:0.78 rgba(42, 212, 255, 170), "
        "stop:1 rgba(42, 212, 255, 0)); "
        "border: 0; "
        "}"
        "#homeLoginPanel QLabel { "
        "color: #EAF6FF; "
        "font-family: \"Microsoft YaHei UI\"; "
        "font-size: 18px; "
        "background: transparent; "
        "}"

        "#homeLoginPanel QLabel#homeLoginTitle { "
        "color: #F4FBFF; "
        "font-family: \"Microsoft YaHei UI\"; "
        "font-size: 54px; "
        "font-weight: 900; "
        "min-height: 78px; "
        "background: transparent; "
        "}"

        "#homeLoginPanel QLineEdit { "
        "color: #FFFFFF; "
        "background: rgba(6, 17, 29, 230); "
        "border: 1px solid rgba(46, 168, 223, 170); "
        "border-radius: 4px; "
        "padding: 10px 14px; "
        "font-family: \"Microsoft YaHei UI\"; "
        "font-size: 18px; "
        "selection-background-color: #147db5; "
        "}"

        "#homeLoginPanel QLineEdit:focus { "
        "border: 1px solid #59d6ff; "
        "}"

        "#homeLoginButton, #homeLoginResetButton, #homeLoginGuestButton { "
        "color: #FFFFFF; "
        "background: #0b4a78; "
        "border: 1px solid #2ea8df; "
        "border-radius: 4px; "
        "padding: 12px 34px; "
        "font-family: \"Microsoft YaHei UI\"; "
        "font-size: 18px; "
        "font-weight: 700; "
        "}"

        "#homeLoginButton:hover, #homeLoginResetButton:hover, #homeLoginGuestButton:hover { "
        "background: #0f619a; "
        "border-color: #59d6ff; "
        "}"

        // 首页模块按钮基础样式。
        "QToolButton[homeModuleButton=\"true\"] { "
        "color: #EAF6FF; "
        "background: qlineargradient("
        "x1:0, y1:0, x2:0, y2:1, "
        "stop:0 rgba(18, 49, 78, 246), "
        "stop:0.55 rgba(14, 40, 64, 248), "
        "stop:1 rgba(11, 33, 54, 252)); "
        "border: 1px solid rgba(83, 166, 205, 105); "
        "border-radius: 10px; "
        "padding: 18px 24px 22px 24px; "
        "font-family: \"Microsoft YaHei UI\"; "
        "}"

        // 01 地图智能制图
        "#moduleButton_mapGeneralization { "
        "border: 1px solid rgba(42, 212, 255, 120); "
        "border-top: 3px solid #2AD4FF; "
        "}"

        // 02 成果质检
        "#moduleButton_mapQuality { "
        "border: 1px solid rgba(64, 169, 255, 120); "
        "border-top: 3px solid #40A9FF; "
        "}"

        // 03 成果管理
        "#moduleButton_mapAchievementManagement { "
        "border: 1px solid rgba(36, 196, 214, 115); "
        "border-top: 3px solid #24C4D6; "
        "}"

        // 04 服务管理
        "#moduleButton_mapServiceManagement { "
        "border: 1px solid rgba(75, 139, 255, 115); "
        "border-top: 3px solid #4B8BFF; "
        "}"

        // 首页模块按钮悬浮态。
        "QToolButton[homeModuleButton=\"true\"]:hover { "
        "color: #FFFFFF; "
        "background: qlineargradient("
        "x1:0, y1:0, x2:0, y2:1, "
        "stop:0 rgba(21, 57, 91, 252), "
        "stop:0.55 rgba(16, 46, 74, 252), "
        "stop:1 rgba(14, 41, 66, 254)); "
        "}"

        // 不同业务模块使用独立的悬浮边框色。
        "#moduleButton_mapGeneralization:hover { "
        "border: 2px solid #46dcff; "
        "}"

        "#moduleButton_mapQuality:hover { "
        "border: 2px solid #9ba5ff; "
        "}"

        "#moduleButton_mapAchievementManagement:hover { "
        "border: 2px solid #4ceac7; "
        "}"

        "#moduleButton_mapServiceManagement:hover { "
        "border: 2px solid #ffd477; "
        "}"

        // 按下状态。
        "QToolButton[homeModuleButton=\"true\"]:pressed { "
        "background: rgba(11, 49, 72, 250); "
        "border: 2px solid #ffffff; "
        "}"

        // 底部结果说明条，与首页模块按钮保持同一视觉语言。
        "#qualityResultLayerHint { "
        "color: #A9C9D8; "
        "background: rgba(9, 35, 52, 205); "
        "border: 1px solid rgba(65, 142, 176, 75); "
        "border-top: 1px solid rgba(42, 212, 255, 110); "
        "border-radius: 4px; "
        "font-family: \"Microsoft YaHei UI\"; "
        "font-size: 15px; "
        "font-weight: 500; "
        "padding: 3px 14px; "
        "}"
    );
    page->setStyleSheet(LTZKUiScale::scaleStyleSheetPixels(
        homeBaseStyleSheet,
        LTZKUiScale::factorForWidget(page)));

    QWidget* canvas = new QWidget(page);
    canvas->setObjectName(QStringLiteral("homeDesignCanvas"));
    canvas->setAttribute(Qt::WA_TranslucentBackground, true);
    canvas->setAutoFillBackground(false);
    canvas->setStyleSheet(QStringLiteral(
        "QWidget#homeDesignCanvas { background: transparent; border: 0; }"));
    canvas->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    canvas->setFixedSize(scaledSize(page, kHomeCanvasBaseWidth, kHomeCanvasBaseHeight));

    QVBoxLayout* rootLayout = new QVBoxLayout(canvas);
    rootLayout->setObjectName(QStringLiteral("homeRootLayout"));
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(scaledInt(page, 4));

    SciFiHomeHeader* homeHeader =
        new SciFiHomeHeader(
            QStringLiteral("陇图智控平台"),
            canvas);
    homeHeader->setObjectName(QStringLiteral("homeHeader"));
    rootLayout->addWidget(homeHeader);

    QFrame* stage = new QFrame(canvas);
    stage->setObjectName(QStringLiteral("homeStage"));
    QVBoxLayout* stageLayout = new QVBoxLayout(stage);
    stageLayout->setObjectName(QStringLiteral("homeStageLayout"));
    stageLayout->setContentsMargins(scaledMargins(stage, 34, 4, 34, 0));
    stageLayout->setSpacing(scaledInt(stage, 10));

    m_homeContentStack = new QStackedWidget(stage);
    m_homeContentStack->setObjectName(QStringLiteral("homeContentStack"));
    m_homeContentStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_homeLoginPage = buildHomeLoginPanel(m_homeContentStack);
    m_homeModulesPage = buildHomeModulesPanel(m_homeContentStack);
    if (m_homeModulesPage)
    {
        m_homeModulesPage->installEventFilter(this);
    }

    m_homeContentStack->addWidget(m_homeLoginPage);
    m_homeContentStack->addWidget(m_homeModulesPage);

    stageLayout->addWidget(m_homeContentStack, 1);

    m_homeThemeButton = new QToolButton(stage);
    m_homeThemeButton->setObjectName(QStringLiteral("homeThemeButton"));
    m_homeThemeButton->setProperty("homeThemeButton", true);
    m_homeThemeButton->setText(QStringLiteral("平台风格：%1").arg(platformThemeLabel(m_currentTheme)));
    m_homeThemeButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_homeThemeButton->setCursor(Qt::PointingHandCursor);
    m_homeThemeButton->setMinimumHeight(scaledInt(stage, 38));
    connect(m_homeThemeButton, &QToolButton::clicked, this, &LTZK::showPlatformThemeDialog);

    QHBoxLayout* homeThemeLayout = new QHBoxLayout();
    homeThemeLayout->setObjectName(QStringLiteral("homeThemeLayout"));
    homeThemeLayout->setContentsMargins(0, 0, 0, scaledInt(stage, 4));
    homeThemeLayout->setSpacing(0);
    homeThemeLayout->addWidget(m_homeThemeButton, 0, Qt::AlignLeft | Qt::AlignBottom);
    homeThemeLayout->addStretch(1);
    stageLayout->addLayout(homeThemeLayout, 0);

    QLabel* resultHint = new QLabel(QStringLiteral("甘肃省基础地理信息中心 @2026"), stage);
    resultHint->setObjectName(QStringLiteral("qualityResultLayerHint"));
    resultHint->setAlignment(Qt::AlignCenter);
    resultHint->setTextFormat(Qt::PlainText);
    resultHint->setWordWrap(false);
    resultHint->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    resultHint->setMinimumHeight(qMax(34, scaledInt(stage, 38)));
    stageLayout->addWidget(resultHint);

    rootLayout->addWidget(stage, 1);
    return page;
}

QWidget* LTZK::buildHomeLoginPanel(QWidget* parent)
{
    HomeLoginPage* page = new HomeLoginPage(parent);
    page->setObjectName(QStringLiteral("homeLoginPage"));


    QFrame* panel = new QFrame(page);
    panel->setObjectName(QStringLiteral("homeLoginPanel"));
    panel->setFixedSize(scaledSize(page, kHomeLoginPanelBaseWidth, kHomeLoginPanelBaseHeight));
    panel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect(panel);
    shadowEffect->setBlurRadius(scaledInt(panel, 24));
    shadowEffect->setColor(QColor(0, 0, 0, 135));
    shadowEffect->setOffset(0, scaledInt(panel, 8));
    panel->setGraphicsEffect(shadowEffect);

    QVBoxLayout* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(scaledMargins(panel, 40, 24, 40, 22));
    panelLayout->setSpacing(scaledInt(panel, 10));

    panelLayout->addSpacing(scaledInt(panel, 12));
    QLabel* titleLabel = new QLabel(QStringLiteral("用户登录"), panel);
    titleLabel->setObjectName(QStringLiteral("homeLoginTitle"));
    QFont homeLoginTitleFont(QStringLiteral("Microsoft YaHei UI"));
    homeLoginTitleFont.setPixelSize(scaledInt(panel, 54));
    homeLoginTitleFont.setWeight(QFont::Black);
    titleLabel->setFont(homeLoginTitleFont);
    titleLabel->setMinimumHeight(scaledInt(panel, 64));
    titleLabel->setAlignment(Qt::AlignCenter);
    panelLayout->addWidget(titleLabel);    QFrame* scanLine = new QFrame(panel);
    scanLine->setObjectName(QStringLiteral("homeLoginScanLine"));
    scanLine->setFixedHeight(scaledInt(panel, 3));
    panelLayout->addWidget(scanLine);

    QGridLayout* formLayout = new QGridLayout();
    formLayout->setObjectName(QStringLiteral("homeLoginFormLayout"));
    formLayout->setContentsMargins(scaledMargins(panel, 0, 8, 0, 0));
    formLayout->setHorizontalSpacing(scaledInt(panel, 12));
    formLayout->setVerticalSpacing(scaledInt(panel, 10));
    formLayout->setColumnMinimumWidth(0, scaledInt(panel, 60));
    formLayout->setColumnStretch(0, 0);
    formLayout->setColumnStretch(1, 1);

    QLabel* userLabel = new QLabel(QStringLiteral("用户名"), panel);
    userLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_homeLoginUserNameEdit = new QLineEdit(panel);
    m_homeLoginUserNameEdit->setObjectName(QStringLiteral("homeLoginUserNameEdit"));
    m_homeLoginUserNameEdit->setPlaceholderText(QStringLiteral("请输入用户名"));
    m_homeLoginUserNameEdit->setMinimumHeight(scaledInt(panel, 36));

    QLabel* passwordLabel = new QLabel(QStringLiteral("密码"), panel);
    passwordLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_homeLoginPasswordEdit = new QLineEdit(panel);
    m_homeLoginPasswordEdit->setObjectName(QStringLiteral("homeLoginPasswordEdit"));
    m_homeLoginPasswordEdit->setEchoMode(QLineEdit::Password);
    m_homeLoginPasswordEdit->setPlaceholderText(QStringLiteral("请输入密码"));
    m_homeLoginPasswordEdit->setMinimumHeight(scaledInt(panel, 36));

    formLayout->addWidget(userLabel, 0, 0);
    formLayout->addWidget(m_homeLoginUserNameEdit, 0, 1);
    formLayout->addWidget(passwordLabel, 1, 0);
    formLayout->addWidget(m_homeLoginPasswordEdit, 1, 1);
    panelLayout->addLayout(formLayout);

    m_homeLoginStatusLabel = new QLabel(panel);
    m_homeLoginStatusLabel->setObjectName(QStringLiteral("homeLoginStatusLabel"));
    m_homeLoginStatusLabel->setWordWrap(true);
    m_homeLoginStatusLabel->setMinimumHeight(scaledInt(panel, 20));
    panelLayout->addWidget(m_homeLoginStatusLabel);

    QPushButton* loginButton = new QPushButton(QStringLiteral("登录"), panel);
    loginButton->setObjectName(QStringLiteral("homeLoginButton"));
    loginButton->setDefault(true);
    loginButton->setMinimumHeight(scaledInt(panel, 42));
    loginButton->setCursor(Qt::PointingHandCursor);

    QPushButton* resetButton = new QPushButton(QStringLiteral("重置"), panel);
    resetButton->setObjectName(QStringLiteral("homeLoginResetButton"));
    resetButton->setMinimumHeight(scaledInt(panel, 42));
    resetButton->setCursor(Qt::PointingHandCursor);

    QPushButton* guestButton = new QPushButton(QStringLiteral("匿名登录"), panel);
    guestButton->setObjectName(QStringLiteral("homeLoginGuestButton"));
    guestButton->setMinimumHeight(scaledInt(panel, 42));
    guestButton->setCursor(Qt::PointingHandCursor);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setObjectName(QStringLiteral("homeLoginButtonLayout"));
    buttonLayout->setContentsMargins(scaledMargins(panel, 0, 4, 0, 0));
    buttonLayout->setSpacing(scaledInt(panel, 12));
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(loginButton);
    buttonLayout->addWidget(resetButton);
    buttonLayout->addWidget(guestButton);
    buttonLayout->addStretch(1);
    panelLayout->addLayout(buttonLayout);

    panelLayout->addSpacing(scaledInt(panel, 12));

    connect(loginButton, &QPushButton::clicked, this, &LTZK::authenticateFromHomePage);
    connect(guestButton, &QPushButton::clicked, this, [this]()
    {
        if (!closeAllPluginFeatureWindows() || !resetProductManagerPluginSession())
        {
            statusBar()->showMessage(
                QStringLiteral("请先保存并关闭尚未关闭的功能窗口，再进入匿名模式。"),
                5000);
            return;
        }

        if (!clearRemoteLayersForGuest())
        {
            statusBar()->showMessage(
                QStringLiteral("请先保存或结束远程图层的编辑，再进入匿名模式。"),
                5000);
            return;
        }

        gCurrentUserSession.clear();
        gCurrentUserSession.isGuest = true;
        publishCurrentUserSession();
        if (m_dataCatalogWidget)
        {
            m_dataCatalogWidget->applyMenuDatabaseConnection(QVariantMap());
        }
        if (m_homeLoginUserNameEdit)
        {
            m_homeLoginUserNameEdit->clear();
        }
        if (m_homeLoginPasswordEdit)
        {
            m_homeLoginPasswordEdit->clear();
        }
        if (m_homeLoginStatusLabel)
        {
            m_homeLoginStatusLabel->clear();
        }

        refreshAuthUiState();
        showHomeModulesPage();
        statusBar()->showMessage(QStringLiteral("已进入匿名模式，可使用本地功能。"), 4000);
    });
    connect(resetButton, &QPushButton::clicked, this, [this]()
        {
            if (m_homeLoginUserNameEdit)
            {
                m_homeLoginUserNameEdit->clear();
                m_homeLoginUserNameEdit->setFocus();
            }
            if (m_homeLoginPasswordEdit)
            {
                m_homeLoginPasswordEdit->clear();
            }
            if (m_homeLoginStatusLabel)
            {
                m_homeLoginStatusLabel->clear();
            }
        });
    connect(m_homeLoginPasswordEdit, &QLineEdit::returnPressed, this, &LTZK::authenticateFromHomePage);

    QFrame* ambientBand = new QFrame(page);
    ambientBand->setObjectName(QStringLiteral("homeLoginAmbientBand"));
    ambientBand->setFixedHeight(scaledInt(page, 38));
    ambientBand->setMaximumWidth(scaledInt(page, 1280));
    ambientBand->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QFrame* lowerTelemetry = new QFrame(page);
    lowerTelemetry->setObjectName(QStringLiteral("homeLoginLowerTelemetry"));
    lowerTelemetry->setFixedHeight(scaledInt(page, 26));
    lowerTelemetry->setMaximumWidth(scaledInt(page, 1180));
    lowerTelemetry->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QVBoxLayout* centerColumnLayout = new QVBoxLayout(page);
    centerColumnLayout->setContentsMargins(0, 0, 0, 0);
    centerColumnLayout->setSpacing(0);
    centerColumnLayout->addStretch(1);
    centerColumnLayout->addWidget(ambientBand, 0, Qt::AlignHCenter);
    centerColumnLayout->addSpacing(scaledInt(page, 20));
    centerColumnLayout->addWidget(panel, 0, Qt::AlignHCenter);
    centerColumnLayout->addSpacing(scaledInt(page, 22));
    centerColumnLayout->addWidget(lowerTelemetry, 0, Qt::AlignHCenter);
    centerColumnLayout->addStretch(2);

    page->refreshScaledMetrics();
    return page;
}

QWidget* LTZK::buildHomeModulesPanel(QWidget* parent)
{
    HomeModulesPanel* panel = new HomeModulesPanel(parent);
    panel->setObjectName(QStringLiteral("homeModulesPage"));

    QVBoxLayout* stageLayout = new QVBoxLayout(panel);
    stageLayout->setContentsMargins(0, 0, 0, 0);
    stageLayout->setSpacing(scaledInt(panel, 10));

    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(scaledInt(panel, kHomeGridHorizontalSpacing));
    grid->setVerticalSpacing(scaledInt(panel, kHomeGridVerticalSpacing));
    QToolButton* achievementButton = createModuleLaunchButton(
        QStringLiteral("地图成果管理"),
        QStringLiteral("01"),
        QStringLiteral("成果存储 · 版本追踪"),
        QStringLiteral("moduleButton_mapAchievementManagement"),
        QIcon(QStringLiteral(":/LTZK/Resources/module_map_achievement.png")));
    panel->registerModuleButton(achievementButton);
    grid->addWidget(achievementButton, 0, 0, Qt::AlignCenter);

    QToolButton* generalizationButton = createModuleLaunchButton(
        QStringLiteral("地图智能制图"),
        QStringLiteral("02"),
        QStringLiteral("智能缩编·一体成图"),
        QStringLiteral("moduleButton_mapGeneralization"),
        QIcon(QStringLiteral(":/LTZK/Resources/module_map_generalization.png")));
    panel->registerModuleButton(generalizationButton);
    grid->addWidget(generalizationButton, 0, 1, Qt::AlignCenter);

    QToolButton* qualityButton = createModuleLaunchButton(
        QStringLiteral("地图成果质检"),
        QStringLiteral("03"),
        QStringLiteral("自动跑批 · 合规审查"),
        QStringLiteral("moduleButton_mapQuality"),
        QIcon(QStringLiteral(":/LTZK/Resources/module_map_quality.png")));
    panel->registerModuleButton(qualityButton);
    grid->addWidget(qualityButton, 1, 0, Qt::AlignCenter);

    QToolButton* serviceButton = createModuleLaunchButton(
        QStringLiteral("地图服务管理"),
        QStringLiteral("04"),
        QStringLiteral("服务台账 · 统计分析"),
        QStringLiteral("moduleButton_mapServiceManagement"),
        QIcon(QStringLiteral(":/LTZK/Resources/module_map_service.png")));
    panel->registerModuleButton(serviceButton);
    grid->addWidget(serviceButton, 1, 1, Qt::AlignCenter);
    stageLayout->addStretch(1);
    QSpacerItem* introGap = new QSpacerItem(
        0,
        scaledInt(panel, 10),
        QSizePolicy::Minimum,
        QSizePolicy::Fixed);
    stageLayout->addItem(introGap);
    stageLayout->addLayout(grid, 0);
    stageLayout->setAlignment(grid, Qt::AlignHCenter);
    QSpacerItem* utilityGap = new QSpacerItem(
        0,
        scaledInt(panel, kHomeGridToUtilityGap),
        QSizePolicy::Minimum,
        QSizePolicy::Fixed);
    stageLayout->addItem(utilityGap);

    m_homeAccessControlButton = new QToolButton(panel);
    m_homeAccessControlButton->setObjectName(QStringLiteral("moduleButton_accessControlAction"));
    m_homeAccessControlButton->setProperty("homeUtilityButton", true);
    m_homeAccessControlButton->setText(QStringLiteral("权限管理"));
    QFont homeUtilityButtonFont = m_homeAccessControlButton->font();
    homeUtilityButtonFont.setPixelSize(scaledInt(panel, kHomeUtilityButtonFontSize));
    homeUtilityButtonFont.setWeight(QFont::DemiBold);
    m_homeAccessControlButton->setFont(homeUtilityButtonFont);
    m_homeAccessControlButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_homeAccessControlButton->setFixedSize(scaledSize(panel, kHomeUtilityButtonWidth, kHomeUtilityButtonHeight));
    m_homeAccessControlButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_homeAccessControlButton->setCursor(Qt::PointingHandCursor);
    m_homeAccessControlButton->setVisible(false);
    connect(m_homeAccessControlButton, &QToolButton::clicked, this, &LTZK::showAccessControlPage);

    m_homeLogoutButton = new QToolButton(panel);
    m_homeLogoutButton->setObjectName(QStringLiteral("moduleButton_logout"));
    m_homeLogoutButton->setProperty("homeUtilityButton", true);
    m_homeLogoutButton->setText(QStringLiteral("退出账号"));
    m_homeLogoutButton->setFont(homeUtilityButtonFont);
    m_homeLogoutButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_homeLogoutButton->setFixedSize(scaledSize(panel, kHomeUtilityButtonWidth, kHomeUtilityButtonHeight));
    m_homeLogoutButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_homeLogoutButton->setCursor(Qt::PointingHandCursor);
    m_homeLogoutButton->setVisible(false);
    connect(m_homeLogoutButton, &QToolButton::clicked, this, &LTZK::logoutCurrentUser);

    QHBoxLayout* homeUtilityActionsLayout = new QHBoxLayout();
    homeUtilityActionsLayout->setContentsMargins(0, 0, 0, 0);
    homeUtilityActionsLayout->setSpacing(scaledInt(panel, 16));
    homeUtilityActionsLayout->addWidget(m_homeAccessControlButton);
    homeUtilityActionsLayout->addWidget(m_homeLogoutButton);
    stageLayout->addLayout(homeUtilityActionsLayout, 0);
    stageLayout->setAlignment(homeUtilityActionsLayout, Qt::AlignHCenter);
    QSpacerItem* bottomGap = new QSpacerItem(
        0,
        scaledInt(panel, kHomeUtilityBottomGap),
        QSizePolicy::Minimum,
        QSizePolicy::Fixed);
    stageLayout->addItem(bottomGap);
    panel->configureScaleAwareLayout(
        stageLayout,
        grid,
        homeUtilityActionsLayout,
        introGap,
        utilityGap,
        bottomGap);
    updateHomeUtilityButtonMetrics();
    return panel;
}

void LTZK::updateHomeUtilityButtonMetrics()
{
    QWidget* scaleWidget = m_homeModulesPage;
    if (!scaleWidget && m_homeAccessControlButton)
    {
        scaleWidget = m_homeAccessControlButton->parentWidget();
    }
    if (!scaleWidget)
    {
        scaleWidget = this;
    }

    const QSize buttonSize = scaledSize(scaleWidget, kHomeUtilityButtonWidth, kHomeUtilityButtonHeight);
    const QList<QToolButton*> buttons = {
        m_homeAccessControlButton,
        m_homeLogoutButton
    };

    for (QToolButton* button : buttons)
    {
        if (!button)
        {
            continue;
        }
        button->setMinimumSize(buttonSize);
        button->setMaximumSize(buttonSize);
        button->resize(buttonSize);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        QFont font = button->font();
        font.setPixelSize(scaledInt(scaleWidget, kHomeUtilityButtonFontSize));
        font.setWeight(QFont::DemiBold);
        button->setFont(font);
        button->updateGeometry();
    }
}

QWidget* LTZK::buildAccessControlPage()
{
    QWidget* page = new QWidget(this);
    page->setObjectName(QStringLiteral("accessControlPage"));
    const QString accessControlBaseStyleSheet = QStringLiteral(
        "#accessControlEditor { "
        "background: rgba(8, 26, 43, 230); "
        "color: #EAF6FF; "
        "}"
        "#accessControlEditor QLabel { "
        "color: #EAF6FF; "
        "font-family: \"Microsoft YaHei UI\"; "
        "font-size: 14px; "
        "}"
        "#accessControlEditor QLineEdit, #accessControlEditor QPlainTextEdit, #accessControlEditor QComboBox { "
        "color: #FFFFFF; "
        "background: #06111d; "
        "border: 1px solid rgba(46, 168, 223, 150); "
        "padding: 6px 8px; "
        "selection-background-color: #147db5; "
        "}"
        "#accessControlEditor QTableWidget { "
        "background: #071827; "
        "alternate-background-color: #0b2238; "
        "color: #EAF6FF; "
        "gridline-color: rgba(46, 168, 223, 80); "
        "border: 1px solid rgba(46, 168, 223, 110); "
        "}"
        "#accessControlEditor QPushButton, #accessControlTopBar QToolButton { "
        "color: #FFFFFF; "
        "background: #0b4a78; "
        "border: 1px solid #2ea8df; "
        "border-radius: 4px; "
        "padding: 7px 16px; "
        "font-family: \"Microsoft YaHei UI\"; "
        "font-size: 14px; "
        "font-weight: 600; "
        "}"
        "#accessControlEditor QPushButton:hover, #accessControlTopBar QToolButton:hover { "
        "background: #0f619a; "
        "border-color: #59d6ff; "
        "}");
    page->setProperty("ltzkAccessControlBaseStyleSheet", accessControlBaseStyleSheet);
    QString accessControlStyle = platformStyleSheet();
    if (m_currentTheme == PlatformTheme::SciFi)
    {
        accessControlStyle += QLatin1Char('\n');
        accessControlStyle += LTZKUiScale::scaleStyleSheetPixels(
            accessControlBaseStyleSheet,
            LTZKUiScale::factorForWidget(page));
    }
    page->setStyleSheet(accessControlStyle);

    QVBoxLayout* rootLayout = new QVBoxLayout(page);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QFrame* topBar = new QFrame(page);
    topBar->setObjectName(QStringLiteral("accessControlTopBar"));
    QHBoxLayout* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(scaledMargins(topBar, 18, 10, 18, 10));
    topLayout->setSpacing(scaledInt(topBar, 12));

    QLabel* titleLabel = new QLabel(QStringLiteral("权限管理子平台"), topBar);
    titleLabel->setObjectName(QStringLiteral("dashboardTitle"));
    titleLabel->setAttribute(Qt::WA_TranslucentBackground);
    titleLabel->setAutoFillBackground(false);
    titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QGraphicsDropShadowEffect* accessTitleGlow = new QGraphicsDropShadowEffect(titleLabel);
    accessTitleGlow->setBlurRadius(scaledInt(titleLabel, 14));
    accessTitleGlow->setColor(QColor(86, 185, 220, 105));
    accessTitleGlow->setOffset(0, 0);
    titleLabel->setGraphicsEffect(accessTitleGlow);
    topLayout->addWidget(titleLabel, 1);

    QToolButton* backButton = new QToolButton(topBar);
    backButton->setObjectName(QStringLiteral("accessControlBackButton"));
    backButton->setText(QStringLiteral("返回首页"));
    backButton->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/template_icon_return_home.svg")));
    backButton->setIconSize(scaledSize(topBar, 20, 20));
    backButton->setCursor(Qt::PointingHandCursor);
    backButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(backButton, &QToolButton::clicked, this, [this]() {
        showHomeModulesPage();
    });
    topLayout->addWidget(backButton);

    QToolButton* logoutButton = new QToolButton(topBar);
    logoutButton->setObjectName(QStringLiteral("accessControlLogoutButton"));
    logoutButton->setText(QStringLiteral("退出账号"));
    logoutButton->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/template_icon_logout.svg")));
    logoutButton->setIconSize(scaledSize(topBar, 20, 20));
    logoutButton->setCursor(Qt::PointingHandCursor);
    logoutButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(logoutButton, &QToolButton::clicked, this, &LTZK::logoutCurrentUser);
    topLayout->addWidget(logoutButton);

    rootLayout->addWidget(topBar);

    m_accessControlWidget = new LTZKAccessControlDialog(page);
    rootLayout->addWidget(m_accessControlWidget, 1);

    return page;
}
QWidget* LTZK::buildSciFiWorkspacePage()
{
    QWidget* page = new QWidget(this);
    page->setObjectName(QStringLiteral("sciFiWorkspacePage"));
    page->setStyleSheet(platformStyleSheet());

    QVBoxLayout* rootLayout = new QVBoxLayout(page);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QFrame* topBar = new QFrame(page);
    topBar->setObjectName(QStringLiteral("dashboardTopBar"));
    QHBoxLayout* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(scaledMargins(topBar, 18, 8, 18, 8));
    topLayout->setSpacing(scaledInt(topBar, 12));

    QWidget* titleContainer = new QWidget(topBar);
    titleContainer->setObjectName(QStringLiteral("workspaceTitleContainer"));
    QHBoxLayout* titleLayout = new QHBoxLayout(titleContainer);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(0);
    m_activeModuleTitle = new QLabel(QStringLiteral("地图成果质检子平台"), titleContainer);
    m_activeModuleTitle->setObjectName(QStringLiteral("dashboardTitle"));
    m_activeModuleTitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QGraphicsDropShadowEffect* titleGlow = new QGraphicsDropShadowEffect(m_activeModuleTitle);
    titleGlow->setColor(QColor(86, 185, 220, 105));
    titleGlow->setBlurRadius(14);
    titleGlow->setOffset(0, 0);
    m_activeModuleTitle->setGraphicsEffect(titleGlow);
    titleLayout->addWidget(m_activeModuleTitle);
    topLayout->addWidget(titleContainer, 0, Qt::AlignLeft | Qt::AlignVCenter);
    QWidget* topActionStrip = new QWidget(topBar);
    topActionStrip->setObjectName(QStringLiteral("workspaceTopActionStrip"));
    topActionStrip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QHBoxLayout* topActionLayout = new QHBoxLayout(topActionStrip);
    topActionLayout->setContentsMargins(0, 0, 0, 0);
    topActionLayout->setSpacing(scaledInt(topActionStrip, 6));

    WorkspaceTopBarGradientScrollArea* pluginScrollArea =
        new WorkspaceTopBarGradientScrollArea(topActionStrip);
    pluginScrollArea->setObjectName(QStringLiteral("workspacePluginActionScrollArea"));
    pluginScrollArea->setWidgetResizable(false);
    pluginScrollArea->setFrameShape(QFrame::NoFrame);
    pluginScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    pluginScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pluginScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    pluginScrollArea->setMinimumWidth(scaledInt(topActionStrip, 720));
    const int topActionButtonHeight = topBarScaledInt(topActionStrip, 84);
    const int topActionContentHeight = topActionButtonHeight;
    pluginScrollArea->setFixedHeight(topActionButtonHeight);
    pluginScrollArea->setBaseContentHeight(topActionContentHeight);
    pluginScrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea#workspacePluginActionScrollArea { "
        "background: transparent; "
        "border: none; "
        "}"
        "QWidget#workspaceTopActionScrollContent, "
        "QWidget#workspacePluginActionStrip { "
        "background: transparent; "
        "}"
    ));
    pluginScrollArea->viewport()->setAutoFillBackground(false);
    pluginScrollArea->viewport()->setProperty(
        "topBarThemeBackgroundActive",
        hasWorkspaceTopBarThemeBackground(m_currentBusinessModuleTitle));
    pluginScrollArea->viewport()->setProperty("topBarThemeKey", platformThemeKey(m_currentTheme));

    QWidget* topActionScrollContent = new QWidget(pluginScrollArea);
    topActionScrollContent->setObjectName(QStringLiteral("workspaceTopActionScrollContent"));
    topActionScrollContent->setFixedHeight(topActionContentHeight);
    topActionScrollContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QHBoxLayout* topActionScrollContentLayout = new QHBoxLayout(topActionScrollContent);
    topActionScrollContentLayout->setContentsMargins(0, 0, 0, 0);
    topActionScrollContentLayout->setSpacing(scaledInt(topActionStrip, 6));
    topActionScrollContentLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    QWidget* pluginStrip = new QWidget(topActionScrollContent);
    pluginStrip->setObjectName(QStringLiteral("workspacePluginActionStrip"));
    pluginStrip->setFixedHeight(topActionContentHeight);
    pluginStrip->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    QHBoxLayout* pluginStripLayout = new QHBoxLayout(pluginStrip);
    pluginStripLayout->setContentsMargins(0, 0, 0, 0);
    pluginStripLayout->setSpacing(scaledInt(topActionStrip, 6));
    pluginStripLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    topActionScrollContentLayout->addWidget(pluginStrip, 0, Qt::AlignVCenter);
    pluginScrollArea->setWidget(topActionScrollContent);
    topActionLayout->addWidget(pluginScrollArea, 1, Qt::AlignVCenter);
    m_workspaceLogoutButton = new QToolButton(topActionScrollContent);
    m_workspaceLogoutButton->setObjectName(QStringLiteral("workspaceLogoutButton"));
    m_workspaceLogoutButton->setProperty("topBarTool", true);
    installTopBarFloatingHoverEffect(m_workspaceLogoutButton);
    m_workspaceLogoutButton->setText(QStringLiteral("\u9000\u51FA\u8D26\u53F7"));
    m_workspaceLogoutButton->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/template_icon_logout.svg")));
    m_workspaceLogoutButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setTopBarToolButtonResponsiveMetrics(m_workspaceLogoutButton, topActionStrip);
    m_workspaceLogoutButton->setCursor(Qt::PointingHandCursor);
    m_workspaceLogoutButton->setVisible(false);
    connect(m_workspaceLogoutButton, &QToolButton::clicked, this, &LTZK::logoutCurrentUser);
    topActionScrollContentLayout->addWidget(m_workspaceLogoutButton, 0, Qt::AlignVCenter);

    QToolButton* backHomeButton = new QToolButton(topActionScrollContent);
    backHomeButton->setObjectName(QStringLiteral("workspaceReturnHomeButton"));
    backHomeButton->setProperty("topBarTool", true);
    installTopBarFloatingHoverEffect(backHomeButton);
    backHomeButton->setText(QStringLiteral("\u8FD4\u56DE\u9996\u9875"));
    backHomeButton->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/template_icon_return_home.svg")));
    backHomeButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setTopBarToolButtonResponsiveMetrics(backHomeButton, topActionStrip);
    backHomeButton->setCursor(Qt::PointingHandCursor);
    connect(backHomeButton, &QToolButton::clicked, this, [this]() {
        showHomeModulesPage();
    });
    topActionScrollContentLayout->addWidget(backHomeButton, 0, Qt::AlignVCenter);
    topActionScrollContentLayout->addStretch(1);
    topLayout->addWidget(topActionStrip, 1);
    rootLayout->addWidget(topBar);

    QSplitter* workspaceSplitter = new QSplitter(Qt::Horizontal, page);
    workspaceSplitter->setObjectName(QStringLiteral("workspaceSplitter"));
    workspaceSplitter->setContentsMargins(scaledMargins(page, 12, 12, 12, 12));
    workspaceSplitter->setHandleWidth(splitterHandleWidth(page));
    workspaceSplitter->setChildrenCollapsible(false);

    QFrame* layerManagerPanel = new QFrame(page);
    layerManagerPanel->setObjectName(QStringLiteral("workspaceLayerManagerPanel"));
    layerManagerPanel->setProperty("dashboardPanel", true);
    layerManagerPanel->setMinimumWidth(scaledInt(page, 260));
    QVBoxLayout* layerManagerLayout = new QVBoxLayout(layerManagerPanel);
    layerManagerLayout->setContentsMargins(scaledMargins(layerManagerPanel, 8, 8, 8, 8));
    layerManagerLayout->setSpacing(0);

    m_workspaceSideTabs = new QTabWidget(layerManagerPanel);
    m_workspaceSideTabs->setObjectName(QStringLiteral("workspaceSideTabs"));
    m_workspaceSideTabs->setTabPosition(QTabWidget::South);
    m_workspaceSideTabs->setDocumentMode(true);

    m_dataCatalogWidget = new DataCatalogWidget(m_workspaceSideTabs);
    m_dataCatalogWidget->setLoadCallback([this](const QgsMimeDataUtils::Uri& uri) {
        loadCatalogDataSource(uri);
        });
    m_dataCatalogWidget->setDataClipCallback([this]() {
        executeConfiguredPluginFeature(
            QStringLiteral("map_generalization_tools"),
            QStringLiteral("数据裁剪"));
        });
    m_dataCatalogWidget->setDataImportCallback([this](int targetDirectoryId) {
        QVariantMap request;
        request.insert(QStringLiteral("operation"), QStringLiteral("import"));
        request.insert(QStringLiteral("directoryId"), targetDirectoryId);
        request.insert(
            QStringLiteral("connection"),
            m_dataCatalogWidget ? m_dataCatalogWidget->databaseConnectionParameters()
                                : QVariantMap());
        QCoreApplication::instance()->setProperty(
            "ltzkProductCatalogRequest",
            request);
        QCoreApplication::instance()->setProperty(
            "ltzkProductImportTargetDirId",
            targetDirectoryId);
        executeConfiguredPluginFeature(
            QStringLiteral("map_product_manager_tools"),
            QStringLiteral("数据导入"));
        QCoreApplication::instance()->setProperty(
            "ltzkProductCatalogRequest", QVariant());
        QCoreApplication::instance()->setProperty(
            "ltzkProductImportTargetDirId", QVariant());
        QTimer::singleShot(0, this, [this]()
        {
            if (m_dataCatalogWidget)
            {
            m_dataCatalogWidget->refreshDatabaseData();
            }
        });
        });
    m_dataCatalogWidget->setProductOperationCallback(
        [this](const QString& operation,
               int directoryId,
               int productId,
               const QList<int>& productIds)
        {
            QVariantList ids;
            for (const int id : productIds)
            {
                ids.append(id);
            }
            QVariantMap request;
            request.insert(QStringLiteral("operation"), operation);
            request.insert(QStringLiteral("directoryId"), directoryId);
            request.insert(QStringLiteral("productId"), productId);
            request.insert(QStringLiteral("productIds"), ids);
            request.insert(
                QStringLiteral("connection"),
                m_dataCatalogWidget ? m_dataCatalogWidget->databaseConnectionParameters()
                                    : QVariantMap());
            QCoreApplication::instance()->setProperty(
                "ltzkProductCatalogRequest",
                request);
            QCoreApplication::instance()->setProperty(
                "ltzkProductImportTargetDirId",
                operation == QStringLiteral("import") ? directoryId : 0);
            executeConfiguredPluginFeature(
                QStringLiteral("map_product_manager_tools"),
                QStringLiteral("成果目录操作"));
            QCoreApplication::instance()->setProperty(
                "ltzkProductCatalogRequest", QVariant());
            QCoreApplication::instance()->setProperty(
                "ltzkProductImportTargetDirId", QVariant());
            QTimer::singleShot(0, this, [this]()
            {
                if (m_dataCatalogWidget)
                {
                    m_dataCatalogWidget->refreshDatabaseData();
                }
            });
        });
    m_dataCatalogWidget->setLocalProductOperationCallback(
        [this](const QString& operation,
               const QgsMimeDataUtils::UriList& localUris)
        {
            QString errorMessage;
            if (!LocalProductOperation::dispatch(
                    operation,
                    localUris,
                    m_mapCanvas,
                    this,
                    &errorMessage) &&
                !errorMessage.isEmpty())
            {
                logLtzKMessage(
                    QStringLiteral("Local product operation failed: %1")
                        .arg(errorMessage),
                    Qgis::Warning);
            }
        });
    m_workspaceSideTabs->addTab(m_dataCatalogWidget, QStringLiteral("数据列表"));
    if (m_layerTreePanelWidget)
    {
        m_workspaceSideTabs->addTab(m_layerTreePanelWidget, QStringLiteral("图层管理"));
        m_workspaceSideTabs->setCurrentWidget(m_layerTreePanelWidget);
    }
    else if (m_layerTreeView)
    {
        m_workspaceSideTabs->addTab(m_layerTreeView, QStringLiteral("图层管理"));
        m_workspaceSideTabs->setCurrentWidget(m_layerTreeView);
    }
    updateDataCatalogVisibility(m_currentBusinessModuleTitle);
    layerManagerLayout->addWidget(m_workspaceSideTabs, 1);

    workspaceSplitter->addWidget(layerManagerPanel);
    QFrame* mapPanel = new QFrame(page);
    mapPanel->setObjectName(QStringLiteral("workspaceMapPanel"));
    QVBoxLayout* mapLayout = new QVBoxLayout(mapPanel);
    mapLayout->setContentsMargins(scaledMargins(mapPanel, 4, 4, 4, 4));
    mapLayout->setSpacing(0);

    m_mapCanvasToolBarHost = new QMainWindow(mapPanel);
    m_mapCanvasToolBarHost->setObjectName(QStringLiteral("mapCanvasToolBarHost"));
    m_mapCanvasToolBarHost->setWindowFlags(Qt::Widget);
    m_mapCanvasToolBarHost->setContentsMargins(0, 0, 0, 0);
    m_mapCanvasToolBarHost->setDockNestingEnabled(true);
    m_mapCanvasToolBarHost->setCentralWidget(m_mapCanvas);
    mapLayout->addWidget(m_mapCanvasToolBarHost, 1);
    initWorkspaceMapStatusBar(mapLayout);
    workspaceSplitter->addWidget(mapPanel);
    workspaceSplitter->setStretchFactor(0, 0);
    workspaceSplitter->setStretchFactor(1, 1);
    workspaceSplitter->setCollapsible(0, false);
    workspaceSplitter->setCollapsible(1, false);
    workspaceSplitter->setSizes(QList<int>() << 320 << 1600);

    QWidget* layerManagerHandle = workspaceSplitter->handle(1);
    layerManagerHandle->setObjectName(QStringLiteral("workspaceLayerManagerSplitterHandle"));
    QGridLayout* layerHandleLayout = new QGridLayout(layerManagerHandle);
    layerHandleLayout->setContentsMargins(0, 0, 0, 0);
    layerHandleLayout->setSpacing(0);
    SplitterTriangleButton* layerManagerCollapseButton = new SplitterTriangleButton(Qt::LeftArrow, layerManagerHandle);
    layerManagerCollapseButton->setObjectName(QStringLiteral("workspaceLayerManagerCollapseButton"));
    layerManagerCollapseButton->setProperty("panelCollapseButton", true);
    layerManagerCollapseButton->setFixedSize(splitterTriangleButtonSize(page));
    layerManagerCollapseButton->setToolTip(QStringLiteral("隐藏/展开工作区侧边栏"));
    QFrame* layerManagerHandleRail = new QFrame(layerManagerHandle);
    layerManagerHandleRail->setObjectName(QStringLiteral("workspaceLayerManagerHandleRail"));
    layerManagerHandleRail->setProperty("splitterHandleRail", true);
    layerManagerHandleRail->setFixedWidth(splitterHandleRailWidth(page));
    layerManagerHandleRail->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    layerHandleLayout->addWidget(layerManagerHandleRail, 0, 0);
    layerHandleLayout->addWidget(layerManagerCollapseButton, 0, 0, Qt::AlignCenter);
    layerManagerCollapseButton->raise();

    const int collapsedLayerWidth = 0;
    int expandedLayerWidth = scaledInt(page, 320);
    int expandedRightWidth = scaledInt(page, 1600);
    connect(layerManagerCollapseButton, &QToolButton::clicked, this, [=]() mutable {
        const QList<int> currentSizes = workspaceSplitter->sizes();
        if (layerManagerPanel->maximumWidth() != collapsedLayerWidth)
        {
            expandedLayerWidth = qMax(currentSizes.value(0, expandedLayerWidth), scaledInt(page, 260));
            expandedRightWidth = qMax(currentSizes.value(1, expandedRightWidth), scaledInt(page, 1600));
            layerManagerPanel->setMinimumWidth(collapsedLayerWidth);
            layerManagerPanel->setMaximumWidth(collapsedLayerWidth);
            workspaceSplitter->setSizes(QList<int>() << collapsedLayerWidth << qMax(expandedRightWidth, scaledInt(page, 1600)));
            layerManagerCollapseButton->setTriangleDirection(Qt::RightArrow);
        }
        else
        {
            layerManagerPanel->setMaximumWidth(QWIDGETSIZE_MAX);
            layerManagerPanel->setMinimumWidth(scaledInt(page, 260));
            workspaceSplitter->setSizes(QList<int>() << qMax(expandedLayerWidth, scaledInt(page, 260)) << qMax(expandedRightWidth, scaledInt(page, 1600)));
            layerManagerCollapseButton->setTriangleDirection(Qt::LeftArrow);
        }
    });

    m_workspaceContentStack = new MapServiceWorkspace(workspaceSplitter, page);
    rootLayout->addWidget(m_workspaceContentStack, 1);
    startupTrace(
        QStringLiteral(
            "buildSciFiWorkspacePage end"));
    return page;
}
QToolButton* LTZK::createWorkspaceToolButton(const QString& title, const QString& objectName, const QIcon& icon)
{
    QToolButton* button = new QToolButton(this);
    button->setObjectName(objectName);
    button->setProperty("workspaceTool", true);
    button->setText(title);
    button->setIcon(icon);
    button->setIconSize(scaledSize(button, 28, 28));
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setMinimumHeight(scaledInt(button, 68));
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QToolButton* LTZK::createTopBarToolButton(
    QAction* action,
    QWidget* parent)
{
    startupTrace(
        QStringLiteral(
            "createTopBarToolButton begin"));

    QToolButton* button =
        new QToolButton(
            parent ? parent : this);

    startupTrace(
        QStringLiteral(
            "createTopBarToolButton after new QToolButton"));

    button->setProperty(
        "topBarTool",
        true);
    installTopBarFloatingHoverEffect(button);

    button->setToolButtonStyle(
        Qt::ToolButtonTextUnderIcon);

    setTopBarToolButtonResponsiveMetrics(button, parent ? parent : this);

    button->setCursor(
        Qt::PointingHandCursor);

    button->setAutoRaise(false);

    if (action)
    {
        startupTrace(
            QStringLiteral(
                "createTopBarToolButton before setDefaultAction"));

        button->setDefaultAction(action);

        button->setText(
            topBarButtonText(
                action->text()));

        startupTrace(
            QStringLiteral(
                "createTopBarToolButton after setDefaultAction"));

        button->setToolTip(
            action->toolTip().isEmpty()
            ? action->text()
            : action->toolTip());

        startupTrace(
            QStringLiteral(
                "createTopBarToolButton after setToolTip"));

        button->setStatusTip(
            action->statusTip());

        startupTrace(
            QStringLiteral(
                "createTopBarToolButton after setStatusTip"));
    }

    startupTrace(
        QStringLiteral(
            "createTopBarToolButton end"));

    return button;
}
QToolButton* LTZK::createModuleLaunchButton(
    const QString& moduleTitle,
    const QString& moduleCode,
    const QString& subtitle,
    const QString& objectName,
    const QIcon& icon)
{
    const bool isHomeModuleButton =
        objectName.startsWith(QStringLiteral("moduleButton_"));

    QToolButton* button = nullptr;

    if (isHomeModuleButton)
    {
        button = new ResponsiveHomeModuleButton(
            moduleTitle,
            moduleCode,
            subtitle,
            objectName,
            icon,
            this);
    }
    else
    {
        button = new QToolButton(this);
        button->setObjectName(objectName);
        button->setProperty("moduleButton", true);
        button->setText(
            moduleCode +
            QStringLiteral("\n") +
            moduleTitle +
            QStringLiteral("\n") +
            subtitle);
        button->setIcon(icon);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(scaledSize(button, 48, 48));
        button->setMinimumHeight(scaledInt(button, 130));
        button->setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Expanding);
    }

    button->setCursor(Qt::PointingHandCursor);

    connect(
        button,
        &QToolButton::clicked,
        this,
        [this, moduleTitle]()
        {
            if (moduleTitle == QStringLiteral("权限管理"))
            {
                showAccessControlPage();
                return;
            }
            activateBusinessModule(moduleTitle);
        });

    return button;
}
QAction* LTZK::firstExecutableActionForMenu(const QString& menuTitle) const
{
    for (QAction* menuAction : menuBar()->actions())
    {
        QMenu* menu = menuAction ? menuAction->menu() : nullptr;
        if (!menu || menu->title() != menuTitle)
        {
            continue;
        }

        QList<QAction*> actions;
        collectPluginExecutableActions(menu, actions);
        return actions.isEmpty() ? nullptr : actions.first();
    }

    return nullptr;
}

void LTZK::refreshModuleActionsFromMenus()
{
    m_moduleActions.clear();
    const QStringList moduleTitles = {
        QStringLiteral("\u667A\u80FD\u5236\u56FE"),
        QStringLiteral("\u5730\u56FE\u6210\u679C\u8D28\u68C0"),
        QStringLiteral("\u5730\u56FE\u6210\u679C\u7BA1\u7406"),
        QStringLiteral("\u5730\u56FE\u670D\u52A1\u7BA1\u7406")
    };

    for (const QString& moduleTitle : moduleTitles)
    {
        QAction* action = firstExecutableActionForMenu(moduleTitle);
        if (action)
        {
            m_moduleActions.insert(moduleTitle, action);

        }
    }
}

QList<QAction*> LTZK::configuredFeatureActionsForModule(const QString& moduleTitle) const
{
    QList<QAction*> actions;
    if (moduleTitle.isEmpty())
    {
        return actions;
    }

    for (QAction* action : findChildren<QAction*>())
    {
        if (!action || !action->objectName().startsWith(QStringLiteral("pluginFeature_")))
        {
            continue;
        }

        bool belongsToModule = false;
        for (const PluginDefinition& plugin : m_pluginBindingConfig.plugins)
        {
            if (!plugin.enabled)
            {
                continue;
            }

            for (const FeatureBinding& binding : plugin.bindings)
            {
                if (binding.enabled &&
                    !binding.menuPath.isEmpty() &&
                    businessModuleMenuTitle(binding.menuPath.first()) == businessModuleMenuTitle(moduleTitle) &&
                    action->objectName() == QStringLiteral("pluginFeature_%1_%2").arg(plugin.id, binding.featureName))
                {
                    belongsToModule = true;
                    break;
                }
            }

            if (belongsToModule)
            {
                break;
            }
        }

        if (belongsToModule && action->isVisible() && !actions.contains(action))
        {
            actions.append(action);
        }
    }

    return actions;
}

void LTZK::setWorkspacePluginActions(const QList<QAction*>& actions)
{
    if (m_pluginToolBar)
    {
        m_pluginToolBar->clear();
        for (QAction* featureAction : actions)
        {
            if (featureAction && featureAction->isVisible())
            {
                m_pluginToolBar->addAction(featureAction);
            }
        }
        m_pluginToolBar->hide();
    }

    QWidget* pluginStrip = m_mapCanvasPage ? m_mapCanvasPage->findChild<QWidget*>(QStringLiteral("workspacePluginActionStrip")) : nullptr;
    QHBoxLayout* pluginLayout = pluginStrip ? qobject_cast<QHBoxLayout*>(pluginStrip->layout()) : nullptr;
    if (!pluginLayout)
    {
        return;
    }

    while (QLayoutItem* item = pluginLayout->takeAt(0))
    {
        if (QWidget* widget = item->widget())
        {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    for (QAction* featureAction : actions)
    {
        if (!featureAction || !featureAction->isVisible())
        {
            continue;
        }

        QToolButton* featureButton = createTopBarToolButton(featureAction, pluginStrip);
        featureButton->setObjectName(QStringLiteral("workspacePluginAction_%1").arg(featureAction->objectName()));
        featureButton->setEnabled(featureAction->isEnabled());
        setWorkspacePluginToolButtonCompactMetrics(featureButton, pluginStrip);
        pluginLayout->addWidget(featureButton, 0, Qt::AlignVCenter);
    }

    pluginLayout->activate();
    pluginStrip->adjustSize();
    pluginLayout->activate();
    pluginStrip->updateGeometry();
    pluginStrip->show();
    if (QWidget* topActionScrollContent = pluginStrip->parentWidget())
    {
        if (QLayout* contentLayout = topActionScrollContent->layout())
        {
            contentLayout->activate();
        }
        topActionScrollContent->adjustSize();
        topActionScrollContent->updateGeometry();
        if (QScrollArea* scrollArea = qobject_cast<QScrollArea*>(topActionScrollContent->parentWidget()))
        {
            if (QScrollBar* horizontalScrollBar = scrollArea->horizontalScrollBar())
            {
                horizontalScrollBar->setValue(0);
            }
            scrollArea->viewport()->update();
        }
    }
    if (m_mapCanvasPage)
    {
        refreshWorkspaceScaledMetrics();
    }
}

void LTZK::updateWorkspaceTopBarThemeBackground(const QString& moduleTitle)
{
    QScrollArea* scrollArea = m_mapCanvasPage
        ? m_mapCanvasPage->findChild<QScrollArea*>(QStringLiteral("workspacePluginActionScrollArea"))
        : nullptr;
    if (!scrollArea)
    {
        return;
    }

    const bool active = hasWorkspaceTopBarThemeBackground(moduleTitle);
    scrollArea->viewport()->setProperty("topBarThemeBackgroundActive", active);
    scrollArea->viewport()->setProperty("topBarThemeKey", platformThemeKey(m_currentTheme));
    scrollArea->viewport()->update();
}

void LTZK::updateDataCatalogVisibility(const QString& moduleTitle)
{
    if (!m_workspaceSideTabs || !m_dataCatalogWidget)
    {
        return;
    }

    const int dataCatalogTabIndex = m_workspaceSideTabs->indexOf(m_dataCatalogWidget);
    if (dataCatalogTabIndex < 0)
    {
        return;
    }

    const bool visible = isDataCatalogVisibleModuleTitle(moduleTitle);
    m_workspaceSideTabs->setTabPosition(visible ? QTabWidget::South : QTabWidget::North);
    if (QTabBar* sideTabBar = m_workspaceSideTabs->tabBar())
    {
        sideTabBar->setExpanding(!visible);
    }
    if (!visible && m_workspaceSideTabs->currentWidget() == m_dataCatalogWidget)
    {
        if (m_layerTreePanelWidget
            && m_workspaceSideTabs->indexOf(m_layerTreePanelWidget) >= 0)
        {
            m_workspaceSideTabs->setCurrentWidget(m_layerTreePanelWidget);
        }
        else if (m_layerTreeView
                 && m_workspaceSideTabs->indexOf(m_layerTreeView) >= 0)
        {
            m_workspaceSideTabs->setCurrentWidget(m_layerTreeView);
        }
    }

    m_workspaceSideTabs->setTabVisible(dataCatalogTabIndex, visible);
    if (moduleTitle == QStringLiteral("\u5730\u56fe\u6210\u679c\u7ba1\u7406"))
    {
        m_workspaceSideTabs->setCurrentWidget(m_dataCatalogWidget);
    }
}

void LTZK::activateBusinessModule(
    const QString& moduleTitle)
{
    if (!ensureAuthenticatedForModule(moduleTitle))
    {
        return;
    }

    if (!m_workspaceStack || !m_mapCanvasPage)
    {
        return;
    }

    const QString previousModuleTitle = m_currentBusinessModuleTitle;
    if (previousModuleTitle != moduleTitle)
    {
        if (!leaveCurrentBusinessModule())
        {
            return;
        }

        if (m_qualityResultScanTimer && m_qualityResultScanTimer->isActive())
        {
            m_qualityResultScanTimer->stop();
        }
        if (m_qualityResultOutputWatcher)
        {
            const QStringList watchedDirectories = m_qualityResultOutputWatcher->directories();
            if (!watchedDirectories.isEmpty())
            {
                m_qualityResultOutputWatcher->removePaths(watchedDirectories);
            }
        }
        m_qualityResultOutputDirs.clear();
        m_qualityResultMonitoringModuleTitle.clear();
        m_qualityResultOrganization.clear();
        m_qualityResultMonitoringWindow.clear();
        m_qualityResultWindowBindingPending = false;
        m_qualityResultPostActionScanActive = false;
        m_qualityResultPostActionScanPending = false;
        m_qualityResultScanReady = false;
        m_qualityResultPostActionScanAttempts = 0;
        m_qualityResultFileSignatures.clear();
        m_qualityResultStablePaths.clear();
    }

    m_workspaceStack->setCurrentWidget(m_mapCanvasPage);
    if (previousModuleTitle != moduleTitle)
    {
        ++m_businessModuleGeneration;
    }
    m_currentBusinessModuleTitle = moduleTitle;
    const bool isMapServiceModule = moduleTitle == QStringLiteral("地图服务管理");
    if (m_workspaceContentStack)
    {
        m_workspaceContentStack->setServiceMode(isMapServiceModule);
    }
    setWorkspaceToolbarsActive(!isMapServiceModule && gCurrentUserSession.isLoggedIn);
    if (m_CurrentCoordinage)
    {
        m_CurrentCoordinage->setVisible(!isMapServiceModule);
    }
    updateWorkspaceTopBarThemeBackground(moduleTitle);
    updateDataCatalogVisibility(moduleTitle);

    if (!isMapServiceModule)
    {
        loadLocalGansuShpBasemap();
    }

    if (m_activeModuleTitle)
    {
        m_activeModuleTitle->setText(QStringLiteral("%1子平台").arg(moduleTitle));
    }

    QMenu* selectedModuleMenu = nullptr;
    for (QAction* menuAction : menuBar()->actions())
    {
        QMenu* menu = menuAction ? menuAction->menu() : nullptr;
        if (menu && businessModuleMenuTitle(menu->title()) == businessModuleMenuTitle(moduleTitle))
        {
            selectedModuleMenu = menu;
            break;
        }
    }

    QList<QAction*> moduleFeatureActions;
    if (selectedModuleMenu)
    {
        collectPluginExecutableActions(selectedModuleMenu, moduleFeatureActions);
    }

    QList<QAction*> workspacePluginActions = configuredFeatureActionsForModule(moduleTitle);
    if (workspacePluginActions.isEmpty())
    {
        workspacePluginActions = moduleFeatureActions;
    }
    setWorkspacePluginActions(isMapServiceModule ? QList<QAction*>() : workspacePluginActions);

    const bool isResultModule = isResultModuleTitle(moduleTitle);
    if (!isResultModule)
    {
        if (m_qualityResultScanTimer && m_qualityResultScanTimer->isActive())
        {
            m_qualityResultScanTimer->stop();
        }
        m_qualityResultPostActionScanActive = false;
        m_qualityResultPostActionScanPending = false;
        m_qualityResultScanReady = false;
        m_qualityResultPostActionScanAttempts = 0;
        m_qualityResultFileSignatures.clear();
        m_qualityResultStablePaths.clear();

        hideQualityResultCanvasLayers();
    }

    if (isMapServiceModule)
    {
        statusBar()->showMessage(QStringLiteral("已进入地图服务管理子平台。"), 6000);
        executeConfiguredPluginFeature(
            QStringLiteral("map_product_manager_tools"),
            QStringLiteral("地图服务管理"));
        return;
    }

    const int layerCount = QgsProject::instance()->layerTreeRoot()->findLayers().size();
    statusBar()->showMessage(
        QStringLiteral("已进入 %1 工作台，地图画布显示当前数据，图层管理显示检查结果。当前图层数：%2")
        .arg(moduleTitle)
        .arg(layerCount),
        6000);

    if (moduleFeatureActions.isEmpty())
    {
        logLtzKMessage(QStringLiteral("Module has no configured feature action: %1").arg(moduleTitle), Qgis::Info);
    }
}
void LTZK::scheduleQualityResultPostActionScan()
{
    if (m_qualityResultMonitoringModuleTitle.isEmpty()
        || m_qualityResultMonitoringModuleTitle != m_currentBusinessModuleTitle
        || !isResultModuleTitle(m_currentBusinessModuleTitle)
        || !m_qualityResultScanTimer)
    {
        return;
    }

    m_qualityResultPostActionScanActive = false;
    m_qualityResultPostActionScanPending = true;
    m_qualityResultScanReady = false;
    m_qualityResultPostActionScanAttempts = 0;
    m_qualityResultFileSignatures.clear();
    m_qualityResultStablePaths.clear();
    if (m_qualityResultScanTimer->isActive())
    {
        m_qualityResultScanTimer->stop();
    }

    QTimer::singleShot(0, this, [this]()
    {
        if (m_qualityResultPostActionScanPending && !hasRunningQualityResultWorker())
        {
            armQualityResultScanAfterCompletion();
        }
    });
}

bool LTZK::hasRunningQualityResultWorker() const
{
    if (!m_qualityResultMonitoringWindow)
    {
        return false;
    }

    const QList<QThread*> workerThreads = m_qualityResultMonitoringWindow->findChildren<QThread*>();
    for (QThread* workerThread : workerThreads)
    {
        if (workerThread && workerThread->isRunning())
        {
            return true;
        }
    }

    return false;
}

void LTZK::armQualityResultScanAfterCompletion()
{
    if (!m_qualityResultPostActionScanPending
        || m_qualityResultMonitoringModuleTitle.isEmpty()
        || m_qualityResultMonitoringModuleTitle != m_currentBusinessModuleTitle
        || !isResultModuleTitle(m_currentBusinessModuleTitle)
        || !m_qualityResultScanTimer
        || hasRunningQualityResultWorker()
        || (m_qualityResultWindowBindingPending && !m_qualityResultMonitoringWindow))
    {
        return;
    }

    m_qualityResultPostActionScanPending = false;
    m_qualityResultPostActionScanActive = true;
    m_qualityResultScanReady = true;
    m_qualityResultPostActionScanAttempts = 0;
    refreshQualityResultOutputDirectories();
    updateQualityResultOutputWatcher();
    m_qualityResultScanTimer->start();
}

void LTZK::beginQualityResultMonitoring(
    const QString& moduleTitle,
    const QString& resultOrganization)
{
    const QString normalizedModuleTitle = moduleTitle.trimmed();
    if (normalizedModuleTitle.isEmpty()
        || !isResultModuleTitle(normalizedModuleTitle)
        || normalizedModuleTitle != m_currentBusinessModuleTitle)
    {
        if (m_qualityResultScanTimer && m_qualityResultScanTimer->isActive())
        {
            m_qualityResultScanTimer->stop();
        }
        if (m_qualityResultOutputWatcher)
        {
            const QStringList watchedDirectories = m_qualityResultOutputWatcher->directories();
            if (!watchedDirectories.isEmpty())
            {
                m_qualityResultOutputWatcher->removePaths(watchedDirectories);
            }
        }
        m_qualityResultOutputDirs.clear();
        m_qualityResultMonitoringModuleTitle.clear();
        m_qualityResultOrganization.clear();
        m_qualityResultMonitoringWindow.clear();
        m_qualityResultWindowBindingPending = false;
        m_qualityResultPostActionScanActive = false;
        m_qualityResultPostActionScanPending = false;
        m_qualityResultScanReady = false;
        m_qualityResultPostActionScanAttempts = 0;
        m_qualityResultFileSignatures.clear();
        m_qualityResultStablePaths.clear();
        m_removedQualityResultGroupPaths.clear();
        return;
    }

    if (m_qualityResultScanTimer && m_qualityResultScanTimer->isActive())
    {
        m_qualityResultScanTimer->stop();
    }
    if (m_qualityResultOutputWatcher)
    {
        const QStringList watchedDirectories = m_qualityResultOutputWatcher->directories();
        if (!watchedDirectories.isEmpty())
        {
            m_qualityResultOutputWatcher->removePaths(watchedDirectories);
        }
    }
    m_qualityResultMonitoringModuleTitle = normalizedModuleTitle;
    m_qualityResultOrganization = resultOrganization.trimmed();
    m_qualityResultOutputDirs.clear();
    m_qualityResultMonitoringWindow.clear();
    m_qualityResultWindowBindingPending = true;
    m_qualityResultPostActionScanActive = false;
    m_qualityResultPostActionScanPending = false;
    m_qualityResultScanReady = false;
    m_qualityResultPostActionScanAttempts = 0;
    m_qualityResultFileSignatures.clear();
    m_qualityResultStablePaths.clear();
    m_removedQualityResultGroupPaths.clear();
    m_qualityResultScanStartedAt = QDateTime::currentDateTime().addSecs(-15);
}

void LTZK::bindQualityResultOutputWindow(QWidget* window)
{
    if (!window || window == this || !window->isWindow()
        || m_qualityResultMonitoringModuleTitle.isEmpty()
        || m_qualityResultMonitoringModuleTitle != m_currentBusinessModuleTitle
        || (!m_qualityResultWindowBindingPending && m_qualityResultMonitoringWindow != window))
    {
        return;
    }

    QList<QLineEdit*> outputPathControls;
    const QList<QLineEdit*> lineEdits = window->findChildren<QLineEdit*>();
    for (QLineEdit* lineEdit : lineEdits)
    {
        if (lineEdit && isQualityResultOutputPathControl(lineEdit, window))
        {
            outputPathControls.append(lineEdit);
        }
    }
    if (outputPathControls.isEmpty())
    {
        return;
    }

    if (m_qualityResultMonitoringWindow != window)
    {
        if (m_qualityResultOutputWatcher)
        {
            const QStringList watchedDirectories = m_qualityResultOutputWatcher->directories();
            if (!watchedDirectories.isEmpty())
            {
                m_qualityResultOutputWatcher->removePaths(watchedDirectories);
            }
        }
        m_qualityResultOutputDirs.clear();
        m_qualityResultMonitoringWindow = window;
    }
    m_qualityResultWindowBindingPending = false;

    if (!window->property("ltzkResultCompletionWindowMonitoringBound").toBool())
    {
        window->setProperty("ltzkResultCompletionWindowMonitoringBound", true);
        connect(window, &QObject::destroyed, this, [this]()
        {
            if (m_qualityResultPostActionScanPending && !hasRunningQualityResultWorker())
            {
                armQualityResultScanAfterCompletion();
            }
        });
    }

    for (QLineEdit* lineEdit : outputPathControls)
    {
        if (lineEdit->property("ltzkResultOutputPathMonitoringBound").toBool())
        {
            continue;
        }
        lineEdit->setProperty("ltzkResultOutputPathMonitoringBound", true);
        QPointer<QLineEdit> selectedOutputControl(lineEdit);
        connect(lineEdit, &QLineEdit::textChanged, this, [this, selectedOutputControl](const QString&)
        {
            if (selectedOutputControl
                && m_qualityResultMonitoringWindow
                && selectedOutputControl->window() == m_qualityResultMonitoringWindow)
            {
                refreshQualityResultOutputDirectories();
            }
        });
    }

    const QList<QThread*> workerThreads = window->findChildren<QThread*>();
    for (QThread* workerThread : workerThreads)
    {
        if (!workerThread || workerThread->property("ltzkResultWorkerMonitoringBound").toBool())
        {
            continue;
        }
        workerThread->setProperty("ltzkResultWorkerMonitoringBound", true);
        connect(workerThread, &QThread::finished, this, &LTZK::handleQualityResultWorkerFinished);
    }

    refreshQualityResultOutputDirectories();
    if (m_qualityResultPostActionScanPending && !hasRunningQualityResultWorker())
    {
        armQualityResultScanAfterCompletion();
    }
}

void LTZK::refreshQualityResultOutputDirectories()
{
    if (m_qualityResultMonitoringModuleTitle.isEmpty()
        || m_qualityResultMonitoringModuleTitle != m_currentBusinessModuleTitle)
    {
        m_qualityResultOutputDirs.clear();
        updateQualityResultOutputWatcher();
        return;
    }

    QWidget* monitoredWindow = m_qualityResultMonitoringWindow.data();
    if (!monitoredWindow)
    {
        return;
    }

    QStringList currentOutputDirs;
    const QList<QLineEdit*> lineEdits = monitoredWindow->findChildren<QLineEdit*>();
    for (QLineEdit* lineEdit : lineEdits)
    {
        if (!lineEdit)
        {
            continue;
        }

        if (!isQualityResultOutputPathControl(lineEdit, monitoredWindow))
        {
            continue;
        }

        const QString text = lineEdit->text().trimmed();
        if (!text.isEmpty() && QFileInfo(text).isDir())
        {
            appendUniqueDir(currentOutputDirs, text);
        }
    }

    m_qualityResultOutputDirs = currentOutputDirs;
    updateQualityResultOutputWatcher();
}

void LTZK::updateQualityResultOutputWatcher()
{
    if (!m_qualityResultOutputWatcher)
    {
        return;
    }

    const QStringList watchedDirectories = m_qualityResultOutputWatcher->directories();
    for (const QString& watchedDirectory : watchedDirectories)
    {
        bool keepWatching = false;
        for (const QString& selectedOutputDir : m_qualityResultOutputDirs)
        {
            if (canonicalOrAbsolutePath(watchedDirectory).compare(
                canonicalOrAbsolutePath(selectedOutputDir), Qt::CaseInsensitive) == 0)
            {
                keepWatching = true;
                break;
            }
        }
        if (!keepWatching)
        {
            m_qualityResultOutputWatcher->removePath(watchedDirectory);
        }
    }

    QStringList directoriesToWatch;
    for (const QString& selectedOutputDir : m_qualityResultOutputDirs)
    {
        if (!QFileInfo(selectedOutputDir).isDir())
        {
            continue;
        }

        appendUniqueDir(directoriesToWatch, selectedOutputDir);
    }

    for (const QString& selectedOutputDir : directoriesToWatch)
    {
        if (!m_qualityResultOutputWatcher->directories().contains(selectedOutputDir, Qt::CaseInsensitive))
        {
            m_qualityResultOutputWatcher->addPath(selectedOutputDir);
        }
    }
}

void LTZK::handleQualityResultDirectoryChanged(const QString& path)
{
    if (m_qualityResultMonitoringModuleTitle.isEmpty()
        || m_qualityResultMonitoringModuleTitle != m_currentBusinessModuleTitle)
    {
        return;
    }

    const QString changedPath = canonicalOrAbsolutePath(path).toLower();
    bool selectedDirectoryChanged = false;
    for (const QString& selectedOutputDir : m_qualityResultOutputDirs)
    {
        const QString selectedOutputDirPath = canonicalOrAbsolutePath(selectedOutputDir).toLower();
        if (changedPath == selectedOutputDirPath
            || changedPath.startsWith(selectedOutputDirPath + QLatin1Char('/')))
        {
            selectedDirectoryChanged = true;
            break;
        }
    }
    if (!selectedDirectoryChanged)
    {
        return;
    }

    updateQualityResultOutputWatcher();
    if (m_qualityResultScanReady && m_qualityResultPostActionScanActive && m_qualityResultScanTimer)
    {
        m_qualityResultScanTimer->start();
    }
}

void LTZK::handleQualityResultWorkerFinished()
{
    if (m_qualityResultMonitoringModuleTitle.isEmpty()
        || m_qualityResultMonitoringModuleTitle != m_currentBusinessModuleTitle)
    {
        return;
    }

    if (!m_qualityResultPostActionScanPending)
    {
        return;
    }

    if (hasRunningQualityResultWorker())
    {
        return;
    }

    refreshQualityResultOutputDirectories();
    updateQualityResultOutputWatcher();
    armQualityResultScanAfterCompletion();
}

QgsVectorLayer* LTZK::addPluginVectorLayer(
    const QString& vectorLayerPath,
    const QString& baseName,
    const QString& providerKey)
{
    QgsVectorLayer* layer = new QgsVectorLayer(vectorLayerPath, baseName, providerKey);
    if (!layer->isValid())
    {
        delete layer;
        return nullptr;
    }

    refreshQualityResultOutputDirectories();
    const QString sourcePath = localPathFromLayerSource(layer->source());
    if (isWaterTaskIntermediate(sourcePath))
    {
        delete layer;
        return nullptr;
    }
    const bool isQualityResult = isResultModuleTitle(m_currentBusinessModuleTitle)
        && QFileInfo(sourcePath).suffix().compare(QStringLiteral("shp"), Qt::CaseInsensitive) == 0
        && isQualityResultLayerInSelectedOutputDirs(sourcePath);
    if (isQualityResult)
    {
        registerQualityResultLayer(layer, m_currentBusinessModuleTitle);
        QgsProject::instance()->addMapLayer(layer, false);
        addQualityResultLayerToTree(layer);
    }
    else
    {
        QgsProject::instance()->addMapLayer(layer);
    }

    return layer;
}

bool LTZK::isQualityResultLayerInSelectedOutputDirs(const QString& sourcePath) const
{
    if (usesMapQualityErrorResultOrganization())
    {
        return isQualityResultLayerInSearchRoots(
            sourcePath,
            collectMapQualityErrorSearchRoots());
    }

    return isQualityResultLayerInSearchRoots(sourcePath, m_qualityResultOutputDirs);
}

bool LTZK::isQualityResultShpFileStable(const QString& shpPath)
{
    const QFileInfo shpInfo(shpPath);
    if (!shpInfo.isFile()
        || shpInfo.suffix().compare(QStringLiteral("shp"), Qt::CaseInsensitive) != 0)
    {
        return false;
    }

    const QString basePath = QDir::fromNativeSeparators(shpInfo.absolutePath())
        + QLatin1Char('/') + shpInfo.completeBaseName();
    const QStringList sidecarSuffixes = {
        QStringLiteral("shp"),
        QStringLiteral("shx"),
        QStringLiteral("dbf"),
        QStringLiteral("prj"),
        QStringLiteral("cpg")
    };

    bool requiredSidecarsPresent = true;
    QStringList signatureParts;
    for (const QString& suffix : sidecarSuffixes)
    {
        const QFileInfo sidecarInfo(basePath + QLatin1Char('.') + suffix);
        const bool exists = sidecarInfo.isFile();
        if ((suffix == QStringLiteral("shp")
             || suffix == QStringLiteral("shx")
             || suffix == QStringLiteral("dbf"))
            && !exists)
        {
            requiredSidecarsPresent = false;
        }

        signatureParts << QStringLiteral("%1:%2:%3:%4")
            .arg(suffix)
            .arg(exists ? 1 : 0)
            .arg(exists ? sidecarInfo.size() : 0)
            .arg(exists ? sidecarInfo.lastModified().toMSecsSinceEpoch() : 0);
    }

    const QString canonicalPath = canonicalOrAbsolutePath(shpPath);
    const QString signature = signatureParts.join(QLatin1Char('|'));
    const QString previousSignature = m_qualityResultFileSignatures.value(canonicalPath);
    m_qualityResultFileSignatures.insert(canonicalPath, signature);
    return requiredSidecarsPresent
        && !previousSignature.isEmpty()
        && previousSignature == signature;
}

bool LTZK::registerQualityResultLayer(QgsVectorLayer* layer, const QString& moduleTitle)
{
    if (!layer || !layer->isValid() || !isResultModuleTitle(moduleTitle))
    {
        return false;
    }

    const QString sourcePath = localPathFromLayerSource(layer->source());
    if (isWaterTaskIntermediate(sourcePath) ||
        QFileInfo(sourcePath).suffix().compare(QStringLiteral("shp"), Qt::CaseInsensitive) != 0)
    {
        return false;
    }

    const QString canonicalPath = canonicalOrAbsolutePath(sourcePath);
    if (canonicalPath.isEmpty())
    {
        return false;
    }

    const bool isNewResultLayer = !m_qualityResultLayers.contains(canonicalPath);
    layer->setCustomProperty(QStringLiteral("ltzk/qualityResultShp"), true);
    layer->setCustomProperty(QStringLiteral("ltzk/qualityResultPath"), canonicalPath);
    m_qualityResultLayers.insert(canonicalPath, layer);
    m_resultLayerModules.insert(canonicalPath, moduleTitle);
    return isNewResultLayer;
}

void LTZK::rememberQualityResultGroupPaths(QgsLayerTreeGroup* group)
{
    if (!group)
    {
        return;
    }

    if (group->customProperty(QStringLiteral("ltzk/qualityResultFolder")).toBool())
    {
        const QString folderPath = group->customProperty(
            QStringLiteral("ltzk/qualityResultFolderPath")).toString().trimmed();
        if (!folderPath.isEmpty())
        {
            m_removedQualityResultGroupPaths.insert(
                QDir::fromNativeSeparators(canonicalOrAbsolutePath(folderPath)).toLower());
        }
    }

    for (QgsLayerTreeNode* child : group->children())
    {
        if (!QgsLayerTree::isGroup(child))
        {
            continue;
        }

        QgsLayerTreeGroup* childGroup = asLayerTreeGroupNode(child);
        rememberQualityResultGroupPaths(childGroup);
    }
}

bool LTZK::isQualityResultGroupPathSuppressed(const QString& folderPath) const
{
    const QString normalizedPath = folderPath.trimmed().isEmpty()
        ? QString()
        : QDir::fromNativeSeparators(canonicalOrAbsolutePath(folderPath)).toLower();
    if (normalizedPath.isEmpty())
    {
        return false;
    }

    for (const QString& removedPath : m_removedQualityResultGroupPaths)
    {
        if (normalizedPath == removedPath ||
            normalizedPath.startsWith(removedPath + QLatin1Char('/')))
        {
            return true;
        }
    }
    return false;
}

QgsLayerTreeGroup* LTZK::ensureQualityResultLayerGroup(const QString& sourcePath)
{
    QgsProject* project = QgsProject::instance();
    if (!project || !project->layerTreeRoot())
    {
        return nullptr;
    }

    const QString normalizedSource = QDir::fromNativeSeparators(
        canonicalOrAbsolutePath(sourcePath));
    QString selectedRoot;
    for (const QString& outputDir : m_qualityResultOutputDirs)
    {
        QString normalizedOutput = QDir::fromNativeSeparators(
            canonicalOrAbsolutePath(outputDir));
        while (normalizedOutput.length() > 1 && normalizedOutput.endsWith(QLatin1Char('/')))
        {
            normalizedOutput.chop(1);
        }

        const bool isInside = normalizedSource.compare(normalizedOutput, Qt::CaseInsensitive) == 0
            || normalizedSource.startsWith(normalizedOutput + QLatin1Char('/'), Qt::CaseInsensitive);
        if (isInside && normalizedOutput.length() > selectedRoot.length())
        {
            selectedRoot = normalizedOutput;
        }
    }

    if (selectedRoot.isEmpty() && usesMapQualityErrorResultOrganization())
    {
        const QStringList errorRoots = collectMapQualityErrorSearchRoots();
        for (const QString& errorRoot : errorRoots)
        {
            const QString normalizedErrorRoot = QDir::fromNativeSeparators(
                canonicalOrAbsolutePath(errorRoot));
            if (normalizedErrorRoot.isEmpty()
                || !(normalizedSource.compare(normalizedErrorRoot, Qt::CaseInsensitive) == 0
                     || normalizedSource.startsWith(
                         normalizedErrorRoot + QLatin1Char('/'), Qt::CaseInsensitive)))
            {
                continue;
            }

            const QString mapRoot = canonicalOrAbsolutePath(
                QFileInfo(errorRoot).dir().absolutePath());
            for (const QString& outputDir : m_qualityResultOutputDirs)
            {
                const QString commonRoot = commonDirectoryAncestor(outputDir, mapRoot);
                if (commonRoot.length() > selectedRoot.length())
                {
                    selectedRoot = commonRoot;
                }
            }

            if (selectedRoot.isEmpty())
            {
                selectedRoot = canonicalOrAbsolutePath(
                    QFileInfo(mapRoot).dir().absolutePath());
            }
        }
    }

    QgsLayerTreeGroup* parentGroup = asLayerTreeGroupNode(project->layerTreeRoot());
    if (selectedRoot.isEmpty())
    {
        return parentGroup;
    }

    QString relativePath = normalizedSource.mid(selectedRoot.length());
    while (relativePath.startsWith(QLatin1Char('/')))
    {
        relativePath.remove(0, 1);
    }
    QStringList pathParts = relativePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (!pathParts.isEmpty())
    {
        pathParts.removeLast();
    }

    QString currentFolderPath = selectedRoot;
    for (const QString& folderName : pathParts)
    {
        currentFolderPath += QLatin1Char('/') + folderName;
        const QString normalizedFolderPath = canonicalOrAbsolutePath(currentFolderPath);
        if (isQualityResultGroupPathSuppressed(normalizedFolderPath))
        {
            return nullptr;
        }

        QgsLayerTreeGroup* childGroup = nullptr;
        for (QgsLayerTreeNode* child : parentGroup->children())
        {
            if (!child || child->nodeType() != QgsLayerTreeNode::NodeGroup)
            {
                continue;
            }

            QgsLayerTreeGroup* candidate = asLayerTreeGroupNode(child);
            if (candidate->customProperty(QStringLiteral("ltzk/qualityResultFolder")).toBool()
                && canonicalOrAbsolutePath(candidate->customProperty(
                    QStringLiteral("ltzk/qualityResultFolderPath")).toString())
                    .compare(normalizedFolderPath, Qt::CaseInsensitive) == 0)
            {
                childGroup = candidate;
                break;
            }
        }

        if (!childGroup)
        {
            childGroup = parentGroup->addGroup(folderName);
            childGroup->setCustomProperty(QStringLiteral("ltzk/qualityResultFolder"), true);
            childGroup->setCustomProperty(
                QStringLiteral("ltzk/qualityResultFolderPath"), normalizedFolderPath);
        }

        childGroup->setExpanded(true);
        parentGroup = childGroup;
    }

    return parentGroup;
}

bool LTZK::moveQualityResultLayerToGroupSafely(QgsVectorLayer* layer)
{
    if (!layer || !layer->isValid())
    {
        return false;
    }

    QgsProject* project = QgsProject::instance();
    QgsLayerTree* root = project ? project->layerTreeRoot() : nullptr;
    QgsLayerTreeGroup* targetGroup = ensureQualityResultLayerGroup(
        localPathFromLayerSource(layer->source()));
    if (!root || !targetGroup)
    {
        return false;
    }

    QgsLayerTreeLayer* layerNode = root->findLayer(layer->id());
    if (!layerNode)
    {
        targetGroup->addLayer(layer);
        targetGroup->setExpanded(true);
        return true;
    }

    QgsLayerTreeGroup* currentParent = asLayerTreeGroupNode(layerNode->parent());
    if (!currentParent || currentParent == targetGroup)
    {
        return false;
    }

    QgsLayerTreeRegistryBridge* bridge = project->layerTreeRegistryBridge();
    const bool bridgeWasEnabled = bridge && bridge->isEnabled();
    if (bridge)
    {
        bridge->setEnabled(false);
    }

    const bool detached = currentParent->takeChild(layerNode);
    if (detached)
    {
        targetGroup->addChildNode(layerNode);
        targetGroup->setExpanded(true);
    }

    if (bridge)
    {
        bridge->setEnabled(bridgeWasEnabled);
    }

    return detached;
}

void LTZK::addQualityResultLayerToTree(QgsVectorLayer* layer)
{
    if (!layer || !layer->isValid())
    {
        return;
    }

    QgsProject* project = QgsProject::instance();
    QgsLayerTree* root = project ? project->layerTreeRoot() : nullptr;
    QgsLayerTreeGroup* targetGroup = ensureQualityResultLayerGroup(
        localPathFromLayerSource(layer->source()));
    if (!root || !targetGroup)
    {
        return;
    }

    if (!root->findLayer(layer->id()))
    {
        targetGroup->addLayer(layer);
        targetGroup->setExpanded(true);
        return;
    }

    moveQualityResultLayerToGroupSafely(layer);
}

int LTZK::removeMapQualitySeriesIntermediateResultNodes()
{
    if (!usesMapQualityErrorResultOrganization())
    {
        return 0;
    }

    QgsProject* project = QgsProject::instance();
    QgsLayerTreeGroup* root = project
        ? asLayerTreeGroupNode(project->layerTreeRoot())
        : nullptr;
    if (!project || !root)
    {
        return 0;
    }

    const QString seriesGroupName = QStringLiteral("\u7CFB\u5217\u56FE\u68C0\u67E5");
    const QString intermediateLayerBaseName = QStringLiteral("merged_index");
    const QStringList canonicalSearchRoots = collectMapQualityErrorSearchRoots();
    const auto containsCanonicalResult = [this, &canonicalSearchRoots](
        auto&& self,
        QgsLayerTreeGroup* group) -> bool
    {
        if (!group)
        {
            return false;
        }

        for (QgsLayerTreeNode* child : group->children())
        {
            if (!child)
            {
                continue;
            }

            if (QgsLayerTree::isLayer(child))
            {
                QgsLayerTreeLayer* layerNode = asLayerTreeLayerNode(child);
                QgsMapLayer* layer = layerNode->layer();
                const QString sourcePath = layer
                    ? localPathFromLayerSource(layer->source())
                    : QString();
                if (isQualityResultLayerInSearchRoots(sourcePath, canonicalSearchRoots))
                {
                    return true;
                }
                continue;
            }

            if (QgsLayerTree::isGroup(child))
            {
                QgsLayerTreeGroup* childGroup = asLayerTreeGroupNode(child);
                if (self(self, childGroup))
                {
                    return true;
                }
            }
        }

        return false;
    };
    const auto containsIntermediateResult = [this, &canonicalSearchRoots, &intermediateLayerBaseName](
        auto&& self,
        QgsLayerTreeGroup* group) -> bool
    {
        if (!group)
        {
            return false;
        }

        for (QgsLayerTreeNode* child : group->children())
        {
            if (!child)
            {
                continue;
            }

            if (QgsLayerTree::isLayer(child))
            {
                QgsLayerTreeLayer* layerNode = asLayerTreeLayerNode(child);
                QgsMapLayer* layer = layerNode->layer();
                const QString sourcePath = layer
                    ? localPathFromLayerSource(layer->source())
                    : QString();
                const QFileInfo sourceInfo(sourcePath);
                if (sourceInfo.suffix().compare(QStringLiteral("shp"), Qt::CaseInsensitive) == 0
                    && sourceInfo.completeBaseName().compare(
                        intermediateLayerBaseName, Qt::CaseInsensitive) == 0
                    && !isQualityResultLayerInSearchRoots(sourcePath, canonicalSearchRoots))
                {
                    return true;
                }
                continue;
            }

            if (QgsLayerTree::isGroup(child))
            {
                QgsLayerTreeGroup* childGroup = asLayerTreeGroupNode(child);
                if (self(self, childGroup))
                {
                    return true;
                }
            }
        }

        return false;
    };
    QgsLayerTreeRegistryBridge* bridge = project->layerTreeRegistryBridge();
    const bool bridgeWasEnabled = bridge && bridge->isEnabled();
    if (bridge)
    {
        bridge->setEnabled(false);
    }

    int removedGroupCount = 0;
    const QList<QgsLayerTreeNode*> rootChildren = root->children();
    for (QgsLayerTreeNode* child : rootChildren)
    {
        if (!child || child->nodeType() != QgsLayerTreeNode::NodeGroup)
        {
            continue;
        }

        QgsLayerTreeGroup* childGroup = asLayerTreeGroupNode(child);
        if (!childGroup
            || childGroup->name().compare(seriesGroupName, Qt::CaseSensitive) != 0
            || !containsIntermediateResult(containsIntermediateResult, childGroup)
            || containsCanonicalResult(containsCanonicalResult, childGroup))
        {
            continue;
        }

        root->removeChildNode(childGroup);
        ++removedGroupCount;
    }

    if (bridge)
    {
        bridge->setEnabled(bridgeWasEnabled);
    }

    if (removedGroupCount > 0)
    {
        refreshMapCanvasLayerStack();
    }

    return removedGroupCount;
}

void LTZK::pruneQualityResultLayerRegistry()
{
    QgsProject* project = QgsProject::instance();
    if (!project)
    {
        return;
    }

    for (auto it = m_qualityResultLayers.begin(); it != m_qualityResultLayers.end();)
    {
        QgsVectorLayer* layer = it.value().data();
        if (!layer || project->mapLayer(layer->id()) != layer)
        {
            const QString resultPath = it.key();
            m_resultLayerModules.remove(resultPath);
            m_qualityResultVisiblePaths.remove(resultPath);
            it = m_qualityResultLayers.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

QStringList LTZK::collectQualityResultSearchRoots() const
{
    if (usesMapQualityErrorResultOrganization())
    {
        return collectMapQualityErrorSearchRoots();
    }

    return m_qualityResultOutputDirs;
}

bool LTZK::usesMapQualityErrorResultOrganization() const
{
    return m_qualityResultOrganization.compare(
        QStringLiteral("mapQualityError"), Qt::CaseInsensitive) == 0;
}

QStringList LTZK::collectQualityErrorDirectories(const QString& searchBase) const
{
    QStringList resultDirectories;
    QString currentBase = canonicalOrAbsolutePath(searchBase);
    if (currentBase.isEmpty() || !QFileInfo(currentBase).isDir())
    {
        return resultDirectories;
    }

    const QString targetName = qualityErrorResultFolderName();
    for (int ancestorLevel = 0;
         ancestorLevel <= kMaxQualityErrorResultSearchAncestorLevels;
         ++ancestorLevel)
    {
        collectDirectoriesNamed(
            currentBase,
            targetName,
            0,
            kMaxQualityErrorResultSearchDepth,
            resultDirectories);

        const QString parent = canonicalOrAbsolutePath(
            QFileInfo(currentBase).dir().absolutePath());
        if (parent.isEmpty() || parent.compare(currentBase, Qt::CaseInsensitive) == 0)
        {
            break;
        }
        currentBase = parent;
    }

    return resultDirectories;
}

QStringList LTZK::collectMapQualityErrorSearchRoots() const
{
    QStringList searchRoots;
    for (const QString& outputDirectory : m_qualityResultOutputDirs)
    {
        const QStringList errorDirectories = collectQualityErrorDirectories(outputDirectory);
        for (const QString& errorDirectory : errorDirectories)
        {
            appendUniqueDir(searchRoots, errorDirectory);
        }
    }
    return searchRoots;
}

bool LTZK::isQualityResultLayerInSearchRoots(
    const QString& sourcePath,
    const QStringList& searchRoots) const
{
    if (sourcePath.trimmed().isEmpty() || isWaterTaskIntermediate(sourcePath))
    {
        return false;
    }

    const QString normalizedSource = QDir::fromNativeSeparators(
        canonicalOrAbsolutePath(sourcePath));
    if (normalizedSource.isEmpty())
    {
        return false;
    }

    for (const QString& searchRoot : searchRoots)
    {
        QString normalizedRoot = QDir::fromNativeSeparators(
            canonicalOrAbsolutePath(searchRoot));
        while (normalizedRoot.length() > 1 && normalizedRoot.endsWith(QLatin1Char('/')))
        {
            normalizedRoot.chop(1);
        }

        if (normalizedRoot.isEmpty())
        {
            continue;
        }

        if (normalizedSource.compare(normalizedRoot, Qt::CaseInsensitive) == 0
            || normalizedSource.startsWith(
                normalizedRoot + QLatin1Char('/'), Qt::CaseInsensitive))
        {
            return true;
        }
    }

    return false;
}

int LTZK::syncQualityResultShpLayers()
{
    if (m_qualityResultMonitoringModuleTitle.isEmpty()
        || m_qualityResultMonitoringModuleTitle != m_currentBusinessModuleTitle)
    {
        return 0;
    }

    const QStringList searchRoots = collectQualityResultSearchRoots();
    if (searchRoots.isEmpty())
    {
        return 0;
    }

    const auto isInCurrentSearchRoots = [this, &searchRoots](const QString& sourcePath)
    {
        return usesMapQualityErrorResultOrganization()
            ? isQualityResultLayerInSearchRoots(sourcePath, searchRoots)
            : isQualityResultLayerInSelectedOutputDirs(sourcePath);
    };

    int addedCount = 0;
    QSet<QString> existingSources;
    const QMap<QString, QgsMapLayer*> existingLayers = QgsProject::instance()->mapLayers();
    for (QgsMapLayer* existingLayer : existingLayers)
    {
        if (existingLayer && isWaterTaskIntermediate(localPathFromLayerSource(existingLayer->source()))) {
            QgsProject::instance()->removeMapLayer(existingLayer->id());
            continue;
        }
        if (!existingLayer)
        {
            continue;
        }

        const QString sourcePath = localPathFromLayerSource(existingLayer->source());
        if (!sourcePath.isEmpty())
        {
            existingSources.insert(QFileInfo(sourcePath).canonicalFilePath().toLower());
            existingSources.insert(canonicalOrAbsolutePath(sourcePath).toLower());

            QgsVectorLayer* existingVectorLayer = qobject_cast<QgsVectorLayer*>(existingLayer);
            if (existingVectorLayer
                && existingVectorLayer->isValid()
                && QFileInfo(sourcePath).suffix().compare(QStringLiteral("shp"), Qt::CaseInsensitive) == 0
                && isInCurrentSearchRoots(sourcePath)
                && isQualityResultShpFileStable(sourcePath))
            {
                const QString canonicalPath = canonicalOrAbsolutePath(sourcePath);
                const bool newlyStable = !m_qualityResultStablePaths.contains(canonicalPath);
                m_qualityResultStablePaths.insert(canonicalPath);
                const bool newlyRegistered = registerQualityResultLayer(
                    existingVectorLayer, m_currentBusinessModuleTitle);
                const bool movedToResultGroup = moveQualityResultLayerToGroupSafely(existingVectorLayer);
                if (newlyStable || newlyRegistered || movedToResultGroup)
                {
                    ++addedCount;
                }
            }
            else if (existingVectorLayer
                     && existingVectorLayer->isValid()
                     && QFileInfo(sourcePath).suffix().compare(QStringLiteral("shp"), Qt::CaseInsensitive) == 0
                     && isInCurrentSearchRoots(sourcePath))
            {
                m_qualityResultStablePaths.remove(canonicalOrAbsolutePath(sourcePath));
            }
        }
    }

    const auto removeEmptyWaterGroups = [&](const auto& self, QgsLayerTreeGroup* parent) -> void {
        if (!parent) return;
        for (QgsLayerTreeNode* node : parent->children()) {
            if (!node || node->nodeType() != QgsLayerTreeNode::NodeGroup) continue;
            QgsLayerTreeGroup* group = asLayerTreeGroupNode(node);
            if (!group) continue;
            self(self, group);
            if (group->name().startsWith(QStringLiteral(".water_task_results_"), Qt::CaseInsensitive)
                && group->findLayers().isEmpty())
                parent->removeChildNode(group);
        }
    };
    removeEmptyWaterGroups(removeEmptyWaterGroups, QgsProject::instance()->layerTreeRoot());

    for (const QString& root : searchRoots)
    {
        int scannedFiles = 0;
        QDirIterator it(root, QStringList() << QStringLiteral("*.shp"), QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            if (++scannedFiles > kMaxQualityResultScanFilesPerRoot)
            {
                logLtzKMessage(QStringLiteral("Quality result scan stopped early to avoid excessive traversal: %1").arg(root), Qgis::Warning);
                break;
            }

            const QString shpPath = it.next();            const QFileInfo shpInfo(shpPath);
            if (!isQualityResultShpPath(shpPath))
            {
                continue;
            }

            if (m_qualityResultScanStartedAt.isValid() && shpInfo.lastModified() < m_qualityResultScanStartedAt)
            {
                continue;
            }

            const QString canonicalPath = canonicalOrAbsolutePath(shpPath);
            if (existingSources.contains(canonicalPath.toLower()) || m_qualityResultLayers.contains(canonicalPath))
            {
                continue;
            }

            const bool stableFile = isQualityResultShpFileStable(shpPath);
            if (!stableFile)
            {
                m_qualityResultStablePaths.remove(canonicalPath);
                continue;
            }

            m_qualityResultStablePaths.insert(canonicalPath);

            QgsVectorLayer* layer = new QgsVectorLayer(shpPath, shpInfo.baseName(), QStringLiteral("ogr"));
            if (!layer->isValid())
            {
                delete layer;
                logLtzKMessage(QStringLiteral("Quality result shp load failed: %1").arg(shpInfo.fileName()), Qgis::Warning);
                continue;
            }

            registerQualityResultLayer(layer, m_currentBusinessModuleTitle);
            QgsProject::instance()->addMapLayer(layer, false);
            addQualityResultLayerToTree(layer);
            existingSources.insert(canonicalPath.toLower());
            ++addedCount;
        }
    }

    if (addedCount > 0)
    {
        logLtzKMessage(QStringLiteral("Check result shp layers added to result board: %1").arg(addedCount));
    }

    return addedCount;
}

int LTZK::regroupQualityResultLayersBySourcePath()
{
    if (m_qualityResultMonitoringModuleTitle.isEmpty()
        || m_qualityResultMonitoringModuleTitle != m_currentBusinessModuleTitle)
    {
        return 0;
    }

    QgsProject* project = QgsProject::instance();
    if (!project)
    {
        return 0;
    }

    int regroupedCount = 0;
    const QMap<QString, QgsMapLayer*> existingLayers = project->mapLayers();
    for (QgsMapLayer* existingLayer : existingLayers)
    {
        QgsVectorLayer* vectorLayer = qobject_cast<QgsVectorLayer*>(existingLayer);
        if (!vectorLayer || !vectorLayer->isValid())
        {
            continue;
        }

        const QString sourcePath = localPathFromLayerSource(vectorLayer->source());
        if (sourcePath.isEmpty()
            || QFileInfo(sourcePath).suffix().compare(QStringLiteral("shp"), Qt::CaseInsensitive) != 0
            || !isQualityResultLayerInSelectedOutputDirs(sourcePath))
        {
            continue;
        }

        const bool newlyRegistered = registerQualityResultLayer(
            vectorLayer, m_currentBusinessModuleTitle);
        const bool movedToResultGroup = moveQualityResultLayerToGroupSafely(vectorLayer);
        if (newlyRegistered || movedToResultGroup)
        {
            ++regroupedCount;
        }
    }

    return regroupedCount;
}

void LTZK::refreshMapCanvasLayerStack()
{
    if (!m_mapCanvas)
    {
        return;
    }

    pruneQualityResultLayerRegistry();
    QList<QgsMapLayer*> canvasLayers;
    const QList<QgsLayerTreeLayer*> layerNodes = QgsProject::instance()->layerTreeRoot()->findLayers();
    for (QgsLayerTreeLayer* layerNode : layerNodes)
    {
        QgsMapLayer* layer = layerNode ? layerNode->layer() : nullptr;
        if (!layer || !layer->isValid() || layer->customProperty(QStringLiteral("ltzk/qualityResultShp")).toBool())
        {
            continue;
        }

        if (layerNode->isVisible())
        {
            canvasLayers << layer;
        }
    }

    const bool showResultModule = isResultModuleTitle(m_currentBusinessModuleTitle);
    if (showResultModule)
    {
        for (const QString& resultPath : m_qualityResultVisiblePaths)
        {
            if (m_resultLayerModules.value(resultPath) != m_currentBusinessModuleTitle)
            {
                continue;
            }

            QgsVectorLayer* layer = m_qualityResultLayers.value(resultPath).data();
            if (layer && layer->isValid())
            {
                canvasLayers << layer;
            }
        }
    }

    const bool hasVisibleShp = visibleLayersContainShp(canvasLayers);
    applyWorkspaceShpCanvasBackground(hasVisibleShp);
    m_mapCanvas->setLayers(canvasLayers);
    m_mapCanvas->refresh();
}

void LTZK::hideQualityResultCanvasLayers()
{
    refreshMapCanvasLayerStack();
}

bool LTZK::visibleLayersContainShp(const QList<QgsMapLayer*>& layers, bool includeSystemBasemap) const
{
    for (QgsMapLayer* layer : layers)
    {
        if (!layer || (!includeSystemBasemap && isSystemBasemapLayer(layer)) || !layer->isValid())
        {
            continue;
        }

        if (layer->customProperty(QStringLiteral("ltzk/qualityResultShp")).toBool())
        {
            return true;
        }

        const QString sourcePath = localPathFromLayerSource(layer->source());
        if (QFileInfo(sourcePath).suffix().compare(QStringLiteral("shp"), Qt::CaseInsensitive) == 0)
        {
            return true;
        }
    }

    return false;
}

QColor LTZK::defaultMapCanvasColor() const
{
    switch (m_currentTheme)
    {
    case PlatformTheme::White:
        return QColor(Qt::white);
    case PlatformTheme::Black:
        return QColor(5, 5, 5);
    case PlatformTheme::SciFi:
        return QColor(QStringLiteral("#DCEFFA"));
    case PlatformTheme::FieldNotes:
        return QColor(QStringLiteral("#E4F0E2"));
    case PlatformTheme::SteelWarmWhite:
        return QColor(QStringLiteral("#E4E7E3"));
    default:
        return QColor(QStringLiteral("#DCEFFA"));
    }
}

void LTZK::applyWorkspaceShpCanvasBackground(bool useLightCanvas)
{
    m_workspaceUsesLightShpCanvas = useLightCanvas;
    if (m_mapCanvas)
    {
        if (m_hasCustomCanvasColor)
        {
            m_mapCanvas->setCanvasColor(m_customCanvasColor);
            return;
        }

        m_mapCanvas->setCanvasColor(useLightCanvas ? QColor(Qt::white) : defaultMapCanvasColor());
    }
}

void LTZK::setShpLayerDisplayColor(QgsVectorLayer* layer)
{
    if (!layer || !layer->isValid())
    {
        return;
    }

    const QString sourcePath = localPathFromLayerSource(layer->source());
    if (QFileInfo(sourcePath).suffix().compare(QStringLiteral("shp"), Qt::CaseInsensitive) != 0)
    {
        return;
    }

    const QColor defaultColor = layer->geometryType() == QgsWkbTypes::PolygonGeometry
        ? QColor(33, 150, 243)
        : QColor(230, 57, 70);
    const QColor selectedColor = chooseLocalizedColor(defaultColor, this, QStringLiteral("选择图层颜色"), platformStyleSheet());
    if (!selectedColor.isValid())
    {
        return;
    }

    switch (layer->geometryType())
    {
    case QgsWkbTypes::PointGeometry:
    {
        QVariantMap properties;
        properties.insert(QStringLiteral("name"), QStringLiteral("circle"));
        properties.insert(QStringLiteral("color"), selectedColor.name());
        properties.insert(QStringLiteral("outline_color"), selectedColor.darker(150).name());
        properties.insert(QStringLiteral("size"), QStringLiteral("2.8"));
        if (QgsMarkerSymbol* symbol = QgsMarkerSymbol::createSimple(properties))
        {
            layer->setRenderer(new QgsSingleSymbolRenderer(symbol));
        }
        break;
    }
    case QgsWkbTypes::LineGeometry:
    {
        QVariantMap properties;
        properties.insert(QStringLiteral("color"), selectedColor.name());
        properties.insert(QStringLiteral("width"), QStringLiteral("0.66"));
        if (QgsLineSymbol* symbol = QgsLineSymbol::createSimple(properties))
        {
            layer->setRenderer(new QgsSingleSymbolRenderer(symbol));
        }
        break;
    }
    case QgsWkbTypes::PolygonGeometry:
    {
        QVariantMap properties;
        properties.insert(QStringLiteral("color"), selectedColor.name());
        properties.insert(QStringLiteral("outline_color"), selectedColor.darker(150).name());
        properties.insert(QStringLiteral("outline_width"), QStringLiteral("0.36"));
        if (QgsFillSymbol* symbol = QgsFillSymbol::createSimple(properties))
        {
            layer->setRenderer(new QgsSingleSymbolRenderer(symbol));
        }
        break;
    }
    default:
        return;
    }

    layer->triggerRepaint();
    if (m_mapCanvas)
    {
        m_mapCanvas->refresh();
    }
}

void LTZK::chooseMapCanvasColor()
{
    if (!m_mapCanvas)
    {
        return;
    }

    const QColor currentColor = m_hasCustomCanvasColor
        ? m_customCanvasColor
        : (m_workspaceUsesLightShpCanvas
            ? QColor(Qt::white)
            : defaultMapCanvasColor());

    const QColor selectedColor = chooseLocalizedColor(currentColor, this, QStringLiteral("选择地图画布颜色"), platformStyleSheet());
    if (!selectedColor.isValid())
    {
        return;
    }

    m_customCanvasColor = selectedColor;
    m_hasCustomCanvasColor = true;
    m_mapCanvas->setCanvasColor(m_customCanvasColor);
    m_mapCanvas->refresh();
}
void LTZK::setQualityResultLayerVisible(const QString& resultPath, bool visible)
{
    if (resultPath.isEmpty())
    {
        return;
    }

    if (visible)
    {
        m_qualityResultVisiblePaths.insert(resultPath);
    }
    else
    {
        m_qualityResultVisiblePaths.remove(resultPath);
    }

    refreshMapCanvasLayerStack();
}

void LTZK::locateQualityResultFeature(const QString& resultPath, qint64 featureId)
{
    pruneQualityResultLayerRegistry();
    QgsVectorLayer* layer = m_qualityResultLayers.value(resultPath).data();
    if (!layer || !layer->isValid() || !m_mapCanvas)
    {
        return;
    }

    setQualityResultLayerVisible(resultPath, true);
    layer->removeSelection();
    layer->selectByIds(QgsFeatureIds() << featureId);

    QgsFeature feature;
    QgsFeatureRequest request(featureId);
    QgsFeatureIterator it = layer->getFeatures(request);
    if (!it.nextFeature(feature) || !feature.hasGeometry())
    {
        return;
    }

    QgsRectangle extent = feature.geometry().boundingBox();
    if (extent.isEmpty())
    {
        extent = layer->extent();
    }

    QgsRectangle canvasExtent = m_mapCanvas->mapSettings().layerExtentToOutputExtent(layer, extent);
    m_mapCanvas->zoomToFeatureExtent(canvasExtent);
    m_mapCanvas->refresh();
}

QString LTZK::qualityResultFeatureIdField(const QgsVectorLayer* layer) const
{
    if (!layer)
    {
        return QString();
    }

    const QStringList candidates = { QStringLiteral("id"), QStringLiteral("ID"), QStringLiteral("fid"), QStringLiteral("FID"), QStringLiteral("objectid"), QStringLiteral("OBJECTID"), QStringLiteral("要素id"), QStringLiteral("要素ID") };
    for (const QString& candidate : candidates)
    {
        if (layer->fields().indexOf(candidate) >= 0)
        {
            return candidate;
        }
    }

    return QString();
}

QString LTZK::qualityResultMessageField(const QgsVectorLayer* layer) const
{
    if (!layer)
    {
        return QString();
    }

    const QStringList candidates = { QStringLiteral("message"), QStringLiteral("MESSAGE"), QStringLiteral("Message"), QStringLiteral("msg"), QStringLiteral("MSG"), QStringLiteral("错误信息"), QStringLiteral("检查信息"), QStringLiteral("信息") };
    for (const QString& candidate : candidates)
    {
        if (layer->fields().indexOf(candidate) >= 0)
        {
            return candidate;
        }
    }

    return QString();
}

QString LTZK::qualityResultErrorTypeField(const QgsVectorLayer* layer) const
{
    if (!layer)
    {
        return QString();
    }

    const QStringList candidates = { QStringLiteral("错误类型"), QStringLiteral("问题类型"), QStringLiteral("error_type"), QStringLiteral("err_type"), QStringLiteral("error"), QStringLiteral("ERROR"), QStringLiteral("type"), QStringLiteral("TYPE"), QStringLiteral("类型") };
    for (const QString& candidate : candidates)
    {
        if (layer->fields().indexOf(candidate) >= 0)
        {
            return candidate;
        }
    }

    return QString();
}
QgsMapCanvas* LTZK::mapCanvas() const
{
    return m_mapCanvas;
}

QgsLayerTreeView* LTZK::layerTreeView() const
{
    return m_layerTreeView;
}

QgsLayerTreeMapCanvasBridge* LTZK::layerTreeCanvasBridge() const
{
    return m_layerTreeCanvasBridge;
}

QgsBrowserGuiModel* LTZK::browserModel() const
{
    return m_dataCatalogWidget ? m_dataCatalogWidget->browserModel() : m_sourceBrowserModel.data();
}

void LTZK::applyMenuDatabaseConnection(const QVariantMap& parameters)
{
    if (m_dataCatalogWidget)
    {
        m_dataCatalogWidget->applyMenuDatabaseConnection(parameters);
    }
}



QgsMapLayer* LTZK::currentMapLayer() const
{
    return m_curMapLayer;
}

QgsMessageBar* LTZK::messageBarWidget() const
{
    return m_messageBar;
}

QMenu* LTZK::pluginMenu() const
{
    return m_pluginMenu;
}

QToolBar* LTZK::pluginToolBar() const
{
    return m_pluginToolBar;
}

QMenu* LTZK::ensureMenu(const QString& title)
{
    const QString canonicalTitle = businessModuleMenuTitle(title);
    for (QAction* action : menuBar()->actions())
    {
        QMenu* existingMenu = action->menu();
        if (existingMenu && businessModuleMenuTitle(existingMenu->title()) == canonicalTitle)
        {
            setConfiguredMenuIconSize(existingMenu);
            return existingMenu;
        }
    }

    QMenu* menu = menuBar()->addMenu(canonicalTitle);
    setConfiguredMenuIconSize(menu);
    return menu;
}


void LTZK::showStartupLogin()
{
    refreshAuthUiState();

    if (gCurrentUserSession.isLoggedIn || gCurrentUserSession.isGuest)
    {
        showHomeModulesPage();
        return;
    }

    showHomeLoginPage();
}

bool LTZK::promptLoginForModule(const QString& moduleTitle)
{
    showHomeLoginPage();

    if (m_homeLoginStatusLabel)
    {
        m_homeLoginStatusLabel->setText(
            QStringLiteral("请先登录后再使用「%1」功能。").arg(moduleTitle));
    }

    statusBar()->showMessage(QStringLiteral("请先登录后再使用该功能。"), 3000);
    return false;
}

void LTZK::authenticateFromHomePage()
{
    if (m_homeLoginPending || !m_homeLoginUserNameEdit || !m_homeLoginPasswordEdit || !m_homeLoginStatusLabel)
    {
        return;
    }

    const QString userName = m_homeLoginUserNameEdit->text().trimmed();
    const QString password = m_homeLoginPasswordEdit->text();
    if (userName.isEmpty())
    {
        m_homeLoginStatusLabel->setText(QStringLiteral("请输入用户名。"));
        m_homeLoginUserNameEdit->setFocus();
        return;
    }
    if (password.isEmpty())
    {
        m_homeLoginStatusLabel->setText(QStringLiteral("请输入密码。"));
        m_homeLoginPasswordEdit->setFocus();
        return;
    }

    struct LoginResult
    {
        UserSession session;
        QString error;
        bool authenticated = false;
    };
    const auto result = std::make_shared<LoginResult>();
    m_homeLoginPending = true;
    m_homeLoginStatusLabel->setText(QStringLiteral("正在登录，请稍候…"));
    m_homeLoginPage->setEnabled(false);

    QThread* worker = QThread::create([userName, password, result]()
    {
        result->authenticated = LTZKAuthManager::instance().authenticateUser(
            userName, password, &result->session, &result->error);
        LTZKAuthManager::closeCurrentThreadConnection();
    });
    connect(worker, &QThread::finished, this, [this, worker, result, userName, password]()
    {
        m_homeLoginPending = false;
        m_homeLoginPage->setEnabled(true);
        if (!result->authenticated)
        {
            m_homeLoginStatusLabel->setText(result->error);
            m_homeLoginPasswordEdit->selectAll();
            m_homeLoginPasswordEdit->setFocus();
            return;
        }
        if (!resetProductManagerPluginSession())
        {
            m_homeLoginStatusLabel->setText(QStringLiteral("请先关闭未能关闭的插件窗口，再重试登录。"));
            return;
        }
        gCurrentUserSession = result->session;
        gCurrentUserSession.isGuest = false;
        publishCurrentUserSession();
        saveUserLog(userName, password);
        m_homeLoginPasswordEdit->clear();
        m_homeLoginStatusLabel->clear();
        refreshAuthUiState();
        showHomeModulesPage();
        statusBar()->showMessage(QStringLiteral("登录成功，已进入首页。"), 3000);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

bool LTZK::resetProductManagerPluginSession()
{
    if (!m_pluginManager)
    {
        return true;
    }

    for (const PluginDefinition& plugin : m_pluginBindingConfig.plugins)
    {
        if (!plugin.enabled || plugin.id != QStringLiteral("map_product_manager_tools"))
        {
            continue;
        }

        const QString pluginPath = resolvePluginDllPath(plugin.dll);
        if (!destroyPluginFeatureWindowsForPlugin(plugin.id))
        {
            return false;
        }
        if (m_pluginManager->unloadPluginIfLoaded(pluginPath))
        {
            // 插件卸载会销毁其 QAction；清理平台侧缓存，下一次调用时
            // 让既有加载流程重新发现插件动作。
            m_pluginActionsByDllPath.remove(pluginActionCacheKey(pluginPath));
        }
        return true;
    }

    return true;
}

bool LTZK::leaveCurrentBusinessModule()
{
    if (m_currentBusinessModuleTitle.isEmpty())
    {
        return true;
    }

    if (!closePluginFeatureWindowsForModule(m_currentBusinessModuleTitle))
    {
        statusBar()->showMessage(
            QStringLiteral("\u6709\u529F\u80FD\u7A97\u53E3\u672A\u80FD\u5173\u95ED\uFF0C\u8BF7\u5148\u4FDD\u5B58\u6216\u5173\u95ED\u7A97\u53E3\u3002"),
            5000);
        return false;
    }

    ++m_businessModuleGeneration;
    m_currentBusinessModuleTitle.clear();
    return true;
}

bool LTZK::closePluginFeatureWindowsForModule(const QString& moduleTitle)
{
    const QString normalizedModuleTitle = businessModuleMenuTitle(moduleTitle);
    const QList<PluginFeatureWindowEntry> windows = m_pluginFeatureWindows;
    bool allWindowsClosed = true;
    for (const PluginFeatureWindowEntry& entry : windows)
    {
        QWidget* window = entry.window.data();
        if (!window || businessModuleMenuTitle(entry.moduleTitle) != normalizedModuleTitle)
        {
            continue;
        }

        if (!window->close())
        {
            allWindowsClosed = false;
        }
    }

    prunePluginFeatureWindows();
    return allWindowsClosed;
}

bool LTZK::closeAllPluginFeatureWindows()
{
    const QList<PluginFeatureWindowEntry> windows = m_pluginFeatureWindows;
    bool allWindowsClosed = true;
    for (const PluginFeatureWindowEntry& entry : windows)
    {
        QWidget* window = entry.window.data();
        if (window && !window->close())
        {
            allWindowsClosed = false;
        }
    }

    prunePluginFeatureWindows();
    return allWindowsClosed;
}

bool LTZK::destroyPluginFeatureWindowsForPlugin(const QString& pluginId)
{
    const QList<PluginFeatureWindowEntry> windows = m_pluginFeatureWindows;
    for (const PluginFeatureWindowEntry& entry : windows)
    {
        QWidget* window = entry.window.data();
        if (window && entry.pluginId == pluginId && !window->close())
        {
            return false;
        }
    }

    for (const PluginFeatureWindowEntry& entry : windows)
    {
        QWidget* window = entry.window.data();
        if (window && entry.pluginId == pluginId)
        {
            delete window;
        }
    }

    prunePluginFeatureWindows();
    return true;
}

void LTZK::destroyAllPluginFeatureWindows()
{
    const QList<PluginFeatureWindowEntry> windows = m_pluginFeatureWindows;
    m_pluginFeatureWindows.clear();
    for (const PluginFeatureWindowEntry& entry : windows)
    {
        QWidget* window = entry.window.data();
        if (!window)
        {
            continue;
        }

        window->close();
        delete window;
    }

    const QList<QPointer<QWidget>> auxiliaryWindows = m_shutdownAuxiliaryWindows;
    m_shutdownAuxiliaryWindows.clear();
    for (const QPointer<QWidget>& windowPointer : auxiliaryWindows)
    {
        QWidget* window = windowPointer.data();
        if (!window)
        {
            continue;
        }

        window->close();
        delete window;
    }
}

bool LTZK::activatePluginFeatureWindows(
    const QString& pluginId,
    const QString& featureName,
    const QString& moduleTitle)
{
    prunePluginFeatureWindows();
    const QString normalizedModuleTitle = businessModuleMenuTitle(moduleTitle);
    QList<QWidget*> windows;
    for (const PluginFeatureWindowEntry& entry : m_pluginFeatureWindows)
    {
        QWidget* window = entry.window.data();
        if (window
            && !entry.duplicate
            && entry.pluginId == pluginId
            && entry.featureName == featureName
            && businessModuleMenuTitle(entry.moduleTitle) == normalizedModuleTitle)
        {
            windows.append(window);
        }
    }

    if (windows.isEmpty())
    {
        for (const PluginFeatureWindowEntry& entry : m_pluginFeatureWindows)
        {
            QWidget* window = entry.window.data();
            if (window
                && entry.pluginId == pluginId
                && entry.featureName == featureName
                && businessModuleMenuTitle(entry.moduleTitle) == normalizedModuleTitle)
            {
                windows.append(window);
                break;
            }
        }
    }

    if (windows.isEmpty())
    {
        return false;
    }

    for (QWidget* window : windows)
    {
        if (window->isMinimized())
        {
            window->showNormal();
        }
        else if (!window->isVisible())
        {
            window->show();
        }
        window->raise();
        window->activateWindow();
    }
    return true;
}

QString LTZK::pluginFeatureWindowSignature(QWidget* window) const
{
    if (!window)
    {
        return QString();
    }

    const QString className = QString::fromLatin1(window->metaObject()->className());
    const QString identity = window->objectName().trimmed().isEmpty()
        ? window->windowTitle().trimmed()
        : window->objectName().trimmed();
    return identity.isEmpty()
        ? className
        : className + QLatin1Char('|') + identity;
}

void LTZK::capturePluginFeatureWindow(QWidget* window)
{
    if (!window
        || window == this
        || !window->isWindow()
        || window->windowModality() != Qt::NonModal
        || window->windowType() == Qt::Popup
        || window->windowType() == Qt::ToolTip
        || QApplication::activeModalWidget())
    {
        return;
    }

    // The plugin may defer showing its dialog to a queued callback. Ignore
    // windows that already existed when the feature was triggered.
    if (!m_pluginWindowCapturePluginId.isEmpty()
        && m_pluginWindowCaptureBaseline.contains(window))
    {
        return;
    }

    const QString signature = pluginFeatureWindowSignature(window);
    if (signature.isEmpty())
    {
        return;
    }

    QString pluginId = m_pluginWindowCapturePluginId;
    QString featureName = m_pluginWindowCaptureFeatureName;
    QString moduleTitle = m_pluginWindowCaptureModuleTitle;
    bool duplicate = false;
    QWidget* existingWindow = nullptr;

    if (!pluginId.isEmpty())
    {
        for (const PluginFeatureWindowEntry& entry : m_pluginFeatureWindows)
        {
            if (entry.window
                && entry.pluginId == pluginId
                && entry.featureName == featureName
                && businessModuleMenuTitle(entry.moduleTitle) == businessModuleMenuTitle(moduleTitle)
                && entry.signature == signature)
            {
                existingWindow = entry.window.data();
                duplicate = existingWindow != window;
                break;
            }
        }
    }
    else
    {
        QList<const PluginFeatureWindowEntry*> matches;
        const QString activeModuleTitle = businessModuleMenuTitle(m_currentBusinessModuleTitle);
        for (const PluginFeatureWindowEntry& entry : m_pluginFeatureWindows)
        {
            if (!entry.window || entry.signature != signature)
            {
                continue;
            }
            if (!activeModuleTitle.isEmpty()
                && businessModuleMenuTitle(entry.moduleTitle) != activeModuleTitle)
            {
                continue;
            }
            matches.append(&entry);
        }

        if (!matches.isEmpty())
        {
            const PluginFeatureWindowEntry* match = matches.first();
            bool oneFeature = true;
            for (const PluginFeatureWindowEntry* candidate : matches)
            {
                if (candidate->pluginId != match->pluginId
                    || candidate->featureName != match->featureName
                    || businessModuleMenuTitle(candidate->moduleTitle)
                        != businessModuleMenuTitle(match->moduleTitle))
                {
                    oneFeature = false;
                    break;
                }
            }
            if (oneFeature)
            {
                pluginId = match->pluginId;
                featureName = match->featureName;
                moduleTitle = match->moduleTitle;
                existingWindow = match->window.data();
                duplicate = existingWindow != window;
            }
        }
    }

    if (existingWindow == window)
    {
        return;
    }

    if (duplicate && existingWindow)
    {
        if (businessModuleMenuTitle(m_currentBusinessModuleTitle)
            == businessModuleMenuTitle(moduleTitle))
        {
            if (existingWindow->isMinimized())
            {
                existingWindow->showNormal();
            }
            else if (!existingWindow->isVisible())
            {
                existingWindow->show();
            }
            existingWindow->raise();
            existingWindow->activateWindow();
        }

        window->hide();
        PluginFeatureWindowEntry duplicateEntry;
        duplicateEntry.pluginId = pluginId;
        duplicateEntry.featureName = featureName;
        duplicateEntry.moduleTitle = moduleTitle;
        duplicateEntry.signature = signature;
        duplicateEntry.window = window;
        duplicateEntry.duplicate = true;
        m_pluginFeatureWindows.append(duplicateEntry);
        connect(window, &QObject::destroyed, this, [this]()
        {
            prunePluginFeatureWindows();
        });
        return;
    }

    if (pluginId.isEmpty())
    {
        return;
    }

    PluginFeatureWindowEntry entry;
    entry.pluginId = pluginId;
    entry.featureName = featureName;
    entry.moduleTitle = moduleTitle;
    entry.signature = signature;
    entry.window = window;
    window->setProperty("ltzkPluginFeatureWindow", true);
    window->setProperty("ltzkPluginFeaturePluginId", pluginId);
    window->setProperty("ltzkPluginFeatureName", featureName);
    window->setProperty("ltzkPluginFeatureModule", moduleTitle);
    m_pluginFeatureWindows.append(entry);
    connect(window, &QObject::destroyed, this, [this]()
    {
        prunePluginFeatureWindows();
    });

    // Reparent after the plugin's show() has returned to avoid changing window
    // flags while Qt is dispatching the original top-level Show event.
    if (pluginId == QStringLiteral("map_product_manager_tools")
        && moduleTitle == QStringLiteral("地图服务管理")
        && featureName == QStringLiteral("地图服务管理")
        && QString::fromLatin1(window->metaObject()->className()) == QStringLiteral("MapServiceDialog"))
    {
        // The plugin initially shows a top-level dialog. Suppress that native
        // window until the queued reparent operation has completed.
        window->setAttribute(Qt::WA_DontShowOnScreen, true);
        const QPointer<QDialog> serviceDialog = qobject_cast<QDialog*>(window);
        const quint64 moduleGeneration = m_businessModuleGeneration;
        QTimer::singleShot(0, this, [this, serviceDialog, moduleGeneration]()
        {
            if (serviceDialog && m_workspaceContentStack
                && moduleGeneration == m_businessModuleGeneration
                && m_currentBusinessModuleTitle == QStringLiteral("地图服务管理"))
            {
                m_workspaceContentStack->embedServiceDialog(serviceDialog.data());
            }
        });
    }

    // A delayed plugin dialog can arrive after its subplatform was left.
    // Track it first so unload cleanup can still destroy it, then close it.
    if (!moduleTitle.isEmpty()
        && (m_pluginWindowCaptureBusinessModuleGeneration != m_businessModuleGeneration
            || businessModuleMenuTitle(moduleTitle)
                != businessModuleMenuTitle(m_currentBusinessModuleTitle)))
    {
        if (!window->close())
        {
            window->hide();
        }
    }
}

void LTZK::prunePluginFeatureWindows()
{
    for (int index = m_pluginFeatureWindows.size() - 1; index >= 0; --index)
    {
        if (!m_pluginFeatureWindows[index].window)
        {
            m_pluginFeatureWindows.removeAt(index);
        }
    }
}

void LTZK::showHomeLoginPage()
{
    if (!leaveCurrentBusinessModule())
    {
        return;
    }

    if (m_homeContentStack && m_homeLoginPage)
    {
        m_homeContentStack->setCurrentWidget(m_homeLoginPage);
    }

    if (m_workspaceStack && m_homePage)
    {
        m_workspaceStack->setCurrentWidget(m_homePage);
    }

    if (m_pluginToolBar)
    {
        m_pluginToolBar->hide();
    }

    if (m_homeLoginUserNameEdit)
    {
        m_homeLoginUserNameEdit->setFocus();
    }

    refreshAuthUiState();
}

void LTZK::showHomeModulesPage()
{
    if (!leaveCurrentBusinessModule())
    {
        return;
    }

    if (m_homeContentStack && m_homeModulesPage)
    {
        m_homeContentStack->setCurrentWidget(m_homeModulesPage);
    }

    if (m_workspaceStack && m_homePage)
    {
        m_workspaceStack->setCurrentWidget(m_homePage);
    }

    if (m_pluginToolBar)
    {
        m_pluginToolBar->hide();
    }

    refreshAuthUiState();
}

QString LTZK::platformSettingsPath() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/config/database.ini"),
        QDir(appDir).absoluteFilePath(QStringLiteral("../config/database.ini")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../config/database.ini")),
        QDir::currentPath() + QStringLiteral("/config/database.ini")
    };

    for (const QString& candidate : candidates)
    {
        if (QFileInfo::exists(candidate))
        {
            return QFileInfo(candidate).absoluteDir().absoluteFilePath(QStringLiteral("platform_settings.ini"));
        }
    }

    return QDir(appDir).absoluteFilePath(QStringLiteral("config/platform_settings.ini"));
}

QString LTZK::userLogPath() const
{
    // 与成果管理插件模板的既有读取规则保持一致：
    // applicationDir/../config/user_login.ini。
    return productManagerAutoLoginPath();
}

void LTZK::saveUserLog(const QString& userName, const QString& password)
{
    const QString path = userLogPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSettings settings(userLogPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue(QStringLiteral("user/user_name"), userName);
    settings.setValue(QStringLiteral("user/password"), password);
    settings.sync();
}

void LTZK::clearUserLog()
{
    const QString path = userLogPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(userLogPath());
    file.open(QIODevice::WriteOnly | QIODevice::Truncate);
}
void LTZK::logoutCurrentUser()
{
    if (!closeAllPluginFeatureWindows())
    {
        statusBar()->showMessage(
            QStringLiteral("\u6709\u529F\u80FD\u7A97\u53E3\u672A\u80FD\u5173\u95ED\uFF0C\u8BF7\u5148\u4FDD\u5B58\u6216\u5173\u95ED\u7A97\u53E3\u518D\u9000\u51FA\u3002"),
            5000);
        return;
    }
    if (!resetProductManagerPluginSession())
    {
        statusBar()->showMessage(
            QStringLiteral("\u63D2\u4EF6\u7A97\u53E3\u672A\u80FD\u5B89\u5168\u5173\u95ED\uFF0C\u672C\u6B21\u9000\u51FA\u5DF2\u53D6\u6D88\u3002"),
            5000);
        return;
    }

    clearUserLog();
    if (QCoreApplication::instance())
    {
        QCoreApplication::instance()->setProperty(
            "ltzkProductCatalogRequest", QVariant());
        QCoreApplication::instance()->setProperty(
            "ltzkProductImportTargetDirId", QVariant());
    }
    gCurrentUserSession.clear();
    publishCurrentUserSession();
    if (m_dataCatalogWidget)
    {
        m_dataCatalogWidget->applyMenuDatabaseConnection(QVariantMap());
    }
    m_currentBusinessModuleTitle.clear();

    if (m_activeModuleTitle)
    {
        m_activeModuleTitle->setText(QStringLiteral("陇图智控平台"));
    }

    if (m_pluginToolBar)
    {
        m_pluginToolBar->hide();
    }

    if (m_homeLoginPasswordEdit)
    {
        m_homeLoginPasswordEdit->clear();
    }

    if (m_homeLoginStatusLabel)
    {
        m_homeLoginStatusLabel->setText(QStringLiteral("已退出当前账号，请重新登录。"));
    }

    showHomeLoginPage();
    statusBar()->showMessage(QStringLiteral("已退出当前账号"), 3000);
}

bool LTZK::ensureAuthenticatedForModule(const QString& moduleTitle)
{
    if (!gCurrentUserSession.isLoggedIn && !gCurrentUserSession.isGuest)
    {
        return promptLoginForModule(moduleTitle);
    }

    if (!canAccessModule(moduleTitle))
    {
        showAccessDeniedMessage(moduleTitle);
        return false;
    }

    return true;
}

bool LTZK::canAccessModule(const QString& moduleTitle) const
{
    if (gCurrentUserSession.isGuest)
    {
        return moduleTitle == QStringLiteral("地图成果管理")
            || moduleTitle == QStringLiteral("地图智能制图")
            || moduleTitle == QStringLiteral("地图成果质检")
            || moduleTitle == QStringLiteral("地图服务管理");
    }

    if (!gCurrentUserSession.isLoggedIn)
    {
        return false;
    }

    if (moduleTitle == QStringLiteral("地图成果管理"))
    {
        return gCurrentUserSession.canOperateProductStore();
    }

    if (moduleTitle == QStringLiteral("权限管理"))
    {
        return gCurrentUserSession.canManageUsers();
    }

    return gCurrentUserSession.canUseReadOnlyModules();
}

void LTZK::showLoginRequiredMessage()
{
    promptLoginForModule(QStringLiteral("当前功能"));
}

void LTZK::showAccessDeniedMessage(const QString& moduleTitle)
{
    QMessageBox::warning(
        this,
        QStringLiteral("权限不足"),
        QStringLiteral("当前用户无权使用「%1」功能，请联系数据库管理员。").arg(moduleTitle));
}

void LTZK::showAccessControlPage()
{
    if (!ensureAuthenticatedForModule(QStringLiteral("权限管理")))
    {
        return;
    }

    if (m_currentBusinessModuleTitle != QStringLiteral("权限管理")
        && !leaveCurrentBusinessModule())
    {
        return;
    }

    if (!m_workspaceStack || !m_accessControlPage)
    {
        return;
    }

    if (m_pluginToolBar)
    {
        m_pluginToolBar->hide();
    }

    if (m_currentBusinessModuleTitle != QStringLiteral("权限管理"))
    {
        ++m_businessModuleGeneration;
    }
    m_currentBusinessModuleTitle = QStringLiteral("权限管理");
    if (m_activeModuleTitle)
    {
        m_activeModuleTitle->setText(QStringLiteral("权限管理子平台"));
    }

    if (m_accessControlWidget)
    {
        m_accessControlWidget->setCurrentOperator(gCurrentUserSession.userName);
        m_accessControlWidget->reloadUsers();
    }

    m_workspaceStack->setCurrentWidget(m_accessControlPage);
    statusBar()->showMessage(QStringLiteral("已进入权限管理页面。"), 3000);
}

void LTZK::refreshAuthUiState()
{
    if (m_homeAccessControlButton)
    {
        m_homeAccessControlButton->setVisible(gCurrentUserSession.canManageUsers());
    }
    if (m_homeLogoutButton)
    {
        m_homeLogoutButton->setVisible(
            gCurrentUserSession.isLoggedIn || gCurrentUserSession.isGuest);
        m_homeLogoutButton->setText(gCurrentUserSession.isGuest
            ? QStringLiteral("退出游客")
            : QStringLiteral("退出账号"));
    }

    if (m_workspaceLogoutButton)
    {
        m_workspaceLogoutButton->setVisible(
            gCurrentUserSession.isLoggedIn || gCurrentUserSession.isGuest);
        m_workspaceLogoutButton->setText(gCurrentUserSession.isGuest
            ? QStringLiteral("退出游客")
            : QStringLiteral("退出账号"));
    }
}

void LTZK::publishCurrentUserSession()
{
    QVariantMap session;
    session.insert(QStringLiteral("userName"), gCurrentUserSession.userName);
    session.insert(QStringLiteral("role"), static_cast<int>(gCurrentUserSession.role));
    session.insert(QStringLiteral("isLoggedIn"), gCurrentUserSession.isLoggedIn);
    session.insert(QStringLiteral("isGuest"), gCurrentUserSession.isGuest);
    LTZKSessionBridge::write(session);
}

bool LTZK::clearRemoteLayersForGuest()
{
    QStringList remoteProviderTypes = {
        QStringLiteral("YOUR_PASSWORD"),
        QStringLiteral("mssql"),
        QStringLiteral("oracle"),
        QStringLiteral("db2"),
        QStringLiteral("wms"),
        QStringLiteral("wfs"),
        QStringLiteral("wcs"),
        QStringLiteral("ows"),
        QStringLiteral("arcgismapserver"),
        QStringLiteral("arcgisfeatureserver"),
        QStringLiteral("vectortile"),
        QStringLiteral("xyz")};
    QStringList remoteLayerIds;

    const QMap<QString, QgsMapLayer*> layers = QgsProject::instance()->mapLayers();
    for (QgsMapLayer* layer : layers)
    {
        if (!layer)
        {
            continue;
        }

        const QString provider = layer->providerType().trimmed().toLower();
        const QString source = layer->source().trimmed().toLower();
        const bool remoteSource = source.startsWith(QStringLiteral("http://"))
            || source.startsWith(QStringLiteral("https://"))
            || source.startsWith(QStringLiteral("ftp://"))
            || source.startsWith(QStringLiteral("/vsicurl/"))
            || source.startsWith(QStringLiteral("/vsicurl_streaming/"))
            || source.startsWith(QStringLiteral("/vsis3/"))
            || source.startsWith(QStringLiteral("/vsigs/"))
            || source.startsWith(QStringLiteral("/vsiaz/"));
        if (!remoteProviderTypes.contains(provider) && !remoteSource)
        {
            continue;
        }

        const QgsVectorLayer* vectorLayer = qobject_cast<QgsVectorLayer*>(layer);
        if (vectorLayer && vectorLayer->isEditable())
        {
            return false;
        }
        remoteLayerIds.append(layer->id());
    }

    for (const QString& layerId : std::as_const(remoteLayerIds))
    {
        if (m_curMapLayer && m_curMapLayer->id() == layerId)
        {
            m_curMapLayer = nullptr;
        }
        QgsProject::instance()->removeMapLayer(layerId);
    }

    if (m_layerTreeView)
    {
        m_curMapLayer = m_layerTreeView->currentLayer();
    }
    refreshMapCanvasLayerStack();
    return true;
}

void LTZK::closeEvent(QCloseEvent* event)
{
    if (m_homeLoginPending)
    {
        event->ignore();
        return;
    }
    if (m_closeEventInProgress)
    {
        event->ignore();
        return;
    }
    m_closeEventInProgress = true;

    const auto collectVisibleAuxiliaryWindows = [this](bool includeTransientWindows)
    {
        QList<QPointer<QWidget>> windows;
        QSet<QWidget*> seenWindows;
        const QList<QWidget*> topLevelWidgets = QApplication::topLevelWidgets();
        for (QWidget* widget : topLevelWidgets)
        {
            if (!widget || widget == this || !widget->isVisible()
                || widget->windowType() == Qt::Desktop
                || seenWindows.contains(widget))
            {
                continue;
            }

            QToolBar* workspaceToolbar = qobject_cast<QToolBar*>(widget);
            if (workspaceToolbar && m_workspaceToolBars.contains(workspaceToolbar))
            {
                continue;
            }

            const Qt::WindowType type = widget->windowType();
            if (!includeTransientWindows
                && (type == Qt::Popup || type == Qt::ToolTip))
            {
                continue;
            }

            seenWindows.insert(widget);
            windows.append(widget);
        }
        return windows;
    };

    const QList<QPointer<QWidget>> windowsToReport = collectVisibleAuxiliaryWindows(false);
    if (!windowsToReport.isEmpty())
    {
        QMessageBox confirmation(this);
        confirmation.setIcon(QMessageBox::Question);
        confirmation.setWindowTitle(QStringLiteral("\u786E\u8BA4\u9000\u51FA"));
        confirmation.setText(
            QStringLiteral("\u5F53\u524D\u8FD8\u6709 %1 \u4E2A\u5F39\u7A97\u672A\u5173\u95ED\uFF0C\u662F\u5426\u5173\u95ED\u5168\u90E8\u5F39\u7A97\u5E76\u9000\u51FA\uFF1F")
                .arg(windowsToReport.size()));
        QPushButton* exitButton = confirmation.addButton(
            QStringLiteral("\u5173\u95ED\u5168\u90E8\u5E76\u9000\u51FA"),
            QMessageBox::AcceptRole);
        QPushButton* cancelButton = confirmation.addButton(
            QStringLiteral("\u53D6\u6D88"),
            QMessageBox::RejectRole);
        confirmation.setDefaultButton(cancelButton);
        confirmation.exec();

        if (confirmation.clickedButton() != exitButton)
        {
            m_closeEventInProgress = false;
            event->ignore();
            return;
        }
    }

    bool allAuxiliaryWindowsClosed = true;
    const QList<QPointer<QWidget>> topLevelWidgets = collectVisibleAuxiliaryWindows(true);
    for (const QPointer<QWidget>& widgetPointer : topLevelWidgets)
    {
        QWidget* widget = widgetPointer.data();
        if (!widget)
        {
            continue;
        }

        bool isTrackedPluginWindow = false;
        for (const PluginFeatureWindowEntry& entry : m_pluginFeatureWindows)
        {
            if (entry.window.data() == widget)
            {
                isTrackedPluginWindow = true;
                break;
            }
        }

        const Qt::WindowType type = widget->windowType();
        if (!isTrackedPluginWindow
            && !widget->parentWidget()
            && type != Qt::Popup
            && type != Qt::ToolTip)
        {
            bool alreadyScheduledForDestruction = false;
            for (const QPointer<QWidget>& scheduledWindow : m_shutdownAuxiliaryWindows)
            {
                if (scheduledWindow.data() == widget)
                {
                    alreadyScheduledForDestruction = true;
                    break;
                }
            }
            if (!alreadyScheduledForDestruction)
            {
                m_shutdownAuxiliaryWindows.append(widget);
            }
        }

        if (!widget->close())
        {
            allAuxiliaryWindowsClosed = false;
        }
    }

    if (!allAuxiliaryWindowsClosed)
    {
        statusBar()->showMessage(
            QStringLiteral("\u6709\u7A97\u53E3\u62D2\u7EDD\u5173\u95ED\uFF0C\u8BF7\u5148\u5173\u95ED\u6216\u4FDD\u5B58\u540E\u518D\u9000\u51FA\u3002"),
            5000);
        m_closeEventInProgress = false;
        event->ignore();
        return;
    }

    QMainWindow::closeEvent(event);
    if (event->isAccepted())
    {
        saveWorkspaceToolbarState();
    }
    if (!event->isAccepted())
    {
        m_closeEventInProgress = false;
    }
}

bool LTZK::eventFilter(QObject* watched, QEvent* event)
{
    if (event && event->type() == QEvent::Show)
    {
        if (QDialog* dialog = qobject_cast<QDialog*>(watched))
        {
            PlatformMenuDatabaseConnectionObserver::watch(
                dialog, this,
                [this](const QVariantMap& parameters, bool remembered)
            {
                // The plugin calls accept() only after its connection and schema
                // initialization have succeeded. Defer the catalog query until
                // its modal dialog has finished closing.
                QTimer::singleShot(0, this, [this, parameters, remembered]()
                {
                    if (gCurrentUserSession.isLoggedIn && m_dataCatalogWidget)
                    {
                        m_dataCatalogWidget->applySuccessfulMenuDatabaseConnection(
                            parameters, remembered);
                    }
                });
            });
        }
    }
    if (watched == m_homePage && event
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
    {
        if (!m_homePage->property("ltzkHomeScaleRefreshPending").toBool())
        {
            m_homePage->setProperty("ltzkHomeScaleRefreshPending", true);
            QTimer::singleShot(0, this, [this]()
            {
                if (!m_homePage)
                {
                    return;
                }
                m_homePage->setProperty("ltzkHomeScaleRefreshPending", false);
                refreshHomePageScaledMetrics();
            });
        }
    }
    QWidget* watchedWidget = qobject_cast<QWidget*>(watched);
    QWidget* window = watchedWidget ? watchedWidget->window() : nullptr;
    if (window == this && m_workspaceContentStack)
    {
        QDialog* serviceDialog = m_workspaceContentStack->serviceDialog();
        if (serviceDialog && (watchedWidget == serviceDialog || serviceDialog->isAncestorOf(watchedWidget)))
        {
            window = serviceDialog;
        }
    }
    // Layer properties are native QGIS dialogs, not plugin feature windows.
    // Their thousands of child events only need initial theming and a native
    // header refresh when a previously hidden page is shown.
    if (window && event
        && (qobject_cast<QgsVectorLayerProperties*>(window) || qobject_cast<QgsRasterLayerProperties*>(window)))
    {
        if (event->type() == QEvent::Show)
        {
            if (watchedWidget == window)
            {
                if (!window->property("ltzkWindowInitialized").toBool())
                    schedulePluginWindowPolish(window);
            }
            else if (window->property("ltzkWindowInitialized").toBool()
                && (qobject_cast<QGroupBox*>(watchedWidget) || qobject_cast<QHeaderView*>(watchedWidget))
                && !window->property("ltzkNativeControlsPolishPending").toBool())
            {
                window->setProperty("ltzkNativeControlsPolishPending", true);
                QPointer<QWidget> targetWindow(window);
                QTimer::singleShot(0, this, [this, targetWindow]()
                {
                    if (!targetWindow) return;
                    targetWindow->setProperty("ltzkNativeControlsPolishPending", false);
                    LTZKNativeDialogStyle::prepareHeaders(targetWindow);
                    LTZKNativeDialogStyle::polish(targetWindow, LTZKUiScale::factorForWidget(this));
                });
            }
        }
        return QMainWindow::eventFilter(watched, event);
    }

    if (window
        && watchedWidget == window
        && event
        && event->type() == QEvent::Show)
    {
        capturePluginFeatureWindow(window);
    }

    if (window && window != this && isPluginWindowOrEmbeddedService(window) && event
        && window->windowType() != Qt::Popup
        && window->windowType() != Qt::ToolTip)
    {
        const QEvent::Type eventType = event->type();
        if (eventType == QEvent::Show && watchedWidget != window
            && (qobject_cast<QGroupBox*>(watchedWidget) || qobject_cast<QHeaderView*>(watchedWidget))
            && LTZKNativeDialogStyle::isQgisDialog(window)
            && !window->property("ltzkNativeControlsPolishPending").toBool())
        {
            // QGIS installs per-group styles when a previously hidden page is shown.
            window->setProperty("ltzkNativeControlsPolishPending", true);
            QPointer<QWidget> targetWindow(window);
            QTimer::singleShot(0, this, [this, targetWindow]()
            {
                if (!targetWindow)
                {
                    return;
                }
                targetWindow->setProperty("ltzkNativeControlsPolishPending", false);
                LTZKNativeDialogStyle::prepareHeaders(targetWindow);
                LTZKNativeDialogStyle::polish(targetWindow, LTZKUiScale::factorForWidget(this));
            });
        }
        const bool isQualitySchemeWindow = window->windowTitle().trimmed().contains(QStringLiteral("质检方案配置"));
        if (isQualitySchemeWindow && (eventType == QEvent::LayoutRequest || eventType == QEvent::ChildAdded))
        {
            QTableView* qualityTable = qobject_cast<QTableView*>(watchedWidget);
            if (eventType == QEvent::ChildAdded)
            {
                QChildEvent* childEvent = static_cast<QChildEvent*>(event);
                qualityTable = qobject_cast<QTableView*>(childEvent->child());
            }
            if (qualityTable)
            {
                scheduleQualityWindowTableColumnWidthNormalization(window);
            }
        }
        if (eventType == QEvent::ChildAdded)
        {
            QChildEvent* childEvent = static_cast<QChildEvent*>(event);
            QWidget* child = qobject_cast<QWidget*>(childEvent->child());
            if (qobject_cast<QLineEdit*>(child)
                && window->windowTitle().trimmed() == QStringLiteral("数据导入向导"))
            {
                // The wizard rebuilds its metadata rows after the data type changes.
                schedulePluginWindowPolish(window);
            }
            if (QComboBox* combo = qobject_cast<QComboBox*>(child))
            {
                if (!window->property("ltzkWindowInitialized").toBool())
                {
                    schedulePluginWindowPolish(window);
                }
                else if (!qobject_cast<QgsVectorLayerProperties*>(window)
                    && !qobject_cast<QgsRasterLayerProperties*>(window))
                {
                    polishDrawingLayoutTreeEmbeddedCombos(this, window, !isLightPlatformTheme(), m_currentTheme == PlatformTheme::Black, combo);
                }
            }
        }
        else if (eventType == QEvent::Show
            || eventType == QEvent::WindowActivate
            || eventType == QEvent::LayoutRequest
            || eventType == QEvent::StyleChange
            || eventType == QEvent::PaletteChange)
        {
            if (!window->property("ltzkWindowInitialized").toBool())
            {
                schedulePluginWindowPolish(window);
            }
        }

        if ((m_qualityResultWindowBindingPending || m_qualityResultMonitoringWindow == window)
            && (eventType == QEvent::Show
                || eventType == QEvent::WindowActivate
                || eventType == QEvent::ChildAdded
                || eventType == QEvent::LayoutRequest))
        {
            QPointer<QWidget> resultWindow(window);
            QTimer::singleShot(0, this, [this, resultWindow]()
            {
                if (resultWindow)
                {
                    bindQualityResultOutputWindow(resultWindow);
                }
            });
        }
    }
    if (watched == m_homeModulesPage && event &&
        (event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest))
    {
        updateHomeUtilityButtonMetrics();
    }
    if (watched && event && watched->property("ltzkSeriesConfigTreePolishTarget").toBool())
    {
        switch (event->type())
        {
        case QEvent::ChildAdded:
        case QEvent::LayoutRequest:
            if (!watched->property("ltzkSeriesConfigTreePolishPending").toBool())
            {
                watched->setProperty("ltzkSeriesConfigTreePolishPending", true);
                QPointer<QObject> watchedObject(watched);
                QTimer::singleShot(0, this, &LTZK::polishSeriesContentTableWindows);
                QTimer::singleShot(120, this, &LTZK::polishSeriesContentTableWindows);
                QTimer::singleShot(240, this, [watchedObject]()
                {
                    if (watchedObject)
                    {
                        watchedObject->setProperty("ltzkSeriesConfigTreePolishPending", false);
                    }
                });
            }
            break;
        default:
            break;
        }
    }
    if (watched && event && watched->property("ltzkPluginEmbeddedTextSuppressTarget").toBool())
    {
        switch (event->type())
        {
        case QEvent::ChildAdded:
        case QEvent::LayoutRequest:
        case QEvent::Resize:
            if (!watched->property("ltzkPluginEmbeddedTextSuppressPending").toBool())
            {
                watched->setProperty("ltzkPluginEmbeddedTextSuppressPending", true);
                QPointer<QObject> watchedObject(watched);
                QTimer::singleShot(0, this, &LTZK::schedulePluginTableEmbeddedTextSuppression);
                QTimer::singleShot(80, this, [watchedObject]()
                {
                    if (watchedObject)
                    {
                        watchedObject->setProperty("ltzkPluginEmbeddedTextSuppressPending", false);
                    }
                });
            }
            break;
        default:
            break;
        }
    }
    if (watched && event
        && watched->property("ltzkDrawingLayoutComboArrowTarget").toBool()
        && (event->type() == QEvent::Show
            || event->type() == QEvent::Resize
            || event->type() == QEvent::LayoutRequest))
    {
        if (QComboBox* combo = qobject_cast<QComboBox*>(watched))
        {
            if (QLabel* arrowLabel = combo->findChild<QLabel*>(QStringLiteral("ltzkDrawingLayoutComboArrow"), Qt::FindDirectChildrenOnly))
            {
                const qreal comboScale = LTZKUiScale::factorForWidget(combo);
                const int arrowSize = LTZKUiScale::scaledInt(14, comboScale);
                const int arrowInset = LTZKUiScale::scaledInt(6, comboScale);
                arrowLabel->setGeometry(qMax(0, combo->width() - arrowSize - arrowInset), qMax(0, (combo->height() - arrowSize) / 2), arrowSize, arrowSize);
                arrowLabel->raise();
                arrowLabel->show();
                arrowLabel->update();
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}
bool LTZK::isPluginWindowOrEmbeddedService(QWidget* widget) const
{
    return widget && (widget->isWindow()
        || (m_workspaceContentStack && widget == m_workspaceContentStack->serviceDialog()));
}

void LTZK::schedulePluginWindowPolish(QWidget* window)
{
    if (!window || window == this || !isPluginWindowOrEmbeddedService(window))
    {
        return;
    }
    if (window->property("ltzkPluginWindowPolishPending").toBool())
    {
        return;
    }
    window->setProperty("ltzkPluginWindowPolishPending", true);
    QPointer<QWidget> targetWindow(window);
    QTimer::singleShot(0, this, [this, targetWindow]()
    {
        if (!targetWindow)
        {
            return;
        }
        targetWindow->setProperty("ltzkPluginWindowPolishPending", false);
        // A newly constructed editor can be top-level when queued, then become
        // an index widget before this callback runs. Never theme it as a window.
        if (!isPluginWindowOrEmbeddedService(targetWindow.data())) return;
        applyPlatformStyleToPluginWindow(targetWindow);
    });
}
void LTZK::polishPluginFrameHierarchy(QWidget* widget)
{
    if (!widget)
    {
        return;
    }

    const QList<QGroupBox*> groupBoxes = widget->findChildren<QGroupBox*>();
    for (QGroupBox* groupBox : groupBoxes)
    {
        if (!groupBox)
        {
            continue;
        }

        int frameDepth = 0;
        for (QWidget* parent = groupBox->parentWidget(); parent && parent != widget; parent = parent->parentWidget())
        {
            if (qobject_cast<QGroupBox*>(parent))
            {
                ++frameDepth;
            }
        }
        groupBox->setProperty("ltzkFrameDepth", QString::number(qMin(frameDepth, 2)));
    }
}
void LTZK::applyPlatformStyleToPluginWindow(QWidget* widget)
{
    if (!widget || widget == this || !isPluginWindowOrEmbeddedService(widget) || widget->windowType() == Qt::ToolTip)
    {
        return;
    }
    if (widget->isHidden() || widget->testAttribute(Qt::WA_WState_Hidden))
    {
        return;
    }

    QString style = platformStyleSheet();
    LTZKNativeDialogStyle::prepareHeaders(widget);
    style += LTZKNativeDialogStyle::nativeHeaderStyleSheet(LTZKUiScale::factorForWidget(this));

    // Native QGIS layer properties contain thousands of controls. Their native
    // headers need our theme, but plugin table delegates, title overlays and
    // plugin-specific layout scans are not applicable to these dialogs.
    if (qobject_cast<QgsVectorLayerProperties*>(widget) || qobject_cast<QgsRasterLayerProperties*>(widget))
    {
        // Global theme already contains the native header rules. Avoid applying
        // a second dialog stylesheet to thousands of child widgets.
        widget->setAutoFillBackground(true);
        LTZKNativeDialogStyle::polish(widget, LTZKUiScale::factorForWidget(this));
        widget->setProperty("ltzkWindowInitialized", true);
        return;
    }

    polishPluginFrameHierarchy(widget);
    const QString windowTitle = widget->windowTitle().trimmed();
    const bool compactQualityWindow = windowTitle.contains(QStringLiteral("质检任务管理"))
        || windowTitle.contains(QStringLiteral("质检方案配置"));
    if (compactQualityWindow)
    {
        const QList<QGroupBox*> groupBoxes = widget->findChildren<QGroupBox*>();
        QGroupBox* topQualityGroupBox = nullptr;
        for (QGroupBox* groupBox : groupBoxes)
        {
            if (!groupBox)
            {
                continue;
            }

            groupBox->setProperty("ltzkCompactTopGroupBox", false);
            bool nestedInGroupBox = false;
            for (QWidget* parent = groupBox->parentWidget(); parent && parent != widget; parent = parent->parentWidget())
            {
                if (qobject_cast<QGroupBox*>(parent))
                {
                    nestedInGroupBox = true;
                    break;
                }
            }
            if (nestedInGroupBox || !groupBox->isVisible())
            {
                continue;
            }

            if (!topQualityGroupBox || groupBox->geometry().top() < topQualityGroupBox->geometry().top())
            {
                topQualityGroupBox = groupBox;
            }
        }
        if (topQualityGroupBox)
        {
            topQualityGroupBox->setProperty("ltzkCompactTopGroupBox", true);
        }

        const QString compactQualityWindowStyle = QStringLiteral(
            "\nQGroupBox[ltzkCompactTopGroupBox=\"true\"] { margin-top: 0px; }\n");
        style += compactQualityWindowStyle;
    }
    if (widget->styleSheet() != style)
        widget->setStyleSheet(style);
    widget->setAutoFillBackground(true);
    installPluginTableEmbeddedWidgetDelegates(widget);
    polishPluginGroupBoxTitles(widget);
    LTZKNativeDialogStyle::polish(widget, LTZKUiScale::factorForWidget(this));
    if (windowTitle == QStringLiteral("数据导入向导"))
    {
        // Keep the hint rule in the style sheet so later window repolishing retains it.
        // Remove the rule as soon as the user enters a value.
        auto refreshPlaceholderStyle = [](QLineEdit* edit)
        {
            const bool showingHint = edit->text().isEmpty() && !edit->placeholderText().isEmpty();
            if (edit->property("ltzkMetadataPlaceholder").toBool() == showingHint)
            {
                return;
            }
            edit->setProperty("ltzkMetadataPlaceholder", showingHint);
            edit->style()->unpolish(edit);
            edit->style()->polish(edit);
            edit->update();
        };
        for (QGroupBox* groupBox : widget->findChildren<QGroupBox*>())
        {
            const QString title = groupBox->property("ltzkPluginOriginalTitle").toString().trimmed();
            if (title != QStringLiteral("基础元数据")
                && title != QStringLiteral("专有元数据"))
            {
                continue;
            }
            QFormLayout* form = qobject_cast<QFormLayout*>(groupBox->layout());
            if (!form)
            {
                continue;
            }
            for (int row = 0; row < form->rowCount(); ++row)
            {
                QLayoutItem* fieldItem = form->itemAt(row, QFormLayout::FieldRole);
                QLineEdit* edit = fieldItem ? qobject_cast<QLineEdit*>(fieldItem->widget()) : nullptr;
                if (!edit)
                {
                    continue;
                }
                if (!edit->property("ltzkMetadataPlaceholderConnected").toBool())
                {
                    edit->setProperty("ltzkMetadataPlaceholderConnected", true);
                    connect(edit, &QLineEdit::textChanged, edit,
                        [edit, refreshPlaceholderStyle](const QString&)
                    {
                        refreshPlaceholderStyle(edit);
                    });
                }
                refreshPlaceholderStyle(edit);
            }
        }
    }
    if (compactQualityWindow)
    {
        normalizeQualityWindowTableColumnWidths(widget);
        scheduleQualityWindowTableColumnWidthNormalization(widget);
    }
    applyPluginWindowGlowEffects(widget);
    LTZKSeriesTableTypography::apply(widget);
    widget->update();
}

void LTZK::scheduleQualityWindowTableColumnWidthNormalization(QWidget* widget)
{
    if (!widget)
    {
        return;
    }
    if (widget->property("ltzkQualitySchemeColumnWidthNormalizationPending").toBool())
    {
        return;
    }

    widget->setProperty("ltzkQualitySchemeColumnWidthNormalizationPending", true);
    QPointer<QWidget> targetWindow(widget);
    const int retryDelays[] = { 0, 120, 300, 600 };
    for (int delay : retryDelays)
    {
        QTimer::singleShot(delay, this, [this, targetWindow, delay]()
        {
            if (!targetWindow)
            {
                return;
            }

            normalizeQualityWindowTableColumnWidths(targetWindow);
            if (delay == 600)
            {
                targetWindow->setProperty("ltzkQualitySchemeColumnWidthNormalizationPending", false);
            }
        });
    }
}

void LTZK::normalizeQualityWindowTableColumnWidths(QWidget* widget)
{
    if (!widget)
    {
        return;
    }

    auto headerForView = [](QAbstractItemView* view) -> QHeaderView*
    {
        if (QTableView* tableView = qobject_cast<QTableView*>(view))
        {
            return tableView->horizontalHeader();
        }
        if (QTreeView* treeView = qobject_cast<QTreeView*>(view))
        {
            return treeView->header();
        }
        return nullptr;
    };

    auto findHeaderColumn = [](QAbstractItemModel* model, const QString& expectedText) -> int
    {
        if (!model)
        {
            return -1;
        }

        for (int column = 0; column < model->columnCount(); ++column)
        {
            const QString headerText = model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString().trimmed();
            if (headerText == expectedText)
            {
                return column;
            }
        }
        return -1;
    };

    const QString windowTitle = widget->windowTitle().trimmed();
    const QList<QAbstractItemView*> views = widget->findChildren<QAbstractItemView*>();
    for (QAbstractItemView* view : views)
    {
        if (!view)
        {
            continue;
        }

        QHeaderView* header = headerForView(view);
        QAbstractItemModel* model = view->model();
        if (!header || !model)
        {
            continue;
        }

        const bool isQualitySchemeWindowCandidate = windowTitle.contains(QStringLiteral("质检方案配置"))
            && qobject_cast<QTableView*>(view) != nullptr;
        if (isQualitySchemeWindowCandidate
            && !view->property("ltzkQualitySchemeColumnWidthSignalsConnected").toBool())
        {
            view->setProperty("ltzkQualitySchemeColumnWidthSignalsConnected", true);
            QPointer<QWidget> targetWindow(widget);
            const auto scheduleNormalization = [this, targetWindow]()
            {
                if (!targetWindow)
                {
                    return;
                }

                QTimer::singleShot(0, this, [this, targetWindow]()
                {
                    if (targetWindow)
                    {
                        normalizeQualityWindowTableColumnWidths(targetWindow);
                    }
                });
            };
            QObject::connect(model, &QAbstractItemModel::modelReset, view,
                [scheduleNormalization]()
                {
                    scheduleNormalization();
                });
            QObject::connect(model, &QAbstractItemModel::columnsInserted, view,
                [scheduleNormalization](const QModelIndex&, int, int)
                {
                    scheduleNormalization();
                });
        }

        if (model->columnCount() <= 0)
        {
            continue;
        }

        const int mapPathColumn = findHeaderColumn(model, QStringLiteral("地图路径"));
        const int mapShpFolderColumn = findHeaderColumn(model, QStringLiteral("地图shp文件夹"));
        const int gdbShpFolderColumn = findHeaderColumn(model, QStringLiteral("GDB shp文件夹"));
        const int mapConfigColumn = findHeaderColumn(model, QStringLiteral("图面配置选择"));
        const bool isMapPathTable = mapPathColumn >= 0
            && mapShpFolderColumn >= 0
            && gdbShpFolderColumn >= 0
            && mapConfigColumn >= 0;

        const int qualityItemColumn = findHeaderColumn(model, QStringLiteral("质检项"));
        const int qualityNameColumn = findHeaderColumn(model, QStringLiteral("质检规则名称"));
        const int qualityDescriptionColumn = findHeaderColumn(model, QStringLiteral("质检规则描述"));
        const int qualityValueColumn = findHeaderColumn(model, QStringLiteral("质检规则数值"));
        const int qualityUnitColumn = findHeaderColumn(model, QStringLiteral("质检规则单位"));
        const int qualityIdColumn = findHeaderColumn(model, QStringLiteral("id"));
        const bool hasQualitySchemeHeaders = qualityIdColumn >= 0
            && qualityItemColumn >= 0
            && qualityNameColumn >= 0
            && qualityDescriptionColumn >= 0
            && qualityValueColumn >= 0
            && qualityUnitColumn >= 0;
        const bool isQualitySchemeWindowTable = windowTitle.contains(QStringLiteral("质检方案配置"))
            && qobject_cast<QTableView*>(view) != nullptr
            && model->columnCount() >= 6;
        const bool isQualitySchemeTable = hasQualitySchemeHeaders || isQualitySchemeWindowTable;

        if (!isMapPathTable && !isQualitySchemeTable)
        {
            continue;
        }

        header->setStretchLastSection(false);
        header->setMinimumSectionSize(60);
        for (int column = 0; column < model->columnCount(); ++column)
        {
            header->setSectionResizeMode(column, QHeaderView::Interactive);
        }

        if (isMapPathTable)
        {
            const int mapPathColumns[] = { mapPathColumn, mapShpFolderColumn, gdbShpFolderColumn };
            const int mapPathColumnWidths[] = { 180, 250, 280 };
            for (int index = 0; index < 3; ++index)
            {
                header->resizeSection(mapPathColumns[index], mapPathColumnWidths[index]);
            }
            const int lastColumn = mapConfigColumn;
            header->setSectionResizeMode(lastColumn, QHeaderView::Stretch);
        }

        if (isQualitySchemeTable)
        {
            if (QTableView* qualityTable = qobject_cast<QTableView*>(view))
            {
                qualityTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            }

            const int nameColumn = qualityNameColumn >= 0 ? qualityNameColumn : 2;
            const int descriptionColumn = qualityDescriptionColumn >= 0 ? qualityDescriptionColumn : 3;
            const int valueColumn = qualityValueColumn >= 0 ? qualityValueColumn : 4;
            const int qualitySchemeColumns[] = { nameColumn, descriptionColumn, valueColumn };
            const int qualitySchemeColumnWidths[] = { 250, 250, 250 };
            for (int index = 0; index < 3; ++index)
            {
                const int column = qualitySchemeColumns[index];
                header->setSectionResizeMode(column, QHeaderView::Interactive);
                header->resizeSection(column, qualitySchemeColumnWidths[index]);
            }

            const int unitColumn = qualityUnitColumn >= 0 ? qualityUnitColumn : 5;
            header->setSectionResizeMode(unitColumn, QHeaderView::Stretch);
        }
    }
}

void LTZK::polishPluginGroupBoxTitles(QWidget* widget)
{
    if (!widget || widget == this || widget->isHidden() || widget->testAttribute(Qt::WA_WState_Hidden))
    {
        return;
    }

    auto isPluginSectionTitleLabel = [](QLabel* label) -> bool
    {
        if (!label)
        {
            return false;
        }

        if (label->objectName() == QStringLiteral("__ltzkPluginGroupBoxTitleOverlay"))
        {
            return true;
        }

        const QString text = label->text().trimmed();
        if (text.isEmpty() || text.size() > 16 || text.contains(QStringLiteral("：")) || text.contains(QChar(':')))
        {
            return false;
        }

        return text.contains(QStringLiteral("配置"))
            || text.contains(QStringLiteral("设置"))
            || text.contains(QStringLiteral("列表"))
            || text.contains(QStringLiteral("输出"));
    };

    auto titleLabelStyle = [this]() -> QString
    {
        auto scaledStyle = [this](const QString& style) -> QString
        {
            return LTZKUiScale::scaleStyleSheetPixels(
                style,
                LTZKUiScale::factorForWidget(this));
        };
        if (m_currentTheme == PlatformTheme::White)
        {
            return scaledStyle(QStringLiteral("QLabel { color: #1e3a8a; background: transparent; border: 0; font-size: 16px; font-weight: 850; padding: 0; min-height: 40px; }"));
        }
        if (m_currentTheme == PlatformTheme::Black)
        {
            return scaledStyle(QStringLiteral("QLabel { color: #f59e0b; background: transparent; border: 0; font-size: 16px; font-weight: 850; padding: 0; min-height: 40px; }"));
        }
        if (m_currentTheme == PlatformTheme::FieldNotes)
        {
            return scaledStyle(QStringLiteral("QLabel { color: #3D5540; background: transparent; border: 0; font-size: 16px; font-weight: 850; padding: 0; min-height: 40px; }"));
        }
        if (m_currentTheme == PlatformTheme::SteelWarmWhite)
        {
            return scaledStyle(QStringLiteral("QLabel { color: #2F5960; background: transparent; border: 0; font-size: 16px; font-weight: 850; padding: 0; min-height: 40px; }"));
        }
        return scaledStyle(QStringLiteral("QLabel { color: #ffffff; background: transparent; border: 0; font-size: 16px; font-weight: 850; padding: 0; min-height: 40px; }"));
    };

    auto applyTitleFont = [&](QLabel* target)
    {
        if (!target)
        {
            return;
        }

        QFont titleFont = target->font();
        titleFont.setPixelSize(scaledInt(target, 16));
        titleFont.setWeight(QFont::Bold);
        target->setFont(titleFont);
        target->setMinimumHeight(qMax(target->minimumHeight(), scaledInt(target, 40)));
        target->setProperty("ltzkPluginSectionTitle", true);
        target->setStyleSheet(titleLabelStyle());
        target->updateGeometry();
        target->update();
    };

    const QList<QGroupBox*> groupBoxes = widget->findChildren<QGroupBox*>();
    for (QGroupBox* groupBox : groupBoxes)
    {
        if (!groupBox)
        {
            continue;
        }

        QString title = groupBox->property("ltzkPluginOriginalTitle").toString().trimmed();
        if (title.isEmpty())
        {
            title = groupBox->title().trimmed();
            if (!title.isEmpty())
            {
                groupBox->setProperty("ltzkPluginOriginalTitle", title);
            }
        }
        if (title.isEmpty())
        {
            continue;
        }

        QLabel* titleOverlay = groupBox->findChild<QLabel*>(QStringLiteral("__ltzkPluginGroupBoxTitleOverlay"), Qt::FindDirectChildrenOnly);
        const bool hasNativeHeader = groupBox->property("ltzkPluginNativeHeader").toBool()
            || groupBox->isCheckable()
            || qobject_cast<QgsCollapsibleGroupBoxBasic*>(groupBox);
        if (hasNativeHeader)
        {
            groupBox->setTitle(title);
            if (titleOverlay)
            {
                titleOverlay->hide();
            }
            continue;
        }

        groupBox->setTitle(QString());

        if (!titleOverlay)
        {
            titleOverlay = new QLabel(groupBox);
            titleOverlay->setObjectName(QStringLiteral("__ltzkPluginGroupBoxTitleOverlay"));
            titleOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        }

        titleOverlay->setText(title);
        applyTitleFont(titleOverlay);
        titleOverlay->adjustSize();
        int titleLeft = groupBox->rect().left();
        const int overlayWidth = qMin(qMax(titleOverlay->sizeHint().width(), 120), qMax(120, groupBox->width() - 36));
        titleOverlay->setGeometry(titleLeft, 0, overlayWidth, 44);
        titleOverlay->raise();
        titleOverlay->show();
    }

    const QList<QLabel*> labels = widget->findChildren<QLabel*>();
    for (QLabel* label : labels)
    {
        // Native QGIS pages already identify their headings with group boxes.
        // A field label such as the keyword list must keep its regular metrics.
        if (LTZKNativeDialogStyle::isQgisDialog(widget))
        {
            continue;
        }
        if (isPluginSectionTitleLabel(label))
        {
            applyTitleFont(label);
        }
    }
}
void LTZK::applyPluginWindowGlowEffects(QWidget* widget)
{
    if (!widget || widget == this)
    {
        return;
    }

    // Frame hierarchy is now carried by restrained QSS borders and surfaces.
    // Remove only effects previously installed by this host so plugin-owned
    // graphics effects are left untouched.
    auto clearGlow = [](QWidget* target)
    {
        if (!target || !target->property("ltzkGlowEffectApplied").toBool())
        {
            return;
        }
        target->setGraphicsEffect(nullptr);
        target->setProperty("ltzkGlowEffectApplied", QVariant());
        target->setProperty("ltzkGlowEffectTheme", QVariant());
    };

    const QList<QWidget*> descendants = widget->findChildren<QWidget*>();
    for (QWidget* descendant : descendants)
    {
        clearGlow(descendant);
    }
}
void LTZK::installPluginTableEmbeddedWidgetDelegates(QWidget* widget)
{
    if (!widget || widget == this || widget->isHidden() || widget->testAttribute(Qt::WA_WState_Hidden))
    {
        return;
    }

    // Run before the initialization shortcut: rows can be added later and
    // theme/window polishing may replace a control's local stylesheet.
    LTZKSeriesTableTypography::apply(widget);

    if (widget->property("ltzkWindowInitialized").toBool())
    {
        return;
    }

    const QList<QAbstractItemView*> views = widget->findChildren<QAbstractItemView*>();
    for (QAbstractItemView* view : views)
    {
        if (!view)
        {
            continue;
        }
        if (!view->property("ltzkPluginEmbeddedTextSuppressTarget").toBool())
        {
            view->setProperty("ltzkPluginEmbeddedTextSuppressTarget", true);
            view->installEventFilter(this);
        }
        if (QWidget* viewport = view->viewport())
        {
            if (!viewport->property("ltzkPluginEmbeddedTextSuppressTarget").toBool())
            {
                viewport->setProperty("ltzkPluginEmbeddedTextSuppressTarget", true);
                viewport->installEventFilter(this);
            }
        }
        if (view->model())
        {
            suppressPluginTableEmbeddedWidgetTexts(view);
        }
    }
    polishDrawingLayoutTreeEmbeddedCombos(this, widget, !isLightPlatformTheme(), m_currentTheme == PlatformTheme::Black);
    widget->setProperty("ltzkWindowInitialized", true);
}
void LTZK::normalizePluginTableEmbeddedControls(QWidget* widget)
{
    if (!widget)
    {
        return;
    }
    if (widget->isHidden() || widget->testAttribute(Qt::WA_WState_Hidden))
    {
        return;
    }

    auto viewForChild = [](QWidget* child) -> QAbstractItemView*
    {
        for (QWidget* parent = child ? child->parentWidget() : nullptr; parent; parent = parent->parentWidget())
        {
            if (QAbstractItemView* view = qobject_cast<QAbstractItemView*>(parent))
            {
                return view;
            }
        }
        return nullptr;
    };

    auto indexForChild = [](QAbstractItemView* view, QWidget* child) -> QModelIndex
    {
        if (!view || !child || !child->parentWidget())
        {
            return QModelIndex();
        }

        const QPoint viewportPoint = child->parentWidget()->mapTo(view->viewport(), child->geometry().center());
        return view->indexAt(viewportPoint);
    };

    auto fontStyleSheetFragment = [](const QFont& font) -> QString
    {
        QString fragment;
        if (font.pixelSize() > 0)
        {
            fragment += QStringLiteral("font-size: %1px; ").arg(font.pixelSize());
        }
        else if (font.pointSizeF() > 0)
        {
            fragment += QStringLiteral("font-size: %1pt; ").arg(font.pointSizeF());
        }
        if (font.weight() != QFont::Normal)
        {
            fragment += QStringLiteral("font-weight: %1; ").arg(font.weight());
        }
        return fragment;
    };


    constexpr qreal kSeriesCrossScaleRelationTablePointSize = 16.0;

    auto seriesCrossScaleRelationTableFont = [=](const QFont& sourceFont) -> QFont
    {
        QFont tableFont(sourceFont);
        const qreal currentPointSize = tableFont.pointSizeF() > 0 ? tableFont.pointSizeF() : 9.0;
        tableFont.setPointSizeF(qMax(currentPointSize, kSeriesCrossScaleRelationTablePointSize));
        return tableFont;
    };

    auto firstRowPathSelectorControlFont = [=](const QFont& sourceFont) -> QFont
    {
        QFont controlFont(seriesCrossScaleRelationTableFont(sourceFont));
        controlFont.setPointSizeF(qMin(controlFont.pointSizeF(), 14.0));
        return controlFont;
    };

    const QString embeddedLargeControlMetricsFragment = QStringLiteral("min-height: 28px; max-height: 36px; padding: 2px 6px; ");

    auto isSeriesCrossScaleRelationView = [](QAbstractItemView* view) -> bool
    {
        if (!view)
        {
            return false;
        }
        if (view->property("ltzkSeriesCrossScaleRelationTable").toBool())
        {
            return true;
        }

        QAbstractItemModel* model = view->model();
        if (!model)
        {
            return false;
        }

        bool hasCrossScaleRelation = false;
        bool hasCompletedStatus = false;
        const int rowLimit = qMin(model->rowCount(), 20);
        const int columnLimit = qMin(model->columnCount(), 20);
        for (int row = 0; row < rowLimit; ++row)
        {
            for (int column = 0; column < columnLimit; ++column)
            {
                const QModelIndex modelIndex = model->index(row, column);
                const QString text = model->data(modelIndex, Qt::DisplayRole).toString();
                hasCrossScaleRelation = hasCrossScaleRelation || text.contains(QStringLiteral("跨比例尺"));
                hasCompletedStatus = hasCompletedStatus || text.contains(QStringLiteral("已完成"));
            }
        }

        const bool isTargetTable = hasCrossScaleRelation && hasCompletedStatus;
        if (isTargetTable)
        {
            view->setProperty("ltzkSeriesCrossScaleRelationTable", true);
        }
        return isTargetTable;
    };

    auto ensureSeriesCrossScaleRelationTableBodyRowHeights = [](QAbstractItemView* view, const QFont& tableFont)
    {
        QTableView* tableView = qobject_cast<QTableView*>(view);
        if (!tableView || !tableView->model())
        {
            return;
        }

        const QFontMetrics tableFontMetrics(tableFont);
        const int targetRowHeight = qMax(36, tableFontMetrics.height() + 12);
        for (int row = 0; row < tableView->model()->rowCount(); ++row)
        {
            tableView->setRowHeight(row, qMax(tableView->rowHeight(row), targetRowHeight));
        }
    };

    auto applySeriesCrossScaleRelationTableFont = [&](QAbstractItemView* view)
    {
        if (!isSeriesCrossScaleRelationView(view) || !view->model())
        {
            return;
        }

        QAbstractItemModel* model = view->model();
        const QFont tableFont = seriesCrossScaleRelationTableFont(view->font());
        ensureSeriesCrossScaleRelationTableBodyRowHeights(view, tableFont);
        for (int row = 0; row < model->rowCount(); ++row)
        {
            for (int column = 0; column < model->columnCount(); ++column)
            {
                const QModelIndex index = model->index(row, column);
                if (index.isValid())
                {
                    model->setData(index, tableFont, Qt::FontRole);
                }
            }
        }
    };

    auto isFirstRowPathSelectorControl = [](const QModelIndex& index) -> bool
    {
        return index.isValid() && index.row() == 0;
    };

    const QList<QAbstractItemView*> views = widget->findChildren<QAbstractItemView*>();
    for (QAbstractItemView* view : views)
    {
        if (!view)
        {
            continue;
        }
        view->setTextElideMode(Qt::ElideMiddle);
        ensurePluginEmbeddedControlDelegate(view);
        applySeriesCrossScaleRelationTableFont(view);
        if (QTableView* tableView = qobject_cast<QTableView*>(view))
        {
            if (QHeaderView* header = tableView->horizontalHeader())
            {
                header->setMinimumSectionSize(120);
            }
            if (tableView->model() && tableView->model()->columnCount() > 0)
            {
                tableView->setColumnWidth(0, qMax(tableView->columnWidth(0), 420));
            }
            if (tableView->model() && tableView->model()->columnCount() > 2)
            {
                tableView->setColumnWidth(2, qMax(tableView->columnWidth(2), 420));
            }
        }
        if (QTreeView* treeView = qobject_cast<QTreeView*>(view))
        {
            if (QHeaderView* header = treeView->header())
            {
                header->setMinimumSectionSize(120);
            }
            if (treeView->model() && treeView->model()->columnCount() > 0)
            {
                treeView->setColumnWidth(0, qMax(treeView->columnWidth(0), 420));
            }
            if (treeView->model() && treeView->model()->columnCount() > 2)
            {
                treeView->setColumnWidth(2, qMax(treeView->columnWidth(2), 420));
            }
        }
    }

    const QList<QLineEdit*> lineEdits = widget->findChildren<QLineEdit*>();
    for (QLineEdit* lineEdit : lineEdits)
    {
        if (!lineEdit)
        {
            continue;
        }
        if (isComboBoxInternalLineEdit(lineEdit))
        {
            continue;
        }

        QAbstractItemView* view = viewForChild(lineEdit);
        const QModelIndex index = indexForChild(view, lineEdit);
        if (!view || !index.isValid())
        {
            continue;
        }

        clearPluginTableWidgetIndexItemDisplayText(view, index);
        suppressPluginEmbeddedControlItemText(view, index);

        lineEdit->setMinimumWidth(0);
        lineEdit->setTextMargins(0, 0, 0, 0);
        lineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        lineEdit->setAttribute(Qt::WA_StyledBackground, true);
        const QRegularExpression drivePathPattern(QStringLiteral("[A-Za-z]:[/\\\\].*"));
        const QRegularExpressionMatch drivePathMatch = drivePathPattern.match(lineEdit->text());
        const bool isPathLineEdit = drivePathMatch.hasMatch();
        const bool normalizePathLineEditFont = isFirstRowPathSelectorControl(index) && isPathLineEdit;
        const bool normalizeSeriesRelationControlFont = isSeriesCrossScaleRelationView(view);
        const QString seriesRelationControlFontFragment = normalizeSeriesRelationControlFont
            ? fontStyleSheetFragment(seriesCrossScaleRelationTableFont(view->font()))
            : QString();
        if (normalizeSeriesRelationControlFont || normalizePathLineEditFont)
        {
            lineEdit->setStyleSheet(LTZKUiScale::scaleStyleSheetPixels(
                QStringLiteral("QLineEdit { background: palette(base); %1%2 }").arg(
                    normalizeSeriesRelationControlFont ? seriesRelationControlFontFragment : fontStyleSheetFragment(firstRowPathSelectorControlFont(view->font())),
                    embeddedLargeControlMetricsFragment),
                LTZKUiScale::factorForWidget(lineEdit)));
        }
        else
        {
            lineEdit->setStyleSheet(QStringLiteral("QLineEdit { background: palette(base); }"));
        }
        if (drivePathMatch.hasMatch())
        {
            const QString pathText = drivePathMatch.captured(0).trimmed();
            if (!pathText.isEmpty() && pathText != lineEdit->text())
            {
                QSignalBlocker blocker(lineEdit);
                lineEdit->setText(pathText);
                lineEdit->setCursorPosition(0);
            }
        }

        QRect cellRect = view->visualRect(index);
        const int reservedButtonWidth = (normalizeSeriesRelationControlFont || normalizePathLineEditFont) ? 104 : 76;
        const int treeIndicatorReserve = (index.column() == 0 && qobject_cast<QTreeView*>(view))
            ? view->style()->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, view) + 18
            : 0;
        hidePluginCellLabels(widget, view, index);
        const int labelReserve = index.column() == 0 ? treeIndicatorReserve : 0;
        const int editLeft = cellRect.left() + labelReserve + 6;
        const int editRight = cellRect.right() - reservedButtonWidth - 8;
        const int editWidth = qMax(80, editRight - editLeft + 1);
        const int editHeight = (normalizeSeriesRelationControlFont || normalizePathLineEditFont)
            ? qMax(28, qMin(cellRect.height() - 4, 36))
            : qMax(16, qMin(cellRect.height() - 4, 20));
        QRect editRect(editLeft, cellRect.top() + qMax(2, (cellRect.height() - editHeight) / 2), editWidth, editHeight);
        const QPoint mappedTopLeft = view->viewport()->mapTo(lineEdit->parentWidget(), editRect.topLeft());
        editRect.moveTopLeft(mappedTopLeft);
        lineEdit->setGeometry(editRect);
    }

    const QList<QLabel*> labels = widget->findChildren<QLabel*>();
    for (QLabel* label : labels)
    {
        if (!label)
        {
            continue;
        }

        QAbstractItemView* view = viewForChild(label);
        const QModelIndex index = indexForChild(view, label);
        if (!view || !index.isValid() || !isSeriesCrossScaleRelationView(view))
        {
            continue;
        }

        label->setStyleSheet(QStringLiteral("QLabel { %1 }").arg(fontStyleSheetFragment(seriesCrossScaleRelationTableFont(view->font()))));
    }

    const QList<QAbstractButton*> buttons = widget->findChildren<QAbstractButton*>();
    for (QAbstractButton* button : buttons)
    {
        if (!button)
        {
            continue;
        }

        QAbstractItemView* view = viewForChild(button);
        const QModelIndex index = indexForChild(view, button);
        if (!view || !index.isValid())
        {
            continue;
        }

        clearPluginTableWidgetIndexItemDisplayText(view, index);
        suppressPluginEmbeddedControlItemText(view, index);

        const bool normalizeSelectorButtonFont = isFirstRowPathSelectorControl(index)
            && (button->text().contains(QStringLiteral("配置"))
                || button->text().contains(QStringLiteral("选择"))
                || button->text().contains(QStringLiteral("文件夹"))
                || button->text().contains(QStringLiteral("文件路径"))
                || button->text().contains(QStringLiteral("路径")));
        const bool normalizeSeriesRelationControlFont = isSeriesCrossScaleRelationView(view);
        if (normalizeSeriesRelationControlFont || normalizeSelectorButtonFont)
        {
            button->setStyleSheet(LTZKUiScale::scaleStyleSheetPixels(
                QStringLiteral("QPushButton, QToolButton { %1%2 }").arg(
                    normalizeSeriesRelationControlFont
                        ? fontStyleSheetFragment(seriesCrossScaleRelationTableFont(view->font()))
                        : fontStyleSheetFragment(firstRowPathSelectorControlFont(view->font())),
                    embeddedLargeControlMetricsFragment),
                LTZKUiScale::factorForWidget(button)));
        }

        const QFont buttonMetricFont = normalizeSeriesRelationControlFont
            ? seriesCrossScaleRelationTableFont(view->font())
            : (normalizeSelectorButtonFont ? firstRowPathSelectorControlFont(view->font()) : button->font());
        const QFontMetrics buttonFontMetrics(buttonMetricFont);
        const bool isEllipsisButton = button->text().trimmed() == QStringLiteral("..."); // used by buttonWidth calculation
        const int buttonWidth = qMax(
            isEllipsisButton
                ? 40
                : (normalizeSeriesRelationControlFont || normalizeSelectorButtonFont ? 104 : 76),
            buttonFontMetrics.horizontalAdvance(button->text()) + (isEllipsisButton ? 16 : 40));
        button->setMinimumWidth(buttonWidth);
        button->setMaximumWidth(buttonWidth);
        button->setFixedWidth(buttonWidth);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        QRect cellRect = view->visualRect(index);
        const int buttonHeight = (normalizeSeriesRelationControlFont || normalizeSelectorButtonFont)
            ? qMax(28, qMin(cellRect.height() - 4, 36))
            : qMax(16, qMin(cellRect.height() - 4, 20));
        QRect buttonRect(cellRect.right() - buttonWidth - 6, cellRect.top() + qMax(2, (cellRect.height() - buttonHeight) / 2), buttonWidth, buttonHeight);
        const QPoint mappedTopLeft = view->viewport()->mapTo(button->parentWidget(), buttonRect.topLeft());
        buttonRect.moveTopLeft(mappedTopLeft);
        button->setGeometry(buttonRect);
    }
    LTZKSeriesTableTypography::apply(widget);
}
void LTZK::schedulePluginTableEmbeddedTextSuppression()
{
    if (m_workspaceContentStack && m_workspaceContentStack->serviceDialog()
        && m_workspaceContentStack->serviceDialog()->isVisible())
    {
        installPluginTableEmbeddedWidgetDelegates(m_workspaceContentStack->serviceDialog());
    }
    for (QWidget* widget : QApplication::topLevelWidgets())
    {
        if (!widget || widget == this || !widget->isWindow())
        {
            continue;
        }
        if (widget->isHidden() || widget->testAttribute(Qt::WA_WState_Hidden))
        {
            continue;
        }
        installPluginTableEmbeddedWidgetDelegates(widget);
    }
}

void LTZK::scheduleSeriesContentTablePolish()
{
    QTimer::singleShot(350, this, &LTZK::polishSeriesContentTableWindows);
    QTimer::singleShot(900, this, &LTZK::polishSeriesContentTableWindows);
}

void LTZK::polishSeriesContentTableWindows()
{
    for (QWidget* widget : QApplication::topLevelWidgets())
    {
        if (!widget || widget == this || !widget->isWindow())
        {
            continue;
        }
        if (widget->isHidden() || widget->testAttribute(Qt::WA_WState_Hidden))
        {
            continue;
        }

        const QString title = widget->windowTitle();
        if (title.contains(QStringLiteral("质检任务管理")))
        {
            continue;
        }
        if (!title.contains(QStringLiteral("系列图内容检查"))
            || (!title.contains(QStringLiteral("图幅接边")) && !title.contains(QStringLiteral("系列化表达"))))
        {
            continue;
        }

        polishSeriesContentTableWindow(widget);
    }
}

void LTZK::polishSeriesContentTableWindow(QWidget* widget)
{
    if (!widget)
    {
        return;
    }

    auto viewForChild = [](QWidget* child) -> QAbstractItemView*
    {
        for (QWidget* parent = child ? child->parentWidget() : nullptr; parent; parent = parent->parentWidget())
        {
            if (QAbstractItemView* view = qobject_cast<QAbstractItemView*>(parent))
            {
                return view;
            }
        }
        return nullptr;
    };

    auto indexForChild = [](QAbstractItemView* view, QWidget* child) -> QModelIndex
    {
        if (!view || !child || !child->parentWidget())
        {
            return QModelIndex();
        }

        const QPoint viewportPoint = child->parentWidget()->mapTo(view->viewport(), child->geometry().center());
        return view->indexAt(viewportPoint);
    };

    auto readableTableFont = [](const QFont& sourceFont) -> QFont
    {
        QFont font(sourceFont);
        const qreal pointSize = font.pointSizeF() > 0 ? font.pointSizeF() : 9.0;
        font.setPointSizeF(qMax(pointSize,9.0));
        return font;
    };

    auto isSeriesConfigTree = [](QTreeWidget* tree) -> bool
    {
        if (!tree || tree->columnCount() != 6 || !tree->headerItem())
        {
            return false;
        }

        return tree->headerItem()->text(0) == QStringLiteral("主检查地图")
            && tree->headerItem()->text(1) == QStringLiteral("主图比例尺")
            && tree->headerItem()->text(2) == QStringLiteral("关联地图")
            && tree->headerItem()->text(5) == QStringLiteral("状态");
    };

    const QColor seriesItemWidgetBackgroundColor = m_currentTheme == PlatformTheme::White
        ? QColor(QStringLiteral("#ffffff"))
        : (m_currentTheme == PlatformTheme::FieldNotes
            ? QColor(QStringLiteral("#FAF5E8"))
            : (m_currentTheme == PlatformTheme::SteelWarmWhite
                ? QColor(QStringLiteral("#FFFFFF"))
                : (m_currentTheme == PlatformTheme::Black
                    ? QColor(QStringLiteral("#0f0f0f"))
                    : QColor(QStringLiteral("#06111d")))));
    auto polishSeriesItemWidgetPaintState = [seriesItemWidgetBackgroundColor](QTreeWidget* tree, QWidget* itemWidget)
    {
        if (!tree || !itemWidget)
        {
            return;
        }

        if (QLayout* itemWidgetLayout = itemWidget->layout())
        {
            itemWidgetLayout->setContentsMargins(0, 0, 0, 0);
            itemWidgetLayout->setSpacing(0);
        }

        QPalette itemWidgetPalette = itemWidget->palette();
        itemWidgetPalette.setColor(QPalette::Window, seriesItemWidgetBackgroundColor);
        itemWidget->setPalette(itemWidgetPalette);
        itemWidget->setBackgroundRole(QPalette::Window);
        itemWidget->setAttribute(Qt::WA_StyledBackground, true);
        itemWidget->setAutoFillBackground(true);
        itemWidget->raise();
        itemWidget->update();
    };

    std::function<void(QTreeWidget*, QTreeWidgetItem*)> clearSeriesTreeItemWidgetText =
        [&](QTreeWidget* tree, QTreeWidgetItem* item)
    {
        if (!tree || !item)
        {
            return;
        }

        for (int column = 0; column < tree->columnCount(); ++column)
        {
            QWidget* itemWidget = tree->itemWidget(item, column);
            if (!itemWidget)
            {
                continue;
            }

            polishSeriesItemWidgetPaintState(tree, itemWidget);
            const QString itemText = item->text(column);
            if (!itemText.isEmpty())
            {
                if (!item->data(column, Qt::UserRole).isValid())
                {
                    item->setData(column, Qt::UserRole, itemText);
                }
                if (item->toolTip(column).isEmpty())
                {
                    item->setToolTip(column, itemText);
                }
                item->setText(column, QString());
            }
        }

        for (int childIndex = 0; childIndex < item->childCount(); ++childIndex)
        {
            clearSeriesTreeItemWidgetText(tree, item->child(childIndex));
        }
    };

    const QList<QTreeWidget*> trees = widget->findChildren<QTreeWidget*>();
    for (QTreeWidget* tree : trees)
    {
        if (!isSeriesConfigTree(tree))
        {
            continue;
        }

        if (!tree->property("ltzkSeriesConfigTreePolishTarget").toBool())
        {
            tree->setProperty("ltzkSeriesConfigTreePolishTarget", true);
            tree->installEventFilter(this);
        }
        for (int topLevelIndex = 0; topLevelIndex < tree->topLevelItemCount(); ++topLevelIndex)
        {
            clearSeriesTreeItemWidgetText(tree, tree->topLevelItem(topLevelIndex));
        }
        tree->doItemsLayout();
        if (tree->viewport())
        {
            if (tree->viewport())
            {
                tree->viewport()->update();
            }
        }
    }
    const QList<QAbstractItemView*> views = widget->findChildren<QAbstractItemView*>();
    for (QAbstractItemView* view : views)
    {
        if (!view)
        {
            continue;
        }

        const QFont tableFont = readableTableFont(view->font());
        view->setFont(tableFont);
        view->setTextElideMode(Qt::ElideMiddle);
        if (QTableView* tableView = qobject_cast<QTableView*>(view))
        {
            if (QHeaderView* verticalHeader = tableView->verticalHeader())
            {
                const int minimumRowHeight = scaledInt(view, 28);
                verticalHeader->setMinimumSectionSize(qMax(verticalHeader->minimumSectionSize(), minimumRowHeight));
                verticalHeader->setDefaultSectionSize(qMax(verticalHeader->defaultSectionSize(), minimumRowHeight));
            }
        }

        QAbstractItemModel* model = view->model();
        if (!model)
        {
            continue;
        }

        const int rowLimit = qMin(model->rowCount(), 80);
        const int columnLimit = qMin(model->columnCount(), 20);
        for (int row = 0; row < rowLimit; ++row)
        {
            for (int column = 0; column < columnLimit; ++column)
            {
                const QModelIndex index = model->index(row, column);
                if (index.isValid() && view->indexWidget(index))
                {
                    clearPluginTableWidgetIndexItemDisplayText(view, index);
                    suppressPluginEmbeddedControlItemText(view, index);
                }
            }
        }
    }

    const QString lineEditStyle = QStringLiteral(
        "QLineEdit { background: palette(base); border: 0; padding: 2px 6px; min-height: 22px; max-height: 28px; }"
        "QLineEdit:focus { background: palette(base); border: 0; padding: 2px 6px; min-height: 22px; max-height: 28px; }");
    const QList<QLineEdit*> lineEdits = widget->findChildren<QLineEdit*>();
    for (QLineEdit* lineEdit : lineEdits)
    {
        if (!lineEdit)
        {
            continue;
        }
        if (isComboBoxInternalLineEdit(lineEdit))
        {
            continue;
        }

        QAbstractItemView* view = viewForChild(lineEdit);
        const QModelIndex index = indexForChild(view, lineEdit);
        if (!view || !index.isValid())
        {
            continue;
        }

        clearPluginTableWidgetIndexItemDisplayText(view, index);
        suppressPluginEmbeddedControlItemText(view, index);
        lineEdit->setFont(readableTableFont(lineEdit->font()));
        lineEdit->setAttribute(Qt::WA_StyledBackground, true);
        const int minimumControlHeight = scaledInt(lineEdit, 22);
        const int maximumControlHeight = scaledInt(lineEdit, 28);
        lineEdit->setMinimumHeight(qMax(lineEdit->minimumHeight(), minimumControlHeight));
        lineEdit->setMaximumHeight(lineEdit->maximumHeight() == QWIDGETSIZE_MAX
            ? maximumControlHeight
            : qMin(qMax(lineEdit->maximumHeight(), minimumControlHeight), maximumControlHeight));
        lineEdit->setStyleSheet(LTZKUiScale::scaleStyleSheetPixels(lineEditStyle, LTZKUiScale::factorForWidget(lineEdit)));
        lineEdit->update();
    }

    const QString buttonTextColor = m_currentTheme == PlatformTheme::White
        ? QStringLiteral("#1e293b")
        : (m_currentTheme == PlatformTheme::FieldNotes
            ? QStringLiteral("#3E4D3D")
            : (m_currentTheme == PlatformTheme::SteelWarmWhite
                ? QStringLiteral("#2D3D40")
                : (m_currentTheme == PlatformTheme::Black ? QStringLiteral("#fafafa") : QStringLiteral("#f8feff"))));
    const QString buttonBackgroundColor = m_currentTheme == PlatformTheme::White
        ? QStringLiteral("#ffffff")
        : (m_currentTheme == PlatformTheme::FieldNotes
            ? QStringLiteral("#F1E5CE")
            : (m_currentTheme == PlatformTheme::SteelWarmWhite
                ? QStringLiteral("#F4F2EB")
                : (m_currentTheme == PlatformTheme::Black ? QStringLiteral("#0f0f0f") : QStringLiteral("#06111d"))));
    const QString buttonSeparatorColor = m_currentTheme == PlatformTheme::White
        ? QStringLiteral("#cbd5e1")
        : (m_currentTheme == PlatformTheme::FieldNotes
            ? QStringLiteral("#C7B88F")
            : (m_currentTheme == PlatformTheme::SteelWarmWhite
                ? QStringLiteral("#AAB6B3")
                : (m_currentTheme == PlatformTheme::Black ? QStringLiteral("#525252") : QStringLiteral("rgba(91, 206, 255, 95)"))));
    const QString buttonHoverTextColor = m_currentTheme == PlatformTheme::White
        ? QStringLiteral("#1d4ed8")
        : (m_currentTheme == PlatformTheme::FieldNotes
            ? QStringLiteral("#3D5540")
            : (m_currentTheme == PlatformTheme::SteelWarmWhite
                ? QStringLiteral("#2F5960")
                : (m_currentTheme == PlatformTheme::Black ? QStringLiteral("#fef3c7") : QStringLiteral("#ffffff"))));
    const QString buttonHoverBackgroundColor = m_currentTheme == PlatformTheme::White
        ? QStringLiteral("#eff6ff")
        : (m_currentTheme == PlatformTheme::FieldNotes
            ? QStringLiteral("#E4DFC8")
            : (m_currentTheme == PlatformTheme::SteelWarmWhite
                ? QStringLiteral("#DDE8E3")
                : (m_currentTheme == PlatformTheme::Black ? QStringLiteral("#171717") : QStringLiteral("#082644"))));
    const QString buttonHoverSeparatorColor = m_currentTheme == PlatformTheme::White
        ? QStringLiteral("#2563eb")
        : (m_currentTheme == PlatformTheme::FieldNotes
            ? QStringLiteral("#6C7956")
            : (m_currentTheme == PlatformTheme::SteelWarmWhite
                ? QStringLiteral("#668B8C")
                : (m_currentTheme == PlatformTheme::Black ? QStringLiteral("#f59e0b") : QStringLiteral("rgba(91, 206, 255, 140)"))));
    const QString buttonStyle = QStringLiteral(
        "QPushButton, QToolButton { color: %1; background: %2; border: 0; border-left: 1px solid %3; padding: 2px 8px; min-height: 22px; max-height: 28px; font-weight: 600; }"
        "QPushButton:hover, QToolButton:hover { color: %4; background: %5; border-left-color: %6; }")
        .arg(buttonTextColor, buttonBackgroundColor, buttonSeparatorColor,
            buttonHoverTextColor, buttonHoverBackgroundColor, buttonHoverSeparatorColor);
    const QList<QAbstractButton*> buttons = widget->findChildren<QAbstractButton*>();
    for (QAbstractButton* button : buttons)
    {
        QAbstractItemView* view = viewForChild(button);
        const QModelIndex index = indexForChild(view, button);
        if (!button || !view || !index.isValid())
        {
            continue;
        }

        clearPluginTableWidgetIndexItemDisplayText(view, index);
        suppressPluginEmbeddedControlItemText(view, index);
        button->setFont(readableTableFont(button->font()));
        const int minimumControlHeight = scaledInt(button, 22);
        const int maximumControlHeight = scaledInt(button, 28);
        button->setMinimumHeight(qMax(button->minimumHeight(), minimumControlHeight));
        button->setMaximumHeight(button->maximumHeight() == QWIDGETSIZE_MAX
            ? maximumControlHeight
            : qMin(qMax(button->maximumHeight(), minimumControlHeight), maximumControlHeight));
        button->setStyleSheet(LTZKUiScale::scaleStyleSheetPixels(buttonStyle, LTZKUiScale::factorForWidget(button)));
        button->update();
    }

    widget->update();
    LTZKSeriesTableTypography::apply(widget);
}
void LTZK::applyPlatformStyleToTopLevelWidgets()
{
    if (m_workspaceContentStack && m_workspaceContentStack->serviceDialog())
    {
        applyPlatformStyleToPluginWindow(m_workspaceContentStack->serviceDialog());
    }
    for (QWidget* widget : QApplication::topLevelWidgets())
    {
        if (!widget || widget == this || widget->windowType() == Qt::ToolTip)
        {
            continue;
        }

        if (widget->isWindow())
        {
            applyPlatformStyleToPluginWindow(widget);
        }
    }
}

void LTZK::applySciFiStyleToPluginWindow(QWidget* widget)
{
    applyPlatformStyleToPluginWindow(widget);
}

void LTZK::applySciFiStyleToTopLevelWidgets()
{
    applyPlatformStyleToTopLevelWidgets();
}

void LTZK::cachePluginActionsForDll(const QString& pluginPath, const QList<QAction*>& actions)
{
    QList<QPointer<QAction>> cachedActions;
    for (QAction* action : actions)
    {
        if (!action || action->isSeparator()
            || action->objectName().startsWith(QStringLiteral("pluginFeature_")))
        {
            continue;
        }

        bool alreadyCached = false;
        for (const QPointer<QAction>& cachedAction : cachedActions)
        {
            if (cachedAction.data() == action)
            {
                alreadyCached = true;
                break;
            }
        }
        if (!alreadyCached)
        {
            cachedActions.append(QPointer<QAction>(action));
        }
    }

    m_pluginActionsByDllPath.insert(pluginActionCacheKey(pluginPath), cachedActions);
}

QList<QAction*> LTZK::pluginActionsForDll(const QString& pluginPath) const
{
    QList<QAction*> actions;
    const QList<QPointer<QAction>> cachedActions = m_pluginActionsByDllPath.value(pluginActionCacheKey(pluginPath));
    for (const QPointer<QAction>& cachedAction : cachedActions)
    {
        QAction* action = cachedAction.data();
        if (!action || action->isSeparator()
            || action->objectName().startsWith(QStringLiteral("pluginFeature_"))
            || actions.contains(action))
        {
            continue;
        }
        actions.append(action);
    }
    return actions;
}

void LTZK::executeConfiguredPluginFeature(
    const QString& pluginId,
    const QString& featureName)
{
    for (const PluginDefinition& plugin : m_pluginBindingConfig.plugins)
    {
        if (!plugin.enabled || plugin.id != pluginId)
        {
            continue;
        }

        for (const FeatureBinding& binding : plugin.bindings)
        {
            if (binding.enabled && binding.featureName == featureName)
            {
                executePluginFeatureAction(plugin, binding);
                return;
            }
        }
    }

    QMessageBox::warning(
        this,
        QStringLiteral("插件动作未找到"),
        QStringLiteral("未找到配置的插件功能：%1").arg(featureName));
}

void LTZK::executePluginFeatureAction(const PluginDefinition& plugin, const FeatureBinding& binding)
{
    if (!m_pluginManager)
    {
        return;
    }

    if (binding.requiresAccount && !gCurrentUserSession.isLoggedIn)
    {
        if (gCurrentUserSession.isGuest)
        {
            QMessageBox prompt(this);
            prompt.setIcon(QMessageBox::Information);
            prompt.setWindowTitle(QStringLiteral("需要登录账户"));
            prompt.setText(QStringLiteral("匿名模式无法使用“%1”，此功能需要登录账户后才能连接数据库或远程服务。")
                .arg(binding.featureName));
            prompt.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
            prompt.setButtonText(QMessageBox::Yes, QStringLiteral("登录账户"));
            prompt.setButtonText(QMessageBox::Cancel, QStringLiteral("取消"));
            if (prompt.exec() == QMessageBox::Yes)
            {
                showHomeLoginPage();
                if (m_homeLoginStatusLabel)
                {
                    m_homeLoginStatusLabel->setText(QStringLiteral("请登录账户以使用“%1”。")
                        .arg(binding.featureName));
                }
            }
        }
        else
        {
            promptLoginForModule(binding.featureName);
        }
        return;
    }

    const QString bindingModuleTitle = binding.menuPath.isEmpty()
        ? QString()
        : businessModuleDisplayTitle(binding.menuPath.first());
    if (!bindingModuleTitle.isEmpty() && !ensureAuthenticatedForModule(bindingModuleTitle))
    {
        return;
    }
    const QVariantMap catalogRequest = QCoreApplication::instance()
        ? QCoreApplication::instance()->property("ltzkProductCatalogRequest").toMap()
        : QVariantMap();
    const bool catalogProductAction = !catalogRequest.isEmpty() &&
        plugin.id == QStringLiteral("map_product_manager_tools") &&
        catalogRequest.contains(QStringLiteral("operation")) &&
        catalogRequest.contains(QStringLiteral("connection"));
    if (!bindingModuleTitle.isEmpty()
        && !catalogProductAction
        && businessModuleMenuTitle(bindingModuleTitle)
            != businessModuleMenuTitle(m_currentBusinessModuleTitle))
    {
        statusBar()->showMessage(
            QStringLiteral("\u8BF7\u5148\u8FDB\u5165\u5BF9\u5E94\u5B50\u5E73\u53F0\u518D\u542F\u52A8\u529F\u80FD\u3002"),
            4000);
        return;
    }
    const bool watchesResultOutputs = plugin.watchResultOutputs && binding.watchResultOutputs;
    const QString resultModuleTitle = bindingModuleTitle;
    if (!catalogProductAction &&
        activatePluginFeatureWindows(plugin.id, binding.featureName, bindingModuleTitle))
    {
        if (watchesResultOutputs)
        {
            beginQualityResultMonitoring(resultModuleTitle, binding.resultOrganization);
            scheduleQualityResultPostActionScan();
        }
        return;
    }

    const QList<QAction*> beforeActions = collectPluginExecutableActions(menuBar(), m_pluginMenu, m_pluginToolBar);
    QString pluginError;
    const QString pluginPath = resolvePluginDllPath(plugin.dll);
    bool loadedNow = m_pluginManager->loadPlugin(pluginPath, &pluginError);
    if (loadedNow)
    {
        logLtzKMessage(QStringLiteral("Plugin loaded: %1").arg(pluginPath));
    }
    else if (!pluginError.contains(QStringLiteral("already loaded"), Qt::CaseInsensitive))
    {
        logLtzKMessage(QStringLiteral("Plugin load failed: %1").arg(pluginError), Qgis::Warning);
        QMessageBox::critical(this, QStringLiteral("\u63D2\u4EF6\u52A0\u8F7D\u5931\u8D25"), pluginError);
        return;
    }

    if (plugin.id == QStringLiteral("map_product_manager_tools") &&
        binding.featureName == QStringLiteral("成果目录操作"))
    {
        const QVariantMap request = QCoreApplication::instance()
            ? QCoreApplication::instance()->property("ltzkProductCatalogRequest").toMap()
            : QVariantMap();
        QByteArray operationUtf8;
        QVector<int32_t> productIds;
        QByteArray connectionJsonUtf8;
        LTZKProductCatalogOperationRequest apiRequest{};
        if (request.value(QStringLiteral("localSources")).toList().isEmpty() &&
            buildProductCatalogApiRequest(
                request,
                operationUtf8,
                productIds,
                connectionJsonUtf8,
                apiRequest))
        {
            int apiResult = LTZK_PRODUCT_CATALOG_DISPATCH_FAILED;
            QString apiError;
            const bool apiResolved = m_pluginManager->invokeProductCatalogOperation(
                pluginPath,
                apiRequest,
                &apiResult,
                &apiError);
            if (apiResolved)
            {
                if (apiResult == LTZK_PRODUCT_CATALOG_ACCEPTED ||
                    apiResult == LTZK_PRODUCT_CATALOG_QUEUED)
                {
                    if (QCoreApplication::instance())
                    {
                        QCoreApplication::instance()->setProperty(
                            "ltzkProductCatalogRequest", QVariant());
                        QCoreApplication::instance()->setProperty(
                            "ltzkProductImportTargetDirId", QVariant());
                    }
                    logLtzKMessage(
                        QStringLiteral("Product catalog API dispatched: operation=%1, result=%2")
                            .arg(QString::fromUtf8(operationUtf8))
                            .arg(apiResult));
                    QTimer::singleShot(0, this, &LTZK::applyPlatformStyleToTopLevelWidgets);
                    QTimer::singleShot(0, this, &LTZK::schedulePluginTableEmbeddedTextSuppression);
                    return;
                }

                if (QCoreApplication::instance())
                {
                    QCoreApplication::instance()->setProperty(
                        "ltzkProductCatalogRequest", QVariant());
                    QCoreApplication::instance()->setProperty(
                        "ltzkProductImportTargetDirId", QVariant());
                }
                const QString detail = apiError.isEmpty()
                    ? QStringLiteral("返回码：%1").arg(apiResult)
                    : apiError;
                logLtzKMessage(
                    QStringLiteral("Product catalog API dispatch failed: %1").arg(detail),
                    Qgis::Warning);
                QMessageBox::warning(
                    this,
                    QStringLiteral("成果目录操作失败"),
                    detail);
                return;
            }

            // 兼容尚未重新编译的新旧 DLL：API 不存在时继续使用旧的 QAction 桥接。
            if (!apiError.isEmpty())
            {
                logLtzKMessage(
                    QStringLiteral("Product catalog API unavailable, fallback to QAction: %1")
                        .arg(apiError),
                    Qgis::Info);
            }
        }
    }

    QList<QAction*> pluginActions;
    if (loadedNow)
    {
        const QList<QAction*> afterActions = collectPluginExecutableActions(menuBar(), m_pluginMenu, m_pluginToolBar);
        cachePluginActionsForDll(pluginPath, actionsAddedByPlugin(beforeActions, afterActions));
    }
    pluginActions = pluginActionsForDll(pluginPath);

    if (loadedNow)
    {
        applyConfiguredPluginActionIcons(plugin.actionIcons, pluginActions, platformThemeKey(m_currentTheme));
    }

    const bool productDataImport =
        plugin.id == QStringLiteral("map_product_manager_tools") &&
        binding.featureName == QStringLiteral("数据导入");
    QAction* pluginAction = productDataImport
        ? findProductDataImportAction(pluginActions)
        : findConfiguredPluginAction(
            pluginActions, binding.actionKeywords, binding.fallbackActionIndex);
    if (!pluginAction && productDataImport)
    {
        // The plugin may already be loaded while the per-DLL action cache is empty.
        // Its own menu remains a child of this window even when hidden.
        QMenu* productMenu = findChild<QMenu*>(QStringLiteral("mMapProductMenu"));
        if (productMenu)
        {
            pluginAction = findProductDataImportAction(productMenu->actions());
        }
    }
    if (!pluginAction
        && plugin.id == QStringLiteral("map_product_manager_tools")
        && binding.featureName == QStringLiteral("成果目录操作"))
    {
        // 该动作由 DLL 放在隐藏菜单项中，优先从当前菜单树重新查找，
        // 避免旧缓存或 DLL 已提前加载时把动作解析成“用户登录”。
        const QList<QAction*> livePluginActions = collectPluginExecutableActions(menuBar(), m_pluginMenu, m_pluginToolBar);
        pluginAction = findProductCatalogOperationAction(livePluginActions);
        if (pluginAction)
        {
            logLtzKMessage(
                QStringLiteral("Resolved product catalog bridge action from live plugin menus: %1 [%2]")
                    .arg(pluginAction->text(), pluginAction->objectName()));
        }
    }
    if (!pluginAction)
    {
        QStringList availableActions;
        for (QAction* action : pluginActions)
        {
            if (action)
            {
                availableActions.append(
                    QStringLiteral("%1 [%2]").arg(action->text(), action->objectName()));
            }
        }
        logLtzKMessage(
            QStringLiteral("Plugin action not found: plugin=%1, feature=%2, actions=%3")
                .arg(plugin.id, binding.featureName, availableActions.join(QStringLiteral(", "))),
            Qgis::Warning);
        QMessageBox::warning(this, QStringLiteral("\u63D2\u4EF6\u52A8\u4F5C\u672A\u627E\u5230"), QStringLiteral("\u63D2\u4EF6\u5DF2\u52A0\u8F7D\uFF0C\u4F46\u672A\u627E\u5230\u7ED1\u5B9A\u914D\u7F6E\u4E2D\u7684\u76EE\u6807\u83DC\u5355\u52A8\u4F5C\u3002"));
        return;
    }

    if (watchesResultOutputs)
    {
        beginQualityResultMonitoring(resultModuleTitle, binding.resultOrganization);
    }

    m_pluginWindowCapturePluginId = plugin.id;
    m_pluginWindowCaptureFeatureName = binding.featureName;
    m_pluginWindowCaptureModuleTitle = bindingModuleTitle;
    m_pluginWindowCaptureBusinessModuleGeneration = m_businessModuleGeneration;
    m_pluginWindowCaptureBaseline.clear();
    const QList<QWidget*> topLevelWidgets = QApplication::topLevelWidgets();
    for (QWidget* topLevelWidget : topLevelWidgets)
    {
        if (topLevelWidget)
        {
            m_pluginWindowCaptureBaseline.insert(topLevelWidget);
        }
    }
    const quint64 captureGeneration = ++m_pluginWindowCaptureGeneration;
    pluginAction->trigger();
    QTimer::singleShot(3000, this, [this, captureGeneration]()
    {
        if (m_pluginWindowCaptureGeneration != captureGeneration)
        {
            return;
        }
        m_pluginWindowCapturePluginId.clear();
        m_pluginWindowCaptureFeatureName.clear();
        m_pluginWindowCaptureModuleTitle.clear();
        m_pluginWindowCaptureBaseline.clear();
        m_pluginWindowCaptureBusinessModuleGeneration = 0;
    });
    if (watchesResultOutputs)
    {
        scheduleQualityResultPostActionScan();
    }
    QTimer::singleShot(0, this, &LTZK::applyPlatformStyleToTopLevelWidgets);
    QTimer::singleShot(0, this, &LTZK::schedulePluginTableEmbeddedTextSuppression);
}

void LTZK::bindPluginFeatureActions(const PluginBindingConfig& config)
{
    if (!m_pluginManager)
    {
        return;
    }

    for (const PluginDefinition& plugin : config.plugins)
    {
        if (!plugin.enabled)
        {
            continue;
        }

        for (const FeatureBinding& binding : plugin.bindings)
        {
            if (!binding.enabled || !binding.showInMenu || binding.menuPath.isEmpty() || binding.featureName.isEmpty())
            {
                continue;
            }

            QMenu* menu = ensureConfiguredMenuPath(this, binding.menuPath);
            if (!menu)
            {
                continue;
            }

            QAction* featureAction = menu->addAction(binding.featureName);
            featureAction->setObjectName(QStringLiteral("pluginFeature_%1_%2").arg(plugin.id, binding.featureName));
            const QIcon icon = themeAwareConfiguredIcon(binding.icon, platformThemeKey(m_currentTheme));
            if (!icon.isNull())
            {
                featureAction->setIcon(icon);
            }

            const PluginDefinition pluginCopy = plugin;
            const FeatureBinding bindingCopy = binding;
            connect(featureAction, &QAction::triggered, this, [this, pluginCopy, bindingCopy]() {
                executePluginFeatureAction(pluginCopy, bindingCopy);
                });
        }
    }
}

void LTZK::refreshConfiguredPluginActionIcons()
{
    const QString themeKey = platformThemeKey(m_currentTheme);
    for (const PluginDefinition& plugin : m_pluginBindingConfig.plugins)
    {
        if (!plugin.enabled || plugin.id != QStringLiteral("map_generalization_tools"))
        {
            continue;
        }

        for (const FeatureBinding& binding : plugin.bindings)
        {
            if (!binding.enabled || !binding.showInMenu || !binding.icon.contains(QStringLiteral("icon_intelligent_")))
            {
                continue;
            }

            const QString objectName = QStringLiteral("pluginFeature_%1_%2").arg(plugin.id, binding.featureName);
            QAction* featureAction = findChild<QAction*>(objectName);
            const QIcon icon = themeAwareConfiguredIcon(binding.icon, themeKey);
            if (featureAction && !icon.isNull())
            {
                featureAction->setIcon(icon);
            }
        }
    }
}

void LTZK::openOwsSourceSelect(const QString& service)
{
    openDataSourceManager(service.toLower());
}

void LTZK::loadLocalGansuShpBasemap()
{
    startupTrace(QStringLiteral("loadLocalGansuShpBasemap begin"));
    if (!m_mapCanvas)
    {
        return;
    }

    applyWorkspaceShpCanvasBackground(m_workspaceUsesLightShpCanvas);
    m_mapCanvas->refresh();

    const QString gansuPath = localGansuShpBasemapPath();
    const QString layerName = QStringLiteral("\u7518\u8083\u7701\u672c\u5730\u5e95\u56fe");

    for (QgsMapLayer* layer : QgsProject::instance()->mapLayers())
    {
        if (layer && layer->customProperty(QStringLiteral("ltzk/systemBasemap")).toBool() && layer->name() == layerName)
        {
            refreshMapCanvasLayerStack();
            setGansuInitialMapView();
            return;
        }
    }

    QgsVectorLayer* gansuLayer = new QgsVectorLayer(gansuPath, layerName, QStringLiteral("ogr"));
    if (!gansuLayer->isValid())
    {
        logLtzKMessage(QStringLiteral("Local Gansu SHP basemap load failed: %1").arg(gansuPath), Qgis::Warning);
        delete gansuLayer;
        setGansuInitialMapView();
        return;
    }

    gansuLayer->setSubsetString(QStringLiteral("\"\u7701\u7ea7\" = '\u7518\u8083\u7701'"));
    gansuLayer->setCustomProperty(QStringLiteral("ltzk/systemBasemap"), true);
    styleLocalGansuBasemapLayer(gansuLayer);
    QgsProject::instance()->addMapLayer(gansuLayer, false);
    QgsProject::instance()->layerTreeRoot()->insertLayer(0, gansuLayer);

    refreshMapCanvasLayerStack();
    setGansuInitialMapView();
    logLtzKMessage(QStringLiteral("Local Gansu SHP basemap loaded."));
}

void LTZK::styleLocalGansuBasemapLayer(QgsVectorLayer* layer)
{
    if (!layer)
    {
        return;
    }

    const bool isLightTheme = isLightPlatformTheme();
    QVariantMap properties;
    const bool isFieldNotesTheme = m_currentTheme == PlatformTheme::FieldNotes;
    const bool isSteelWarmWhiteTheme = m_currentTheme == PlatformTheme::SteelWarmWhite;
    properties.insert(QStringLiteral("color"),
        m_currentTheme == PlatformTheme::White ? QStringLiteral("241,245,249,235")
        : (isFieldNotesTheme ? QStringLiteral("230,223,197,235")
            : (isSteelWarmWhiteTheme ? QStringLiteral("213,222,218,235") : QStringLiteral("6,26,45,210"))));
    properties.insert(QStringLiteral("outline_color"),
        m_currentTheme == PlatformTheme::White ? QStringLiteral("71,85,105,225")
        : (isFieldNotesTheme ? QStringLiteral("115,134,95,225")
            : (isSteelWarmWhiteTheme ? QStringLiteral("102,139,140,225") : QStringLiteral("255,255,255,245"))));
    properties.insert(QStringLiteral("outline_width"), QStringLiteral("0.28"));
    properties.insert(QStringLiteral("outline_style"), QStringLiteral("solid"));

    QgsFillSymbol* symbol = QgsFillSymbol::createSimple(properties);
    if (symbol)
    {
        layer->setRenderer(new QgsSingleSymbolRenderer(symbol));
    }
    layer->setOpacity(isLightTheme ? 1.0 : 0.82);
    applyLocalGansuBasemapLabels(layer, isLightTheme);
}
void LTZK::setGansuInitialMapView()
{
    if (!m_mapCanvas)
    {
        return;
    }

    const QgsCoordinateReferenceSystem webMercatorCrs(QStringLiteral("EPSG:3857"));
    auto projectCrs = QgsProject::instance()->crs();
    if (!projectCrs.isValid())
    {
        projectCrs = webMercatorCrs;
        QgsProject::instance()->setCrs(projectCrs);
    }
    m_mapCanvas->setDestinationCrs(projectCrs);

    QgsRectangle gansuExtent = gansuWebMercatorExtent();
    QgsPointXY gansuCenter = gansuWebMercatorCenter();
    try
    {
        const QgsCoordinateTransform transform(webMercatorCrs, projectCrs, QgsProject::instance());
        gansuExtent = transform.transformBoundingBox(gansuExtent);
        gansuCenter = transform.transform(gansuCenter);
    }
    catch (const QgsCsException&)
    {
        return;
    }
    for (QgsMapLayer* layer : QgsProject::instance()->mapLayers())
    {
        if (!layer || !isSystemBasemapLayer(layer) || !layer->isValid())
        {
            continue;
        }

        const QgsRectangle basemapExtent =
            m_mapCanvas->mapSettings().layerExtentToOutputExtent(layer, layer->extent());
        if (basemapExtent.isEmpty() || !basemapExtent.isFinite())
        {
            continue;
        }

        gansuCenter = basemapExtent.center();
        break;
    }
    m_constrainingMapView = true;
    m_mapCanvas->setExtent(gansuExtent);
    m_mapCanvas->setCenter(gansuCenter);
    m_constrainingMapView = false;
    m_mapCanvas->refresh();

    m_initialMapExtent = m_mapCanvas->extent();
    m_initialMapScale = m_mapCanvas->scale();
    m_hasInitialMapView = true;
}

void LTZK::setDefaultPanMapTool()
{
    if (m_mapCanvas && m_panTool)
    {
        m_mapCanvas->setMapTool(m_panTool);
    }
}

void LTZK::rememberInitialMapView()
{
    if (!m_mapCanvas || m_hasInitialMapView)
    {
        return;
    }

    const QgsRectangle extent = m_mapCanvas->extent();
    const double scale = m_mapCanvas->scale();
    if (scale <= 0.0 || extent.isEmpty() || !extent.isFinite())
    {
        return;
    }

    m_initialMapExtent = extent;
    m_initialMapScale = scale;
    m_hasInitialMapView = true;
}

void LTZK::resetInitialMapView()
{
    m_hasInitialMapView = false;
    m_initialMapScale = 0.0;
    m_initialMapExtent = QgsRectangle();

    if (!m_mapCanvas)
    {
        return;
    }

    QgsRectangle businessExtent;
    bool hasBusinessExtent = false;
    for (QgsMapLayer* layer : QgsProject::instance()->mapLayers())
    {
        if (!layer || isSystemBasemapLayer(layer) || !layer->isValid())
        {
            continue;
        }

        QgsRectangle layerExtent = m_mapCanvas->mapSettings().layerExtentToOutputExtent(layer, layer->extent());
        if (layerExtent.isEmpty() || !layerExtent.isFinite())
        {
            continue;
        }

        if (hasBusinessExtent)
        {
            businessExtent.combineExtentWith(layerExtent);
        }
        else
        {
            businessExtent = layerExtent;
            hasBusinessExtent = true;
        }
    }

    if (!hasBusinessExtent)
    {
        setGansuInitialMapView();
        return;
    }

    m_mapCanvas->setExtent(businessExtent);
    m_mapCanvas->refresh();
    rememberInitialMapView();
}

void LTZK::restoreInitialMapView()
{
    if (!m_mapCanvas)
    {
        return;
    }

    if (!m_hasInitialMapView)
    {
        resetInitialMapView();
    }

    if (!m_hasInitialMapView)
    {
        QMessageBox::information(this, QStringLiteral("Zoom Unavailable"), QStringLiteral("No initial map scale is available."));
        return;
    }

    m_mapCanvas->setExtent(m_initialMapExtent);
    m_mapCanvas->zoomScale(m_initialMapScale);
    m_mapCanvas->refresh();
}

QMenu* LTZK::ensurePluginSubMenu(const QString& pluginName)
{
    if (!m_pluginMenu || pluginName.isEmpty())
    {
        return m_pluginMenu;
    }

    const QString objectName = QStringLiteral("pluginMenu_%1").arg(pluginName);
    QMenu* existingMenu = m_pluginMenu->findChild<QMenu*>(objectName);
    if (existingMenu)
    {
        return existingMenu;
    }

    QMenu* subMenu = m_pluginMenu->addMenu(pluginName);
    subMenu->setObjectName(objectName);
    return subMenu;
}

QgsVectorLayer* createTempLayer(QgsWkbTypes::GeometryType geomType, QString crs, QgsFields fields, QgsFeatureList features)
{
    QgsVectorLayer* newLayerPtr = nullptr;

    if (geomType == QgsWkbTypes::GeometryType::PointGeometry)
    {
        QString layerDef = "Point?crs=" + crs;
        newLayerPtr = new QgsVectorLayer(layerDef, QStringLiteral("PointLayer"), QStringLiteral("memory"));
    }
    else if (geomType == QgsWkbTypes::GeometryType::LineGeometry)
    {
        QString layerDef = "LineString?crs=" + crs;
        newLayerPtr = new QgsVectorLayer(layerDef, QStringLiteral("LineLayer"), QStringLiteral("memory"));
    }
    else if (geomType == QgsWkbTypes::GeometryType::PolygonGeometry)
    {
        QString layerDef = "MultiPolygon?crs=" + crs;
        newLayerPtr = new QgsVectorLayer(layerDef, QStringLiteral("PolyLayer"), QStringLiteral("memory"));
    }

    if (newLayerPtr == nullptr && !newLayerPtr->isValid())
    {
        return newLayerPtr;
    }

    newLayerPtr->commitChanges();
    if (!newLayerPtr->isEditable())
    {
        newLayerPtr->startEditing();
    }
    QgsFields sourceFlds = fields;
    for (int i = 0; i < sourceFlds.size(); i++)
    {
        newLayerPtr->editBuffer()->addAttribute(sourceFlds[i]);
    }
    newLayerPtr->commitChanges();

    if (!newLayerPtr->isEditable())
    {
        newLayerPtr->startEditing();
    }
    newLayerPtr->editBuffer()->addFeatures(features);
    bool flag = newLayerPtr->commitChanges();// 提交编辑。
    newLayerPtr->editingStopped();
    return newLayerPtr;
}
void LTZK::openProjectFile(const QString& filename)
{
    QFileInfo fi(filename);
    if (!fi.exists() || !fi.isFile())
    {
        return;
    }

    if (!QgsProject::instance()->read(filename))
    {
        logLtzKMessage(QStringLiteral("Project open failed: %1").arg(filename), Qgis::Warning);
        QMessageBox::warning(this, QStringLiteral("打开工程失败"), QStringLiteral("无法加载所选 QGIS 工程文件。"));
        return;
    }

    const QList<QgsMapLayer*> layers = QgsProject::instance()->mapLayers().values();
    m_curMapLayer = layers.isEmpty() ? nullptr : layers.first();

    // QgsProject keeps its layer-tree root when reading a project. Keep the
    // existing bridge alive so subsequent source-manager imports still render.
    if (!m_layerTreeCanvasBridge)
    {
        m_layerTreeCanvasBridge = new QgsLayerTreeMapCanvasBridge(
            QgsProject::instance()->layerTreeRoot(), m_mapCanvas, this);
    }
    m_layerTreeCanvasBridge->setAutoSetupOnFirstLayer(false);
    m_layerTreeCanvasBridge->setCanvasLayers();
    logLtzKMessage(QStringLiteral("Project opened: %1").arg(filename));
    resetInitialMapView();
}

bool LTZK::isProjectFile(const QString& filePath) const
{
    const QString suffix = QStringLiteral(".") + QFileInfo(filePath).suffix().toLower();
    return suffix == QStringLiteral(".qgs") || suffix == QStringLiteral(".qgz");
}

bool LTZK::isVectorLayerFile(const QString& filePath) const
{
    const QString suffix = QStringLiteral(".") + QFileInfo(filePath).suffix().toLower();
    const QStringList supportedExtensions = {
        QStringLiteral(".shp"),
        QStringLiteral(".geojson"),
        QStringLiteral(".gpkg"),
        QStringLiteral(".json"),
        QStringLiteral(".kml"),
        QStringLiteral(".gml"),
        QStringLiteral(".dxf"),
        QStringLiteral(".pdf")
    };
    return supportedExtensions.contains(suffix);
}

bool LTZK::isRasterLayerFile(const QString& filePath) const
{
    const QString suffix = QStringLiteral(".") + QFileInfo(filePath).suffix().toLower();
    const QStringList supportedExtensions = {
        QStringLiteral(".tif"),
        QStringLiteral(".tiff"),
        QStringLiteral(".img"),
        QStringLiteral(".vrt"),
        QStringLiteral(".pdf")
    };
    return supportedExtensions.contains(suffix);
}

QStringList LTZK::collectSupportedLayerFiles(const QString& path) const
{
    QStringList files;
    QFileInfo info(path);
    if (!info.exists())
    {
        return files;
    }

    if (info.isDir() && info.suffix().compare(QStringLiteral("gdb"), Qt::CaseInsensitive) != 0)
    {
        QStringList directories{path};
        while (!directories.isEmpty())
        {
            const QDir directory(directories.takeLast());
            for (const QFileInfo& child : directory.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks))
            {
                if (child.isDir() && child.suffix().compare(QStringLiteral("gdb"), Qt::CaseInsensitive) != 0)
                    directories << child.absoluteFilePath();
                else if (isSupportedDataSourceFile(child.absoluteFilePath()))
                    files << child.absoluteFilePath();
            }
        }
        return files;
    }

    if (isSupportedDataSourceFile(path))
    {
        files << path;
    }
    return files;
}

void LTZK::addLayerFiles(const QStringList& filePaths)
{
    loadDataSources(filePaths);
}

void LTZK::dragEnterEvent(QDragEnterEvent* event)
{
    if (QgsMimeDataUtils::isUriList(event->mimeData()))
    {
        event->acceptProposedAction();
        return;
    }
    if (!event->mimeData()->hasUrls())
    {
        return;
    }

    for (const QUrl& url : event->mimeData()->urls())
    {
        const QString localPath = url.toLocalFile();
        if (localPath.isEmpty())
        {
            continue;
        }
        QFileInfo info(localPath);
        if (info.isDir() || isProjectFile(localPath) || isSupportedDataSourceFile(localPath))
        {
            event->acceptProposedAction();
            return;
        }
    }
}

void LTZK::dropEvent(QDropEvent* event)
{
    if (QgsMimeDataUtils::isUriList(event->mimeData()))
    {
        for (const auto& uri : QgsMimeDataUtils::decodeUriList(event->mimeData())) loadCatalogDataSource(uri);
        event->acceptProposedAction();
        return;
    }
    if (!event->mimeData()->hasUrls())
    {
        return;
    }

    QStringList projectFiles;
    QStringList layerFiles;
    for (const QUrl& url : event->mimeData()->urls())
    {
        const QString localPath = url.toLocalFile();
        if (localPath.isEmpty())
        {
            continue;
        }
        if (isProjectFile(localPath))
        {
            projectFiles << localPath;
            continue;
        }
        layerFiles << collectSupportedLayerFiles(localPath);
    }

    if (!projectFiles.isEmpty())
    {
        openProjectFile(projectFiles.first());
    }
    addLayerFiles(layerFiles);
    event->acceptProposedAction();
}
void LTZK::on_actionOpenProject_triggered()
{
    QString filename = QFileDialog::getOpenFileName(this, QStringLiteral("打开工程"), "", "QGIS project (*.qgs *.qgz)");
    openProjectFile(filename);
}

void LTZK::on_actionSaveProject_triggered()
{
    QString filename = QFileDialog::getSaveFileName(this, QStringLiteral("\u5DE5\u7A0B\u4FDD\u5B58\u4E3A"), "", "QGIS project (*.qgs)");
    if (filename.isEmpty())
    {
        return;
    }
    QFileInfo file(filename);
    if (QgsProject::instance()->write(filename))
    {
        logLtzKMessage(QStringLiteral("Project saved: %1").arg(filename));
    }
    else
    {
        logLtzKMessage(QStringLiteral("Project save failed: %1").arg(filename), Qgis::Warning);
    }
}

void LTZK::on_actionSaveAsProject_triggered()
{

}

void LTZK::on_actionExit_triggered()
{
    this->close();
}

void LTZK::on_actionLayerTreeControl_triggered()
{
    if (m_workspaceStack && m_mapCanvasPage)
    {
        m_workspaceStack->setCurrentWidget(m_mapCanvasPage);
    }

    if (m_layerTreeView)
    {
        m_layerTreeView->setFocus();
    }
}

void LTZK::on_actionAddVectorData_triggered()
{
    openDataSourceManager(QStringLiteral("ogr"));
}

void LTZK::on_actionAddRasterData_triggered()
{
    openDataSourceManager(QStringLiteral("raster"));
}

void LTZK::renameLayerTreeGroup(QgsLayerTreeGroup* groupNode)
{
    if (!groupNode)
    {
        return;
    }

    bool accepted = false;
    const QString newName = QInputDialog::getText(
        this,
        QStringLiteral("重命名组"),
        QStringLiteral("新的组名称："),
        QLineEdit::Normal,
        groupNode->name(),
        &accepted).trimmed();
    if (!accepted)
    {
        return;
    }
    if (newName.isEmpty() ||
        newName.contains(QChar('\n')) ||
        newName.contains(QChar('\r')))
    {
        QMessageBox::warning(
            this,
            QStringLiteral("重命名组"),
            QStringLiteral("组名称不能为空，也不能包含换行。"));
        return;
    }
    if (newName == groupNode->name())
    {
        return;
    }

    groupNode->setName(newName);
    if (m_layerTreeView)
    {
        m_layerTreeView->viewport()->update();
    }
}

void LTZK::removeLayerTreeNodes(
    const QList<QgsLayerTreeNode*>& nodes)
{
    if (nodes.isEmpty() || !confirmLayerTreeRemoval(this, nodes))
    {
        return;
    }

    QgsProject* project = QgsProject::instance();
    if (!project)
    {
        return;
    }

    m_curMapLayer = nullptr;

    QStringList removedNames;
    QList<QgsLayerTreeGroup*> groupsToRemove;
    QList<QgsLayerTreeLayer*> treeOnlyLayersToRemove;
    QStringList layerIdsToRemove;
    QSet<QString> layerIdsSeen;

    for (QgsLayerTreeNode* node : nodes)
    {
        if (!node)
        {
            continue;
        }

        if (QgsLayerTree::isGroup(node))
        {
            QgsLayerTreeGroup* groupNode = asLayerTreeGroupNode(node);
            if (!groupNode || !QgsLayerTree::isGroup(groupNode->parent()))
            {
                continue;
            }

            rememberQualityResultGroupPaths(groupNode);
            removedNames.append(groupNode->name());
            groupsToRemove.append(groupNode);
            continue;
        }

        if (!QgsLayerTree::isLayer(node))
        {
            continue;
        }

        QgsLayerTreeLayer* layerNode = asLayerTreeLayerNode(node);
        if (!layerNode || !QgsLayerTree::isGroup(layerNode->parent()))
        {
            continue;
        }

        QgsMapLayer* layer = layerNode->layer();
        if (layer && !layer->id().isEmpty())
        {
            if (!layerIdsSeen.contains(layer->id()))
            {
                layerIdsSeen.insert(layer->id());
                layerIdsToRemove.append(layer->id());
                removedNames.append(layer->name());
            }
        }
        else
        {
            removedNames.append(layerNode->name());
            treeOnlyLayersToRemove.append(layerNode);
        }
    }

    for (QgsLayerTreeGroup* groupNode : groupsToRemove)
    {
        if (!groupNode || !QgsLayerTree::isGroup(groupNode->parent()))
        {
            continue;
        }

        QgsLayerTreeGroup* parentGroup = asLayerTreeGroupNode(
            groupNode->parent());
        if (parentGroup)
        {
            parentGroup->removeChildNode(groupNode);
        }
    }

    if (!layerIdsToRemove.isEmpty())
    {
        project->removeMapLayers(layerIdsToRemove);
    }

    for (QgsLayerTreeLayer* layerNode : treeOnlyLayersToRemove)
    {
        if (!layerNode || !QgsLayerTree::isGroup(layerNode->parent()))
        {
            continue;
        }

        QgsLayerTreeGroup* parentGroup = asLayerTreeGroupNode(
            layerNode->parent());
        if (parentGroup)
        {
            parentGroup->removeChildNode(layerNode);
        }
    }

    if (m_mapCanvas)
    {
        m_mapCanvas->refresh();
    }
    if (!removedNames.isEmpty())
    {
        logLtzKMessage(
            QStringLiteral("Layer/group removed: %1")
                .arg(removedNames.join(QStringLiteral(", "))));
    }
}

void LTZK::removeLayer()
{
    removeSelectedLayerTreeNodes();
}

void LTZK::removeSelectedLayerTreeNodes()
{
    if (!m_layerTreeView)
    {
        return;
    }
    QList<QgsLayerTreeNode*> targets = selectedLayerTreeRemovalTargets(m_layerTreeView);
    if (targets.isEmpty())
    {
        targets = selectedLayerTreeRemovalTargets(m_layerTreeView, m_layerTreeView->currentNode());
    }
    removeLayerTreeNodes(targets);
}

void LTZK::removeCheckedLayerTreeNodes()
{
    if (!m_layerTreeView)
    {
        return;
    }
    QgsLayerTree* root = QgsProject::instance()->layerTreeRoot();
    QgsLayerTreeModel* model = m_layerTreeView->layerTreeModel();
    if (!root || !model)
    {
        return;
    }
    removeLayerTreeNodes(collectCheckedLayerTreeNodes(root, model));
}

void LTZK::onCurrentLayerChanged(QgsMapLayer* layer)
{
    m_curMapLayer = layer;

    if (m_mapCanvas)
    {
        const QgsMapTool* activeTool = m_mapCanvas->mapTool();
        const bool editingToolActive = activeTool == m_capturePointTool
            || activeTool == m_captureLineTool
            || activeTool == m_capturePolygonTool
            || activeTool == m_vertexEditTool;
        if (editingToolActive && m_panTool)
        {
            m_mapCanvas->setMapTool(m_panTool);
        }
    }
    updateWorkspaceEditingActions();
}

void LTZK::initLayerTreeView()
{
    startupTrace(
        QStringLiteral(
            "initLayerTreeView begin"));

    QgsLayerTree* root =
        QgsProject::instance()->layerTreeRoot();

    startupTrace(
        QStringLiteral(
            "before QgsLayerTreeModel"));

    QgsLayerTreeModel* model =
        new LTZKLayerTreeModel(
            root,
            this);

    startupTrace(
        QStringLiteral(
            "after QgsLayerTreeModel"));

    qDebug()
        << "layer tree root address:"
        << static_cast<const void*>(root);

    startupTrace(
        QStringLiteral(
            "before model flags"));

    model->setFlag(
        QgsLayerTreeModel::AllowNodeReorder);

    startupTrace(
        QStringLiteral(
            "after AllowNodeReorder"));

    model->setFlag(
        QgsLayerTreeModel::AllowNodeRename);

    startupTrace(
        QStringLiteral(
            "after AllowNodeRename"));

    model->setFlag(
        QgsLayerTreeModel::
        AllowNodeChangeVisibility);

    model->setFlag(
        QgsLayerTreeModel::ActionHierarchical);

    startupTrace(
        QStringLiteral(
            "after AllowNodeChangeVisibility"));

    model->setFlag(
        QgsLayerTreeModel::ShowLegendAsTree);

    startupTrace(
        QStringLiteral(
            "after ShowLegendAsTree"));

    /*
     * 不启用嵌入式控件。
     * 该模式依赖 QgsGui 的默认委托即可满足当前图层树。
     */
     // model->setFlag(
     //     QgsLayerTreeModel::UseEmbeddedWidgets);

    model->setAutoCollapseLegendNodes(10);

    startupTrace(
        QStringLiteral(
            "after model flags"));

    startupTrace(
        QStringLiteral(
            "before panel QWidget"));

    m_layerTreePanelWidget =
        new QWidget(this);

    startupTrace(
        QStringLiteral(
            "after panel QWidget"));

    m_layerTreePanelWidget->setObjectName(
        QStringLiteral(
            "workspaceLayerTreePanelWidget"));
    m_layerTreePanelWidget->setAttribute(Qt::WA_StyledBackground, true);
    m_layerTreePanelWidget->setAutoFillBackground(true);

    startupTrace(
        QStringLiteral(
            "before QgsLayerTreeView"));

    m_layerTreeView =
        new QgsLayerTreeView(
            m_layerTreePanelWidget);

    startupTrace(
        QStringLiteral(
            "after QgsLayerTreeView"));

    startupTrace(
        QStringLiteral(
            "before layer tree setModel"));

    m_layerTreeView->setModel(model);
    m_layerTreeDefaultActions = m_layerTreeView->defaultActions();

    m_layerTreeView->setSelectionMode(
        QAbstractItemView::ExtendedSelection);

    startupTrace(
        QStringLiteral(
            "after layer tree setModel"));

    // Legend and style edits also change the view model. Rebuilding the canvas
    // layer stack for those edits can remove visible quality-result layers.
    // Only actual tree visibility, membership or order changes affect the stack.
    connect(root, &QgsLayerTreeNode::visibilityChanged, this, [this]() {
        refreshMapCanvasLayerStack();
        });
    connect(root, &QgsLayerTreeNode::addedChildren, this, [this]() {
        refreshMapCanvasLayerStack();
        });
    connect(root, &QgsLayerTreeNode::removedChildren, this, [this]() {
        refreshMapCanvasLayerStack();
        });
    connect(root, &QgsLayerTree::layerOrderChanged, this, [this]() {
        refreshMapCanvasLayerStack();
        });

    m_layerTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_layerTreeView, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex index = m_layerTreeView->indexAt(pos);
        if (!index.isValid() || !m_layerTreeView->selectionModel())
        {
            return;
        }

        QItemSelectionModel* selectionModel = m_layerTreeView->selectionModel();
        if (selectionModel->isSelected(index))
        {
            selectionModel->setCurrentIndex(
                index,
                QItemSelectionModel::NoUpdate);
        }
        else
        {
            selectionModel->setCurrentIndex(
                index,
                QItemSelectionModel::ClearAndSelect |
                QItemSelectionModel::Rows);
        }

        QgsLayerTreeNode* contextNode = m_layerTreeView->index2node(index);
        if (!contextNode)
        {
            return;
        }

        QgsLayerTreeGroup* contextGroup =
            asLayerTreeGroupNode(contextNode);
        QgsLayerTreeLayer* contextLayerNode =
            asLayerTreeLayerNode(contextNode);
        QgsMapLayer* layer = contextLayerNode
            ? contextLayerNode->layer()
            : nullptr;
        const QList<QgsLayerTreeNode*> selectedNodes =
            m_layerTreeView->selectedNodes(true);
        const QList<QgsLayerTreeNode*> removalTargets =
            mergeLayerTreeRemovalTargets(m_layerTreeView, contextNode);
        const QString removalActionText =
            QStringLiteral("删除已勾选/选中项（%1）")
                .arg(removalTargets.size());

        m_curMapLayer = layer;
        QMenu menu(m_layerTreeView);
        if (contextGroup)
        {
            QAction* actionRenameGroup = menu.addAction(QStringLiteral("重命名"));
            actionRenameGroup->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/mActionRenameLayer.svg")));
            const bool singleGroupSelection = selectedNodes.size() == 1 &&
                selectedNodes.first() == contextGroup;
            actionRenameGroup->setEnabled(singleGroupSelection);
            connect(actionRenameGroup, &QAction::triggered, this,
                [this, contextGroup]()
                {
                    renameLayerTreeGroup(contextGroup);
                });

            QAction* actionRemoveGroup = menu.addAction(
                QStringLiteral("删除组及已勾选项（%1）")
                    .arg(removalTargets.size()));
            actionRemoveGroup->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/mActionRemoveLayer.svg")));
            actionRemoveGroup->setEnabled(!removalTargets.isEmpty());
            connect(actionRemoveGroup, &QAction::triggered, this,
                [this, removalTargets]()
                {
                    removeLayerTreeNodes(removalTargets);
                });
        }
        else if (layer)
        {
            QAction* actionZoomToLayer = menu.addAction(
                QIcon(QStringLiteral(":/LTZK/Resources/mActionZoomToLayer.svg")),
                QStringLiteral("缩放至图层"));
            connect(actionZoomToLayer, &QAction::triggered, this, [this, layer]() {
                if (!layer || !layer->isValid() || !m_mapCanvas)
                {
                    return;
                }

                QgsRectangle extent = m_mapCanvas->mapSettings().layerExtentToOutputExtent(layer, layer->extent());
                m_mapCanvas->zoomToFeatureExtent(extent);
                m_mapCanvas->refresh();
                });

            if (layer->type() == QgsMapLayerType::VectorLayer)
            {
                QgsVectorLayer* vectorLayer = qobject_cast<QgsVectorLayer*>(layer);
                const QString sourcePath = vectorLayer ? localPathFromLayerSource(vectorLayer->source()) : QString();
                if (vectorLayer && QFileInfo(sourcePath).suffix().compare(QStringLiteral("shp"), Qt::CaseInsensitive) == 0)
                {
                    QAction* actionSetLayerColor = menu.addAction(
                        QIcon(QStringLiteral(":/LTZK/Resources/mIconColorWheel.svg")),
                        QStringLiteral("快速设置图层颜色"));
                    connect(actionSetLayerColor, &QAction::triggered, this, [this, vectorLayer]() {
                        setShpLayerDisplayColor(vectorLayer);
                        });
                }

                if (vectorLayer)
                {
                    menu.addSeparator();
                    QAction* toggleEditingAction = menu.addAction(
                        QIcon(QStringLiteral(":/LTZK/Resources/mActionToggleEditing.svg")),
                        vectorLayer->isEditable()
                            ? QStringLiteral("停止编辑") : QStringLiteral("开始编辑"));
                    toggleEditingAction->setCheckable(true);
                    toggleEditingAction->setChecked(vectorLayer->isEditable());
                    toggleEditingAction->setEnabled(
                        vectorLayer->isEditable() || layerCanStartEditing(vectorLayer));
                    connect(toggleEditingAction, &QAction::toggled, this,
                        [this, vectorLayer](bool enabled)
                        {
                            setLayerEditing(vectorLayer, enabled);
                        });

                    QAction* saveEditsAction = menu.addAction(
                        QIcon(QStringLiteral(":/LTZK/Resources/mActionSaveEdits.svg")),
                        QStringLiteral("保存编辑"));
                    saveEditsAction->setEnabled(
                        vectorLayer->isEditable() && vectorLayer->isModified());
                    connect(saveEditsAction, &QAction::triggered, this,
                        [this, vectorLayer]() { saveLayerEdits(vectorLayer, true); });

                    QAction* rollbackEditsAction = menu.addAction(
                        QIcon(QStringLiteral(":/LTZK/Resources/mActionRollbackEdits.svg")),
                        QStringLiteral("回滚编辑"));
                    rollbackEditsAction->setEnabled(
                        vectorLayer->isEditable() && vectorLayer->isModified());
                    connect(rollbackEditsAction, &QAction::triggered, this,
                        [this, vectorLayer]() { rollbackLayerEdits(vectorLayer, true); });
                }

                QAction* actionOpenAttributeTable = menu.addAction(
                    QIcon(QStringLiteral(":/LTZK/Resources/mActionAttributes.svg")),
                    QStringLiteral("打开属性表"));
                connect(actionOpenAttributeTable, &QAction::triggered, this, &LTZK::on_actionOpenAttrTable_triggered);
            }

            QAction* actionRemoveContextLayer = menu.addAction(
                QIcon(QStringLiteral(":/LTZK/Resources/mActionRemoveLayer.svg")),
                removalActionText);
            actionRemoveContextLayer->setEnabled(!removalTargets.isEmpty());
            connect(actionRemoveContextLayer, &QAction::triggered, this,
                [this, removalTargets]()
                {
                    removeLayerTreeNodes(removalTargets);
                });
        }

        if (m_layerTreeDefaultActions)
        {
            menu.addSeparator();
            QSet<QString> layerTreeFallbackIcons;
            const auto addLayerTreeAction = [&menu, &layerTreeFallbackIcons, contextGroup](QAction* action, const QString& text,
                const QString& fallbackIconPath)
            {
                if (action)
                {
                    action->setText(text);
                    action->setToolTip(text);
                    action->setStatusTip(text);
                    QString iconPath = fallbackIconPath;
                    if (iconPath.endsWith(QStringLiteral("mActionAttributes.svg")))
                        iconPath = QStringLiteral(":/LTZK/Resources/mActionFeatureCount.svg");
                    else if (iconPath.endsWith(QStringLiteral("mIconProperties.svg")))
                        iconPath = QStringLiteral(":/LTZK/Resources/mActionRenameLayer.svg");
                    else if (!contextGroup && iconPath.endsWith(QStringLiteral("mActionZoomToLayer.svg")))
                        iconPath = QStringLiteral(":/LTZK/Resources/mActionZoomToArea.svg");
                    if (layerTreeFallbackIcons.contains(iconPath)
                        && iconPath.endsWith(QStringLiteral("mActionShowAllLayers.svg")))
                        iconPath = QStringLiteral(":/LTZK/Resources/mActionShowSelectedLayers.svg");
                    layerTreeFallbackIcons.insert(iconPath);
                    action->setIcon(QIcon(iconPath));
                    menu.addAction(action);
                }
            };
            if (contextGroup)
            {
                addLayerTreeAction(m_layerTreeDefaultActions->actionAddGroup(&menu),
                    QStringLiteral("新建组"), QStringLiteral(":/LTZK/Resources/mActionAddGroup.svg"));
                addLayerTreeAction(m_layerTreeDefaultActions->actionZoomToGroup(m_mapCanvas, &menu),
                    QStringLiteral("缩放至组"), QStringLiteral(":/LTZK/Resources/mActionZoomToLayer.svg"));
                addLayerTreeAction(m_layerTreeDefaultActions->actionShowFeatureCount(&menu),
                    QStringLiteral("显示要素数量"), QStringLiteral(":/LTZK/Resources/mActionAttributes.svg"));
                addLayerTreeAction(m_layerTreeDefaultActions->actionShowInOverview(&menu),
                    QStringLiteral("在概览图中显示"), QStringLiteral(":/LTZK/Resources/mActionInOverview.svg"));
                menu.addSeparator();
                addLayerTreeAction(m_layerTreeDefaultActions->actionMutuallyExclusiveGroup(&menu),
                    QStringLiteral("组内图层互斥显示"), QStringLiteral(":/LTZK/Resources/mActionToggleAllLayers.svg"));
                addLayerTreeAction(m_layerTreeDefaultActions->actionCheckAndAllChildren(&menu),
                    QStringLiteral("显示组及所有子项"), QStringLiteral(":/LTZK/Resources/mActionShowAllLayers.svg"));
                addLayerTreeAction(m_layerTreeDefaultActions->actionUncheckAndAllChildren(&menu),
                    QStringLiteral("隐藏组及所有子项"), QStringLiteral(":/LTZK/Resources/mActionHideAllLayers.svg"));
                addLayerTreeAction(m_layerTreeDefaultActions->actionCheckAndAllParents(&menu),
                    QStringLiteral("显示组及所有父组"), QStringLiteral(":/LTZK/Resources/mActionShowAllLayers.svg"));
                addLayerTreeAction(m_layerTreeDefaultActions->actionGroupSelected(&menu),
                    QStringLiteral("将选中项编组"), QStringLiteral(":/LTZK/Resources/mActionGroupItems.svg"));
                menu.addSeparator();
                addLayerTreeAction(m_layerTreeDefaultActions->actionMoveOutOfGroup(&menu),
                    QStringLiteral("移出当前组"), QStringLiteral(":/LTZK/Resources/mActionUngroupItems.svg"));
                addLayerTreeAction(m_layerTreeDefaultActions->actionMoveToTop(&menu),
                    QStringLiteral("移至顶部"), QStringLiteral(":/LTZK/Resources/mActionMoveItemsToTop.svg"));
                addLayerTreeAction(m_layerTreeDefaultActions->actionMoveToBottom(&menu),
                    QStringLiteral("移至底部"), QStringLiteral(":/LTZK/Resources/mActionMoveItemsToBottom.svg"));
            }
            else if (layer)
            {
                if (m_layerTreeView->selectedLayers().size() > 1)
                {
                    addLayerTreeAction(m_layerTreeDefaultActions->actionZoomToLayers(m_mapCanvas, &menu),
                        QStringLiteral("缩放至选中图层"), QStringLiteral(":/LTZK/Resources/mActionZoomToLayer.svg"));
                }
                addLayerTreeAction(m_layerTreeDefaultActions->actionShowInOverview(&menu),
                    QStringLiteral("在概览图中显示"), QStringLiteral(":/LTZK/Resources/mActionInOverview.svg"));
                addLayerTreeAction(m_layerTreeDefaultActions->actionShowFeatureCount(&menu),
                    QStringLiteral("显示要素数量"), QStringLiteral(":/LTZK/Resources/mActionAttributes.svg"));
                addLayerTreeAction(m_layerTreeDefaultActions->actionRenameGroupOrLayer(&menu),
                    QStringLiteral("重命名图层"), QStringLiteral(":/LTZK/Resources/mIconProperties.svg"));
                menu.addSeparator();
                addLayerTreeAction(m_layerTreeDefaultActions->actionMoveOutOfGroup(&menu),
                    QStringLiteral("移出当前组"), QStringLiteral(":/LTZK/Resources/mActionUngroupItems.svg"));
                addLayerTreeAction(m_layerTreeDefaultActions->actionMoveToTop(&menu),
                    QStringLiteral("移至顶部"), QStringLiteral(":/LTZK/Resources/mActionMoveItemsToTop.svg"));
                addLayerTreeAction(m_layerTreeDefaultActions->actionMoveToBottom(&menu),
                    QStringLiteral("移至底部"), QStringLiteral(":/LTZK/Resources/mActionMoveItemsToBottom.svg"));
                if (qobject_cast<QgsVectorLayer*>(layer))
                {
                    addLayerTreeAction(m_layerTreeDefaultActions->actionZoomToSelection(m_mapCanvas, &menu),
                        QStringLiteral("缩放至选中要素"), QStringLiteral(":/LTZK/Resources/mActionZoomToSelected.svg"));
                }
            }
        }

        if (layer)
        {
            QMenu* layerOperations = menu.addMenu(
                QIcon(QStringLiteral(":/LTZK/Resources/mIconProperties.svg")),
                QStringLiteral("图层操作"));
            layerOperations->menuAction()->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/mIconLayerTree.svg")));
            QAction* propertiesAction = layerOperations->addAction(
                QIcon(QStringLiteral(":/LTZK/Resources/mIconProperties.svg")),
                QStringLiteral("图层属性与样式设置…"));
            const QPointer<QgsMapLayer> propertiesLayer(layer);
            connect(propertiesAction, &QAction::triggered, this, [this, propertiesLayer]() {
                showLayerProperties(propertiesLayer.data());
            });

            QAction* duplicateAction = layerOperations->addAction(
                QIcon(QStringLiteral(":/LTZK/Resources/mActionDuplicateLayer.svg")),
                QStringLiteral("复制图层"));
            connect(duplicateAction, &QAction::triggered, this, [layer]()
                {
                    if (!layer || !layer->isValid())
                    {
                        return;
                    }
                    QgsMapLayer* duplicate = layer->clone();
                    if (duplicate)
                    {
                        duplicate->setName(layer->name() + QStringLiteral(" (副本)"));
                        QgsProject::instance()->addMapLayer(duplicate);
                    }
                });

            if (QgsVectorLayer* vectorLayer = qobject_cast<QgsVectorLayer*>(layer))
            {
                layerOperations->addSeparator();
                QAction* filterAction = layerOperations->addAction(
                    QIcon(QStringLiteral(":/LTZK/Resources/mActionFilter.svg")),
                    QStringLiteral("筛选…"));
                connect(filterAction, &QAction::triggered, this, [this, vectorLayer]()
                    {
                        QgsQueryBuilder builder(vectorLayer, this);
                        builder.setWindowTitle(QStringLiteral("设置矢量图层筛选条件"));
                        builder.setSql(vectorLayer->subsetString());
                        if (builder.exec())
                        {
                            vectorLayer->setSubsetString(builder.sql());
                        }
                    });
                QAction* clearFilterAction = layerOperations->addAction(
                    QIcon(QStringLiteral(":/LTZK/Resources/mActionFilter2.svg")),
                    QStringLiteral("清除筛选"));
                clearFilterAction->setEnabled(!vectorLayer->subsetString().isEmpty());
                connect(clearFilterAction, &QAction::triggered, this, [vectorLayer]()
                    {
                        vectorLayer->setSubsetString(QString());
                    });

                QAction* exportAction = layerOperations->addAction(
                    QIcon(QStringLiteral(":/LTZK/Resources/mActionExport.svg")),
                    QStringLiteral("导出为 GeoPackage…"));
                connect(exportAction, &QAction::triggered, this, [this, vectorLayer]()
                    {
                        const QString outputFile = QFileDialog::getSaveFileName(
                            this, QStringLiteral("导出矢量图层"),
                            vectorLayer->name() + QStringLiteral(".gpkg"),
                            QStringLiteral("GeoPackage (*.gpkg)"));
                        if (outputFile.isEmpty())
                        {
                            return;
                        }

                        QgsVectorFileWriter::SaveVectorOptions options;
                        options.driverName = QStringLiteral("GPKG");
                        options.fileEncoding = QStringLiteral("UTF-8");
                        options.onlySelectedFeatures = false;
                        QString errorMessage;
                        const QgsVectorFileWriter::WriterError result =
                            QgsVectorFileWriter::writeAsVectorFormatV3(
                                vectorLayer, outputFile,
                                QgsProject::instance()->transformContext(),
                                options, &errorMessage);
                        if (result == QgsVectorFileWriter::NoError)
                        {
                            QMessageBox::information(this, QStringLiteral("导出完成"),
                                QStringLiteral("图层已导出到：\n%1").arg(outputFile));
                        }
                        else
                        {
                            QMessageBox::critical(this, QStringLiteral("导出失败"), errorMessage);
                        }
                    });
            }
            else if (QgsRasterLayer* rasterLayer = qobject_cast<QgsRasterLayer*>(layer))
            {
                QAction* exportAction = layerOperations->addAction(
                    QIcon(QStringLiteral(":/LTZK/Resources/mActionExport.svg")),
                    QStringLiteral("导出为 GeoTIFF…"));
                connect(exportAction, &QAction::triggered, this, [this, rasterLayer]()
                    {
                        const QString outputFile = QFileDialog::getSaveFileName(
                            this, QStringLiteral("导出栅格图层"),
                            rasterLayer->name() + QStringLiteral(".tif"),
                            QStringLiteral("GeoTIFF (*.tif *.tiff)"));
                        if (outputFile.isEmpty())
                        {
                            return;
                        }

                        QgsRasterFileWriter writer(outputFile);
                        writer.setOutputFormat(QStringLiteral("GTiff"));
                        const QgsRasterFileWriter::WriterError result = writer.writeRaster(
                            rasterLayer->pipe(), rasterLayer->width(), rasterLayer->height(),
                            rasterLayer->extent(), rasterLayer->crs(),
                            QgsProject::instance()->transformContext());
                        if (result == QgsRasterFileWriter::NoError)
                        {
                            QMessageBox::information(this, QStringLiteral("导出完成"),
                                QStringLiteral("图层已导出到：\n%1").arg(outputFile));
                        }
                        else
                        {
                            QMessageBox::critical(this, QStringLiteral("导出失败"),
                                QStringLiteral("栅格写入失败，错误代码：%1").arg(static_cast<int>(result)));
                        }
                    });
            }
        }

        menu.addSeparator();
        QAction* removeCheckedAction = menu.addAction(
            QIcon(QStringLiteral(":/LTZK/Resources/mActionRemoveLayer.svg")),
            QStringLiteral("批量移除已勾选图层…"));
        removeCheckedAction->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/mActionRemove.svg")));
        connect(removeCheckedAction, &QAction::triggered, this, [this]()
            {
                QgsLayerTreeGroup* root = QgsProject::instance()->layerTreeRoot();
                if (root && m_layerTreeView && m_layerTreeView->layerTreeModel())
                {
                    removeLayerTreeNodes(collectCheckedLayerTreeNodes(
                        root, m_layerTreeView->layerTreeModel()));
                }
            });

        menu.exec(m_layerTreeView->viewport()->mapToGlobal(pos));
        });

    m_layerTreeCanvasBridge = new QgsLayerTreeMapCanvasBridge(QgsProject::instance()->layerTreeRoot(), m_mapCanvas, this);
    startupTrace(QStringLiteral("layer tree canvas bridge created"));

    QAction* actionAddGroup = m_layerTreeDefaultActions
        ? m_layerTreeDefaultActions->actionAddGroup(this)
        : new QAction(QStringLiteral("新建组"), this);
    actionAddGroup->setText(QStringLiteral("新建组"));
    actionAddGroup->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/mActionAddGroup.svg")));
    actionAddGroup->setToolTip(QStringLiteral("新建组\n有选中项时，将选中项放入新组"));
    if (!m_layerTreeDefaultActions)
    {
        connect(actionAddGroup, &QAction::triggered, this, []() {
            QgsProject::instance()->layerTreeRoot()->addGroup(QStringLiteral("新建组"));
            });
    }

    QAction* actionExpandAll = new QAction(QStringLiteral("展开所有组"), this);
    actionExpandAll->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/mActionExpandTree.svg")));
    actionExpandAll->setToolTip(QStringLiteral("展开所有图层和组"));
    connect(actionExpandAll, &QAction::triggered, m_layerTreeView, &QgsLayerTreeView::expandAllNodes);

    QAction* actionCollapseAll = new QAction(QStringLiteral("折叠所有组"), this);
    actionCollapseAll->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/mActionCollapseTree.svg")));
    actionCollapseAll->setToolTip(QStringLiteral("折叠所有图层和组"));
    connect(actionCollapseAll, &QAction::triggered, m_layerTreeView, &QgsLayerTreeView::collapseAllNodes);

    QAction* actionRemoveLayer = new QAction(QStringLiteral("移除选中项"), this);
    actionRemoveLayer->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/mActionRemoveLayer.svg")));
    actionRemoveLayer->setToolTip(QStringLiteral("从当前工程中移除选中的图层或组"));
    connect(actionRemoveLayer, &QAction::triggered, this, &LTZK::removeLayer);

    QToolBar* toolbar = new QToolBar(m_layerTreePanelWidget);
    toolbar->setObjectName(QStringLiteral("workspaceLayerTreeToolBar"));
    m_layerTreeToolBar = toolbar;
    const int layerToolbarIconSize = scaledInt(toolbar, 20);
    const int layerToolbarButtonSize = scaledInt(toolbar, 30);
    toolbar->setIconSize(QSize(layerToolbarIconSize, layerToolbarIconSize));
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolbar->addAction(actionAddGroup);
    QAction* showLayerProperties = toolbar->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mIconProperties.svg")), QStringLiteral("图层属性/样式"));
    showLayerProperties->setToolTip(QStringLiteral("打开当前图层\n属性和样式设置"));
    connect(showLayerProperties, &QAction::triggered, this, &LTZK::showCurrentLayerProperties);

    QToolButton* visibilityButton = new QToolButton(toolbar);
    visibilityButton->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/mActionShowAllLayers.svg")));
    visibilityButton->setToolTip(QStringLiteral("图层可见性\n与图例过滤"));
    visibilityButton->setPopupMode(QToolButton::InstantPopup);
    QMenu* visibilityMenu = new QMenu(visibilityButton);
    visibilityButton->setMenu(visibilityMenu);
    toolbar->addWidget(visibilityButton);

    QAction* showAllLayers = visibilityMenu->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionShowAllLayers.svg")), QStringLiteral("显示全部图层"));
    connect(showAllLayers, &QAction::triggered, this,
        []() { QgsProject::instance()->layerTreeRoot()->setItemVisibilityChecked(true); });
    QAction* hideAllLayers = visibilityMenu->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionHideAllLayers.svg")), QStringLiteral("隐藏全部图层"));
    connect(hideAllLayers, &QAction::triggered, this,
        []() { QgsProject::instance()->layerTreeRoot()->setItemVisibilityChecked(false); });
    visibilityMenu->addSeparator();
    QAction* showSelectedLayers = visibilityMenu->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionShowSelectedLayers.svg")), QStringLiteral("显示选中图层"));
    connect(showSelectedLayers, &QAction::triggered, this,
        [this]()
        {
            for (QgsLayerTreeNode* node : m_layerTreeView->selectedNodes(true))
            {
                node->setItemVisibilityChecked(true);
            }
        });
    QAction* hideSelectedLayers = visibilityMenu->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionHideSelectedLayers.svg")), QStringLiteral("隐藏选中图层"));
    connect(hideSelectedLayers, &QAction::triggered, this,
        [this]()
        {
            for (QgsLayerTreeNode* node : m_layerTreeView->selectedNodes(true))
            {
                node->setItemVisibilityChecked(false);
            }
        });
    QAction* toggleSelectedLayers = visibilityMenu->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionToggleAllLayers.svg")), QStringLiteral("切换选中图层可见性"));
    connect(toggleSelectedLayers, &QAction::triggered, this,
        [this]()
        {
            const QList<QgsLayerTreeNode*> selected = m_layerTreeView->selectedNodes(true);
            if (selected.isEmpty())
            {
                return;
            }
            const bool visible = !selected.first()->itemVisibilityChecked();
            for (QgsLayerTreeNode* node : selected)
            {
                node->setItemVisibilityChecked(visible);
            }
        });
    QAction* toggleSelectedIndependently = visibilityMenu->addAction(
        QStringLiteral("分别反转选中图层可见性"));
    connect(toggleSelectedIndependently, &QAction::triggered, this,
        [this]()
        {
            for (QgsLayerTreeNode* node : m_layerTreeView->selectedNodes(true))
            {
                node->setItemVisibilityChecked(!node->itemVisibilityChecked());
            }
        });
    QAction* hideDeselectedLayers = visibilityMenu->addAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionHideDeselectedLayers.svg")), QStringLiteral("隐藏未选中图层"));
    connect(hideDeselectedLayers, &QAction::triggered, this,
        [this]()
        {
            QSet<QgsLayerTreeNode*> selected;
            for (QgsLayerTreeNode* node : m_layerTreeView->selectedNodes(true))
            {
                selected.insert(node);
            }
            for (QgsLayerTreeLayer* layerNode : QgsProject::instance()->layerTreeRoot()->findLayers())
            {
                bool included = false;
                for (QgsLayerTreeNode* parent = layerNode; parent; parent = parent->parent())
                {
                    if (selected.contains(parent))
                    {
                        included = true;
                        break;
                    }
                }
                if (!included)
                {
                    layerNode->setItemVisibilityChecked(false);
                }
            }
        });
    visibilityMenu->addSeparator();
    QAction* mapLegendFilter = visibilityMenu->addAction(QStringLiteral("按当前地图范围筛选图例"));
    mapLegendFilter->setCheckable(true);
    connect(mapLegendFilter, &QAction::toggled, this,
        [this](bool enabled)
        {
            m_layerLegendMapFilterEnabled = enabled;
            refreshLayerLegendMapFilter();
        });
    QAction* expressionLegendFilter = visibilityMenu->addAction(
        QStringLiteral("应用图层图例中的表达式过滤"));
    expressionLegendFilter->setCheckable(true);
    connect(expressionLegendFilter, &QAction::toggled, this,
        [this](bool enabled)
        {
            m_layerLegendExpressionFilterEnabled = enabled;
            refreshLayerLegendMapFilter();
        });
    QAction* clearLegendFilters = visibilityMenu->addAction(QStringLiteral("清除图例过滤"));
    connect(clearLegendFilters, &QAction::triggered, this,
        [this, mapLegendFilter, expressionLegendFilter]()
        {
            mapLegendFilter->setChecked(false);
            expressionLegendFilter->setChecked(false);
            m_layerLegendMapFilterEnabled = false;
            m_layerLegendExpressionFilterEnabled = false;
            refreshLayerLegendMapFilter();
        });

    QToolButton* mapThemesButton = new QToolButton(toolbar);
    mapThemesButton->setIcon(QIcon(QStringLiteral(":/LTZK/Resources/icon_theme_manager.svg")));
    mapThemesButton->setToolTip(QStringLiteral("地图主题"));
    mapThemesButton->setPopupMode(QToolButton::InstantPopup);
    QMenu* mapThemesMenu = new QMenu(mapThemesButton);
    connect(mapThemesMenu, &QMenu::aboutToShow, this,
        [this, mapThemesMenu]() { populateMapThemeMenu(mapThemesMenu); });
    mapThemesButton->setMenu(mapThemesMenu);
    toolbar->addWidget(mapThemesButton);

    QFrame* filterRow = new QFrame(m_layerTreePanelWidget);
    filterRow->setObjectName(QStringLiteral("workspaceLayerTreeFilterRow"));
    QHBoxLayout* filterRowLayout = new QHBoxLayout(filterRow);
    filterRowLayout->setContentsMargins(scaledMargins(filterRow, 4, 3, 4, 3));
    filterRowLayout->setSpacing(0);

    m_layerTreeFilterEdit = new QLineEdit(filterRow);
    m_layerTreeFilterEdit->setObjectName(QStringLiteral("workspaceLayerTreeFilterEdit"));
    m_layerTreeFilterEdit->setClearButtonEnabled(true);
    m_layerTreeFilterEdit->setPlaceholderText(QStringLiteral("搜索图层…"));
    m_layerTreeFilterEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_layerTreeFilterEdit, &QLineEdit::textChanged, this,
        [this](const QString& text)
        {
            if (m_layerTreeView && m_layerTreeView->proxyModel())
            {
                m_layerTreeView->proxyModel()->setFilterText(text);
            }
        });
    filterRowLayout->addWidget(m_layerTreeFilterEdit);

    toolbar->addAction(actionExpandAll);
    toolbar->addAction(actionCollapseAll);
    QToolButton* removeButton = new QToolButton(toolbar);
    QAction* removeDefaultAction = new QAction(
        QIcon(QStringLiteral(":/LTZK/Resources/mActionRemoveLayer.svg")),
        QStringLiteral("移除选中项"), removeButton);
    connect(removeDefaultAction, &QAction::triggered, this,
        &LTZK::removeSelectedLayerTreeNodes);
    removeButton->setDefaultAction(removeDefaultAction);
    removeButton->setToolTip(QStringLiteral("移除选中项\n或批量移除已勾选项"));
    removeButton->setPopupMode(QToolButton::MenuButtonPopup);
    QMenu* removeMenu = new QMenu(removeButton);
    QAction* removeSelectedAction = removeMenu->addAction(QStringLiteral("移除选中项"));
    connect(removeSelectedAction, &QAction::triggered, this, &LTZK::removeSelectedLayerTreeNodes);
    QAction* removeCheckedAction = removeMenu->addAction(QStringLiteral("批量移除已勾选项"));
    connect(removeCheckedAction, &QAction::triggered, this, &LTZK::removeCheckedLayerTreeNodes);
    removeButton->setMenu(removeMenu);
    toolbar->addWidget(removeButton);

    auto compactLayerToolbarButton = [layerToolbarIconSize, layerToolbarButtonSize](QToolButton* button)
    {
        if (!button)
        {
            return;
        }
        button->setIconSize(QSize(layerToolbarIconSize, layerToolbarIconSize));
        button->setFixedSize(layerToolbarButtonSize, layerToolbarButtonSize);
        button->setStyleSheet(QStringLiteral("QToolButton { padding: 1px; margin: 0; }"));
    };

    const QList<QAction*> compactActions = {
        actionAddGroup, showLayerProperties, actionExpandAll, actionCollapseAll
    };
    for (QAction* action : compactActions)
    {
        compactLayerToolbarButton(
            qobject_cast<QToolButton*>(toolbar->widgetForAction(action)));
    }
    for (QToolButton* button : { visibilityButton, mapThemesButton, removeButton })
    {
        compactLayerToolbarButton(button);
    }

    connect(m_mapCanvas, &QgsMapCanvas::extentsChanged, this, &LTZK::refreshLayerLegendMapFilter);

    QVBoxLayout* vBoxLayout = new QVBoxLayout(m_layerTreePanelWidget);
    vBoxLayout->setMargin(0);
    vBoxLayout->setContentsMargins(0, 0, 0, 0);
    vBoxLayout->setSpacing(0);
    vBoxLayout->addWidget(toolbar);
    vBoxLayout->addWidget(filterRow);
    vBoxLayout->addWidget(m_layerTreeView);


    startupTrace(QStringLiteral("initLayerTreeView end"));
}
void LTZK::on_actionAddWmsLayer_triggered()
{
    openOwsSourceSelect(QStringLiteral("WMS"));
}

void LTZK::on_actionAddWfsLayer_triggered()
{
    openOwsSourceSelect(QStringLiteral("WFS"));
}

void LTZK::on_actionAddWcsLayer_triggered()
{
    openOwsSourceSelect(QStringLiteral("WCS"));
}


void LTZK::on_actionOpenAttrTable_triggered()
{
    // 优先使用图层树当前选中图层。
    if (m_layerTreeView && m_layerTreeView->currentLayer())
    {
        m_curMapLayer = m_layerTreeView->currentLayer();
    }

    QgsVectorLayer* layer = qobject_cast<QgsVectorLayer*>(m_curMapLayer);
    if (layer == nullptr || !layer->isValid())
    {
        logLtzKMessage(QStringLiteral("Attribute table open failed: no valid vector layer."), Qgis::Warning);
        qDebug() << QStringLiteral("当前图层无效或不是矢量图层。");
        return;
    }


    openAttributeTable(layer);
    return;

#if 0 // Replaced by the shared QGIS DualView attribute-table dialog.
    logLtzKMessage(QStringLiteral("Attribute table opened: %1").arg(layer->name()));


    // 创建属性表视图。
    QgsAttributeTableView* tv = new QgsAttributeTableView();
    tv->setAttribute(Qt::WA_DeleteOnClose);

    QScreen* attributeTableScreen = screen();
    if (!attributeTableScreen)
    {
        attributeTableScreen = QApplication::primaryScreen();
    }

    const QRect availableGeometry = attributeTableScreen
        ? attributeTableScreen->availableGeometry()
        : QRect();
    const int availableWidth = availableGeometry.width() > 0 ? availableGeometry.width() : 1000;
    const int availableHeight = availableGeometry.height() > 0 ? availableGeometry.height() : 650;
    const QSize attributeTableMinimumSize(
        qMin(availableWidth, 800),
        qMin(availableHeight, 500));
    const QSize attributeTableDefaultSize(
        qMin(availableWidth, qMax(800, qMin(1100, qRound(availableWidth * 0.78)))),
        qMin(availableHeight, qMax(500, qMin(700, qRound(availableHeight * 0.68)))));
    tv->setMinimumSize(attributeTableMinimumSize);
    tv->resize(attributeTableDefaultSize);

    // 使用图层缓存作为属性表模型的数据来源。
    QgsVectorLayerCache* lc = new QgsVectorLayerCache(layer, qMax(layer->featureCount(), 1LL), tv);

    // 属性表模型负责读取矢量图层字段和要素。
    QgsAttributeTableModel* tm = new QgsAttributeTableModel(lc, tv);
    tm->loadLayer();

    // 过滤模型接入地图画布，支持按当前范围等方式过滤。
    QgsAttributeTableFilterModel* tfm = new QgsAttributeTableFilterModel(m_mapCanvas, tm, tv);
    tfm->setFilterMode(QgsAttributeTableFilterModel::ShowAll);
    tv->setModel(tfm);

    // 显示属性表窗口。
    tv->show();
    tv->raise();
    tv->activateWindow();  // 激活属性表窗口。

#endif
}


void LTZK::on_actionOpenFields_triggered()
{
    // 优先使用图层树当前选中图层。
    if (m_layerTreeView && m_layerTreeView->currentLayer())
    {
        m_curMapLayer = m_layerTreeView->currentLayer();
    }

    QgsVectorLayer* layer = qobject_cast<QgsVectorLayer*>(m_curMapLayer);
    if (layer == nullptr || !layer->isValid())
    {
        logLtzKMessage(QStringLiteral("Field editor open failed: no valid vector layer."), Qgis::Warning);
        qDebug() << QStringLiteral("当前图层无效或不是矢量图层。");
        return;
    }


    logLtzKMessage(QStringLiteral("Field editor opened: %1").arg(layer->name()));
    // 字段属性面板独立显示。
    if (!layer->isEditable())
    {
        layer->startEditing();
    }

    // 只保留一个字段管理窗口实例。
    static QgsSourceFieldsProperties* pWidget = nullptr;
    if (pWidget)  // 关闭旧窗口，避免重复打开多个字段面板。
    {
        pWidget->raise();
        pWidget->activateWindow();
        return;
    }

    // 让字段管理器跟随当前矢量图层。
    pWidget = new QgsSourceFieldsProperties(layer, nullptr);
    pWidget->loadRows();

    // 显示窗口。
    pWidget->show();
    pWidget->raise();
    pWidget->activateWindow();

    // 窗口销毁时清空静态指针 pWidget。
    connect(pWidget, &QgsSourceFieldsProperties::destroyed, [=]() {
        pWidget = nullptr;
        });
}


void LTZK::mouseCoordinateChanged(const QgsPointXY& newCoordinate)
{
    updateWorkspaceMapCoordinates(newCoordinate);
    if (!m_CurrentCoordinage || !m_mapCanvas)
    {
        return;
    }
    const QString unavailable = QStringLiteral("\u7ECF\u5EA6\uFF1A--  \u7EAC\u5EA6\uFF1A--");
    const auto sourceCrs = m_mapCanvas->mapSettings().destinationCrs();
    if (!sourceCrs.isValid() || !std::isfinite(newCoordinate.x()) || !std::isfinite(newCoordinate.y()))
    {
        m_CurrentCoordinage->setText(unavailable);
        return;
    }
    try
    {
        const QgsCoordinateTransform transform(sourceCrs,
            QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")), QgsProject::instance());
        const QgsPointXY geographic = transform.transform(newCoordinate);
        m_CurrentCoordinage->setText(QStringLiteral("\u7ECF\u5EA6\uFF1A%1  \u7EAC\u5EA6\uFF1A%2")
            .arg(QString::number(geographic.x(), 'f', 6))
            .arg(QString::number(geographic.y(), 'f', 6)));
    }
    catch (const QgsCsException&)
    {
        m_CurrentCoordinage->setText(unavailable);
    }
}


// 将 Web Mercator 坐标转换为 WGS84 经纬度。
void LTZK::convertMercatorToWGS84(const QgsPoint& curPoint, QgsPoint& newPoint)
{
    QgsCoordinateReferenceSystem dstCrs("EPSG:4326");
    QgsCoordinateReferenceSystem srcCrs("EPSG:3857");
    const QgsCoordinateTransformContext context;
    QgsCoordinateTransform crsTras(srcCrs, dstCrs, context);

    auto tmpPt = crsTras.transform(curPoint);
    newPoint.setX(tmpPt.x());
    newPoint.setY(tmpPt.y());

}


void LTZK::on_actionCoordTrans_triggered()
{
    // shp 坐标转换。
    QgsVectorLayer* layer = (QgsVectorLayer*)m_curMapLayer;
    QString outputFile = QStringLiteral("E:/output.shp");;
    if (layer == nullptr || !layer->isValid() || QgsMapLayerType::VectorLayer != m_curMapLayer->type())
    {
        return;
    }

    // 获取源图层坐标系并定义目标坐标系。
    QString targetCrsCode = "EPSG:4326";
    QgsCoordinateReferenceSystem shpCrs = layer->crs();// 源坐标系。

    QgsCoordinateReferenceSystem targetCrs = QgsCoordinateReferenceSystem();
    targetCrs.createFromProj("+proj=utm +zone=49 +datum=WGS84 +units=m +no_defs");// 目标坐标系。

    if (!targetCrs.isValid()) {
        qDebug() << "目标坐标系创建成功:" << targetCrs.authid();
        return;
    }
    // 构造坐标转换对象。
    QString srsDbName = QgsApplication::srsDatabaseFilePath();
    QgsCoordinateTransform* pTransform = new QgsCoordinateTransform(shpCrs, targetCrs, QgsProject::instance());
    if (pTransform == nullptr || !pTransform->isValid())
    {
        return;
    }
    // 遍历源要素，转换几何后写入 postTransfeatureList。
    QgsFeatureList postTransfeatureList;
    QgsFeatureIterator iter = layer->getFeatures();
    QgsFeature feature;
    while ((iter.nextFeature(feature)))
    {
        QgsGeometry g = feature.geometry();
        QgsAttributes f = feature.attributes();
        // transform 会修改 geometry，需要先复制一份 geometry。
        // QgsGeometry::transform 返回几何操作结果。
        // 转换成功后把新 Geometry 写回 feature。
        // 转换失败则清空 QgsGeometry，避免写出错误 geometry。
        if (g.transform(*pTransform) == Qgis::GeometryOperationResult::Success)
        {
            feature.setGeometry(g);
        }
        else
        {
            feature.clearGeometry();
        }
        postTransfeatureList << feature;
    }

    // 创建临时 shp 图层，将转换后的要素写入输出文件。
    QString errorMessage;
    const QString fileFormat = "ESRI Shapefile";
    const QString enc = "System";
    QgsFields fields = layer->fields();
    QgsVectorLayer* targetLayer = createTempLayer(layer->geometryType(), targetCrs.authid(), fields, postTransfeatureList);
    QString errMsg;
    QgsVectorFileWriter::SaveVectorOptions saveOptions;
    saveOptions.fileEncoding = targetLayer->dataProvider()->encoding();
    saveOptions.driverName = "ESRI Shapefile";
    targetLayer->setCrs(targetCrs);// 设置目标坐标系。

    QgsVectorFileWriter::WriterError err = QgsVectorFileWriter::writeAsVectorFormatV3(targetLayer, outputFile, targetLayer->transformContext(), saveOptions, &errMsg);
    if (err != QgsVectorFileWriter::WriterError::NoError)
    {
        return;
    }
    QgsProject::instance()->addMapLayer(targetLayer);
    logLtzKMessage(QStringLiteral("Coordinate transform output added: %1").arg(outputFile));

    return;

    // qgs 工程坐标转换示例。
    //QgsCoordinateReferenceSystem targetCrs = QgsCoordinateReferenceSystem();
    //targetCrs.createFromProj("+proj=utm +zone=49 +datum=WGS84 +units=m +no_defs");// 目标坐标系。
    //QgsProject* project = QgsProject::instance();
    //project->setCrs(targetCrs);
    //return;

    // 项目坐标系已更新。

}

void LTZK::on_actionCoordTransShp_triggered()
{
    // shp 坐标转换。
    if (m_curMapLayer == nullptr || QgsMapLayerType::VectorLayer != m_curMapLayer->type())
    {
        return;
    }

    QgsVectorLayer* layer = qobject_cast<QgsVectorLayer*>(m_curMapLayer);
    if (layer == nullptr || !layer->isValid())
    {
        return;
    }

    QString outputFile = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("坐标转换输出 Shapefile"),
        QStringLiteral("E:/output.shp"),
        QStringLiteral("Shapefile (*.shp)")
    );
    if (outputFile.isEmpty())
    {
        return;
    }
    if (!outputFile.endsWith(QStringLiteral(".shp"), Qt::CaseInsensitive))
    {
        outputFile += QStringLiteral(".shp");
    }

    // 获取源图层坐标系并定义目标坐标系。
    QString targetCrsCode = "EPSG:4326";
    QgsCoordinateReferenceSystem shpCrs = layer->crs();// 源坐标系。

    QgsCoordinateReferenceSystem targetCrs = QgsCoordinateReferenceSystem();
    targetCrs.createFromProj("+proj=utm +zone=49 +datum=WGS84 +units=m +no_defs");// 目标坐标系。

    if (!targetCrs.isValid()) {
        qDebug() << "目标坐标系创建成功:" << targetCrs.authid();
        return;
    }
    // 构造坐标转换对象。
    QString srsDbName = QgsApplication::srsDatabaseFilePath();
    QgsCoordinateTransform* pTransform = new QgsCoordinateTransform(shpCrs, targetCrs, QgsProject::instance());
    if (pTransform == nullptr || !pTransform->isValid())
    {
        return;
    }
    // 遍历源要素，转换几何后写入 postTransfeatureList。
    QgsFeatureList postTransfeatureList;
    QgsFeatureIterator iter = layer->getFeatures();
    QgsFeature feature;
    while ((iter.nextFeature(feature)))
    {
        QgsGeometry g = feature.geometry();
        QgsAttributes f = feature.attributes();
        // transform 会修改 geometry，需要先复制一份 geometry。
        // QgsGeometry::transform 返回几何操作结果。
        // 转换成功后把新 Geometry 写回 feature。
        // 转换失败则清空 QgsGeometry，避免写出错误 geometry。
        if (g.transform(*pTransform) == Qgis::GeometryOperationResult::Success)
        {
            feature.setGeometry(g);
        }
        else
        {
            feature.clearGeometry();
        }
        postTransfeatureList << feature;
    }

    // 创建临时 shp 图层，将转换后的要素写入输出文件。
    QString errorMessage;
    const QString fileFormat = "ESRI Shapefile";
    const QString enc = "System";
    QgsFields fields = layer->fields();
    QgsVectorLayer* targetLayer = createTempLayer(layer->geometryType(), targetCrs.authid(), fields, postTransfeatureList);
    QString errMsg;
    QgsVectorFileWriter::SaveVectorOptions saveOptions;
    saveOptions.fileEncoding = targetLayer->dataProvider()->encoding();
    saveOptions.driverName = "ESRI Shapefile";
    targetLayer->setCrs(targetCrs);// 设置目标坐标系。

    QgsVectorFileWriter::WriterError err = QgsVectorFileWriter::writeAsVectorFormatV3(targetLayer, outputFile, targetLayer->transformContext(), saveOptions, &errMsg);
    if (err != QgsVectorFileWriter::WriterError::NoError)
    {
        return;
    }
    QgsProject::instance()->addMapLayer(targetLayer);
    logLtzKMessage(QStringLiteral("Coordinate transform output added: %1").arg(outputFile));

    return;
}

void LTZK::on_actionCoordTransqgis_triggered()
{
    // qgs 工程坐标转换示例。
    QgsCoordinateReferenceSystem targetCrs = QgsCoordinateReferenceSystem();
    targetCrs.createFromProj("+proj=utm +zone=49 +datum=WGS84 +units=m +no_defs");// 目标坐标系。
    QgsProject* project = QgsProject::instance();
    project->setCrs(targetCrs);
    logLtzKMessage(QStringLiteral("Project CRS changed: %1").arg(targetCrs.authid()));
    return;
}

void LTZK::on_actionCoordTranstif_triggered()
{
    // tif 坐标转换。
    QgsRasterLayer* layer = (QgsRasterLayer*)m_curMapLayer;
    QString outputFile = QStringLiteral("E:/output.tif");;
    if (layer == nullptr || !layer->isValid())
    {
        return;
    }

    // 获取源栅格坐标系并定义目标坐标系。
    QString targetCrsCode = "EPSG:4326";
    QgsCoordinateReferenceSystem shpCrs = layer->crs();// 源坐标系。

    QgsCoordinateReferenceSystem targetCrs = QgsCoordinateReferenceSystem();
    targetCrs.createFromProj("+proj=utm +zone=49 +datum=WGS84 +units=m +no_defs");// 目标坐标系。
    layer->setCrs(targetCrs);

    if (!targetCrs.isValid()) {
        qDebug() << "目标坐标系创建成功:" << targetCrs.authid();
        return;
    }

    QgsRasterFileWriter writer(outputFile);

    // 通过 QgsRasterPipe 克隆栅格数据提供者。
    QgsRasterPipe pipe;
    pipe.set(layer->dataProvider()->clone());
    // 使用 QgsRasterBlockFeedback 接收写出反馈。
    QgsRasterBlockFeedback feedback;
    // 如需写出 tif，可启用下面的 writeRaster 调用。
    //writer.writeRaster(&pipe, layer->width(), layer->height(), layer->extent(), layer->crs(), &feedback);

    QgsProject::instance()->addMapLayer(layer);

    return;
}
void LTZK::on_actionCoordTranspoint_triggered()
{
    QgsPointXY curPoint(11472426, 4197692);
    QgsCoordinateReferenceSystem srcCrs = QgsCoordinateReferenceSystem(); // 源坐标系。
    srcCrs.createFromProj("+proj=merc +lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs");
    QgsCoordinateReferenceSystem dstCrs = QgsCoordinateReferenceSystem();
    dstCrs.createFromProj("+proj=utm +zone=49 +datum=WGS84 +units=m +no_defs");// WGS84 坐标系。

    QgsCoordinateTransform* crsTras = new QgsCoordinateTransform(srcCrs, dstCrs, QgsProject::instance());
    if (crsTras == nullptr || !crsTras->isValid())
    {
        return;
    }
    auto tmpPt = crsTras->transform(curPoint);
    QgsPoint* newPoint = new QgsPoint();
    newPoint->setX(tmpPt.x());
    newPoint->setY(tmpPt.y());

    QList<QgsField> fields;
    fields.append(QgsField("id", QVariant::Int)); // 添加 id 字段。

    QgsVectorLayer* pointLayer = new QgsVectorLayer("Point?crs=EPSG:4326", "PointLayer", "memory");
    if (!pointLayer) {
        qWarning() << "Layer not created!";
        return;
    }

    // 添加字段。
    pointLayer->dataProvider()->addAttributes(fields);
    pointLayer->updateFields();

    // 创建点要素。
    QgsFeature feature;
    feature.setGeometry(QgsGeometry::fromPointXY(*newPoint)); // 使用 newPoint 创建点几何。
    feature.setAttributes(QgsAttributes() << 1); // 设置属性，ID 为 1。

    // 添加点要素到内存图层。
    pointLayer->dataProvider()->addFeature(feature);
    pointLayer->updateExtents();

    // 添加转换后的点图层到项目。
    QgsProject::instance()->addMapLayer(pointLayer);
    logLtzKMessage(QStringLiteral("Coordinate transform point layer added."));
}

void LTZK::on_actionZoomIn_triggered()
{
    m_mapCanvas->setMapTool(m_zoomInTool);
}
void LTZK::on_actionZoomOut_triggered()
{
    m_mapCanvas->setMapTool(m_zoomOutTool);
}
void LTZK::on_actionPan_triggered()
{
    m_mapCanvas->setMapTool(m_panTool);
}
void LTZK::on_actionZoomActual_triggered()
{
    restoreInitialMapView();
}
void LTZK::on_actionFullExtent_triggered()
{
    resetInitialMapView();
}


void LTZK::on_actionLoadQgisPlugin_triggered()
{
    const QString pluginPath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("\u52A0\u8F7D QGIS C++ \u63D2\u4EF6"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/plugins"),
        QStringLiteral("QGIS \u63D2\u4EF6 DLL (*.dll)")
    );

    if (pluginPath.isEmpty())
    {
        return;
    }

    QString errorMessage;
    if (!m_pluginManager->loadPlugin(pluginPath, &errorMessage))
    {
        logLtzKMessage(QStringLiteral("Plugin load failed: %1").arg(errorMessage), Qgis::Warning);
        QMessageBox::critical(this, QStringLiteral("\u63D2\u4EF6\u52A0\u8F7D\u5931\u8D25"), errorMessage);
        return;
    }

    logLtzKMessage(QStringLiteral("Plugin loaded: %1").arg(pluginPath));
}




































































































































































































