#pragma once

#include "domain/Task.h"

#include <QAbstractListModel>
#include <QHash>
#include <QStringList>
#include <QTimer>
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
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(int priorityFilterIndex READ priorityFilterIndex WRITE setPriorityFilterIndex
                   NOTIFY priorityFilterIndexChanged)
    Q_PROPERTY(QStringList priorityFilterOptions READ priorityFilterOptions CONSTANT)
    Q_PROPERTY(bool hasActiveFilters READ hasActiveFilters NOTIFY hasActiveFiltersChanged)
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
        OrderReasonTextRole,
    };
    Q_ENUM(Role)

    explicit TaskListViewModel(model::TaskService &taskService, QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] int count() const noexcept;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool showArchived() const noexcept;
    [[nodiscard]] QString searchText() const;
    [[nodiscard]] int priorityFilterIndex() const noexcept;
    [[nodiscard]] QStringList priorityFilterOptions() const;
    [[nodiscard]] bool hasActiveFilters() const;
    [[nodiscard]] QString errorMessage() const;
    void setShowArchived(bool showArchived);
    void setSearchText(const QString &searchText);
    void setPriorityFilterIndex(int priorityFilterIndex);

    /// 从Service重新获取快照并重建当前展示投影。
    Q_INVOKABLE void reload();
    /// 只清除关键字和优先级条件，活动/归档视图保持不变。
    Q_INVOKABLE void clearFilters();
    /// 按稳定TaskId请求软归档，并把领域错误映射为展示状态。
    Q_INVOKABLE bool archiveTask(const QString &taskId);
    /// 按稳定TaskId请求恢复，并保留Service给出的业务约束。
    Q_INVOKABLE bool restoreTask(const QString &taskId);
    Q_INVOKABLE void clearError();

signals:
    void showArchivedChanged();
    void searchTextChanged();
    void priorityFilterIndexChanged();
    void hasActiveFiltersChanged();
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
    // 每分钟重新请求Model计划，使“已逾期”等随时间变化的推荐理由及时刷新。
    QTimer m_reloadTimer;
    // 全量计划顺序与当前可见投影分离，搜索和筛选不会修改领域数据。
    QList<model::Task> m_allTasks;
    QList<model::Task> m_visibleTasks;
    QHash<model::TaskId, QString> m_orderReasonTexts;
    bool m_showArchived{false};
    QString m_searchText;
    // 0表示全部，1～4分别映射Low～Urgent；非法索引不会替换当前条件。
    int m_priorityFilterIndex{0};
    QString m_errorMessage;
};

} // namespace smartmate::viewmodel
