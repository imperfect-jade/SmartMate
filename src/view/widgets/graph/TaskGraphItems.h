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
    [[nodiscard]] QString taskId() const;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
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
    void rebuildPath();

    QPersistentModelIndex m_index;
    WidgetTheme m_theme;
    QPainterPath m_path;
    QPolygonF m_arrow;
    QRectF m_bounds;
};

} // namespace smartmate::view::widgets
