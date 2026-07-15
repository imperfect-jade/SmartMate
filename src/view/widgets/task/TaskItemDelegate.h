#pragma once

#include <QStyledItemDelegate>

namespace smartmate::viewmodel { class TaskListContract; }

namespace smartmate::view::widgets {

/// 任务卡片 Delegate 只消费 Contract Role，并把稳定 TaskId 事件转发给页面。
class TaskItemDelegate final : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit TaskItemDelegate(viewmodel::TaskListContract &tasks,
                              QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const override;
    /// 返回当前卡片中可用于开始任务的专用拖拽柄区域；不具备资格时返回空区域。
    [[nodiscard]] QRect dragHandleRect(const QRect &itemRect,
                                       const QModelIndex &index) const;
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;

signals:
    /// 需要确认或打开其他视图的动作交给页面；命令身份始终使用稳定 TaskId。
    void detailsRequested(const QString &taskId);
    void editRequested(const QString &taskId);
    void editDependenciesRequested(const QString &taskId);
    void cancelRequested(const QString &taskId, const QString &title);
    void archiveRequested(const QString &taskId, const QString &title);
    void deleteRequested(const QString &taskId, const QString &title);

private:
    [[nodiscard]] QRect primaryActionRect(const QRect &rect) const;
    [[nodiscard]] QRect menuRect(const QRect &rect) const;
    /// 非拥有列表 Contract；Delegate 只读取 Role 并调用其强类型命令。
    viewmodel::TaskListContract &m_tasks;
};

} // namespace smartmate::view::widgets
