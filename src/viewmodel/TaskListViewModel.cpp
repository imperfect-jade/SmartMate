#include "TaskListViewModel.h"

#include "TaskErrorMapper.h"
#include "planner/TaskOrderingPolicy.h"
#include "services/TaskService.h"

#include <QDateTime>

#include <algorithm>
#include <utility>

namespace smartmate::viewmodel {

namespace {
constexpr auto deadlineFormat = "yyyy-MM-dd HH:mm";
constexpr int allPrioritiesFilterIndex = 0;
constexpr int firstPriorityFilterIndex = 1;
constexpr int priorityFilterOptionCount = 5;
constexpr auto detailDateTimeFormat = "yyyy-MM-dd HH:mm";
const model::TaskCommandAvailability emptyAvailability{};

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

[[nodiscard]] QString blockingReasonText(
    const QList<model::TaskId> &blockingIds,
    const QHash<model::TaskId, QString> &taskTitles,
    const QHash<QString, int> &titleCounts)
{
    if (blockingIds.isEmpty()) {
        return QStringLiteral("存在尚未完成或取消的前置任务");
    }

    QStringList visibleTitles;
    visibleTitles.reserve(blockingIds.size());
    for (const model::TaskId &id : blockingIds) {
        const QString title = taskTitles.value(id);
        if (title.isEmpty()) {
            visibleTitles.push_back(
                QStringLiteral("未知任务（%1）")
                    .arg(id.toString(QUuid::WithoutBraces).left(8)));
        } else if (titleCounts.value(title) > 1) {
            visibleTitles.push_back(
                QStringLiteral("“%1”（%2）")
                    .arg(title, id.toString(QUuid::WithoutBraces).left(8)));
        } else {
            visibleTitles.push_back(QStringLiteral("“%1”").arg(title));
        }
    }
    return QStringLiteral("等待%1完成或取消")
        .arg(visibleTitles.join(QStringLiteral("、")));
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
    connect(&m_taskService, &model::TaskService::dependenciesChanged, this,
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
    case BlockedRole:
        return m_dependencyProjections.value(task.id()).blocked;
    case BlockingReasonTextRole:
        return m_dependencyProjections.value(task.id()).blockingReasonText;
    case PredecessorCountRole:
        return m_dependencyProjections.value(task.id()).predecessorCount;
    case UnlockCountRole:
        return m_dependencyProjections.value(task.id()).unlockCount;
    case CanEditTaskRole:
        return availabilityFor(task.id()).canEditTask;
    case CanEditDependenciesRole:
        return availabilityFor(task.id()).canEditDependencies;
    case CanStartRole:
        return availabilityFor(task.id()).canStart;
    case CanCancelRole:
        return availabilityFor(task.id()).canCancel;
    case CanCompleteRole:
        return availabilityFor(task.id()).canComplete;
    case CanRedoRole:
        return availabilityFor(task.id()).canRedo;
    case CanArchiveRole:
        return availabilityFor(task.id()).canArchive;
    case CanRestoreRole:
        return availabilityFor(task.id()).canRestore;
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
        {BlockedRole, "blocked"},
        {BlockingReasonTextRole, "blockingReasonText"},
        {PredecessorCountRole, "predecessorCount"},
        {UnlockCountRole, "unlockCount"},
        {CanEditTaskRole, "canEditTask"},
        {CanEditDependenciesRole, "canEditDependencies"},
        {CanStartRole, "canStart"},
        {CanCancelRole, "canCancel"},
        {CanCompleteRole, "canComplete"},
        {CanRedoRole, "canRedo"},
        {CanArchiveRole, "canArchive"},
        {CanRestoreRole, "canRestore"},
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

TaskListViewModel::FocusState TaskListViewModel::focusState() const noexcept
{
    return m_focusState;
}

QString TaskListViewModel::focusTaskId() const
{
    return m_focusTaskId.isNull()
        ? QString{}
        : m_focusTaskId.toString(QUuid::WithoutBraces);
}

QString TaskListViewModel::focusTitle() const
{
    const auto *task = focusTask();
    return task != nullptr ? task->title() : QString{};
}

QString TaskListViewModel::focusDescription() const
{
    const auto *task = focusTask();
    return task != nullptr ? task->description() : QString{};
}

QString TaskListViewModel::focusStatusText() const
{
    const auto *task = focusTask();
    return task != nullptr ? statusText(task->status()) : QString{};
}

QString TaskListViewModel::focusPriorityText() const
{
    const auto *task = focusTask();
    return task != nullptr ? priorityText(task->priority()) : QString{};
}

QString TaskListViewModel::focusDeadlineText() const
{
    const auto *task = focusTask();
    return task != nullptr && task->deadline().has_value()
        ? task->deadline()->toLocalTime().toString(QString::fromLatin1(deadlineFormat))
        : QString{};
}

int TaskListViewModel::focusEstimatedMinutes() const noexcept
{
    const auto *task = focusTask();
    return task != nullptr && task->estimatedMinutes().has_value()
        ? *task->estimatedMinutes() : 0;
}

QString TaskListViewModel::focusReasonText() const
{
    return m_orderReasonTexts.value(m_focusTaskId);
}

bool TaskListViewModel::focusCanStart() const noexcept
{
    return availabilityFor(m_focusTaskId).canStart;
}

bool TaskListViewModel::focusCanComplete() const noexcept
{
    return availabilityFor(m_focusTaskId).canComplete;
}

QString TaskListViewModel::selectedTaskId() const
{
    return m_selectedTaskId.isNull()
        ? QString{}
        : m_selectedTaskId.toString(QUuid::WithoutBraces);
}

QString TaskListViewModel::selectedTitle() const
{
    const auto *task = selectedTask();
    return task != nullptr ? task->title() : QString{};
}

QString TaskListViewModel::selectedDescription() const
{
    const auto *task = selectedTask();
    return task != nullptr ? task->description() : QString{};
}

QString TaskListViewModel::selectedStatusText() const
{
    const auto *task = selectedTask();
    return task != nullptr ? statusText(task->status()) : QString{};
}

QString TaskListViewModel::selectedPriorityText() const
{
    const auto *task = selectedTask();
    return task != nullptr ? priorityText(task->priority()) : QString{};
}

QString TaskListViewModel::selectedDeadlineText() const
{
    const auto *task = selectedTask();
    return task != nullptr && task->deadline().has_value()
        ? task->deadline()->toLocalTime().toString(QString::fromLatin1(deadlineFormat))
        : QString{};
}

int TaskListViewModel::selectedEstimatedMinutes() const noexcept
{
    const auto *task = selectedTask();
    return task != nullptr && task->estimatedMinutes().has_value()
        ? *task->estimatedMinutes() : 0;
}

QString TaskListViewModel::selectedCreatedAtText() const
{
    const auto *task = selectedTask();
    return task != nullptr
        ? task->createdAtUtc().toLocalTime().toString(
              QString::fromLatin1(detailDateTimeFormat))
        : QString{};
}

QString TaskListViewModel::selectedUpdatedAtText() const
{
    const auto *task = selectedTask();
    return task != nullptr
        ? task->updatedAtUtc().toLocalTime().toString(
              QString::fromLatin1(detailDateTimeFormat))
        : QString{};
}

QString TaskListViewModel::selectedReasonText() const
{
    return m_orderReasonTexts.value(m_selectedTaskId);
}

QString TaskListViewModel::selectedBlockingReasonText() const
{
    return m_dependencyProjections.value(m_selectedTaskId).blockingReasonText;
}

int TaskListViewModel::selectedPredecessorCount() const noexcept
{
    return m_dependencyProjections.value(m_selectedTaskId).predecessorCount;
}

int TaskListViewModel::selectedUnlockCount() const noexcept
{
    return m_dependencyProjections.value(m_selectedTaskId).unlockCount;
}

bool TaskListViewModel::selectedCanEditTask() const noexcept
{
    return availabilityFor(m_selectedTaskId).canEditTask;
}

bool TaskListViewModel::selectedCanEditDependencies() const noexcept
{
    return availabilityFor(m_selectedTaskId).canEditDependencies;
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
    QHash<model::TaskId, QString> taskTitles;
    QHash<QString, int> titleCounts;
    QHash<model::TaskId, DependencyProjection> dependencyProjections;
    QHash<model::TaskId, model::TaskCommandAvailability> availabilities;
    allTasks.reserve(result.value->size());
    orderReasonTexts.reserve(result.value->size());
    taskTitles.reserve(result.value->size());
    titleCounts.reserve(result.value->size());
    dependencyProjections.reserve(result.value->size());
    availabilities.reserve(result.value->size());
    for (const model::PlannedTask &plannedTask : *result.value) {
        taskTitles.insert(plannedTask.task.id(), plannedTask.task.title());
        ++titleCounts[plannedTask.task.title()];
    }
    // Model决定推荐顺序和语义理由；ViewModel只保存中文展示映射并继续筛选。
    for (const model::PlannedTask &plannedTask : *result.value) {
        allTasks.push_back(plannedTask.task);
        orderReasonTexts.insert(plannedTask.task.id(),
                                orderReasonText(plannedTask.reason));
        const model::TaskDependencyState &state = plannedTask.dependencyState;
        dependencyProjections.insert(
            plannedTask.task.id(),
            DependencyProjection{
                state.blocked,
                state.blocked
                    ? blockingReasonText(state.unsatisfiedPredecessorIds,
                                         taskTitles, titleCounts)
                    : QString{},
                static_cast<int>(state.predecessorIds.size()),
                state.unlockCount,
            });
        availabilities.insert(plannedTask.task.id(), plannedTask.availability);
    }
    // 分钟定时刷新若没有产生新顺序或理由，不重置QML模型，避免列表滚动位置跳动。
    if (m_allTasks == allTasks && m_orderReasonTexts == orderReasonTexts
        && m_dependencyProjections == dependencyProjections
        && m_availabilities == availabilities) {
        return;
    }

    m_allTasks = std::move(allTasks);
    m_orderReasonTexts = std::move(orderReasonTexts);
    m_dependencyProjections = std::move(dependencyProjections);
    m_availabilities = std::move(availabilities);
    rebuildFocusTask();
    if (!m_selectedTaskId.isNull() && selectedTask() == nullptr) {
        m_selectedTaskId = model::TaskId{};
    }
    rebuildVisibleTasks();
    emit selectionChanged();
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

bool TaskListViewModel::startTask(const QString &taskId)
{
    return performTransition(taskId, model::TaskTransition::Start);
}

bool TaskListViewModel::cancelTask(const QString &taskId)
{
    return performTransition(taskId, model::TaskTransition::Cancel);
}

bool TaskListViewModel::completeTask(const QString &taskId)
{
    return performTransition(taskId, model::TaskTransition::Complete);
}

bool TaskListViewModel::redoTask(const QString &taskId)
{
    return performTransition(taskId, model::TaskTransition::Redo);
}

bool TaskListViewModel::archiveTask(const QString &taskId)
{
    return performTransition(taskId, model::TaskTransition::Archive);
}

bool TaskListViewModel::restoreTask(const QString &taskId)
{
    return performTransition(taskId, model::TaskTransition::Restore);
}

void TaskListViewModel::clearError()
{
    setError({});
}

bool TaskListViewModel::selectTask(const QString &taskId)
{
    const model::TaskId id = parseTaskId(taskId);
    if (id.isNull() || taskForId(id) == nullptr) {
        return false;
    }
    if (m_selectedTaskId == id) {
        return true;
    }
    m_selectedTaskId = id;
    emit selectionChanged();
    return true;
}

void TaskListViewModel::clearSelection()
{
    if (m_selectedTaskId.isNull()) {
        return;
    }
    m_selectedTaskId = model::TaskId{};
    emit selectionChanged();
}

bool TaskListViewModel::performTransition(const QString &taskId,
                                          const model::TaskTransition transition)
{
    const model::TaskId id = parseTaskId(taskId);
    if (id.isNull()) {
        setError(taskErrorMessage(model::TaskError::NotFound));
        return false;
    }

    model::TaskResult result;
    switch (transition) {
    case model::TaskTransition::Start:
        result = m_taskService.startTask(id);
        break;
    case model::TaskTransition::Cancel:
        result = m_taskService.cancelTask(id);
        break;
    case model::TaskTransition::Complete:
        result = m_taskService.completeTask(id);
        break;
    case model::TaskTransition::Redo:
        result = m_taskService.redoTask(id);
        break;
    case model::TaskTransition::Archive:
        result = m_taskService.archiveTask(id);
        break;
    case model::TaskTransition::Restore:
        result = m_taskService.restoreTask(id);
        break;
    }

    if (!result.ok()) {
        setError(taskErrorMessage(result.error));
        return false;
    }
    setError({});
    return true;
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

const model::Task *TaskListViewModel::taskForId(
    const model::TaskId &taskId) const
{
    if (taskId.isNull()) {
        return nullptr;
    }
    const auto iterator = std::find_if(
        m_allTasks.cbegin(), m_allTasks.cend(), [&taskId](const model::Task &task) {
            return task.id() == taskId;
        });
    return iterator == m_allTasks.cend() ? nullptr : &*iterator;
}

const model::Task *TaskListViewModel::focusTask() const
{
    return taskForId(m_focusTaskId);
}

const model::Task *TaskListViewModel::selectedTask() const
{
    return taskForId(m_selectedTaskId);
}

const model::TaskCommandAvailability &TaskListViewModel::availabilityFor(
    const model::TaskId &taskId) const
{
    const auto iterator = m_availabilities.constFind(taskId);
    return iterator == m_availabilities.cend() ? emptyAvailability : iterator.value();
}

void TaskListViewModel::rebuildFocusTask()
{
    const FocusState oldState = m_focusState;
    const model::TaskId oldId = m_focusTaskId;
    m_focusTaskId = model::TaskId{};

    bool hasTodo = false;
    for (const model::Task &task : m_allTasks) {
        if (task.status() == model::TaskStatus::InProgress) {
            m_focusTaskId = task.id();
            m_focusState = FocusState::InProgress;
            break;
        }
        hasTodo = hasTodo || task.status() == model::TaskStatus::Todo;
    }
    if (m_focusTaskId.isNull()) {
        for (const model::Task &task : m_allTasks) {
            if (availabilityFor(task.id()).canStart) {
                m_focusTaskId = task.id();
                m_focusState = FocusState::Suggested;
                break;
            }
        }
    }
    if (m_focusTaskId.isNull()) {
        m_focusState = hasTodo ? FocusState::AllBlocked : FocusState::NoTasks;
    }
    if (oldState != m_focusState || oldId != m_focusTaskId) {
        emit focusTaskChanged();
    } else {
        // 同一任务的标题、时间或推荐理由也可能变化。
        emit focusTaskChanged();
    }
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
