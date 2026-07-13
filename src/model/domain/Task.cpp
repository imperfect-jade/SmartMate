#include "domain/Task.h"

#include <utility>

namespace smartmate::model {

Task::Task(TaskId id,
           QString title,
           QString description,
           TaskPriority priority,
           TaskStatus status,
           std::optional<TaskStatus> statusBeforeArchive,
           std::optional<QDateTime> deadline,
           std::optional<int> estimatedMinutes,
           QDateTime createdAtUtc,
           QDateTime updatedAtUtc,
           std::optional<TaskCategoryId> categoryId)
    : m_id(std::move(id))
    , m_title(std::move(title))
    , m_description(std::move(description))
    , m_priority(priority)
    , m_status(status)
    , m_statusBeforeArchive(std::move(statusBeforeArchive))
    , m_deadline(std::move(deadline))
    , m_estimatedMinutes(std::move(estimatedMinutes))
    , m_categoryId(std::move(categoryId))
    , m_createdAtUtc(std::move(createdAtUtc))
    , m_updatedAtUtc(std::move(updatedAtUtc))
{
    // 可选描述在领域中用空文本表示。Qt 的 null QString 只是输入表示差异，
    // 不应继续传播为持久化层的 SQL NULL。
    if (m_description.isNull()) {
        m_description = QStringLiteral("");
    }
}

const TaskId &Task::id() const noexcept
{
    return m_id;
}

const QString &Task::title() const noexcept
{
    return m_title;
}

const QString &Task::description() const noexcept
{
    return m_description;
}

TaskPriority Task::priority() const noexcept
{
    return m_priority;
}

TaskStatus Task::status() const noexcept
{
    return m_status;
}

bool Task::canEditDetails() const noexcept
{
    return m_status == TaskStatus::Todo;
}

bool Task::canDeletePermanently() const noexcept
{
    return m_status == TaskStatus::Archived;
}

const std::optional<TaskStatus> &Task::statusBeforeArchive() const noexcept
{
    return m_statusBeforeArchive;
}

const std::optional<QDateTime> &Task::deadline() const noexcept
{
    return m_deadline;
}

const std::optional<int> &Task::estimatedMinutes() const noexcept
{
    return m_estimatedMinutes;
}

const std::optional<TaskCategoryId> &Task::categoryId() const noexcept
{
    return m_categoryId;
}

const QDateTime &Task::createdAtUtc() const noexcept
{
    return m_createdAtUtc;
}

const QDateTime &Task::updatedAtUtc() const noexcept
{
    return m_updatedAtUtc;
}

} // namespace smartmate::model
