#include "services/TaskService.h"

#include "dependencies/TaskDependencyGraph.h"
#include "services/internal/TaskServiceResultSupport.h"
#include "services/internal/TaskServiceSnapshotSupport.h"

#include <QSet>

#include <utility>

namespace smartmate::model {

using namespace task_service_detail;

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
        // 去重后的数量变化用于拒绝重复输入，不能悄悄吞掉错误关系。
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
        // 资格仅约束“新增”关系；历史取消关系保留后仍允许用户通过本次保存移除。
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
            // 先在内存中验证替换后的完整图，确认无缺失端点、重复边与环后再写入。
            TaskDependencyGraph{tasks, replacementDependencies}.validation();
        if (!validation.ok()) {
            return graphFailure<QList<TaskDependency>>(validation);
        }

        if (currentPredecessors == normalizedPredecessors) {
            // 幂等保存不触发事务和 dependenciesChanged，避免所有依赖投影无效刷新。
            return TaskDependencyListResult::success(currentIncoming);
        }

        // Repository 端口负责“删除旧入边 + 插入新入边”的单事务原子替换。
        m_dependencyRepository.replacePredecessors(taskId, normalizedPredecessors);
        QList<TaskDependency> replacedIncoming;
        replacedIncoming.reserve(normalizedPredecessors.size());
        for (const TaskId &predecessorId : normalizedPredecessors) {
            replacedIncoming.append({predecessorId, taskId});
        }
        // 原子替换完成后只宣告依赖快照失效；订阅 ViewModel 再查询各自需要的数据。
        emit dependenciesChanged();
        return TaskDependencyListResult::success(std::move(replacedIncoming));
    } catch (const RepositoryException &exception) {
        return dependencyPersistenceFailure(exception);
    } catch (...) {
        return unexpectedDependencyPersistenceFailure();
    }
}

} // namespace smartmate::model
