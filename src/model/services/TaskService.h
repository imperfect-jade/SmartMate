#pragma once

#include "domain/TaskCreationRequest.h"
#include "repositories/ITaskCreationRepository.h"
#include "repositories/ITaskDependencyRepository.h"
#include "repositories/ITaskRepository.h"
#include "services/TaskResult.h"

#include <QObject>

namespace smartmate::model {

/// 任务业务规则与持久化编排的唯一入口，不拥有注入的 Repository。
class TaskService final : public QObject {
    Q_OBJECT

public:
    TaskService(ITaskRepository &repository,
                ITaskDependencyRepository &dependencyRepository,
                ITaskCreationRepository &creationRepository,
                QObject *parent = nullptr);

    /// 执行无副作用的权威业务校验：不访问 Repository，也不发出状态通知。
    [[nodiscard]] TaskValidationResult validateDraft(const TaskDraft &draft) const;
    [[nodiscard]] TaskListResult listTasks() const;
    /// 返回新建表单可选的全部活动前置任务，排序稳定且不包含归档任务。
    [[nodiscard]] TaskListResult listEligibleCreationPredecessors() const;
    /// 读取任务快照并应用 Model 排序策略，不持久化随时间变化的推荐排名。
    [[nodiscard]] TaskPlanResult listRecommendedTasks() const;
    [[nodiscard]] TaskDependencyListResult listDependencies() const;
    /// 生成依赖图领域快照；只保留活动节点及与其处于同一依赖组件的归档节点。
    [[nodiscard]] TaskGraphResult taskGraphSnapshot() const;
    /// 原子替换活动 Todo 任务的全部前置；空列表清空关系，失败不会部分写入。
    [[nodiscard]] TaskDependencyListResult replaceTaskPredecessors(
        const TaskId &taskId,
        const QList<TaskId> &predecessorIds);
    [[nodiscard]] TaskResult findTask(const TaskId &id) const;
    /// 校验草稿、生成稳定 TaskId，并持久化新的领域快照。
    [[nodiscard]] TaskResult createTask(const TaskDraft &draft);
    /// 原子新建任务与全部前置关系；任意校验或写入失败均不发送状态通知。
    [[nodiscard]] TaskResult createTask(const TaskCreationRequest &request);
    /// 按稳定 TaskId 更新任务，同时保留创建时间和归档恢复点不变量。
    [[nodiscard]] TaskResult updateTask(const TaskId &id, const TaskDraft &draft);
    /// 按稳定 TaskId 执行软归档，不做物理删除。
    [[nodiscard]] TaskResult archiveTask(const TaskId &id);
    /// 恢复归档前状态；若目标状态为进行中，仍执行单进行中约束。
    [[nodiscard]] TaskResult restoreTask(const TaskId &id);

signals:
    /// 仅在一次实际写入成功后发出；失败或无写入的幂等操作不会通知。
    void tasksChanged();
    /// 仅在前置集合实际替换成功后发出一次。
    void dependenciesChanged();

private:
    // 非拥有引用，其生命周期必须长于 TaskService。
    ITaskRepository &m_repository;
    ITaskDependencyRepository &m_dependencyRepository;
    /// 独立命令端口保证跨 tasks 与 task_dependencies 的写入具有事务边界。
    ITaskCreationRepository &m_creationRepository;
};

} // namespace smartmate::model
