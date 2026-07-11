#pragma once

#include "domain/Task.h"

#include <QAbstractListModel>
#include <QtQmlIntegration/qqmlintegration.h>

namespace smartmate::model {
class TaskService;
}

namespace smartmate::viewmodel {

/// 将领域任务集合投影成 QML 可绑定的列表；它负责展示格式与命令转发，
/// 不拥有任务真相，也不直接访问 Repository。
class TaskListViewModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool showArchived READ showArchived WRITE setShowArchived NOTIFY showArchivedChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    QML_NAMED_ELEMENT(TaskListViewModel)
    QML_UNCREATABLE("TaskListViewModel is owned by AppViewModel")

public:
    /// 行号会随筛选和排序变化，只有 taskId 是可用于编辑、归档和恢复的稳定身份。
    enum Role {
        TaskIdRole = Qt::UserRole + 1,
        TitleRole,
        DescriptionRole,
        StatusRole,
        StatusTextRole,
        PriorityRole,
        PriorityTextRole,
        DeadlineTextRole,
        EstimatedMinutesRole,
        ArchivedRole,
    };
    Q_ENUM(Role)

    explicit TaskListViewModel(model::TaskService &taskService, QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] int count() const noexcept;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool showArchived() const noexcept;
    [[nodiscard]] QString errorMessage() const;
    void setShowArchived(bool showArchived);

    /// 从Service重新获取快照并重建当前展示投影。
    Q_INVOKABLE void reload();
    /// 按稳定TaskId请求软归档，并把领域错误映射为展示状态。
    Q_INVOKABLE bool archiveTask(const QString &taskId);
    /// 按稳定TaskId请求恢复，并保留Service给出的业务约束。
    Q_INVOKABLE bool restoreTask(const QString &taskId);
    Q_INVOKABLE void clearError();

signals:
    void showArchivedChanged();
    void countChanged();
    void errorMessageChanged();
    void errorOccurred(const QString &message);

private:
    [[nodiscard]] static QString statusText(model::TaskStatus status);
    [[nodiscard]] static QString priorityText(model::TaskPriority priority);
    [[nodiscard]] static model::TaskId parseTaskId(const QString &taskId);
    void rebuildVisibleTasks();
    void setError(const QString &message);

    // Service 由组合根拥有；列表只保留非拥有引用并监听其变化通知。
    model::TaskService &m_taskService;
    // 全量快照与当前可见投影分离，切换活动/归档视图不会修改领域数据。
    QList<model::Task> m_allTasks;
    QList<model::Task> m_visibleTasks;
    bool m_showArchived{false};
    QString m_errorMessage;
};

} // namespace smartmate::viewmodel
