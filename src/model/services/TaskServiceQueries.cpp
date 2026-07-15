#include "services/TaskService.h"

#include "dependencies/TaskDependencyGraph.h"
#include "planner/TaskCommandPolicy.h"
#include "planner/TaskOrderingPolicy.h"
#include "services/internal/TaskServiceResultSupport.h"
#include "services/internal/TaskServiceSnapshotSupport.h"

#include <QDateTime>
#include <QSet>

#include <algorithm>
#include <utility>

namespace smartmate::model {

using namespace task_service_detail;

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
        // 候选沿用 Model 推荐顺序，ViewModel 只做文案投影，不得自行重排。
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
        // 推荐排序依赖一张有效 DAG；损坏的持久化图不能被静默投影为可信计划。
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
        // 命令资格与排序理由均由同一份 Model 快照计算，避免界面显示与执行条件分裂。
        const auto availabilities = taskCommandAvailabilities(tasks, dependencies);
        for (PlannedTask &planned : plan) {
            planned.availability = availabilities.value(planned.task.id());
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

TaskDependencyEditContextResult TaskService::taskDependencyEditContext(
    const TaskId &taskId) const
{
    try {
        const QList<Task> tasks = m_repository.findAll();
        const QList<TaskDependency> dependencies =
            m_dependencyRepository.findAllDependencies();
        const Task *target = findTaskInList(tasks, taskId);
        if (target == nullptr) {
            return TaskDependencyEditContextResult::failure(
                TaskError::DependencyEndpointNotFound,
                QStringLiteral("Dependency target task was not found."),
                TaskErrorContext{{}, {taskId}, {}});
        }
        if (target->status() != TaskStatus::Todo) {
            return TaskDependencyEditContextResult::failure(
                TaskError::DependencyTargetNotEditable,
                QStringLiteral("Only an active Todo task can edit predecessors."),
                TaskErrorContext{{}, {taskId}, {}});
        }

        const TaskDependencyGraph graph{tasks, dependencies};
        const DependencyGraphValidation validation = graph.validation();
        if (!validation.ok()) {
            return graphFailure<TaskDependencyEditContext>(validation);
        }

        const QList<TaskId> currentPredecessors = graph.predecessorIds(taskId);
        const QSet<TaskId> selected(currentPredecessors.cbegin(),
                                    currentPredecessors.cend());
        TaskDependencyEditContext context{*target, {}, {}};
        context.taskTitles.reserve(tasks.size());
        for (const Task &task : tasks) {
            // 完整标题映射用于显示循环路径，不把领域对象直接暴露给 ViewModel Contract。
            context.taskTitles.insert(task.id(), task.title());
        }

        const QList<PlannedTask> ordered = orderTasks(
            tasks, dependencies, QDateTime::currentDateTimeUtc());
        context.candidates.reserve(ordered.size());
        for (const PlannedTask &planned : ordered) {
            const Task &candidate = planned.task;
            if (candidate.id() == taskId) {
                continue;
            }
            const bool alreadySelected = selected.contains(candidate.id());
            const bool eligible = candidate.status() != TaskStatus::Archived
                && candidate.status() != TaskStatus::Cancelled;
            // 已存在的取消/归档关系必须保留显示且可移除，只禁止把它们作为新关系加入。
            if (eligible || alreadySelected) {
                context.candidates.append(
                    {candidate, alreadySelected, eligible || alreadySelected});
            }
        }
        return TaskDependencyEditContextResult::success(std::move(context));
    } catch (const RepositoryException &exception) {
        return TaskDependencyEditContextResult::failure(
            TaskError::PersistenceFailure, QString::fromUtf8(exception.what()));
    } catch (...) {
        return TaskDependencyEditContextResult::failure(
            TaskError::PersistenceFailure,
            QStringLiteral("Unexpected dependency repository failure."));
    }
}

TaskGraphResult TaskService::taskGraphSnapshot() const
{
    return taskGraphSnapshot({});
}

TaskGraphResult TaskService::taskGraphSnapshot(const TaskGraphQuery &query) const
{
    try {
        if (query.scope == TaskGraphCategoryScope::SpecificCategory) {
            if (!query.categoryId.has_value()
                || !m_categoryRepository.findCategoryById(*query.categoryId)
                        .has_value()) {
                return TaskGraphResult::failure(
                    TaskError::TaskCategoryNotFound,
                    QStringLiteral("Task graph category was not found."));
            }
        }
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
        // 归档节点仅在与活动任务连通时作为依赖上下文保留，孤立历史节点不进入图快照。
        const QSet<TaskId> baseVisibleIds(connectedIds.cbegin(), connectedIds.cend());

        QSet<TaskId> coreIds;
        for (const Task &task : tasks) {
            if (!baseVisibleIds.contains(task.id())) {
                continue;
            }
            const bool matches = query.scope == TaskGraphCategoryScope::All
                || (query.scope == TaskGraphCategoryScope::Uncategorized
                    && !task.categoryId().has_value())
                || (query.scope == TaskGraphCategoryScope::SpecificCategory
                    && task.categoryId() == query.categoryId);
            if (matches) {
                coreIds.insert(task.id());
            }
        }

        QSet<TaskId> visibleIds = coreIds;
        if (query.scope != TaskGraphCategoryScope::All) {
            // 只补充一跳上下文，避免跨类别关系把整个连通分量重新展开。
            for (const TaskDependency &dependency : dependencies) {
                if (coreIds.contains(dependency.predecessorId)
                    && baseVisibleIds.contains(dependency.successorId)) {
                    visibleIds.insert(dependency.successorId);
                }
                if (coreIds.contains(dependency.successorId)
                    && baseVisibleIds.contains(dependency.predecessorId)) {
                    visibleIds.insert(dependency.predecessorId);
                }
            }
        }

        QList<TaskDependency> visibleDependencies;
        for (const TaskDependency &dependency : dependencies) {
            const bool endpointsVisible = visibleIds.contains(dependency.predecessorId)
                && visibleIds.contains(dependency.successorId);
            const bool touchesCore = coreIds.contains(dependency.predecessorId)
                || coreIds.contains(dependency.successorId);
            if (endpointsVisible
                && (query.scope == TaskGraphCategoryScope::All || touchesCore)) {
                visibleDependencies.append(dependency);
            }
        }
        QList<Task> visibleTasks;
        visibleTasks.reserve(visibleIds.size());
        for (const Task &task : tasks) {
            if (visibleIds.contains(task.id())) {
                visibleTasks.append(task);
            }
        }
        const QHash<TaskId, int> levels =
            // 层级按裁剪后的可见图重算，避免隐藏节点在画布上制造无意义空层。
            TaskDependencyGraph{visibleTasks, visibleDependencies}.dependencyLevels();

        TaskGraphSnapshot snapshot;
        const QList<PlannedTask> recommended = orderTasks(
            tasks, dependencies, QDateTime::currentDateTimeUtc());
        // 排名、闭包和命令资格仍基于完整业务图；类别裁剪只影响展示范围。
        const auto availabilities = taskCommandAvailabilities(tasks, dependencies);
        snapshot.nodes.reserve(visibleIds.size());
        for (const PlannedTask &plannedTask : recommended) {
            if (!visibleIds.contains(plannedTask.task.id())) {
                continue;
            }
            snapshot.nodes.append({plannedTask.task,
                                   plannedTask.dependencyState,
                                   availabilities.value(plannedTask.task.id()),
                                   levels.value(plannedTask.task.id(), 0),
                                   graph.predecessorClosure(plannedTask.task.id()),
                                   graph.successorClosure(plannedTask.task.id()),
                                   coreIds.contains(plannedTask.task.id())});
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
        TaskError::TaskDetailsNotEditable,
        QStringLiteral("Only a Todo task can edit details."),
        TaskErrorContext{{}, {id}, {}});
}

} // namespace smartmate::model
