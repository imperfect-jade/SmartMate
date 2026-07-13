#pragma once

#include "view/widgets/theme/WidgetTheme.h"

#include <QGraphicsView>

namespace smartmate::viewmodel { class TaskGraphContract; }

namespace smartmate::view::widgets {

class TaskGraphEdgeItem;
class TaskGraphNodeItem;

/// 只负责场景同步、平移缩放和视口定位的依赖图 View。
class DependencyGraphView final : public QGraphicsView {
    Q_OBJECT
public:
    explicit DependencyGraphView(viewmodel::TaskGraphContract &graph,
                                 QWidget *parent = nullptr);

    [[nodiscard]] qreal zoomFactor() const noexcept;
    void setZoomFactor(qreal factor);
    void fitContent();
    void centerSelectedNode();
    void setTheme(const WidgetTheme &theme);
    [[nodiscard]] int nodeItemCount() const noexcept;
    [[nodiscard]] int edgeItemCount() const noexcept;

signals:
    void zoomFactorChanged(qreal factor);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void rebuildScene();
    void refreshNodes(bool animate = true);
    void refreshEdges();
    void updateSceneRect();

    viewmodel::TaskGraphContract &m_graph;
    WidgetTheme m_theme;
    QList<TaskGraphNodeItem *> m_nodes;
    QList<TaskGraphEdgeItem *> m_edges;
    qreal m_zoomFactor{1.0};
};

} // namespace smartmate::view::widgets
