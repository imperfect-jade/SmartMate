#pragma once

#include "services/TaskService.h"
#include "dependencies/TaskDependencyGraph.h"

#include <QDateTime>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <exception>
#include <optional>
#include <utility>

namespace smartmate::model {
namespace {

// 字段上限只由公开的 validateDraft 解释和执行，所有写入路径必须复用它。
[[nodiscard]] QString stableId(const TaskId &taskId)
{
    return taskId.toString(QUuid::WithoutBraces);
}

void normalizeIds(QList<TaskId> &taskIds)
{
    std::sort(taskIds.begin(), taskIds.end(), [](const TaskId &left,
                                                 const TaskId &right) {
        return stableId(left) < stableId(right);
    });
    taskIds.erase(std::unique(taskIds.begin(), taskIds.end()), taskIds.end());
}

[[nodiscard]] QList<TaskId> otherInProgressTaskIds(
    const QList<Task> &tasks,
    const std::optional<TaskId> &excludedId)
{
    QList<TaskId> result;
    for (const Task &task : tasks) {
        if (task.status() == TaskStatus::InProgress
            && (!excludedId.has_value() || task.id() != *excludedId)) {
            result.append(task.id());
        }
    }
    normalizeIds(result);
    return result;
}

[[nodiscard]] TaskResult persistenceFailure(const RepositoryException &exception)
{
    return TaskResult::failure(TaskError::PersistenceFailure,
                               QString::fromUtf8(exception.what()));
}

// 业务层先做快速预检，数据库唯一约束再防御多实例竞争；写失败后重读以恢复准确错误语义。
[[nodiscard]] TaskResult mapInProgressWriteFailure(
    ITaskRepository &repository,
    const TaskId &attemptedTaskId,
    const RepositoryException &writeFailure)
{
    try {
        const QList<TaskId> conflictingIds = otherInProgressTaskIds(
            repository.findAll(), std::optional<TaskId>{attemptedTaskId});
        if (!conflictingIds.isEmpty()) {
            return TaskResult::failure(
                TaskError::InProgressConflict,
                QStringLiteral("Another task became in progress before this write completed."),
                TaskErrorContext{{}, conflictingIds, {}});
        }
    } catch (...) {
        // 无法确认竞争时保留原写入错误，避免把普通持久化故障误报为状态冲突。
    }
    return persistenceFailure(writeFailure);
}

[[nodiscard]] TaskResult unexpectedPersistenceFailure()
{
    return TaskResult::failure(TaskError::PersistenceFailure,
                               QStringLiteral("Unexpected repository failure."));
}

[[nodiscard]] TaskBatchResult batchPersistenceFailure(
    const RepositoryException &exception)
{
    return TaskBatchResult::failure(TaskError::PersistenceFailure,
                                    QString::fromUtf8(exception.what()));
}

[[nodiscard]] TaskBatchResult unexpectedBatchPersistenceFailure()
{
    return TaskBatchResult::failure(
        TaskError::PersistenceFailure,
        QStringLiteral("Unexpected batch repository failure."));
}

[[nodiscard]] TaskListResult persistenceListFailure(const RepositoryException &exception)
{
    return TaskListResult::failure(TaskError::PersistenceFailure,
                                   QString::fromUtf8(exception.what()));
}

[[nodiscard]] TaskListResult unexpectedPersistenceListFailure()
{
    return TaskListResult::failure(TaskError::PersistenceFailure,
                                   QStringLiteral("Unexpected repository failure."));
}

[[nodiscard]] TaskPlanResult persistencePlanFailure(const RepositoryException &exception)
{
    return TaskPlanResult::failure(TaskError::PersistenceFailure,
                                   QString::fromUtf8(exception.what()));
}

[[nodiscard]] TaskPlanResult unexpectedPersistencePlanFailure()
{
    return TaskPlanResult::failure(TaskError::PersistenceFailure,
                                   QStringLiteral("Unexpected repository failure."));
}

[[nodiscard]] TaskGraphResult persistenceGraphFailure(
    const RepositoryException &exception)
{
    return TaskGraphResult::failure(TaskError::PersistenceFailure,
                                    QString::fromUtf8(exception.what()));
}

[[nodiscard]] TaskGraphResult unexpectedPersistenceGraphFailure()
{
    return TaskGraphResult::failure(TaskError::PersistenceFailure,
                                    QStringLiteral("Unexpected graph repository failure."));
}

[[nodiscard]] TaskDependencyListResult dependencyPersistenceFailure(
    const RepositoryException &exception)
{
    return TaskDependencyListResult::failure(
        TaskError::PersistenceFailure, QString::fromUtf8(exception.what()));
}

[[nodiscard]] TaskDependencyListResult unexpectedDependencyPersistenceFailure()
{
    return TaskDependencyListResult::failure(
        TaskError::PersistenceFailure,
        QStringLiteral("Unexpected dependency repository failure."));
}

[[nodiscard]] TaskError graphError(const DependencyGraphError error)
{
    switch (error) {
    case DependencyGraphError::None:
        return TaskError::None;
    case DependencyGraphError::MissingTask:
        return TaskError::DependencyEndpointNotFound;
    case DependencyGraphError::SelfDependency:
        return TaskError::DependencySelfReference;
    case DependencyGraphError::DuplicateDependency:
        return TaskError::DependencyDuplicate;
    case DependencyGraphError::Cycle:
        return TaskError::DependencyCycle;
    }
    return TaskError::DependencyCycle;
}

[[nodiscard]] TaskErrorContext graphContext(
    const DependencyGraphValidation &validation)
{
    return {{}, validation.conflictingTaskIds, validation.cyclePath};
}

template<typename T>
[[nodiscard]] ServiceResult<T> graphFailure(
    const DependencyGraphValidation &validation)
{
    return ServiceResult<T>::failure(
        graphError(validation.error),
        QStringLiteral("Task dependency graph validation failed."),
        graphContext(validation));
}

[[nodiscard]] const Task *findTaskInList(const QList<Task> &tasks,
                                         const TaskId &taskId)
{
    const auto iterator = std::find_if(tasks.cbegin(), tasks.cend(),
                                       [&taskId](const Task &task) {
                                           return task.id() == taskId;
                                       });
    return iterator == tasks.cend() ? nullptr : &*iterator;
}

/// 批量命令一次建立稳定TaskId到快照行的索引，避免任务较多时逐个线性查找退化为O(N²)。
[[nodiscard]] QHash<TaskId, qsizetype> taskIndexesById(
    const QList<Task> &tasks)
{
    QHash<TaskId, qsizetype> indexes;
    indexes.reserve(tasks.size());
    for (qsizetype index = 0; index < tasks.size(); ++index) {
        indexes.insert(tasks.at(index).id(), index);
    }
    return indexes;
}

[[nodiscard]] TaskResult singleTaskResult(TaskBatchResult batchResult,
                                          const TaskId &taskId)
{
    if (!batchResult.ok()) {
        return TaskResult::failure(batchResult.error,
                                   std::move(batchResult.detail),
                                   std::move(batchResult.context));
    }
    const Task *task = findTaskInList(batchResult.value->tasks, taskId);
    if (task == nullptr) {
        return TaskResult::failure(
            TaskError::PersistenceFailure,
            QStringLiteral("Successful batch result omitted the requested task."));
    }
    return TaskResult::success(*task);
}

void replaceTaskSnapshot(QList<Task> &tasks, const Task &replacement)
{
    const auto iterator = std::find_if(tasks.begin(), tasks.end(),
                                       [&replacement](const Task &task) {
                                           return task.id() == replacement.id();
                                       });
    if (iterator != tasks.end()) {
        *iterator = replacement;
    }
}

[[nodiscard]] TaskStatus effectiveStatus(const Task &task) noexcept
{
    if (task.status() == TaskStatus::Archived) {
        return task.statusBeforeArchive().value_or(TaskStatus::Todo);
    }
    return task.status();
}

[[nodiscard]] bool requiresSatisfiedPredecessors(const Task &task) noexcept
{
    const TaskStatus status = effectiveStatus(task);
    return status == TaskStatus::InProgress || status == TaskStatus::Done;
}

struct ProtectedDependencyViolation final {
    TaskId successorId;
    QList<TaskId> blockingTaskIds;
};

/// 检查当前以及归档前为 InProgress/Done 的后继，供读计划与所有写入路径复用。
[[nodiscard]] QList<ProtectedDependencyViolation> protectedStateViolations(
    const QList<Task> &tasks,
    const TaskDependencyGraph &graph)
{
    QList<ProtectedDependencyViolation> violations;
    for (const Task &task : tasks) {
        if (!requiresSatisfiedPredecessors(task)) {
            continue;
        }
        QList<TaskId> blockers = graph.unsatisfiedPredecessorIds(task.id());
        if (!blockers.isEmpty()) {
            violations.append({task.id(), std::move(blockers)});
        }
    }
    return violations;
}

[[nodiscard]] TaskErrorContext stateViolationContext(
    const QList<ProtectedDependencyViolation> &violations)
{
    QList<TaskId> blockingTaskIds;
    QList<TaskId> conflictingTaskIds;
    for (const ProtectedDependencyViolation &violation : violations) {
        blockingTaskIds.append(violation.blockingTaskIds);
        conflictingTaskIds.append(violation.successorId);
    }
    normalizeIds(blockingTaskIds);
    normalizeIds(conflictingTaskIds);
    return {blockingTaskIds, conflictingTaskIds, {}};
}

/// 写状态前验证“已开始或已完成的后继，其全部前置必须完成或取消”的领域不变量。
[[nodiscard]] std::optional<TaskResult> dependencyStateFailure(
    const QList<Task> &tasks,
    const QList<TaskDependency> &dependencies,
    const TaskId &changedTaskId)
{
    const TaskDependencyGraph graph{tasks, dependencies};
    const DependencyGraphValidation validation = graph.validation();
    if (!validation.ok()) {
        return graphFailure<Task>(validation);
    }

    QList<ProtectedDependencyViolation> changedTaskViolations;
    QList<ProtectedDependencyViolation> otherViolations;
    for (const ProtectedDependencyViolation &violation
         : protectedStateViolations(tasks, graph)) {
        if (violation.successorId == changedTaskId) {
            changedTaskViolations.append(violation);
        } else {
            otherViolations.append(violation);
        }
    }

    if (!changedTaskViolations.isEmpty()) {
        return TaskResult::failure(
            TaskError::TaskBlocked,
            QStringLiteral("A blocked task cannot enter or restore an active/completed state."),
            stateViolationContext(changedTaskViolations));
    }

    if (!otherViolations.isEmpty()) {
        return TaskResult::failure(
            TaskError::DependencyStateConflict,
            QStringLiteral("The status change would invalidate an active or completed successor."),
            stateViolationContext(otherViolations));
    }
    return std::nullopt;
}

/// 对批量转换后的最终快照统一检查依赖；同类冲突聚合后一次返回，结果不受输入顺序影响。
[[nodiscard]] std::optional<TaskBatchResult> batchDependencyStateFailure(
    const QList<Task> &tasks,
    const QList<TaskDependency> &dependencies,
    const QSet<TaskId> &changedTaskIds)
{
    const TaskDependencyGraph graph{tasks, dependencies};
    const DependencyGraphValidation validation = graph.validation();
    if (!validation.ok()) {
        return graphFailure<TaskBatchOutcome>(validation);
    }

    QList<ProtectedDependencyViolation> changedTaskViolations;
    QList<ProtectedDependencyViolation> otherViolations;
    for (const ProtectedDependencyViolation &violation
         : protectedStateViolations(tasks, graph)) {
        if (changedTaskIds.contains(violation.successorId)) {
            changedTaskViolations.append(violation);
        } else {
            otherViolations.append(violation);
        }
    }
    if (!changedTaskViolations.isEmpty()) {
        return TaskBatchResult::failure(
            TaskError::TaskBlocked,
            QStringLiteral("One or more restored tasks are blocked by dependencies."),
            stateViolationContext(changedTaskViolations));
    }
    if (!otherViolations.isEmpty()) {
        return TaskBatchResult::failure(
            TaskError::DependencyStateConflict,
            QStringLiteral("The batch status change would invalidate protected successors."),
            stateViolationContext(otherViolations));
    }
    return std::nullopt;
}

[[nodiscard]] QList<TaskDependency> dependenciesForSuccessor(
    const QList<TaskDependency> &dependencies,
    const TaskId &successorId)
{
    QList<TaskDependency> result;
    for (const TaskDependency &dependency : dependencies) {
        if (dependency.successorId == successorId) {
            result.append(dependency);
        }
    }
    std::sort(result.begin(), result.end(), [](const TaskDependency &left,
                                               const TaskDependency &right) {
        return stableId(left.predecessorId) < stableId(right.predecessorId);
    });
    return result;
}

[[nodiscard]] Task makeTaskWithDetails(const Task &source,
                                       const TaskDraft &draft,
                                       QDateTime updatedAtUtc)
{
    // 普通编辑只替换详情；状态与归档恢复点必须原样保留，避免绕过状态机。
    const std::optional<QDateTime> deadline = draft.deadline.has_value()
        ? std::optional<QDateTime>{draft.deadline->toUTC()}
        : std::nullopt;
    return Task{source.id(),
                draft.title.trimmed(),
                draft.description,
                draft.priority,
                source.status(),
                source.statusBeforeArchive(),
                deadline,
                draft.estimatedMinutes,
                source.createdAtUtc(),
                std::move(updatedAtUtc)};
}

[[nodiscard]] Task makeTaskWithStatus(
    const Task &source,
    const TaskStatus status,
    std::optional<TaskStatus> statusBeforeArchive,
    QDateTime updatedAtUtc)
{
    // 状态命令不得意外改写用户详情，尤其要保留旧截止时间中的秒和毫秒。
    return Task{source.id(),
                source.title(),
                source.description(),
                source.priority(),
                status,
                std::move(statusBeforeArchive),
                source.deadline(),
                source.estimatedMinutes(),
                source.createdAtUtc(),
                std::move(updatedAtUtc)};
}

[[nodiscard]] Task makeNewTask(const TaskId &taskId,
                               const TaskDraft &draft,
                               const QDateTime &nowUtc)
{
    const std::optional<QDateTime> deadline = draft.deadline.has_value()
        ? std::optional<QDateTime>{draft.deadline->toUTC()}
        : std::nullopt;
    // 新任务只能从 Todo 开始；后续状态变化必须经过显式状态命令。
    return Task{taskId,
                draft.title.trimmed(),
                draft.description,
                draft.priority,
                TaskStatus::Todo,
                std::nullopt,
                deadline,
                draft.estimatedMinutes,
                nowUtc,
                nowUtc};
}

} // namespace

} // namespace smartmate::model
