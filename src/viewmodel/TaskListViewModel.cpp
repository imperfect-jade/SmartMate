#include "TaskListViewModel.h"

#include "TaskErrorMapper.h"
#include "planner/TaskOrderingPolicy.h"
#include "services/TaskService.h"

#include <QDateTime>

#include <utility>

namespace smartmate::viewmodel {

namespace {
constexpr auto deadlineFormat = "yyyy-MM-dd HH:mm";
constexpr int allPrioritiesFilterIndex = 0;
constexpr int firstPriorityFilterIndex = 1;
constexpr int priorityFilterOptionCount = 5;

[[nodiscard]] QString orderReasonText(const model::TaskOrderReason reason)
{
    using enum model::TaskOrderReason;

    switch (reason) {
    case InProgress:
        return QStringLiteral("正在进行");
    case Overdue:
        return QStringLiteral("已逾期");
    case UrgentPriority:
        return QStringLiteral("紧急优先");
    case HighPriority:
        return QStringLiteral("高优先");
    case UpcomingDeadline:
        return QStringLiteral("截止较近");
    case Todo:
        return QStringLiteral("待办");
    case Completed:
        return QStringLiteral("已完成");
    case Cancelled:
        return QStringLiteral("已取消");
    case Archived:
        return QStringLiteral("已归档");
    }
    return {};
}
}

TaskListViewModel::TaskListViewModel(model::TaskService &taskService, QObject *parent)
    : QAbstractListModel(parent)
    , m_taskService(taskService)
    , m_reloadTimer(this)
{
    // Service 是多个 ViewModel 共享的状态源；成功写入后的统一信号会触发
    // 列表重建，无需让编辑器或 QML 手动刷新本列表。
    connect(&m_taskService, &model::TaskService::tasksChanged, this,
            &TaskListViewModel::reload);
    connect(&m_reloadTimer, &QTimer::timeout, this, &TaskListViewModel::reload);
    m_reloadTimer.start(60'000);
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
    case OrderReasonTextRole:
        return m_orderReasonTexts.value(task.id());
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
        {OrderReasonTextRole, "orderReasonText"},
    };
}

bool TaskListViewModel::showArchived() const noexcept
{
    return m_showArchived;
}

QString TaskListViewModel::searchText() const
{
    return m_searchText;
}

int TaskListViewModel::priorityFilterIndex() const noexcept
{
    return m_priorityFilterIndex;
}

QStringList TaskListViewModel::priorityFilterOptions() const
{
    return {QStringLiteral("全部优先级"),
            QStringLiteral("低"),
            QStringLiteral("普通"),
            QStringLiteral("高"),
            QStringLiteral("紧急")};
}

bool TaskListViewModel::hasActiveFilters() const
{
    return !m_searchText.trimmed().isEmpty()
        || m_priorityFilterIndex != allPrioritiesFilterIndex;
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

void TaskListViewModel::setSearchText(const QString &searchText)
{
    if (m_searchText == searchText) {
        return;
    }

    const bool previouslyActive = hasActiveFilters();
    m_searchText = searchText;
    emit searchTextChanged();
    if (previouslyActive != hasActiveFilters()) {
        emit hasActiveFiltersChanged();
    }
    rebuildVisibleTasks();
}

void TaskListViewModel::setPriorityFilterIndex(const int priorityFilterIndex)
{
    if (priorityFilterIndex < allPrioritiesFilterIndex
        || priorityFilterIndex >= priorityFilterOptionCount
        || m_priorityFilterIndex == priorityFilterIndex) {
        return;
    }

    const bool previouslyActive = hasActiveFilters();
    m_priorityFilterIndex = priorityFilterIndex;
    emit priorityFilterIndexChanged();
    if (previouslyActive != hasActiveFilters()) {
        emit hasActiveFiltersChanged();
    }
    rebuildVisibleTasks();
}

void TaskListViewModel::reload()
{
    const auto result = m_taskService.listRecommendedTasks();
    if (!result.ok()) {
        setError(taskErrorMessage(result.error));
        return;
    }

    setError({});
    QList<model::Task> allTasks;
    QHash<model::TaskId, QString> orderReasonTexts;
    allTasks.reserve(result.value->size());
    orderReasonTexts.reserve(result.value->size());
    // Model决定推荐顺序和语义理由；ViewModel只保存中文展示映射并继续筛选。
    for (const model::PlannedTask &plannedTask : *result.value) {
        allTasks.push_back(plannedTask.task);
        orderReasonTexts.insert(plannedTask.task.id(),
                                orderReasonText(plannedTask.reason));
    }
    // 分钟定时刷新若没有产生新顺序或理由，不重置QML模型，避免列表滚动位置跳动。
    if (m_allTasks == allTasks && m_orderReasonTexts == orderReasonTexts) {
        return;
    }

    m_allTasks = std::move(allTasks);
    m_orderReasonTexts = std::move(orderReasonTexts);
    rebuildVisibleTasks();
}

void TaskListViewModel::clearFilters()
{
    const bool searchChanged = !m_searchText.isEmpty();
    const bool priorityChanged =
        m_priorityFilterIndex != allPrioritiesFilterIndex;
    if (!searchChanged && !priorityChanged) {
        return;
    }

    const bool previouslyActive = hasActiveFilters();
    m_searchText.clear();
    m_priorityFilterIndex = allPrioritiesFilterIndex;
    if (searchChanged) {
        emit searchTextChanged();
    }
    if (priorityChanged) {
        emit priorityFilterIndexChanged();
    }
    if (previouslyActive != hasActiveFilters()) {
        emit hasActiveFiltersChanged();
    }
    // 批量清除只重建一次，避免QML列表短暂经过中间筛选状态。
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

    const QString keyword = m_searchText.trimmed();
    const bool filtersPriority =
        m_priorityFilterIndex != allPrioritiesFilterIndex;
    const auto selectedPriority = static_cast<model::TaskPriority>(
        m_priorityFilterIndex - firstPriorityFilterIndex);

    for (const auto &task : m_allTasks) {
        const bool archived = task.status() == model::TaskStatus::Archived;
        if (archived != m_showArchived) {
            continue;
        }
        if (!keyword.isEmpty()
            && !task.title().contains(keyword, Qt::CaseInsensitive)
            && !task.description().contains(keyword, Qt::CaseInsensitive)) {
            continue;
        }
        if (filtersPriority && task.priority() != selectedPriority) {
            continue;
        }
        // 过滤只删除不匹配项，剩余任务严格保留Model给出的计划顺序。
        m_visibleTasks.push_back(task);
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
