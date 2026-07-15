#include "services/TaskService.h"

#include "services/internal/TaskServiceResultSupport.h"
#include "services/internal/TaskServiceSnapshotSupport.h"

#include <QDateTime>
#include <QSet>

#include <optional>
#include <utility>

namespace smartmate::model {
namespace {

// 业务层先做快速预检，数据库唯一约束再防御多实例竞争；写失败后重读以恢复准确错误语义。
[[nodiscard]] TaskResult mapInProgressWriteFailure(
    ITaskRepository &repository,
    const TaskId &attemptedTaskId,
    const RepositoryException &writeFailure)
{
    try {
        const QList<TaskId> conflictingIds =
            task_service_detail::otherInProgressTaskIds(
                repository.findAll(),
                std::optional<TaskId>{attemptedTaskId});
        if (!conflictingIds.isEmpty()) {
            return TaskResult::failure(
                TaskError::InProgressConflict,
                QStringLiteral("Another task became in progress before this write completed."),
                TaskErrorContext{{}, conflictingIds, {}});
        }
    } catch (...) {
        // 无法确认竞争时保留原写入错误，避免把普通持久化故障误报为状态冲突。
    }
    return task_service_detail::persistenceFailure(writeFailure);
}

} // namespace

using namespace task_service_detail;

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
    return singleTaskResult(archiveTasks({id}), id);
}

TaskBatchResult TaskService::archiveTasks(const QList<TaskId> &taskIds)
{
    return applyBatchTransition(taskIds, TaskTransition::Archive);
}

TaskResult TaskService::restoreTask(const TaskId &id)
{
    return singleTaskResult(restoreTasks({id}), id);
}

TaskBatchResult TaskService::restoreTasks(const QList<TaskId> &taskIds)
{
    return applyBatchTransition(taskIds, TaskTransition::Restore);
}

TaskResult TaskService::deleteArchivedTask(const TaskId &id)
{
    return singleTaskResult(deleteArchivedTasks({id}), id);
}

TaskBatchResult TaskService::deleteArchivedTasks(const QList<TaskId> &taskIds)
{
    QList<TaskId> normalizedTaskIds = taskIds;
    // 批量选择以稳定 ID 表示；排序去重使事务输入与错误结果具有确定性。
    normalizeIds(normalizedTaskIds);
    if (normalizedTaskIds.isEmpty()) {
        return TaskBatchResult::failure(
            TaskError::EmptyTaskSelection,
            QStringLiteral("At least one task must be selected."));
    }

    try {
        const QList<Task> tasks = m_repository.findAll();
        const QHash<TaskId, qsizetype> taskIndexes = taskIndexesById(tasks);
        // 统一读取依赖快照，使批量命令的读失败语义一致；边的实际去重与删除由原子端口负责。
        const QList<TaskDependency> dependencies =
            m_dependencyRepository.findAllDependencies();
        Q_UNUSED(dependencies);

        QList<TaskId> missingTaskIds;
        QList<TaskId> ineligibleTaskIds;
        QList<Task> selectedTasks;
        selectedTasks.reserve(normalizedTaskIds.size());
        for (const TaskId &taskId : std::as_const(normalizedTaskIds)) {
            const auto taskIndex = taskIndexes.constFind(taskId);
            if (taskIndex == taskIndexes.cend()) {
                missingTaskIds.append(taskId);
                continue;
            }
            const Task &task = tasks.at(taskIndex.value());
            if (!task.canDeletePermanently()) {
                ineligibleTaskIds.append(taskId);
            } else {
                selectedTasks.append(task);
            }
        }

        if (!missingTaskIds.isEmpty()) {
            return TaskBatchResult::failure(
                TaskError::NotFound,
                QStringLiteral("One or more selected tasks were not found."),
                TaskErrorContext{{}, std::move(missingTaskIds), {}});
        }
        if (!ineligibleTaskIds.isEmpty()) {
            return TaskBatchResult::failure(
                TaskError::TaskDeletionNotAllowed,
                QStringLiteral("Only archived tasks can be permanently deleted."),
                TaskErrorContext{{}, std::move(ineligibleTaskIds), {}});
        }

        const TaskDeletionWriteResult writeResult =
            // 端口在同一事务中条件删除任务及其全部入边、出边，并报告并发冲突。
            m_deletionRepository.deleteArchivedTasksWithDependencies(
                normalizedTaskIds);
        if (!writeResult.conflictingTaskIds.isEmpty()) {
            QList<TaskId> conflicts = writeResult.conflictingTaskIds;
            normalizeIds(conflicts);
            // 条件删除无法区分“目标消失”和“状态变化”；回滚后重读以恢复准确业务错误。
            const QList<Task> latestTasks = m_repository.findAll();
            const QHash<TaskId, qsizetype> latestIndexes =
                taskIndexesById(latestTasks);
            QList<TaskId> missingIds;
            QList<TaskId> statusConflictIds;
            for (const TaskId &conflictId : std::as_const(conflicts)) {
                const auto latestIndex = latestIndexes.constFind(conflictId);
                if (latestIndex == latestIndexes.cend()) {
                    missingIds.append(conflictId);
                } else if (!latestTasks.at(latestIndex.value())
                                .canDeletePermanently()) {
                    statusConflictIds.append(conflictId);
                }
            }
            if (!missingIds.isEmpty()) {
                return TaskBatchResult::failure(
                    TaskError::NotFound,
                    QStringLiteral("Selected tasks disappeared before permanent deletion."),
                    TaskErrorContext{{}, std::move(missingIds), {}});
            }
            if (!statusConflictIds.isEmpty()) {
                return TaskBatchResult::failure(
                    TaskError::TaskDeletionNotAllowed,
                    QStringLiteral("Selected tasks changed before permanent deletion."),
                    TaskErrorContext{{}, std::move(statusConflictIds), {}});
            }
            return TaskBatchResult::failure(
                TaskError::PersistenceFailure,
                QStringLiteral("Deletion repository reported an unexplained batch conflict."),
                TaskErrorContext{{}, std::move(conflicts), {}});
        }
        if (writeResult.deletedTaskCount != normalizedTaskIds.size()) {
            // 原子端口必须满足“全有或全无”；数量不匹配视为端口契约被破坏。
            return TaskBatchResult::failure(
                TaskError::PersistenceFailure,
                QStringLiteral("Deletion repository did not delete the complete batch."));
        }

        // 永久删除事务成功后任务投影必然失效；一次批量命令只广播一次。
        emit tasksChanged();
        if (writeResult.removedDependencyCount > 0) {
            // 删除确实移除了入边/出边时，依赖图和阻塞投影才需要刷新。
            emit dependenciesChanged();
        }
        return TaskBatchResult::success(
            TaskBatchOutcome{std::move(selectedTasks),
                             writeResult.removedDependencyCount});
    } catch (const RepositoryException &exception) {
        return batchPersistenceFailure(exception);
    } catch (...) {
        return unexpectedBatchPersistenceFailure();
    }
}

TaskBatchResult TaskService::applyBatchTransition(
    const QList<TaskId> &taskIds,
    const TaskTransition transition)
{
    QList<TaskId> normalizedTaskIds = taskIds;
    normalizeIds(normalizedTaskIds);
    if (normalizedTaskIds.isEmpty()) {
        return TaskBatchResult::failure(
            TaskError::EmptyTaskSelection,
            QStringLiteral("At least one task must be selected."));
    }

    try {
        // tasks 会被逐项替换为转换后的最终假想快照，尚未触碰持久化状态。
        QList<Task> tasks = m_repository.findAll();
        const QHash<TaskId, qsizetype> taskIndexes = taskIndexesById(tasks);
        const QList<TaskDependency> dependencies =
            m_dependencyRepository.findAllDependencies();
        QList<TaskId> missingTaskIds;
        QList<TaskId> ineligibleTaskIds;
        QList<Task> transitionedTasks;
        transitionedTasks.reserve(normalizedTaskIds.size());
        QList<TaskStateChange> changes;
        changes.reserve(normalizedTaskIds.size());
        QSet<TaskId> changedTaskIds;
        const QDateTime nowUtc = QDateTime::currentDateTimeUtc();

        for (const TaskId &taskId : std::as_const(normalizedTaskIds)) {
            const auto taskIndex = taskIndexes.constFind(taskId);
            if (taskIndex == taskIndexes.cend()) {
                missingTaskIds.append(taskId);
                continue;
            }
            const Task current = tasks.at(taskIndex.value());
            const std::optional<TaskStatus> targetStatus =
                TaskStateMachine::targetStatus(current, transition);
            if (!targetStatus.has_value()) {
                ineligibleTaskIds.append(taskId);
                continue;
            }
            const std::optional<TaskStatus> statusBeforeArchive =
                transition == TaskTransition::Archive
                ? std::optional<TaskStatus>{current.status()}
                : std::nullopt;
            Task transitioned = makeTaskWithStatus(
                current, *targetStatus, statusBeforeArchive, nowUtc);
            changes.append({taskId,
                            // 携带期望旧状态，让 Repository 用条件更新防御预检后的并发变化。
                            current.status(),
                            *targetStatus,
                            statusBeforeArchive,
                            nowUtc});
            transitionedTasks.append(transitioned);
            tasks[taskIndex.value()] = transitioned;
            changedTaskIds.insert(taskId);
        }

        if (!missingTaskIds.isEmpty()) {
            return TaskBatchResult::failure(
                TaskError::NotFound,
                QStringLiteral("One or more selected tasks were not found."),
                TaskErrorContext{{}, std::move(missingTaskIds), {}});
        }
        if (!ineligibleTaskIds.isEmpty()) {
            return TaskBatchResult::failure(
                TaskError::InvalidTaskTransition,
                QStringLiteral("One or more selected tasks do not allow this transition."),
                TaskErrorContext{{}, std::move(ineligibleTaskIds), {}});
        }
        if (const auto dependencyFailure = batchDependencyStateFailure(
                tasks, dependencies, changedTaskIds)) {
            return *dependencyFailure;
        }

        // 所有目标和最终图一次验证完成后，才通过批量端口原子写入整组状态变化。
        const TaskBatchWriteResult writeResult =
            m_batchTransitionRepository.updateTaskStatesAtomically(changes);
        if (!writeResult.conflictingTaskIds.isEmpty()) {
            QList<TaskId> conflicts = writeResult.conflictingTaskIds;
            normalizeIds(conflicts);
            return TaskBatchResult::failure(
                TaskError::InvalidTaskTransition,
                QStringLiteral("Selected task states changed before the batch write."),
                TaskErrorContext{{}, std::move(conflicts), {}});
        }
        if (writeResult.updatedTaskCount != changes.size()) {
            return TaskBatchResult::failure(
                TaskError::PersistenceFailure,
                QStringLiteral("Batch transition repository did not update every task."));
        }

        // 整批原子写入成功后统一通知，禁止逐项 emit 导致绑定观察到中间状态。
        emit tasksChanged();
        return TaskBatchResult::success(
            TaskBatchOutcome{std::move(transitionedTasks), 0});
    } catch (const RepositoryException &exception) {
        return batchPersistenceFailure(exception);
    } catch (...) {
        return unexpectedBatchPersistenceFailure();
    }
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
            // Service 始终询问唯一领域状态机，不能由调用者直接指定任意目标状态。
            TaskStateMachine::targetStatus(*current, transition);
        if (!targetStatus.has_value()) {
            return TaskResult::failure(
                TaskError::InvalidTaskTransition,
                QStringLiteral("The task state does not allow this transition."),
                TaskErrorContext{{}, {id}, {}});
        }

        if (*targetStatus == TaskStatus::InProgress) {
            // 先提供清晰业务错误；数据库唯一约束仍负责封堵多进程并发竞争。
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

        // 写入完成后广播快照失效；失败路径绝不通知，绑定不会显示未提交状态。
        emit tasksChanged();
        return TaskResult::success(std::move(transitioned));
    } catch (const RepositoryException &exception) {
        return persistenceFailure(exception);
    } catch (...) {
        return unexpectedPersistenceFailure();
    }
}

} // namespace smartmate::model

