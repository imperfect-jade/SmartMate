#pragma once

#include "domain/Task.h"
#include "domain/TaskStateMachine.h"
#include "planner/TaskOrderingPolicy.h"

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
    Q_PROPERTY(FocusState focusState READ focusState NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusTaskId READ focusTaskId NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusTitle READ focusTitle NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusDescription READ focusDescription NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusStatusText READ focusStatusText NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusPriorityText READ focusPriorityText NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusDeadlineText READ focusDeadlineText NOTIFY focusTaskChanged)
    Q_PROPERTY(int focusEstimatedMinutes READ focusEstimatedMinutes NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusReasonText READ focusReasonText NOTIFY focusTaskChanged)
    Q_PROPERTY(bool focusCanStart READ focusCanStart NOTIFY focusTaskChanged)
    Q_PROPERTY(bool focusCanComplete READ focusCanComplete NOTIFY focusTaskChanged)
    Q_PROPERTY(QString selectedTaskId READ selectedTaskId NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedTitle READ selectedTitle NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedDescription READ selectedDescription NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedStatusText READ selectedStatusText NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedPriorityText READ selectedPriorityText NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedDeadlineText READ selectedDeadlineText NOTIFY selectionChanged)
    Q_PROPERTY(int selectedEstimatedMinutes READ selectedEstimatedMinutes NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedCreatedAtText READ selectedCreatedAtText NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedUpdatedAtText READ selectedUpdatedAtText NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedReasonText READ selectedReasonText NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedBlockingReasonText READ selectedBlockingReasonText
                   NOTIFY selectionChanged)
    Q_PROPERTY(int selectedPredecessorCount READ selectedPredecessorCount NOTIFY selectionChanged)
    Q_PROPERTY(int selectedUnlockCount READ selectedUnlockCount NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedCanEditTask READ selectedCanEditTask NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedCanEditDependencies READ selectedCanEditDependencies
                   NOTIFY selectionChanged)
    QML_NAMED_ELEMENT(TaskListViewModel)
    QML_UNCREATABLE("TaskListViewModel is owned by AppViewModel")

public:
    enum class FocusState {
        NoTasks = 0,
        Suggested = 1,
        InProgress = 2,
        AllBlocked = 3,
    };
    Q_ENUM(FocusState)

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
        BlockedRole,
        BlockingReasonTextRole,
        PredecessorCountRole,
        UnlockCountRole,
        CanEditTaskRole,
        CanEditDependenciesRole,
        CanStartRole,
        CanCancelRole,
        CanCompleteRole,
        CanRedoRole,
        CanArchiveRole,
        CanRestoreRole,
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
    [[nodiscard]] FocusState focusState() const noexcept;
    [[nodiscard]] QString focusTaskId() const;
    [[nodiscard]] QString focusTitle() const;
    [[nodiscard]] QString focusDescription() const;
    [[nodiscard]] QString focusStatusText() const;
    [[nodiscard]] QString focusPriorityText() const;
    [[nodiscard]] QString focusDeadlineText() const;
    [[nodiscard]] int focusEstimatedMinutes() const noexcept;
    [[nodiscard]] QString focusReasonText() const;
    [[nodiscard]] bool focusCanStart() const noexcept;
    [[nodiscard]] bool focusCanComplete() const noexcept;
    [[nodiscard]] QString selectedTaskId() const;
    [[nodiscard]] QString selectedTitle() const;
    [[nodiscard]] QString selectedDescription() const;
    [[nodiscard]] QString selectedStatusText() const;
    [[nodiscard]] QString selectedPriorityText() const;
    [[nodiscard]] QString selectedDeadlineText() const;
    [[nodiscard]] int selectedEstimatedMinutes() const noexcept;
    [[nodiscard]] QString selectedCreatedAtText() const;
    [[nodiscard]] QString selectedUpdatedAtText() const;
    [[nodiscard]] QString selectedReasonText() const;
    [[nodiscard]] QString selectedBlockingReasonText() const;
    [[nodiscard]] int selectedPredecessorCount() const noexcept;
    [[nodiscard]] int selectedUnlockCount() const noexcept;
    [[nodiscard]] bool selectedCanEditTask() const noexcept;
    [[nodiscard]] bool selectedCanEditDependencies() const noexcept;
    void setShowArchived(bool showArchived);
    void setSearchText(const QString &searchText);
    void setPriorityFilterIndex(int priorityFilterIndex);

    /// 从Service重新获取快照并重建当前展示投影。
    Q_INVOKABLE void reload();
    /// 只清除关键字和优先级条件，活动/归档视图保持不变。
    Q_INVOKABLE void clearFilters();
    /// 请求 Todo → InProgress；阻塞与单进行中约束由 Model 最终判定。
    Q_INVOKABLE bool startTask(const QString &taskId);
    /// 请求 Todo/InProgress → Cancelled；取消确认由 View 负责。
    Q_INVOKABLE bool cancelTask(const QString &taskId);
    /// 请求 InProgress → Done，禁止绕过进行中状态。
    Q_INVOKABLE bool completeTask(const QString &taskId);
    /// 请求 Done/Cancelled → Todo，并由 Model 重新计算依赖有效性。
    Q_INVOKABLE bool redoTask(const QString &taskId);
    /// 按稳定TaskId请求软归档，并把领域错误映射为展示状态。
    Q_INVOKABLE bool archiveTask(const QString &taskId);
    /// 按稳定TaskId请求恢复，并保留Service给出的业务约束。
    Q_INVOKABLE bool restoreTask(const QString &taskId);
    Q_INVOKABLE void clearError();
    Q_INVOKABLE bool selectTask(const QString &taskId);
    Q_INVOKABLE void clearSelection();

signals:
    void showArchivedChanged();
    void searchTextChanged();
    void priorityFilterIndexChanged();
    void hasActiveFiltersChanged();
    void countChanged();
    void errorMessageChanged();
    void errorOccurred(const QString &message);
    void focusTaskChanged();
    void selectionChanged();

private:
    /// Model 依赖状态的界面投影；中文原因在 reload() 时一次生成，QML 不拼接图数据。
    struct DependencyProjection {
        bool blocked{false};
        QString blockingReasonText;
        int predecessorCount{0};
        int unlockCount{0};

        bool operator==(const DependencyProjection &) const = default;
    };

    [[nodiscard]] static QString statusText(model::TaskStatus status);
    [[nodiscard]] static QString priorityText(model::TaskPriority priority);
    [[nodiscard]] static model::TaskId parseTaskId(const QString &taskId);
    [[nodiscard]] const model::Task *taskForId(const model::TaskId &taskId) const;
    [[nodiscard]] const model::Task *focusTask() const;
    [[nodiscard]] const model::Task *selectedTask() const;
    [[nodiscard]] const model::TaskCommandAvailability &availabilityFor(
        const model::TaskId &taskId) const;
    void rebuildFocusTask();
    bool performTransition(const QString &taskId, model::TaskTransition transition);
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
    QHash<model::TaskId, DependencyProjection> m_dependencyProjections;
    QHash<model::TaskId, model::TaskCommandAvailability> m_availabilities;
    model::TaskId m_focusTaskId;
    FocusState m_focusState{FocusState::NoTasks};
    model::TaskId m_selectedTaskId;
    bool m_showArchived{false};
    QString m_searchText;
    // 0表示全部，1～4分别映射Low～Urgent；非法索引不会替换当前条件。
    int m_priorityFilterIndex{0};
    QString m_errorMessage;
};

} // namespace smartmate::viewmodel
