#include "TaskItemDelegate.h"

#include "viewmodel/contracts/TaskListContract.h"

#include <QApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>

namespace smartmate::view::widgets {
using Role = viewmodel::TaskListContract::Role;

TaskItemDelegate::TaskItemDelegate(viewmodel::TaskListContract &tasks,
                                   QObject *parent)
    : QStyledItemDelegate(parent), m_tasks(tasks)
{
}

QSize TaskItemDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return {520, 112};
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
    painter->setBrush(option.palette.base());
    painter->drawRoundedRect(card, 10, 10);

    const bool blocked = index.data(Role::BlockedRole).toBool();
    const bool overdue = index.data(Role::OverdueRole).toBool();
    painter->setPen(Qt::NoPen);
    painter->setBrush(blocked ? QColor(QStringLiteral("#d97706"))
                              : overdue ? QColor(QStringLiteral("#dc2626"))
                                        : option.palette.highlight());
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
    painter->setPen(option.palette.text().color());
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignTop,
                      index.data(Role::TitleRole).toString());

    painter->setFont(option.font);
    painter->setPen(option.palette.color(QPalette::PlaceholderText));
    const QString meta = QStringLiteral("%1 · %2优先级%3%4")
        .arg(index.data(Role::StatusTextRole).toString(),
             index.data(Role::PriorityTextRole).toString(),
             index.data(Role::DeadlineTextRole).toString().isEmpty()
                 ? QString{} : QStringLiteral(" · 截止 %1").arg(index.data(Role::DeadlineTextRole).toString()),
             overdue ? QStringLiteral(" · 已逾期") : QString{});
    painter->drawText(textRect.adjusted(0, 30, 0, 0), Qt::AlignLeft | Qt::AlignTop, meta);
    QString reason = blocked
        ? QStringLiteral("阻塞：%1").arg(index.data(Role::BlockingReasonTextRole).toString())
        : QStringLiteral("推荐：%1").arg(index.data(Role::OrderReasonTextRole).toString());
    if (index.data(Role::HasCategoryRole).toBool()) {
        reason.prepend(QStringLiteral("%1 · ").arg(index.data(Role::CategoryNameRole).toString()));
    }
    painter->drawText(textRect.adjusted(0, 56, 0, 0), Qt::AlignLeft | Qt::AlignTop, reason);

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
    const QString id = index.data(Role::TaskIdRole).toString();
    const QString title = index.data(Role::TitleRole).toString();
    if (m_tasks.bulkSelectionMode()) {
        if (index.data(Role::BulkSelectableRole).toBool()) m_tasks.toggleBulkSelection(id);
        return true;
    }
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
