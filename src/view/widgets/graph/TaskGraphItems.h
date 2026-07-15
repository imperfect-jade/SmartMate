#pragma once

#include "view/widgets/theme/WidgetTheme.h"

#include <QGraphicsObject>
#include <QPainterPath>
#include <QPen>
#include <QPersistentModelIndex>

namespace smartmate::viewmodel { class TaskGraphContract; }

namespace smartmate::view::widgets {

/// 直接绘制 TaskGraphContract 节点 Role 的图元，不参与任何图计算。
class TaskGraphNodeItem final : public QGraphicsObject {
    Q_OBJECT
public:
    TaskGraphNodeItem(viewmodel::TaskGraphContract &graph,
                      const QModelIndex &index,
                      const WidgetTheme &theme);

    [[nodiscard]] QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;
    void refresh(const WidgetTheme &theme, bool animate = true);
    /// 返回 Role 中的稳定 TaskId，供点击命令和视口定位使用。
    [[nodiscard]] QString taskId() const;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    /// Contract 与持久索引均为观察引用；模型重置后图元会由 View 重建。
    viewmodel::TaskGraphContract &m_graph;
    QPersistentModelIndex m_index;
    WidgetTheme m_theme;
    QSizeF m_size;
};

/// 按 Contract 已提供的正交折线和箭头顶点绘制一条依赖边。
class TaskGraphEdgeItem final : public QGraphicsObject {
    Q_OBJECT
public:
    TaskGraphEdgeItem(const QModelIndex &index, const WidgetTheme &theme);

    [[nodiscard]] QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;
    void refresh(const WidgetTheme &theme);
    /// 返回当前 Contract Role 对应的线型，供绘制和语义视觉回归共用。
    [[nodiscard]] QPen presentationPen() const;

private:
    /// 只把 Contract 提供的折点转换为 QPainterPath，不重新计算依赖几何。
    void rebuildPath();

    /// 持久索引跟随行移动；模型重置时所属图元会被销毁重建。
    QPersistentModelIndex m_index;
    WidgetTheme m_theme;
    QPainterPath m_path;
    QPolygonF m_arrow;
    QRectF m_bounds;
};

} // namespace smartmate::view::widgets
