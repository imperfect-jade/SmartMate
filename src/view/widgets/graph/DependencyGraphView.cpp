#include "DependencyGraphView.h"

#include "TaskGraphItems.h"
#include "viewmodel/contracts/TaskGraphContract.h"

#include <QAbstractItemModel>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QScrollBar>

#include <algorithm>

namespace smartmate::view::widgets {

DependencyGraphView::DependencyGraphView(viewmodel::TaskGraphContract &graph,
                                         QWidget *parent)
    : QGraphicsView(new QGraphicsScene, parent)
    , m_graph(graph)
    , m_theme(WidgetTheme::fromAccentIndex(0))
{
    setObjectName(QStringLiteral("dependencyGraphViewport"));
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                   | QPainter::SmoothPixmapTransform);
    setAlignment(Qt::AlignCenter);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFrameShape(QFrame::NoFrame);

    connect(&m_graph, &QAbstractItemModel::modelReset,
            this, &DependencyGraphView::rebuildScene);
    connect(&m_graph, &QAbstractItemModel::rowsInserted,
            this, &DependencyGraphView::rebuildScene);
    connect(&m_graph, &QAbstractItemModel::rowsRemoved,
            this, &DependencyGraphView::rebuildScene);
    connect(&m_graph, &QAbstractItemModel::dataChanged,
            this, [this] { refreshNodes(); });
    connect(&m_graph, &viewmodel::TaskGraphContract::contentWidthChanged,
            this, &DependencyGraphView::updateSceneRect);
    connect(&m_graph, &viewmodel::TaskGraphContract::contentHeightChanged,
            this, &DependencyGraphView::updateSceneRect);

    QAbstractItemModel *edgeModel = m_graph.edges();
    connect(edgeModel, &QAbstractItemModel::modelReset,
            this, &DependencyGraphView::rebuildScene);
    connect(edgeModel, &QAbstractItemModel::rowsInserted,
            this, &DependencyGraphView::rebuildScene);
    connect(edgeModel, &QAbstractItemModel::rowsRemoved,
            this, &DependencyGraphView::rebuildScene);
    connect(edgeModel, &QAbstractItemModel::dataChanged,
            this, [this] { refreshEdges(); });
    rebuildScene();
}

qreal DependencyGraphView::zoomFactor() const noexcept { return m_zoomFactor; }

void DependencyGraphView::setZoomFactor(const qreal factor)
{
    const qreal normalized = std::clamp(factor, 0.5, 2.0);
    if (qFuzzyCompare(m_zoomFactor, normalized)) return;
    m_zoomFactor = normalized;
    resetTransform();
    scale(m_zoomFactor, m_zoomFactor);
    emit zoomFactorChanged(m_zoomFactor);
}

void DependencyGraphView::fitContent()
{
    if (m_graph.empty() || m_graph.contentWidth() <= 0
        || m_graph.contentHeight() <= 0) {
        setZoomFactor(1.0);
        return;
    }
    const qreal horizontal = std::max(1, viewport()->width() - 32)
        / m_graph.contentWidth();
    const qreal vertical = std::max(1, viewport()->height() - 32)
        / m_graph.contentHeight();
    setZoomFactor(std::min(horizontal, vertical));
    centerOn(sceneRect().center());
}

void DependencyGraphView::centerSelectedNode()
{
    if (m_graph.selectedTaskId().isEmpty()) return;
    centerOn(m_graph.selectedNodeCenterX(), m_graph.selectedNodeCenterY());
}

void DependencyGraphView::setTheme(const WidgetTheme &theme)
{
    m_theme = theme;
    setBackgroundBrush(theme.surface);
    refreshEdges();
    refreshNodes(false);
}

int DependencyGraphView::nodeItemCount() const noexcept { return m_nodes.size(); }
int DependencyGraphView::edgeItemCount() const noexcept { return m_edges.size(); }

void DependencyGraphView::mousePressEvent(QMouseEvent *event)
{
    QGraphicsItem *hit = itemAt(event->position().toPoint());
    bool nodeHit = false;
    while (hit) {
        if (dynamic_cast<TaskGraphNodeItem *>(hit)) {
            nodeHit = true;
            break;
        }
        hit = hit->parentItem();
    }
    if (event->button() == Qt::LeftButton && !nodeHit) m_graph.clearSelection();
    QGraphicsView::mousePressEvent(event);
}

void DependencyGraphView::rebuildScene()
{
    scene()->clear();
    m_nodes.clear();
    m_edges.clear();
    QAbstractItemModel *edgeModel = m_graph.edges();
    for (int row = 0; row < edgeModel->rowCount(); ++row) {
        auto *item = new TaskGraphEdgeItem(edgeModel->index(row, 0), m_theme);
        scene()->addItem(item);
        m_edges.append(item);
    }
    for (int row = 0; row < m_graph.rowCount(); ++row) {
        auto *item = new TaskGraphNodeItem(m_graph, m_graph.index(row, 0), m_theme);
        scene()->addItem(item);
        m_nodes.append(item);
    }
    updateSceneRect();
}

void DependencyGraphView::refreshNodes(const bool animate)
{
    for (TaskGraphNodeItem *item : std::as_const(m_nodes))
        item->refresh(m_theme, animate);
}

void DependencyGraphView::refreshEdges()
{
    for (TaskGraphEdgeItem *item : std::as_const(m_edges)) item->refresh(m_theme);
}

void DependencyGraphView::updateSceneRect()
{
    scene()->setSceneRect(0, 0, m_graph.contentWidth(), m_graph.contentHeight());
}

} // namespace smartmate::view::widgets
