#include "services/TaskService.h"

#include "domain/TaskConstraints.h"
#include "planner/TaskOrderingPolicy.h"

#include <QDateTime>

#include <exception>
#include <optional>

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

[[nodiscard]] bool containsOtherInProgressTask(const QList<Task> &tasks,
                                               const std::optional<TaskId> &excludedId)
{
    for (const Task &task : tasks) {
        if (task.status() == TaskStatus::InProgress
            && (!excludedId.has_value() || task.id() != *excludedId)) {
            return true;
        }
    }
    return false;
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
        if (containsOtherInProgressTask(
                repository.findAll(), std::optional<TaskId>{attemptedTaskId})) {
            return TaskResult::failure(
                TaskError::InProgressConflict,
                QStringLiteral("Another task became in progress before this write completed."));
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

} // namespace

TaskService::TaskService(ITaskRepository &repository, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
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

TaskPlanResult TaskService::listRecommendedTasks() const
{
    try {
        return TaskPlanResult::success(
            orderTasks(m_repository.findAll(), QDateTime::currentDateTimeUtc()));
    } catch (const RepositoryException &exception) {
        return persistencePlanFailure(exception);
    } catch (...) {
        return unexpectedPersistencePlanFailure();
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
    const TaskValidationResult validation = validateDraft(draft);
    if (!validation.ok()) {
        return TaskResult::failure(validation.error, validation.detail);
    }

    try {
        // 这是单进行中规则的业务预检；写入时发生的竞争由失败后重读兜底。
        if (draft.status == TaskStatus::InProgress
            && containsOtherInProgressTask(m_repository.findAll(), std::nullopt)) {
            return TaskResult::failure(TaskError::InProgressConflict,
                                       QStringLiteral("Another task is already in progress."));
        }

        const QDateTime now = QDateTime::currentDateTimeUtc();
        const std::optional<QDateTime> deadline = draft.deadline.has_value()
            ? std::optional<QDateTime>{draft.deadline->toUTC()}
            : std::nullopt;
        // 新建即归档没有历史状态，因此约定恢复为待办。
        const std::optional<TaskStatus> statusBeforeArchive =
            draft.status == TaskStatus::Archived
            ? std::optional<TaskStatus>{TaskStatus::Todo}
            : std::nullopt;
        Task task{QUuid::createUuid(),
                  draft.title.trimmed(),
                  draft.description,
                  draft.priority,
                  draft.status,
                  statusBeforeArchive,
                  deadline,
                  draft.estimatedMinutes,
                  now,
                  now};
        try {
            m_repository.insert(task);
        } catch (const RepositoryException &exception) {
            if (draft.status == TaskStatus::InProgress) {
                return mapInProgressWriteFailure(m_repository, task.id(), exception);
            }
            return persistenceFailure(exception);
        }
        emit tasksChanged();
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
        const std::optional<Task> current = m_repository.findById(id);
        if (!current.has_value()) {
            return TaskResult::failure(TaskError::NotFound,
                                       QStringLiteral("Task was not found."));
        }
        if (draft.status == TaskStatus::InProgress
            && containsOtherInProgressTask(m_repository.findAll(), id)) {
            return TaskResult::failure(TaskError::InProgressConflict,
                                       QStringLiteral("Another task is already in progress."));
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
        const std::optional<Task> current = m_repository.findById(id);
        if (!current.has_value()) {
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
        const std::optional<Task> current = m_repository.findById(id);
        if (!current.has_value()) {
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
        if (restoredStatus == TaskStatus::InProgress
            && containsOtherInProgressTask(m_repository.findAll(), id)) {
            return TaskResult::failure(TaskError::InProgressConflict,
                                       QStringLiteral("Another task is already in progress."));
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
