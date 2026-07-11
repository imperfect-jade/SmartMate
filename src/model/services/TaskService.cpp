#include "services/TaskService.h"

#include "dependencies/TaskDependencyGraph.h"
#include "domain/TaskConstraints.h"
#include "planner/TaskOrderingPolicy.h"

#include <QDateTime>
#include <QSet>

#include <algorithm>
#include <exception>
#include <optional>
#include <utility>

namespace smartmate::model {
namespace {

// 字段上限只由公开的 validateDraft 解释和执行，所有写入路径必须复用它。
constexpr int maximumTitleLength = 200;
constexpr int maximumDescriptionLength = 5000;

[[nodiscard]] bool isValidPriority(TaskPriority priority) noexcept
{
    switch (priority) {
    case TaskPriority::Low:
    case TaskPriority::Normal:
    case TaskPriority::High:
    case TaskPriority::Urgent:
        return true;
    }
    return false;
}

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

TaskService::TaskService(ITaskRepository &repository,
                         ITaskDependencyRepository &dependencyRepository,
                         ITaskCreationRepository &creationRepository,
                         QObject *parent)
    : QObject(parent)
    , m_repository(repository)
    , m_dependencyRepository(dependencyRepository)
    , m_creationRepository(creationRepository)
{
}

TaskValidationResult TaskService::validateDraft(const TaskDraft &draft) const
{
    const QString normalizedTitle = draft.title.trimmed();
    if (normalizedTitle.isEmpty()) {
        return TaskValidationResult::failure(
            TaskError::EmptyTitle, QStringLiteral("Task title must not be empty."));
    }
    if (normalizedTitle.size() > maximumTitleLength) {
        return TaskValidationResult::failure(
            TaskError::TitleTooLong,
            QStringLiteral("Task title exceeds 200 characters."));
    }
    if (draft.description.size() > maximumDescriptionLength) {
        return TaskValidationResult::failure(
            TaskError::DescriptionTooLong,
            QStringLiteral("Task description exceeds 5000 characters."));
    }
    if (draft.deadline.has_value() && !draft.deadline->isValid()) {
        return TaskValidationResult::failure(
            TaskError::InvalidDeadline, QStringLiteral("Task deadline is invalid."));
    }
    if (draft.estimatedMinutes.has_value()
        && (*draft.estimatedMinutes < TaskConstraints::minimumEstimatedMinutes
            || *draft.estimatedMinutes > TaskConstraints::maximumEstimatedMinutes)) {
        return TaskValidationResult::failure(
            TaskError::InvalidEstimate,
            QStringLiteral("Estimated minutes must be between 1 and 525600."));
    }
    if (!isValidPriority(draft.priority)) {
        return TaskValidationResult::failure(
            TaskError::InvalidPriority, QStringLiteral("Task priority is invalid."));
    }
    return TaskValidationResult::success();
}

TaskListResult TaskService::listTasks() const
{
    try {
        return TaskListResult::success(m_repository.findAll());
    } catch (const RepositoryException &exception) {
        return persistenceListFailure(exception);
    } catch (...) {
        return unexpectedPersistenceListFailure();
    }
}

TaskListResult TaskService::listEligibleCreationPredecessors() const
{
    try {
        const QList<Task> tasks = m_repository.findAll();
        const QList<PlannedTask> recommended = orderTasks(
            tasks, QDateTime::currentDateTimeUtc());
        QList<Task> eligibleTasks;
        eligibleTasks.reserve(recommended.size());
        for (const PlannedTask &plannedTask : recommended) {
            if (plannedTask.task.status() != TaskStatus::Archived
                && plannedTask.task.status() != TaskStatus::Cancelled) {
                eligibleTasks.append(plannedTask.task);
            }
        }
        return TaskListResult::success(std::move(eligibleTasks));
    } catch (const RepositoryException &exception) {
        return persistenceListFailure(exception);
    } catch (...) {
        return unexpectedPersistenceListFailure();
    }
}

TaskPlanResult TaskService::listRecommendedTasks() const
{
    try {
        const QList<Task> tasks = m_repository.findAll();
        const QList<TaskDependency> dependencies =
            m_dependencyRepository.findAllDependencies();
        const TaskDependencyGraph graph{tasks, dependencies};
        const DependencyGraphValidation validation = graph.validation();
        if (!validation.ok()) {
            return graphFailure<QList<PlannedTask>>(validation);
        }
        const QList<ProtectedDependencyViolation> stateViolations =
            protectedStateViolations(tasks, graph);
        if (!stateViolations.isEmpty()) {
            return TaskPlanResult::failure(
                TaskError::DependencyStateConflict,
                QStringLiteral("Stored task dependencies violate an active/completed state."),
                stateViolationContext(stateViolations));
        }
        QList<PlannedTask> plan = orderTasks(
            tasks, dependencies, QDateTime::currentDateTimeUtc());
        const bool hasInProgress = std::any_of(
            tasks.cbegin(), tasks.cend(), [](const Task &task) {
                return task.status() == TaskStatus::InProgress;
            });
        for (PlannedTask &planned : plan) {
            const Task &task = planned.task;
            auto &availability = planned.availability;
            availability.canEditTask = task.canEditDetails();
            availability.canEditDependencies = task.status() == TaskStatus::Todo;
            availability.canStart = TaskStateMachine::canApply(
                                            task, TaskTransition::Start)
                && !planned.dependencyState.blocked && !hasInProgress;
            availability.canCancel = TaskStateMachine::canApply(
                task, TaskTransition::Cancel);
            availability.canComplete = TaskStateMachine::canApply(
                                               task, TaskTransition::Complete)
                && !planned.dependencyState.blocked;
            availability.canArchive = TaskStateMachine::canApply(
                task, TaskTransition::Archive);

            const auto canApplyWithoutDependencyConflict =
                [&](const TaskTransition transition) {
                    const auto target = TaskStateMachine::targetStatus(task, transition);
                    if (!target.has_value()) {
                        return false;
                    }
                    QList<Task> hypothetical = tasks;
                    replaceTaskSnapshot(
                        hypothetical,
                        makeTaskWithStatus(task, *target, std::nullopt,
                                           task.updatedAtUtc()));
                    return !dependencyStateFailure(
                        hypothetical, dependencies, task.id()).has_value();
                };
            availability.canRedo = canApplyWithoutDependencyConflict(
                TaskTransition::Redo);
            availability.canRestore = canApplyWithoutDependencyConflict(
                TaskTransition::Restore);
        }
        return TaskPlanResult::success(std::move(plan));
    } catch (const RepositoryException &exception) {
        return persistencePlanFailure(exception);
    } catch (...) {
        return unexpectedPersistencePlanFailure();
    }
}

TaskDependencyListResult TaskService::listDependencies() const
{
    try {
        return TaskDependencyListResult::success(
            m_dependencyRepository.findAllDependencies());
    } catch (const RepositoryException &exception) {
        return dependencyPersistenceFailure(exception);
    } catch (...) {
        return unexpectedDependencyPersistenceFailure();
    }
}

TaskGraphResult TaskService::taskGraphSnapshot() const
{
    try {
        const QList<Task> tasks = m_repository.findAll();
        const QList<TaskDependency> dependencies =
            m_dependencyRepository.findAllDependencies();
        const TaskDependencyGraph graph{tasks, dependencies};
        const DependencyGraphValidation validation = graph.validation();
        if (!validation.ok()) {
            return graphFailure<TaskGraphSnapshot>(validation);
        }

        const QList<ProtectedDependencyViolation> stateViolations =
            protectedStateViolations(tasks, graph);
        if (!stateViolations.isEmpty()) {
            return TaskGraphResult::failure(
                TaskError::DependencyStateConflict,
                QStringLiteral("Stored task dependencies violate an active/completed state."),
                stateViolationContext(stateViolations));
        }

        QList<TaskId> activeTaskIds;
        for (const Task &task : tasks) {
            if (task.status() != TaskStatus::Archived) {
                activeTaskIds.append(task.id());
            }
        }
        const QList<TaskId> connectedIds = graph.connectedTaskIds(activeTaskIds);
        const QSet<TaskId> visibleIds(connectedIds.cbegin(), connectedIds.cend());
        const QHash<TaskId, int> levels = graph.dependencyLevels();

        TaskGraphSnapshot snapshot;
        const QList<PlannedTask> recommended = orderTasks(
            tasks, dependencies, QDateTime::currentDateTimeUtc());
        snapshot.nodes.reserve(visibleIds.size());
        for (const PlannedTask &plannedTask : recommended) {
            if (!visibleIds.contains(plannedTask.task.id())) {
                continue;
            }
            snapshot.nodes.append({plannedTask.task,
                                   plannedTask.dependencyState,
                                   levels.value(plannedTask.task.id(), 0)});
        }

        QList<TaskDependency> visibleDependencies;
        for (const TaskDependency &dependency : dependencies) {
            if (visibleIds.contains(dependency.predecessorId)
                && visibleIds.contains(dependency.successorId)) {
                visibleDependencies.append(dependency);
            }
        }
        std::sort(visibleDependencies.begin(), visibleDependencies.end(),
                  [](const TaskDependency &left,
                     const TaskDependency &right) {
            const QString leftPredecessor = stableId(left.predecessorId);
            const QString rightPredecessor = stableId(right.predecessorId);
            if (leftPredecessor != rightPredecessor) {
                return leftPredecessor < rightPredecessor;
            }
            return stableId(left.successorId) < stableId(right.successorId);
        });
        snapshot.edges.reserve(visibleDependencies.size());
        for (const TaskDependency &dependency : visibleDependencies) {
            const Task *predecessor = findTaskInList(
                tasks, dependency.predecessorId);
            snapshot.edges.append(
                {dependency,
                 predecessor != nullptr
                     ? TaskDependencyGraph::dependencyResolution(*predecessor)
                     : TaskDependencyResolution::Pending});
        }
        return TaskGraphResult::success(std::move(snapshot));
    } catch (const RepositoryException &exception) {
        return persistenceGraphFailure(exception);
    } catch (...) {
        return unexpectedPersistenceGraphFailure();
    }
}

TaskDependencyListResult TaskService::replaceTaskPredecessors(
    const TaskId &taskId,
    const QList<TaskId> &predecessorIds)
{
    try {
        const QList<Task> tasks = m_repository.findAll();
        const Task *target = findTaskInList(tasks, taskId);
        if (target == nullptr) {
            return TaskDependencyListResult::failure(
                TaskError::DependencyEndpointNotFound,
                QStringLiteral("Dependency target task was not found."),
                TaskErrorContext{{}, {taskId}, {}});
        }
        if (target->status() != TaskStatus::Todo) {
            return TaskDependencyListResult::failure(
                TaskError::DependencyTargetNotEditable,
                QStringLiteral("Only an active Todo task can replace predecessors."),
                TaskErrorContext{{}, {taskId}, {}});
        }

        QList<TaskId> normalizedPredecessors = predecessorIds;
        normalizeIds(normalizedPredecessors);
        if (normalizedPredecessors.size() != predecessorIds.size()) {
            QList<TaskId> duplicateIds;
            QSet<TaskId> seenIds;
            for (const TaskId &predecessorId : predecessorIds) {
                if (seenIds.contains(predecessorId)) {
                    duplicateIds.append(predecessorId);
                } else {
                    seenIds.insert(predecessorId);
                }
            }
            normalizeIds(duplicateIds);
            return TaskDependencyListResult::failure(
                TaskError::DependencyDuplicate,
                QStringLiteral("Task predecessor list contains duplicates."),
                TaskErrorContext{{}, duplicateIds, {}});
        }
        if (normalizedPredecessors.contains(taskId)) {
            return TaskDependencyListResult::failure(
                TaskError::DependencySelfReference,
                QStringLiteral("A task cannot depend on itself."),
                TaskErrorContext{{}, {taskId}, {}});
        }

        QList<TaskId> missingIds;
        for (const TaskId &predecessorId : normalizedPredecessors) {
            if (findTaskInList(tasks, predecessorId) == nullptr) {
                missingIds.append(predecessorId);
            }
        }
        normalizeIds(missingIds);
        if (!missingIds.isEmpty()) {
            return TaskDependencyListResult::failure(
                TaskError::DependencyEndpointNotFound,
                QStringLiteral("One or more predecessor tasks were not found."),
                TaskErrorContext{{}, missingIds, {}});
        }

        const QList<TaskDependency> currentDependencies =
            m_dependencyRepository.findAllDependencies();
        const QList<TaskDependency> currentIncoming =
            dependenciesForSuccessor(currentDependencies, taskId);
        QList<TaskId> currentPredecessors;
        currentPredecessors.reserve(currentIncoming.size());
        for (const TaskDependency &dependency : currentIncoming) {
            currentPredecessors.append(dependency.predecessorId);
        }

        QList<TaskId> ineligibleIds;
        for (const TaskId &predecessorId : normalizedPredecessors) {
            const Task *predecessor = findTaskInList(tasks, predecessorId);
            if (!currentPredecessors.contains(predecessorId)
                && predecessor != nullptr
                && (predecessor->status() == TaskStatus::Archived
                    || predecessor->status() == TaskStatus::Cancelled)) {
                ineligibleIds.append(predecessorId);
            }
        }
        normalizeIds(ineligibleIds);
        if (!ineligibleIds.isEmpty()) {
            return TaskDependencyListResult::failure(
                TaskError::DependencyPredecessorNotEligible,
                QStringLiteral("A newly selected predecessor must not be archived or cancelled."),
                TaskErrorContext{{}, ineligibleIds, {}});
        }

        QList<TaskDependency> replacementDependencies;
        replacementDependencies.reserve(
            currentDependencies.size() - currentIncoming.size()
            + normalizedPredecessors.size());
        for (const TaskDependency &dependency : currentDependencies) {
            if (dependency.successorId != taskId) {
                replacementDependencies.append(dependency);
            }
        }
        for (const TaskId &predecessorId : normalizedPredecessors) {
            replacementDependencies.append({predecessorId, taskId});
        }

        const DependencyGraphValidation validation =
            TaskDependencyGraph{tasks, replacementDependencies}.validation();
        if (!validation.ok()) {
            return graphFailure<QList<TaskDependency>>(validation);
        }

        if (currentPredecessors == normalizedPredecessors) {
            return TaskDependencyListResult::success(currentIncoming);
        }

        m_dependencyRepository.replacePredecessors(taskId, normalizedPredecessors);
        QList<TaskDependency> replacedIncoming;
        replacedIncoming.reserve(normalizedPredecessors.size());
        for (const TaskId &predecessorId : normalizedPredecessors) {
            replacedIncoming.append({predecessorId, taskId});
        }
        emit dependenciesChanged();
        return TaskDependencyListResult::success(std::move(replacedIncoming));
    } catch (const RepositoryException &exception) {
        return dependencyPersistenceFailure(exception);
    } catch (...) {
        return unexpectedDependencyPersistenceFailure();
    }
}

TaskResult TaskService::findTask(const TaskId &id) const
{
    try {
        const std::optional<Task> task = m_repository.findById(id);
        if (!task.has_value()) {
            return TaskResult::failure(TaskError::NotFound,
                                       QStringLiteral("Task was not found."));
        }
        return TaskResult::success(*task);
    } catch (const RepositoryException &exception) {
        return persistenceFailure(exception);
    } catch (...) {
        return unexpectedPersistenceFailure();
    }
}

TaskResult TaskService::findEditableTask(const TaskId &id) const
{
    TaskResult result = findTask(id);
    if (!result.ok() || result.value->canEditDetails()) {
        return result;
    }
    return TaskResult::failure(
        TaskError::ArchivedTaskNotEditable,
        QStringLiteral("An archived task must be restored before editing."),
        TaskErrorContext{{}, {id}, {}});
}

TaskResult TaskService::createTask(const TaskDraft &draft)
{
    return createTask(TaskCreationRequest{draft, {}});
}

TaskResult TaskService::createTask(const TaskCreationRequest &request)
{
    const TaskValidationResult validation = validateDraft(request.task);
    if (!validation.ok()) {
        return TaskResult::failure(validation.error, validation.detail);
    }

    try {
        const QList<Task> tasks = m_repository.findAll();
        QList<TaskId> normalizedPredecessors = request.predecessorIds;
        normalizeIds(normalizedPredecessors);
        if (normalizedPredecessors.size() != request.predecessorIds.size()) {
            QList<TaskId> duplicateIds;
            QSet<TaskId> seenIds;
            for (const TaskId &predecessorId : request.predecessorIds) {
                if (seenIds.contains(predecessorId)) {
                    duplicateIds.append(predecessorId);
                } else {
                    seenIds.insert(predecessorId);
                }
            }
            normalizeIds(duplicateIds);
            return TaskResult::failure(
                TaskError::DependencyDuplicate,
                QStringLiteral("Task predecessor list contains duplicates."),
                TaskErrorContext{{}, duplicateIds, {}});
        }

        QList<TaskId> missingIds;
        QList<TaskId> ineligibleIds;
        for (const TaskId &predecessorId : normalizedPredecessors) {
            const Task *predecessor = findTaskInList(tasks, predecessorId);
            if (predecessor == nullptr) {
                missingIds.append(predecessorId);
            } else if (predecessor->status() == TaskStatus::Archived
                       || predecessor->status() == TaskStatus::Cancelled) {
                ineligibleIds.append(predecessorId);
            }
        }
        normalizeIds(missingIds);
        if (!missingIds.isEmpty()) {
            return TaskResult::failure(
                TaskError::DependencyEndpointNotFound,
                QStringLiteral("One or more creation predecessors were not found."),
                TaskErrorContext{{}, missingIds, {}});
        }
        normalizeIds(ineligibleIds);
        if (!ineligibleIds.isEmpty()) {
            return TaskResult::failure(
                TaskError::DependencyPredecessorNotEligible,
                QStringLiteral("A creation predecessor must not be archived or cancelled."),
                TaskErrorContext{{}, ineligibleIds, {}});
        }

        TaskId taskId;
        do {
            taskId = QUuid::createUuid();
        } while (findTaskInList(tasks, taskId) != nullptr);
        Task task = makeNewTask(taskId,
                                request.task,
                                QDateTime::currentDateTimeUtc());

        QList<Task> hypotheticalTasks = tasks;
        hypotheticalTasks.append(task);
        QList<TaskDependency> hypotheticalDependencies =
            m_dependencyRepository.findAllDependencies();
        for (const TaskId &predecessorId : normalizedPredecessors) {
            hypotheticalDependencies.append({predecessorId, task.id()});
        }
        const TaskDependencyGraph graph{hypotheticalTasks,
                                        hypotheticalDependencies};
        const DependencyGraphValidation graphValidation = graph.validation();
        if (!graphValidation.ok()) {
            return graphFailure<Task>(graphValidation);
        }
        const QList<ProtectedDependencyViolation> stateViolations =
            protectedStateViolations(hypotheticalTasks, graph);
        if (!stateViolations.isEmpty()) {
            return TaskResult::failure(
                TaskError::DependencyStateConflict,
                QStringLiteral("Stored task dependencies violate an active/completed state."),
                stateViolationContext(stateViolations));
        }

        try {
            m_creationRepository.insertTaskWithPredecessors(
                task, normalizedPredecessors);
        } catch (const RepositoryException &exception) {
            return persistenceFailure(exception);
        }
        emit tasksChanged();
        if (!normalizedPredecessors.isEmpty()) {
            emit dependenciesChanged();
        }
        return TaskResult::success(std::move(task));
    } catch (const RepositoryException &exception) {
        return persistenceFailure(exception);
    } catch (...) {
        return unexpectedPersistenceFailure();
    }
}

TaskResult TaskService::updateTask(const TaskId &id, const TaskDraft &draft)
{
    try {
        const QList<Task> tasks = m_repository.findAll();
        const Task *current = findTaskInList(tasks, id);
        if (current == nullptr) {
            return TaskResult::failure(TaskError::NotFound,
                                       QStringLiteral("Task was not found."));
        }
        // 编辑资格必须在Model最终判定，避免View隐藏按钮后仍可绕过界面更新归档任务。
        if (!current->canEditDetails()) {
            return TaskResult::failure(
                TaskError::ArchivedTaskNotEditable,
                QStringLiteral("An archived task must be restored before editing."),
                TaskErrorContext{{}, {id}, {}});
        }
        const TaskValidationResult validation = validateDraft(draft);
        if (!validation.ok()) {
            return TaskResult::failure(validation.error, validation.detail);
        }
        Task updated = makeTaskWithDetails(
            *current, draft, QDateTime::currentDateTimeUtc());
        if (!m_repository.update(updated)) {
            return TaskResult::failure(TaskError::NotFound,
                                       QStringLiteral("Task was not found during update."));
        }
        emit tasksChanged();
        return TaskResult::success(std::move(updated));
    } catch (const RepositoryException &exception) {
        return persistenceFailure(exception);
    } catch (...) {
        return unexpectedPersistenceFailure();
    }
}

TaskResult TaskService::startTask(const TaskId &id)
{
    return applyTransition(id, TaskTransition::Start);
}

TaskResult TaskService::cancelTask(const TaskId &id)
{
    return applyTransition(id, TaskTransition::Cancel);
}

TaskResult TaskService::completeTask(const TaskId &id)
{
    return applyTransition(id, TaskTransition::Complete);
}

TaskResult TaskService::redoTask(const TaskId &id)
{
    return applyTransition(id, TaskTransition::Redo);
}

TaskResult TaskService::archiveTask(const TaskId &id)
{
    return applyTransition(id, TaskTransition::Archive);
}

TaskResult TaskService::restoreTask(const TaskId &id)
{
    return applyTransition(id, TaskTransition::Restore);
}

TaskResult TaskService::applyTransition(const TaskId &id,
                                        const TaskTransition transition)
{
    try {
        const QList<Task> tasks = m_repository.findAll();
        const Task *current = findTaskInList(tasks, id);
        if (current == nullptr) {
            return TaskResult::failure(TaskError::NotFound,
                                       QStringLiteral("Task was not found."));
        }

        const std::optional<TaskStatus> targetStatus =
            TaskStateMachine::targetStatus(*current, transition);
        if (!targetStatus.has_value()) {
            return TaskResult::failure(
                TaskError::InvalidTaskTransition,
                QStringLiteral("The task state does not allow this transition."),
                TaskErrorContext{{}, {id}, {}});
        }

        if (*targetStatus == TaskStatus::InProgress) {
            const QList<TaskId> conflictingIds = otherInProgressTaskIds(tasks, id);
            if (!conflictingIds.isEmpty()) {
                return TaskResult::failure(
                    TaskError::InProgressConflict,
                    QStringLiteral("Another task is already in progress."),
                    TaskErrorContext{{}, conflictingIds, {}});
            }
        }

        const std::optional<TaskStatus> statusBeforeArchive =
            transition == TaskTransition::Archive
            ? std::optional<TaskStatus>{current->status()}
            : std::nullopt;
        Task transitioned = makeTaskWithStatus(
            *current,
            *targetStatus,
            statusBeforeArchive,
            QDateTime::currentDateTimeUtc());

        // Cancel 会让依赖边停止阻塞，只可能修复而不会制造后继冲突；
        // 因此必须绕过受保护后继检查，避免旧的无关异常阻止用户取消任务。
        if (transition != TaskTransition::Cancel) {
            QList<Task> transitionedTasks = tasks;
            replaceTaskSnapshot(transitionedTasks, transitioned);
            const QList<TaskDependency> dependencies =
                m_dependencyRepository.findAllDependencies();
            if (const auto dependencyFailure = dependencyStateFailure(
                    transitionedTasks, dependencies, id)) {
                return *dependencyFailure;
            }
        }

        try {
            if (!m_repository.update(transitioned)) {
                return TaskResult::failure(
                    TaskError::NotFound,
                    QStringLiteral("Task was not found during state transition."));
            }
        } catch (const RepositoryException &exception) {
            if (*targetStatus == TaskStatus::InProgress) {
                return mapInProgressWriteFailure(m_repository, id, exception);
            }
            return persistenceFailure(exception);
        }

        emit tasksChanged();
        return TaskResult::success(std::move(transitioned));
    } catch (const RepositoryException &exception) {
        return persistenceFailure(exception);
    } catch (...) {
        return unexpectedPersistenceFailure();
    }
}

} // namespace smartmate::model
