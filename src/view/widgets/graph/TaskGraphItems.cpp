#include "TaskGraphItems.h"

#include "viewmodel/contracts/TaskGraphContract.h"

#include <QFontMetricsF>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QStyleOptionGraphicsItem>

namespace smartmate::view::widgets {

using Graph = viewmodel::TaskGraphContract;

TaskGraphNodeItem::TaskGraphNodeItem(Graph &graph, const QModelIndex &index,
                                     const WidgetTheme &theme)
    : m_graph(graph), m_index(index), m_theme(theme)
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setCursor(Qt::PointingHandCursor);
    setZValue(1.0);
    refresh(theme, false);
}

QRectF TaskGraphNodeItem::boundingRect() const
{
    return {0.0, 0.0, m_size.width(), m_size.height()};
}

QString TaskGraphNodeItem::taskId() const
{
    return m_index.data(Graph::TaskIdRole).toString();
}

void TaskGraphNodeItem::refresh(const WidgetTheme &theme, const bool animate)
{
    if (!m_index.isValid()) return;
    // 尺寸、位置和强调级别全部读取 Role；图元不计算拓扑层级或节点布局。
    prepareGeometryChange();
    m_theme = theme;
    m_size = {m_index.data(Graph::NodeWidthRole).toReal(),
              m_index.data(Graph::NodeHeightRole).toReal()};
    setPos(m_index.data(Graph::NodeXRole).toReal(),
           m_index.data(Graph::NodeYRole).toReal());
    setTransformOriginPoint(boundingRect().center());
    const bool selected = m_index.data(Graph::SelectedRole).toBool();
    const bool matched = m_index.data(Graph::FilterMatchedRole).toBool();
    const bool unrelated = m_index.data(Graph::EmphasisLevelRole).toInt()
        == Graph::UnrelatedEmphasis;
    const bool core = m_index.data(Graph::CoreNodeRole).toBool();
    const qreal targetOpacity = (!matched || unrelated) ? 0.32
        : (!core && !selected) ? 0.68 : 1.0;
    const qreal targetScale = selected ? 1.025 : 1.0;
    if (animate) {
        auto *opacityAnimation = new QPropertyAnimation(this, "opacity", this);
        opacityAnimation->setDuration(140);
        opacityAnimation->setEndValue(targetOpacity);
        opacityAnimation->start(QAbstractAnimation::DeleteWhenStopped);
        auto *scaleAnimation = new QPropertyAnimation(this, "scale", this);
        scaleAnimation->setDuration(140);
        scaleAnimation->setEndValue(targetScale);
        scaleAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        setOpacity(targetOpacity);
        setScale(targetScale);
    }
    setToolTip(m_index.data(Graph::BlockingReasonTextRole).toString());
    update();
}

void TaskGraphNodeItem::paint(QPainter *painter,
                              const QStyleOptionGraphicsItem *, QWidget *)
{
    const QRectF card = boundingRect();
    const bool selected = m_index.data(Graph::SelectedRole).toBool();
    const bool core = m_index.data(Graph::CoreNodeRole).toBool();
    const bool archived = m_index.data(Graph::ArchivedRole).toBool();
    const bool blocked = m_index.data(Graph::BlockedRole).toBool();
    const int status = m_index.data(Graph::StatusIndexRole).toInt();
    const QColor stateColor = blocked ? m_theme.warning : m_theme.statusColor(status);
    const QColor fill = selected ? m_theme.primarySoft
        : (!core || archived) ? m_theme.surfaceStrong : m_theme.surfaceElevated;
    const QColor border = selected || status == 1 ? m_theme.primary
        : !core ? m_theme.archived
        : blocked ? m_theme.warning
        : archived ? m_theme.archived : m_theme.borderStrong;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(border, selected ? 3.0 : status == 1 ? 2.5 : 1.5));
    painter->setBrush(fill);
    painter->drawRoundedRect(card.adjusted(1.5, 1.5, -1.5, -1.5), 11, 11);
    painter->setPen(Qt::NoPen);
    painter->setBrush(stateColor);
    painter->drawRoundedRect(QRectF{0, 0, 5, card.height()}, 3, 3);

    const qreal left = 15.0;
    painter->drawEllipse(QRectF{left, 14, 9, 9});
    QFont titleFont = painter->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 1.0);
    painter->setFont(titleFont);
    painter->setPen(m_theme.textPrimary);
    const QString title = m_index.data(Graph::TitleRole).toString();
    const bool hasCategory = m_index.data(Graph::HasCategoryRole).toBool();
    const qreal titleRight = hasCategory
        ? card.width() - (blocked ? 105.0 : 88.0)
        : card.width() - (blocked ? 30.0 : 13.0);
    painter->drawText(QRectF{30, 7, titleRight - 30, 25},
                      Qt::AlignVCenter | Qt::AlignLeft,
                      QFontMetricsF(titleFont).elidedText(
                          title, Qt::ElideRight, titleRight - 30));

    if (hasCategory) {
        const QColor accent{m_index.data(Graph::CategoryAccentRole).toString()};
        const QString category = m_index.data(Graph::CategoryNameRole).toString();
        QFont badgeFont = painter->font();
        badgeFont.setPointSizeF(std::max(7.0, badgeFont.pointSizeF() - 2.0));
        painter->setFont(badgeFont);
        const QString badgeText = QFontMetricsF(badgeFont).elidedText(
            category, Qt::ElideRight, 58.0);
        painter->setPen(QPen(accent, 1.0));
        QColor badgeFill = accent;
        badgeFill.setAlphaF(0.12);
        painter->setBrush(badgeFill);
        const qreal badgeX = card.width() - (blocked ? 98.0 : 80.0);
        painter->drawRoundedRect(QRectF{badgeX, 8, 69, 20}, 10, 10);
        painter->setPen(accent);
        painter->drawText(QRectF{badgeX + 5, 8, 59, 20},
                          Qt::AlignCenter, badgeText);
    }
    if (blocked) {
        painter->setPen(m_theme.warning);
        painter->drawText(QRectF{card.width() - 27, 7, 18, 22},
                          Qt::AlignCenter, QStringLiteral("🔒"));
    }

    QFont bodyFont = painter->font();
    bodyFont.setBold(false);
    bodyFont.setPointSizeF(std::max(8.0, bodyFont.pointSizeF() - 1.0));
    painter->setFont(bodyFont);
    painter->setPen(m_theme.textSecondary);
    painter->drawText(QRectF{left, 37, card.width() - 26, 19},
        Qt::AlignVCenter | Qt::AlignLeft,
        QStringLiteral("%1 · %2优先级")
            .arg(m_index.data(Graph::StatusTextRole).toString(),
                 m_index.data(Graph::PriorityTextRole).toString()));
    QString footer;
    if (!core) footer = tr("跨类别上下文");
    else if (blocked) footer = tr("等待前置任务");
    else if (m_index.data(Graph::UnlockCountRole).toInt() > 0)
        footer = tr("完成后解锁 %1 项").arg(
            m_index.data(Graph::UnlockCountRole).toInt());
    else footer = m_index.data(Graph::DeadlineTextRole).toString();
    painter->setPen(blocked ? m_theme.warning : m_theme.textMuted);
    painter->drawText(QRectF{left, 62, card.width() - 26, 18},
                      Qt::AlignVCenter | Qt::AlignLeft,
                      QFontMetricsF(bodyFont).elidedText(
                          footer, Qt::ElideRight, card.width() - 26));
    painter->setPen(QPen(m_theme.borderStrong, 1.0));
    painter->setBrush(m_theme.surfaceElevated);
    painter->drawEllipse(QPointF{card.width() / 2.0, 0.0}, 5, 5);
    painter->drawEllipse(QPointF{card.width() / 2.0, card.height()}, 5, 5);
}

void TaskGraphNodeItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 图元只转发稳定 ID 选择命令，选中 Role 的变化由 Contract 通知回来。
        m_graph.selectTask(taskId());
        event->accept();
        return;
    }
    QGraphicsObject::mousePressEvent(event);
}

void TaskGraphNodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    m_graph.setHoveredTask(taskId());
    QGraphicsObject::hoverEnterEvent(event);
}

void TaskGraphNodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_graph.clearHoveredTask();
    QGraphicsObject::hoverLeaveEvent(event);
}

TaskGraphEdgeItem::TaskGraphEdgeItem(const QModelIndex &index,
                                     const WidgetTheme &theme)
    : m_index(index), m_theme(theme)
{
    setAcceptedMouseButtons(Qt::NoButton);
    setZValue(0.0);
    rebuildPath();
    refresh(theme);
}

QRectF TaskGraphEdgeItem::boundingRect() const { return m_bounds; }

void TaskGraphEdgeItem::rebuildPath()
{
    // 正交折点和箭头三角形均来自 Contract，View 只转换成 Qt 绘制对象。
    prepareGeometryChange();
    m_path = {};
    const QVariantList points = m_index.data(Graph::EdgeRoutePointsRole).toList();
    if (!points.isEmpty()) {
        m_path.moveTo(points.constFirst().toPointF());
        for (qsizetype i = 1; i < points.size(); ++i)
            m_path.lineTo(points.at(i).toPointF());
    }
    m_arrow = {
        QPointF{m_index.data(Graph::EdgeArrowTipXRole).toReal(),
                m_index.data(Graph::EdgeArrowTipYRole).toReal()},
        QPointF{m_index.data(Graph::EdgeArrowLeftXRole).toReal(),
                m_index.data(Graph::EdgeArrowLeftYRole).toReal()},
        QPointF{m_index.data(Graph::EdgeArrowRightXRole).toReal(),
                m_index.data(Graph::EdgeArrowRightYRole).toReal()},
    };
    m_bounds = m_path.boundingRect().united(m_arrow.boundingRect()).adjusted(-6, -6, 6, 6);
}

void TaskGraphEdgeItem::refresh(const WidgetTheme &theme)
{
    if (!m_index.isValid()) return;
    m_theme = theme;
    rebuildPath();
    setOpacity(m_index.data(Graph::EdgeDimmedRole).toBool() ? 0.24 : 1.0);
    update();
}

void TaskGraphEdgeItem::paint(QPainter *painter,
                              const QStyleOptionGraphicsItem *, QWidget *)
{
    const QPen pen = presentationPen();
    const QColor color = pen.color();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_path);
    painter->setPen(QPen(color, 1.0));
    painter->setBrush(color);
    painter->drawPolygon(m_arrow);
}

QPen TaskGraphEdgeItem::presentationPen() const
{
    // 线型是边语义 Role 的视觉映射，不用于反推或改变依赖状态。
    const bool cancelled = m_index.data(Graph::EdgeCancelledRole).toBool();
    const bool satisfied = m_index.data(Graph::EdgeSatisfiedRole).toBool();
    const bool emphasized = m_index.data(Graph::EdgeHighlightedRole).toBool()
        || m_index.data(Graph::EdgeHoveredRole).toBool();
    const QColor color = cancelled ? m_theme.textDisabled
        : satisfied ? m_theme.done : m_theme.warning;
    QPen pen{color, emphasized ? 4.0 : 2.2};
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    if (cancelled) {
        pen.setStyle(Qt::DashLine);
        pen.setDashPattern({5.0, 4.0});
    }
    return pen;
}

} // namespace smartmate::view::widgets
