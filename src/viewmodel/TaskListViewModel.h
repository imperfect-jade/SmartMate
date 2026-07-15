#pragma once

#include "domain/Task.h"
#include "domain/TaskStateMachine.h"
#include "planner/TaskOrderingPolicy.h"
#include "TaskProjectionSources.h"
#include "viewmodel/contracts/TaskListContract.h"

#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVariantList>

namespace smartmate::model {
class TaskService;
class TaskCategoryService;
}

namespace smartmate::viewmodel {

/// 将领域任务集合投影成 Qt Widgets 可绑定的列表；它负责展示格式与命令转发，
/// 不拥有任务真相，也不直接访问 Repository。
/// Service 通知触发全量计划重载；搜索、筛选和批量选择只改变会话级可见投影。
class TaskListViewModel final : public TaskListContract {
    Q_OBJECT
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
public:
    TaskListViewModel(model::TaskService &taskService,
                      TaskPlanProjectionSource &planSource,
                      TaskCategoryProjectionSource &categorySource,
                      QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] int count() const noexcept override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool showArchived() const noexcept override;
    [[nodiscard]] QString searchText() const override;
    [[nodiscard]] int priorityFilterIndex() const noexcept override;
    [[nodiscard]] QStringList priorityFilterOptions() const override;
    [[nodiscard]] QVariantList categoryFilterOptions() const override;
    [[nodiscard]] int categoryFilterMode() const noexcept override;
    [[nodiscard]] QString categoryFilterCategoryId() const override;
    [[nodiscard]] bool hasActiveFilters() const override;
    [[nodiscard]] bool bulkSelectionMode() const noexcept override;
    [[nodiscard]] int bulkSelectedCount() const noexcept override;
    [[nodiscard]] int bulkSelectableVisibleCount() const override;
    [[nodiscard]] bool allVisibleSelected() const override;
    [[nodiscard]] bool canBulkArchive() const noexcept override;
    [[nodiscard]] bool canBulkRestore() const override;
    [[nodiscard]] bool canBulkDelete() const noexcept override;
    [[nodiscard]] QString errorMessage() const;
    void setShowArchived(bool showArchived) override;
    void setSearchText(const QString &searchText) override;
    void setPriorityFilterIndex(int priorityFilterIndex) override;
    /// mode: 0=全部，1=未分类，2=指定类别；指定类别始终使用稳定CategoryId。
    bool setCategoryFilter(int mode, const QString &categoryId = {}) override;

    /// 从Service重新获取快照并重建当前展示投影。
    void reload() override;
    /// 只清除关键字和优先级条件，活动/归档视图保持不变。
    void clearFilters() override;
    /// 请求 Todo → InProgress；阻塞与单进行中约束由 Model 最终判定。
    bool startTask(const QString &taskId) override;
    /// 请求 Todo/InProgress → Cancelled；取消确认由 View 负责。
    bool cancelTask(const QString &taskId) override;
    /// 请求 InProgress → Done，禁止绕过进行中状态。
    bool completeTask(const QString &taskId) override;
    /// 请求 Done/Cancelled → Todo，并由 Model 重新计算依赖有效性。
    bool redoTask(const QString &taskId) override;
    /// 按稳定TaskId请求软归档，并把领域错误映射为展示状态。
    bool archiveTask(const QString &taskId) override;
    /// 按稳定TaskId请求恢复，并保留Service给出的业务约束。
    bool restoreTask(const QString &taskId) override;
    /// 永久删除已归档任务；关联依赖由 Model 的原子删除端口统一清理。
    bool deleteArchivedTask(const QString &taskId) override;
    /// 进入会话级批量选择模式；此时不会自动选择任何任务。
    void beginBulkSelection() override;
    /// 按稳定 TaskId 切换选择；不具备当前批量操作资格的任务会被忽略。
    bool toggleBulkSelection(const QString &taskId) override;
    /// 选择或取消选择当前筛选结果中所有具备批量资格的任务。
    void toggleSelectAllVisible() override;
    /// 清空批量选择但保持批量模式。
    void clearBulkSelection() override;
    /// 清空选择并退出批量模式。
    void cancelBulkSelection() override;
    /// 将选中的已完成/已取消任务作为一个原子批次归档。
    bool archiveSelectedTasks() override;
    /// 将选中的归档任务作为一个原子批次恢复。
    bool restoreSelectedTasks() override;
    /// 将选中的归档任务及其关联依赖作为一个原子批次永久删除。
    bool deleteSelectedArchivedTasks() override;
    Q_INVOKABLE void clearError();

signals:
    void errorMessageChanged();
    void errorOccurred(const QString &message);

private:
    /// Model 依赖状态的界面投影；中文原因在 reload() 时一次生成，Widget 不拼接图数据。
    [[nodiscard]] static model::TaskId parseTaskId(const QString &taskId);
    [[nodiscard]] const model::Task *taskForId(const model::TaskId &taskId) const;
    [[nodiscard]] const model::TaskCommandAvailability &availabilityFor(
        const model::TaskId &taskId) const;
    [[nodiscard]] bool isBulkSelectable(const model::TaskId &taskId) const;
    [[nodiscard]] QList<model::TaskId> sortedBulkSelection() const;
    [[nodiscard]] QString taskIdsContext(const QList<model::TaskId> &taskIds) const;
    /// 删除刷新后已不存在、不可见或不再具备批量资格的稳定 ID。
    void pruneBulkSelection();
    /// 原子替换批量选择并只通知相关 Role/属性。
    void setBulkSelection(QSet<model::TaskId> selection);
    /// 将状态转换枚举映射到对应 Service 强类型命令并统一处理错误。
    bool performTransition(const QString &taskId, model::TaskTransition transition);
    /// 保留 Model 顺序应用会话筛选，并用模型重置协议替换可见行。
    void rebuildVisibleTasks();
    /// 去重错误属性与流程通知。
    void setError(const QString &message);
    void applyPlanProjection();
    void applyCategories();
    void syncSourceError();
    [[nodiscard]] const model::TaskCategory *categoryForTask(
        const model::Task *task) const;

    // Service 由组合根拥有；列表只保留非拥有引用并监听其变化通知。
    model::TaskService &m_taskService;
    TaskPlanProjectionSource &m_planSource;
    TaskCategoryProjectionSource &m_categorySource;
    /// 当前筛选后、仍保持 Model 推荐相对顺序的行快照。
    QList<model::Task> m_visibleTasks;
    // 当前搜索、筛选与活动/归档范围内的稳定ID集合，用于O(1)批量资格检查。
    QSet<model::TaskId> m_visibleTaskIds;
    // 批量选择只保存稳定 TaskId，且绝不写入持久化层。
    QSet<model::TaskId> m_bulkSelectedTaskIds;
    bool m_bulkSelectionMode{false};
    // 以下筛选字段只属于当前 ViewModel 会话，不写入 SQLite 或 QSettings。
    bool m_showArchived{false};
    QString m_searchText;
    // 0表示全部，1～4分别映射Low～Urgent；非法索引不会替换当前条件。
    int m_priorityFilterIndex{0};
    /// 0=全部、1=未分类、2=指定类别；筛选状态只存在于当前会话。
    int m_categoryFilterMode{0};
    model::TaskCategoryId m_categoryFilterCategoryId;
    QString m_errorMessage;
};

} // namespace smartmate::viewmodel
