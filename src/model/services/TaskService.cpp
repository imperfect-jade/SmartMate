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

[[nodiscard]] bool isValidStatus(TaskStatus status) noexcept
{
    switch (status) {
    case TaskStatus::Todo:
    case TaskStatus::InProgress:
    case TaskStatus::Done:
    case TaskStatus::Cancelled:
    case TaskStatus::Archived:
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

/// 写任务前验证“已开始或已完成的后继，其全部前置必须完成”的领域不变量。
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

[[nodiscard]] Task makeTask(const Task &source,
                            const TaskDraft &draft,
                            std::optional<TaskStatus> statusBeforeArchive,
                            QDateTime updatedAtUtc)
{
    // 更新保留稳定 ID 与创建时间，同时统一标题、UTC 截止时间和最后修改时间。
    const std::optional<QDateTime> deadline = draft.deadline.has_value()
        ? std::optional<QDateTime>{draft.deadline->toUTC()}
        : std::nullopt;
    return Task{source.id(),
                draft.title.trimmed(),
                draft.description,
                draft.priority,
                draft.status,
                statusBeforeArchive,
                deadline,
                draft.estimatedMinutes,
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
    // 新建即归档没有历史工作阶段，因此恢复点固定为 Todo。
    const std::optional<TaskStatus> statusBeforeArchive =
        draft.status == TaskStatus::Archived
        ? std::optional<TaskStatus>{TaskStatus::Todo}
        : std::nullopt;
    return Task{taskId,
                draft.title.trimmed(),
                draft.description,
                draft.priority,
                draft.status,
                statusBeforeArchive,
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
    if (!isValidStatus(draft.status)) {
        return TaskValidationResult::failure(
            TaskError::InvalidStatus, QStringLiteral("Task status is invalid."));
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
            if (plannedTask.task.status() != TaskStatus::Archived) {
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
        return TaskPlanResult::success(orderTasks(
            tasks, dependencies, QDateTime::currentDateTimeUtc()));
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
            snapshot.edges.append({dependency,
                                   predecessor != nullptr
                                       && TaskDependencyGraph::satisfiesDependency(
                                           *predecessor)});
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
                && predecessor->status() == TaskStatus::Archived) {
                ineligibleIds.append(predecessorId);
            }
        }
        normalizeIds(ineligibleIds);
        if (!ineligibleIds.isEmpty()) {
            return TaskDependencyListResult::failure(
                TaskError::DependencyPredecessorNotEligible,
                QStringLiteral("A newly selected predecessor must not be archived."),
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
        // 这是单进行中规则的业务预检；写入时发生的竞争由失败后重读兜底。
        const QList<Task> tasks = m_repository.findAll();
        const QList<TaskId> conflictingIds =
            request.task.status == TaskStatus::InProgress
            ? otherInProgressTaskIds(tasks, std::nullopt)
            : QList<TaskId>{};
        if (!conflictingIds.isEmpty()) {
            return TaskResult::failure(
                TaskError::InProgressConflict,
                QStringLiteral("Another task is already in progress."),
                TaskErrorContext{{}, conflictingIds, {}});
        }

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
        QList<TaskId> archivedIds;
        for (const TaskId &predecessorId : normalizedPredecessors) {
            const Task *predecessor = findTaskInList(tasks, predecessorId);
            if (predecessor == nullptr) {
                missingIds.append(predecessorId);
            } else if (predecessor->status() == TaskStatus::Archived) {
                archivedIds.append(predecessorId);
            }
        }
        normalizeIds(missingIds);
        if (!missingIds.isEmpty()) {
            return TaskResult::failure(
                TaskError::DependencyEndpointNotFound,
                QStringLiteral("One or more creation predecessors were not found."),
                TaskErrorContext{{}, missingIds, {}});
        }
        normalizeIds(archivedIds);
        if (!archivedIds.isEmpty()) {
            return TaskResult::failure(
                TaskError::DependencyPredecessorNotEligible,
                QStringLiteral("A creation predecessor must not be archived."),
                TaskErrorContext{{}, archivedIds, {}});
        }
        if (!normalizedPredecessors.isEmpty()
            && request.task.status != TaskStatus::Todo) {
            return TaskResult::failure(
                TaskError::DependencyTargetNotEditable,
                QStringLiteral("A new task with predecessors must use Todo status."));
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
            if (request.task.status == TaskStatus::InProgress) {
                return mapInProgressWriteFailure(m_repository, task.id(), exception);
            }
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
    const TaskValidationResult validation = validateDraft(draft);
    if (!validation.ok()) {
        return TaskResult::failure(validation.error, validation.detail);
    }

    try {
        const QList<Task> tasks = m_repository.findAll();
        const Task *current = findTaskInList(tasks, id);
        if (current == nullptr) {
            return TaskResult::failure(TaskError::NotFound,
                                       QStringLiteral("Task was not found."));
        }
        const QList<TaskId> conflictingIds = draft.status == TaskStatus::InProgress
            ? otherInProgressTaskIds(tasks, id)
            : QList<TaskId>{};
        if (!conflictingIds.isEmpty()) {
            return TaskResult::failure(
                TaskError::InProgressConflict,
                QStringLiteral("Another task is already in progress."),
                TaskErrorContext{{}, conflictingIds, {}});
        }

        std::optional<TaskStatus> statusBeforeArchive;
        if (draft.status == TaskStatus::Archived) {
            // 首次归档记录当前状态；重复保存归档任务时保留原恢复点。
            statusBeforeArchive = current->status() == TaskStatus::Archived
                ? current->statusBeforeArchive().value_or(TaskStatus::Todo)
                : current->status();
        }

        Task updated = makeTask(*current,
                                draft,
                                statusBeforeArchive,
                                QDateTime::currentDateTimeUtc());
        QList<Task> updatedTasks = tasks;
        replaceTaskSnapshot(updatedTasks, updated);
        const QList<TaskDependency> dependencies =
            m_dependencyRepository.findAllDependencies();
        if (const auto dependencyFailure =
                dependencyStateFailure(updatedTasks, dependencies, id)) {
            return *dependencyFailure;
        }
        try {
            if (!m_repository.update(updated)) {
                return TaskResult::failure(TaskError::NotFound,
                                           QStringLiteral("Task was not found during update."));
            }
        } catch (const RepositoryException &exception) {
            if (draft.status == TaskStatus::InProgress) {
                return mapInProgressWriteFailure(m_repository, id, exception);
            }
            return persistenceFailure(exception);
        }
        emit tasksChanged();
        return TaskResult::success(std::move(updated));
    } catch (const RepositoryException &exception) {
        return persistenceFailure(exception);
    } catch (...) {
        return unexpectedPersistenceFailure();
    }
}

TaskResult TaskService::archiveTask(const TaskId &id)
{
    try {
        const QList<Task> tasks = m_repository.findAll();
        const Task *current = findTaskInList(tasks, id);
        if (current == nullptr) {
            return TaskResult::failure(TaskError::NotFound,
                                       QStringLiteral("Task was not found."));
        }
        if (current->status() == TaskStatus::Archived) {
            // 已归档任务是幂等成功，没有实际写入，因此不发出 tasksChanged。
            return TaskResult::success(*current);
        }

        const TaskDraft archivedDraft{current->title(),
                                      current->description(),
                                      current->priority(),
                                      TaskStatus::Archived,
                                      current->deadline(),
                                      current->estimatedMinutes()};
        // 软删除记录当前状态，恢复时才能回到归档前的工作阶段。
        Task archived = makeTask(*current,
                                 archivedDraft,
                                 current->status(),
                                 QDateTime::currentDateTimeUtc());
        QList<Task> archivedTasks = tasks;
        replaceTaskSnapshot(archivedTasks, archived);
        const QList<TaskDependency> dependencies =
            m_dependencyRepository.findAllDependencies();
        if (const auto dependencyFailure =
                dependencyStateFailure(archivedTasks, dependencies, id)) {
            return *dependencyFailure;
        }
        if (!m_repository.update(archived)) {
            return TaskResult::failure(TaskError::NotFound,
                                       QStringLiteral("Task was not found during archive."));
        }
        emit tasksChanged();
        return TaskResult::success(std::move(archived));
    } catch (const RepositoryException &exception) {
        return persistenceFailure(exception);
    } catch (...) {
        return unexpectedPersistenceFailure();
    }
}

TaskResult TaskService::restoreTask(const TaskId &id)
{
    try {
        const QList<Task> tasks = m_repository.findAll();
        const Task *current = findTaskInList(tasks, id);
        if (current == nullptr) {
            return TaskResult::failure(TaskError::NotFound,
                                       QStringLiteral("Task was not found."));
        }
        if (current->status() != TaskStatus::Archived) {
            return TaskResult::failure(TaskError::InvalidStatus,
                                       QStringLiteral("Only archived tasks can be restored."));
        }

        TaskStatus restoredStatus = current->statusBeforeArchive().value_or(TaskStatus::Todo);
        if (restoredStatus == TaskStatus::Archived || !isValidStatus(restoredStatus)) {
            // 对缺失或非法的旧数据采用安全的待办状态，避免恢复后仍处于归档。
            restoredStatus = TaskStatus::Todo;
        }
        const QList<TaskId> conflictingIds = restoredStatus == TaskStatus::InProgress
            ? otherInProgressTaskIds(tasks, id)
            : QList<TaskId>{};
        if (!conflictingIds.isEmpty()) {
            return TaskResult::failure(
                TaskError::InProgressConflict,
                QStringLiteral("Another task is already in progress."),
                TaskErrorContext{{}, conflictingIds, {}});
        }

        const TaskDraft restoredDraft{current->title(),
                                      current->description(),
                                      current->priority(),
                                      restoredStatus,
                                      current->deadline(),
                                      current->estimatedMinutes()};
        // 恢复完成后清空恢复点，使实体重新满足非归档状态不携带历史状态的不变量。
        Task restored = makeTask(*current,
                                 restoredDraft,
                                 std::nullopt,
                                 QDateTime::currentDateTimeUtc());
        QList<Task> restoredTasks = tasks;
        replaceTaskSnapshot(restoredTasks, restored);
        const QList<TaskDependency> dependencies =
            m_dependencyRepository.findAllDependencies();
        if (const auto dependencyFailure =
                dependencyStateFailure(restoredTasks, dependencies, id)) {
            return *dependencyFailure;
        }
        try {
            if (!m_repository.update(restored)) {
                return TaskResult::failure(
                    TaskError::NotFound,
                    QStringLiteral("Task was not found during restore."));
            }
        } catch (const RepositoryException &exception) {
            if (restoredStatus == TaskStatus::InProgress) {
                return mapInProgressWriteFailure(m_repository, id, exception);
            }
            return persistenceFailure(exception);
        }
        emit tasksChanged();
        return TaskResult::success(std::move(restored));
    } catch (const RepositoryException &exception) {
        return persistenceFailure(exception);
    } catch (...) {
        return unexpectedPersistenceFailure();
    }
}

} // namespace smartmate::model
