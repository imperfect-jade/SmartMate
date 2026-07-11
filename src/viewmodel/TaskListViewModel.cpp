#include "TaskListViewModel.h"

#include "TaskErrorMapper.h"
#include "services/TaskService.h"

#include <QDateTime>

#include <algorithm>

namespace smartmate::viewmodel {

namespace {
constexpr auto deadlineFormat = "yyyy-MM-dd HH:mm";
}

TaskListViewModel::TaskListViewModel(model::TaskService &taskService, QObject *parent)
    : QAbstractListModel(parent)
    , m_taskService(taskService)
{
    // Service 是多个 ViewModel 共享的状态源；成功写入后的统一信号会触发
    // 列表重建，无需让编辑器或 QML 手动刷新本列表。
    connect(&m_taskService, &model::TaskService::tasksChanged, this,
            &TaskListViewModel::reload);
    reload();
}

int TaskListViewModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visibleTasks.size();
}

int TaskListViewModel::count() const noexcept
{
    return m_visibleTasks.size();
}

QVariant TaskListViewModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visibleTasks.size()) {
        return {};
    }

    const auto &task = m_visibleTasks.at(index.row());
    switch (role) {
    case TaskIdRole:
        return task.id().toString(QUuid::WithoutBraces);
    case TitleRole:
        return task.title();
    case DescriptionRole:
        return task.description();
    case StatusRole:
        return static_cast<int>(task.status());
    case StatusTextRole:
        return statusText(task.status());
    case PriorityRole:
        return static_cast<int>(task.priority());
    case PriorityTextRole:
        return priorityText(task.priority());
    case DeadlineTextRole:
        return task.deadline().has_value()
            ? task.deadline()->toLocalTime().toString(QString::fromLatin1(deadlineFormat))
            : QString{};
    case EstimatedMinutesRole:
        return task.estimatedMinutes().has_value() ? *task.estimatedMinutes() : 0;
    case ArchivedRole:
        return task.status() == model::TaskStatus::Archived;
    default:
        return {};
    }
}

QHash<int, QByteArray> TaskListViewModel::roleNames() const
{
    return {
        {TaskIdRole, "taskId"},
        {TitleRole, "title"},
        {DescriptionRole, "description"},
        {StatusRole, "status"},
        {StatusTextRole, "statusText"},
        {PriorityRole, "priority"},
        {PriorityTextRole, "priorityText"},
        {DeadlineTextRole, "deadlineText"},
        {EstimatedMinutesRole, "estimatedMinutes"},
        {ArchivedRole, "archived"},
    };
}

bool TaskListViewModel::showArchived() const noexcept
{
    return m_showArchived;
}

QString TaskListViewModel::errorMessage() const
{
    return m_errorMessage;
}

void TaskListViewModel::setShowArchived(const bool showArchived)
{
    if (m_showArchived == showArchived) {
        return;
    }

    m_showArchived = showArchived;
    emit showArchivedChanged();
    rebuildVisibleTasks();
}

void TaskListViewModel::reload()
{
    const auto result = m_taskService.listTasks();
    if (!result.ok()) {
        setError(taskErrorMessage(result.error));
        return;
    }

    setError({});
    m_allTasks = *result.value;
    // 这是确定性的展示排序而非任务规划算法；TaskId 用来打破更新时间相同的平局。
    std::sort(m_allTasks.begin(), m_allTasks.end(), [](const model::Task &left,
                                                       const model::Task &right) {
        if (left.updatedAtUtc() != right.updatedAtUtc()) {
            return left.updatedAtUtc() > right.updatedAtUtc();
        }
        return left.id().toString(QUuid::WithoutBraces)
            < right.id().toString(QUuid::WithoutBraces);
    });
    rebuildVisibleTasks();
}

bool TaskListViewModel::archiveTask(const QString &taskId)
{
    // View 只提交稳定 ID；状态转换及其业务约束全部委托给 Model Service。
    const auto id = parseTaskId(taskId);
    if (id.isNull()) {
        setError(taskErrorMessage(model::TaskError::NotFound));
        return false;
    }

    const auto result = m_taskService.archiveTask(id);
    if (!result.ok()) {
        setError(taskErrorMessage(result.error));
        return false;
    }
    setError({});
    return true;
}

bool TaskListViewModel::restoreTask(const QString &taskId)
{
    const auto id = parseTaskId(taskId);
    if (id.isNull()) {
        setError(taskErrorMessage(model::TaskError::NotFound));
        return false;
    }

    const auto result = m_taskService.restoreTask(id);
    if (!result.ok()) {
        setError(taskErrorMessage(result.error));
        return false;
    }
    setError({});
    return true;
}

void TaskListViewModel::clearError()
{
    setError({});
}

QString TaskListViewModel::statusText(const model::TaskStatus status)
{
    switch (status) {
    case model::TaskStatus::Todo:
        return QStringLiteral("待办");
    case model::TaskStatus::InProgress:
        return QStringLiteral("进行中");
    case model::TaskStatus::Done:
        return QStringLiteral("已完成");
    case model::TaskStatus::Cancelled:
        return QStringLiteral("已取消");
    case model::TaskStatus::Archived:
        return QStringLiteral("已归档");
    }
    return {};
}

QString TaskListViewModel::priorityText(const model::TaskPriority priority)
{
    switch (priority) {
    case model::TaskPriority::Low:
        return QStringLiteral("低");
    case model::TaskPriority::Normal:
        return QStringLiteral("普通");
    case model::TaskPriority::High:
        return QStringLiteral("高");
    case model::TaskPriority::Urgent:
        return QStringLiteral("紧急");
    }
    return {};
}

model::TaskId TaskListViewModel::parseTaskId(const QString &taskId)
{
    return QUuid::fromString(taskId.trimmed());
}

void TaskListViewModel::rebuildVisibleTasks()
{
    // 整批替换投影时遵循 QAbstractItemModel 的重置协议，使 QML 安全重建 delegate。
    beginResetModel();
    m_visibleTasks.clear();
    m_visibleTasks.reserve(m_allTasks.size());

    for (const auto &task : m_allTasks) {
        const bool archived = task.status() == model::TaskStatus::Archived;
        if (archived == m_showArchived) {
            m_visibleTasks.push_back(task);
        }
    }

    endResetModel();
    emit countChanged();
}

void TaskListViewModel::setError(const QString &message)
{
    // 属性支持持续绑定，事件信号用于立即弹出反馈；二者表达同一份展示错误。
    if (m_errorMessage == message) {
        if (!message.isEmpty()) {
            emit errorOccurred(message);
        }
        return;
    }

    m_errorMessage = message;
    emit errorMessageChanged();
    if (!message.isEmpty()) {
        emit errorOccurred(message);
    }
}

} // namespace smartmate::viewmodel
