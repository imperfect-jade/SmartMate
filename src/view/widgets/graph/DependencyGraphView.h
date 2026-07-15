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
    /// 设置当前视口缩放倍率；只改变 View transform，不改变图节点坐标。
    void setZoomFactor(qreal factor);
    /// 将 Contract 给出的完整内容矩形缩放到视口内。
    void fitContent();
    /// 根据稳定选择投影定位节点，不修改选择状态。
    void centerSelectedNode();
    void setTheme(const WidgetTheme &theme);
    [[nodiscard]] int nodeItemCount() const noexcept;
    [[nodiscard]] int edgeItemCount() const noexcept;

signals:
    /// 视口倍率变化通知页面同步百分比标签。
    void zoomFactorChanged(qreal factor);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    /// 图结构变化时重建图元；Role 局部变化时仅刷新现有图元。
    void rebuildScene();
    void refreshNodes(bool animate = true);
    void refreshEdges();
    void updateSceneRect();

    /// 非拥有图 Contract；其生命周期由组合根保证。
    viewmodel::TaskGraphContract &m_graph;
    WidgetTheme m_theme;
    /// 场景拥有图元，列表仅保存便于批量刷新和定位的观察指针。
    QList<TaskGraphNodeItem *> m_nodes;
    QList<TaskGraphEdgeItem *> m_edges;
    qreal m_zoomFactor{1.0};
};

} // namespace smartmate::view::widgets
