#include "TaskItemDelegate.h"

#include "TaskListView.h"
#include "view/widgets/theme/WidgetTheme.h"
#include "viewmodel/contracts/TaskListContract.h"

#include <QApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStringList>
#include <QStyle>

#include <algorithm>

namespace smartmate::view::widgets {
using Role = viewmodel::TaskListContract::Role;
namespace {

QColor translucent(QColor color, const int alpha = 24)
{
    color.setAlpha(alpha);
    return color;
}

int drawBadge(QPainter &painter, const int left, const int top,
              const QString &text, const QColor &color,
              const QFont &font, const int maximumRight)
{
    painter.setFont(font);
    const QFontMetrics metrics(font);
    const int availableTextWidth = std::max(0, maximumRight - left - 14);
    if (availableTextWidth <= 0) return left;
    const QString shown = metrics.elidedText(text, Qt::ElideRight, availableTextWidth);
    const int width = std::min(maximumRight - left,
                               metrics.horizontalAdvance(shown) + 14);
    const QRect badge{left, top, width, 22};
    painter.setPen(QPen(color, 1));
    painter.setBrush(translucent(color));
    painter.drawRoundedRect(badge, 9, 9);
    painter.setPen(color);
    painter.drawText(badge.adjusted(7, 0, -7, 0), Qt::AlignCenter, shown);
    return badge.right() + 7;
}

} // namespace

TaskItemDelegate::TaskItemDelegate(viewmodel::TaskListContract &tasks,
                                   QObject *parent)
    : QStyledItemDelegate(parent), m_tasks(tasks)
{
}

QSize TaskItemDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return {520, 138};
}

QRect TaskItemDelegate::primaryActionRect(const QRect &rect) const
{
    return {rect.right() - 142, rect.center().y() - 17, 88, 34};
}

QRect TaskItemDelegate::menuRect(const QRect &rect) const
{
    return {rect.right() - 44, rect.center().y() - 17, 34, 34};
}

QRect TaskItemDelegate::dragHandleRect(const QRect &itemRect,
                                       const QModelIndex &index) const
{
    if (m_tasks.bulkSelectionMode()
        || !index.data(Role::CanStartRole).toBool()) {
        return {};
    }
    const QRect card = itemRect.adjusted(2, 4, -2, -4);
    return {card.left() + 12, card.center().y() - 22, 34, 44};
}

void TaskItemDelegate::paint(QPainter *painter,
                             const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    painter->save();
    QRect card = option.rect.adjusted(2, 4, -2, -4);
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(selected ? option.palette.highlight().color()
                                  : option.palette.mid().color(), selected ? 2 : 1));
    const auto *taskList = qobject_cast<const TaskListView *>(option.widget);
    if (!taskList) {
        taskList = qobject_cast<const TaskListView *>(parent());
    }
    const QColor cardSurface = taskList
        ? taskList->cardSurfaceColor()
        : QApplication::palette().color(QPalette::Base);
    const QPalette themePalette = taskList && taskList->window()
        ? taskList->window()->palette() : option.palette;
    const WidgetTheme theme = WidgetTheme::fromPalette(themePalette);
    painter->setBrush(cardSurface);
    painter->drawRoundedRect(card, 10, 10);

    const bool blocked = index.data(Role::BlockedRole).toBool();
    const bool overdue = index.data(Role::OverdueRole).toBool();
    const auto status = static_cast<viewmodel::TaskStatusVisual>(
        index.data(Role::StatusRole).toInt());
    const auto priority = static_cast<viewmodel::TaskPriorityVisual>(
        index.data(Role::PriorityRole).toInt());
    const QColor statusColor = theme.statusColor(status);
    painter->setPen(Qt::NoPen);
    painter->setBrush(blocked ? theme.warning : statusColor);
    painter->drawRoundedRect(QRect{card.left(), card.top(), 5, card.height()}, 2, 2);

    const QRect dragHandle = dragHandleRect(option.rect, index);
    if (!dragHandle.isEmpty()) {
        QStyleOptionButton handle;
        handle.rect = dragHandle;
        handle.text = QStringLiteral("⠿");
        handle.state = QStyle::State_Enabled;
        QApplication::style()->drawControl(QStyle::CE_PushButton, &handle, painter);
    }

    QRect textRect = card.adjusted(dragHandle.isEmpty() ? 18 : 58, 10, -160, -10);
    QFont titleFont = option.font;
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 1.5);
    painter->setFont(titleFont);
    painter->setPen(theme.textPrimary);
    painter->drawText(QRect{textRect.left(), card.top() + 10,
                            textRect.width(), 22},
                      Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(
                          index.data(Role::TitleRole).toString(),
                          Qt::ElideRight, textRect.width()));

    QFont badgeFont = option.font;
    badgeFont.setPointSizeF(std::max(8.0, badgeFont.pointSizeF() - 0.5));
    badgeFont.setBold(true);
    int badgeLeft = textRect.left();
    const int badgeTop = card.top() + 36;
    badgeLeft = drawBadge(*painter, badgeLeft, badgeTop,
                          index.data(Role::StatusTextRole).toString(),
                          statusColor, badgeFont, textRect.right());
    badgeLeft = drawBadge(*painter, badgeLeft, badgeTop,
                          tr("%1优先级").arg(index.data(Role::PriorityTextRole).toString()),
                          theme.priorityColor(priority), badgeFont, textRect.right());
    if (overdue) {
        badgeLeft = drawBadge(*painter, badgeLeft, badgeTop, tr("已逾期"),
                              theme.danger, badgeFont, textRect.right());
    }
    if (index.data(Role::HasCategoryRole).toBool()) {
        QColor categoryColor(index.data(Role::CategoryAccentRole).toString());
        if (!categoryColor.isValid()) categoryColor = theme.primary;
        drawBadge(*painter, badgeLeft, badgeTop,
                  index.data(Role::CategoryNameRole).toString(),
                  categoryColor, badgeFont, textRect.right());
    }

    painter->setFont(option.font);
    QStringList timeValues;
    const QString deadline = index.data(Role::DeadlineTextRole).toString();
    if (!deadline.isEmpty()) timeValues << tr("截止 %1").arg(deadline);
    const int estimatedMinutes = index.data(Role::EstimatedMinutesRole).toInt();
    if (estimatedMinutes > 0) timeValues << tr("预计 %1 分钟").arg(estimatedMinutes);
    const int predecessorCount = index.data(Role::PredecessorCountRole).toInt();
    if (predecessorCount > 0) timeValues << tr("前置 %1 项").arg(predecessorCount);
    const QString timeText = timeValues.isEmpty()
        ? tr("未设置时间与前置任务") : timeValues.join(QStringLiteral("  ·  "));
    painter->setPen(overdue && !deadline.isEmpty() ? theme.danger : theme.textMuted);
    painter->drawText(QRect{textRect.left(), card.top() + 66,
                            textRect.width(), 20},
                      Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(
                          timeText, Qt::ElideRight, textRect.width()));

    QString reason = blocked
        ? tr("阻塞：%1").arg(index.data(Role::BlockingReasonTextRole).toString())
        : tr("推荐：%1").arg(index.data(Role::OrderReasonTextRole).toString());
    const int unlockCount = index.data(Role::UnlockCountRole).toInt();
    if (!blocked && unlockCount > 0) {
        reason += tr(" · 可解锁 %1 项").arg(unlockCount);
    }
    painter->setPen(blocked ? theme.warning : theme.primary);
    painter->drawText(QRect{textRect.left(), card.top() + 94,
                            textRect.width(), 20},
                      Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(
                          reason, Qt::ElideRight, textRect.width()));

    // 主动作完全取决于 Model 投影的资格 Role；Delegate 不复制任务状态机判断。
    if (!m_tasks.bulkSelectionMode()) {
        QString action;
        if (index.data(Role::CanStartRole).toBool()) action = QObject::tr("开始");
        else if (index.data(Role::CanCompleteRole).toBool()) action = QObject::tr("完成");
        else if (index.data(Role::CanRedoRole).toBool()) action = QObject::tr("重做");
        else if (index.data(Role::CanRestoreRole).toBool()) action = QObject::tr("恢复");
        if (!action.isEmpty()) {
            QStyleOptionButton button;
            button.rect = primaryActionRect(card);
            button.text = action;
            button.state = QStyle::State_Enabled;
            QApplication::style()->drawControl(QStyle::CE_PushButton, &button, painter);
        }
        QStyleOptionButton menuButton;
        menuButton.rect = menuRect(card);
        menuButton.text = QStringLiteral("⋯");
        menuButton.state = QStyle::State_Enabled;
        QApplication::style()->drawControl(QStyle::CE_PushButton, &menuButton, painter);
    } else {
        QStyleOptionButton check;
        check.rect = {card.right() - 42, card.center().y() - 12, 24, 24};
        check.state = QStyle::State_Enabled
            | (index.data(Role::BulkSelectedRole).toBool() ? QStyle::State_On : QStyle::State_Off);
        QApplication::style()->drawControl(QStyle::CE_CheckBox, &check, painter);
    }
    painter->restore();
}

bool TaskItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index)
{
    if (event->type() != QEvent::MouseButtonRelease) return false;
    const auto *mouse = static_cast<QMouseEvent *>(event);
    if (mouse->button() != Qt::LeftButton) return false;
    // 行号只用于命中当前卡片，跨层命令和页面导航始终转发稳定 TaskId。
    const QString id = index.data(Role::TaskIdRole).toString();
    const QString title = index.data(Role::TitleRole).toString();
    if (m_tasks.bulkSelectionMode()) {
        if (index.data(Role::BulkSelectableRole).toBool()) m_tasks.toggleBulkSelection(id);
        return true;
    }
    // 可直接执行的状态命令调用 Contract；需要确认的破坏性动作交给页面处理。
    if (primaryActionRect(option.rect.adjusted(2, 4, -2, -4)).contains(mouse->position().toPoint())) {
        if (index.data(Role::CanStartRole).toBool()) m_tasks.startTask(id);
        else if (index.data(Role::CanCompleteRole).toBool()) m_tasks.completeTask(id);
        else if (index.data(Role::CanRedoRole).toBool()) m_tasks.redoTask(id);
        else if (index.data(Role::CanRestoreRole).toBool()) m_tasks.restoreTask(id);
        return true;
    }
    if (menuRect(option.rect.adjusted(2, 4, -2, -4)).contains(mouse->position().toPoint())) {
        QMenu menu;
        QAction *details = menu.addAction(tr("查看详情"));
        QAction *edit = index.data(Role::CanEditTaskRole).toBool() ? menu.addAction(tr("编辑任务")) : nullptr;
        QAction *editDependencies = index.data(Role::CanEditDependenciesRole).toBool()
            ? menu.addAction(tr("编辑前置任务")) : nullptr;
        menu.addSeparator();
        QAction *cancel = index.data(Role::CanCancelRole).toBool() ? menu.addAction(tr("取消任务")) : nullptr;
        QAction *archive = index.data(Role::CanArchiveRole).toBool() ? menu.addAction(tr("归档")) : nullptr;
        QAction *remove = index.data(Role::CanDeletePermanentlyRole).toBool() ? menu.addAction(tr("永久删除")) : nullptr;
        QAction *chosen = menu.exec(mouse->globalPosition().toPoint());
        if (chosen == details) emit detailsRequested(id);
        else if (chosen && chosen == edit) emit editRequested(id);
        else if (chosen && chosen == editDependencies) emit editDependenciesRequested(id);
        else if (chosen && chosen == cancel) emit cancelRequested(id, title);
        else if (chosen && chosen == archive) emit archiveRequested(id, title);
        else if (chosen && chosen == remove) emit deleteRequested(id, title);
        return true;
    }
    emit detailsRequested(id);
    return true;
}

} // namespace smartmate::view::widgets
