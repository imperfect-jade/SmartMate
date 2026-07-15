#include "services/internal/TaskServiceSnapshotSupport.h"

#include "services/internal/TaskServiceResultSupport.h"

#include <algorithm>
#include <utility>

namespace smartmate::model::task_service_detail {

QString stableId(const TaskId &taskId)
{
    return taskId.toString(QUuid::WithoutBraces);
}

namespace {

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

} // namespace

void normalizeIds(QList<TaskId> &taskIds)
{
    std::sort(taskIds.begin(), taskIds.end(), [](const TaskId &left,
                                                 const TaskId &right) {
        return stableId(left) < stableId(right);
    });
    taskIds.erase(std::unique(taskIds.begin(), taskIds.end()), taskIds.end());
}

QList<TaskId> otherInProgressTaskIds(
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

const Task *findTaskInList(const QList<Task> &tasks, const TaskId &taskId)
{
    const auto iterator = std::find_if(
        tasks.cbegin(), tasks.cend(), [&taskId](const Task &task) {
            return task.id() == taskId;
        });
    return iterator == tasks.cend() ? nullptr : &*iterator;
}

QHash<TaskId, qsizetype> taskIndexesById(const QList<Task> &tasks)
{
    QHash<TaskId, qsizetype> indexes;
    indexes.reserve(tasks.size());
    for (qsizetype index = 0; index < tasks.size(); ++index) {
        indexes.insert(tasks.at(index).id(), index);
    }
    return indexes;
}

TaskResult singleTaskResult(TaskBatchResult batchResult, const TaskId &taskId)
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
    const auto iterator = std::find_if(
        tasks.begin(), tasks.end(), [&replacement](const Task &task) {
            return task.id() == replacement.id();
        });
    if (iterator != tasks.end()) {
        *iterator = replacement;
    }
}

QList<ProtectedDependencyViolation> protectedStateViolations(
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

TaskErrorContext stateViolationContext(
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

std::optional<TaskResult> dependencyStateFailure(
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

std::optional<TaskBatchResult> batchDependencyStateFailure(
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

QList<TaskDependency> dependenciesForSuccessor(
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

Task makeTaskWithDetails(const Task &source,
                         const TaskDraft &draft,
                         QDateTime updatedAtUtc)
{
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
                std::move(updatedAtUtc),
                draft.categoryId};
}

Task makeTaskWithStatus(const Task &source,
                        const TaskStatus status,
                        std::optional<TaskStatus> statusBeforeArchive,
                        QDateTime updatedAtUtc)
{
    return Task{source.id(),
                source.title(),
                source.description(),
                source.priority(),
                status,
                std::move(statusBeforeArchive),
                source.deadline(),
                source.estimatedMinutes(),
                source.createdAtUtc(),
                std::move(updatedAtUtc),
                source.categoryId()};
}

Task makeNewTask(const TaskId &taskId,
                 const TaskDraft &draft,
                 const QDateTime &nowUtc)
{
    const std::optional<QDateTime> deadline = draft.deadline.has_value()
        ? std::optional<QDateTime>{draft.deadline->toUTC()}
        : std::nullopt;
    return Task{taskId,
                draft.title.trimmed(),
                draft.description,
                draft.priority,
                TaskStatus::Todo,
                std::nullopt,
                deadline,
                draft.estimatedMinutes,
                nowUtc,
                nowUtc,
                draft.categoryId};
}

} // namespace smartmate::model::task_service_detail
