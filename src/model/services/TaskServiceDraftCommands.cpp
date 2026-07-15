#include "services/TaskService.h"

#include "dependencies/TaskDependencyGraph.h"
#include "services/internal/TaskServiceResultSupport.h"
#include "services/internal/TaskServiceSnapshotSupport.h"

#include <QDateTime>
#include <QSet>

#include <utility>

namespace smartmate::model {

using namespace task_service_detail;

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
        if (request.task.categoryId.has_value()
            && !m_categoryRepository.findCategoryById(*request.task.categoryId)
                    .has_value()) {
            return TaskResult::failure(
                TaskError::TaskCategoryNotFound,
                QStringLiteral("Selected task category was not found."));
        }
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
        // 生成 ID 后仍对当前快照复核，确保创建请求中的关系始终引用唯一稳定端点。
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
        // 新任务和全部前置先组成最终假想快照，所有图规则通过后才进入事务端口。
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
            // 任务与全部依赖边必须一次提交；Service 不循环调用两个 Repository 制造部分成功。
            m_creationRepository.insertTaskWithPredecessors(
                task, normalizedPredecessors);
        } catch (const RepositoryException &exception) {
            return persistenceFailure(exception);
        }
        // 原子命令完成后才通知：各 ViewModel 收到信号后按自身 Contract 职责重新查询。
        emit tasksChanged();
        if (!normalizedPredecessors.isEmpty()) {
            // 只有创建了依赖边才使依赖投影失效，避免图与详情进行无效重载。
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
        // 编辑资格必须在Model最终判定，避免View隐藏按钮后仍可绕过界面更新非待办任务。
        if (!current->canEditDetails()) {
            return TaskResult::failure(
                TaskError::TaskDetailsNotEditable,
                QStringLiteral("Only a Todo task can edit details."),
                TaskErrorContext{{}, {id}, {}});
        }
        const TaskValidationResult validation = validateDraft(draft);
        if (!validation.ok()) {
            return TaskResult::failure(validation.error, validation.detail);
        }
        if (draft.categoryId.has_value()
            && !m_categoryRepository.findCategoryById(*draft.categoryId)
                    .has_value()) {
            return TaskResult::failure(
                TaskError::TaskCategoryNotFound,
                QStringLiteral("Selected task category was not found."));
        }
        Task updated = makeTaskWithDetails(
            *current, draft, QDateTime::currentDateTimeUtc());
        if (!m_repository.update(updated)) {
            return TaskResult::failure(TaskError::NotFound,
                                       QStringLiteral("Task was not found during update."));
        }
        // 这是 Model 失效通知，不携带 Widget 可绑定字段；展示数据由 ViewModel 重新投影。
        emit tasksChanged();
        return TaskResult::success(std::move(updated));
    } catch (const RepositoryException &exception) {
        return persistenceFailure(exception);
    } catch (...) {
        return unexpectedPersistenceFailure();
    }
}

} // namespace smartmate::model

