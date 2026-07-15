#include "TaskGraphViewModel.h"

#include "TaskCategoryPresentation.h"
#include "TaskGraphProjectionModels.h"
#include "TaskPresentationFormatter.h"
#include "services/TaskService.h"

#include <algorithm>

namespace smartmate::viewmodel {

QString TaskGraphViewModel::stableId(const model::TaskId &id)
{
    return id.toString(QUuid::WithoutBraces);
}

TaskGraphViewModel::TaskGraphViewModel(
    model::TaskService &taskService,
    TaskCategoryProjectionSource &categorySource,
    QObject *parent)
    : TaskGraphContract(parent)
    , m_taskService(taskService)
    , m_categorySource(categorySource)
    , m_edges(new TaskGraphEdgeListModel(this))
    , m_selectedPredecessors(new TaskGraphRelationListModel(this))
    , m_selectedSuccessors(new TaskGraphRelationListModel(this))
{
    // Service 信号只表示领域快照已经失效；ViewModel 重新查询完整图投影，
    // 不依赖其他 ViewModel 转发数据，从而保持各 Contract 相互独立。
    connect(&m_taskService, &model::TaskService::tasksChanged,
            this, &TaskGraphViewModel::reload);
    connect(&m_taskService, &model::TaskService::dependenciesChanged,
            this, &TaskGraphViewModel::reload);
    connect(&m_categorySource, &TaskCategoryProjectionSource::categoriesChanged,
            this, &TaskGraphViewModel::applyCategories);
    connect(&m_categorySource,
            &TaskCategoryProjectionSource::taskCategoryAssignmentsChanged,
            this, &TaskGraphViewModel::reload);
    connect(&m_categorySource, &TaskCategoryProjectionSource::refreshFailed,
            this, [this] {
                setErrorMessage(QStringLiteral("类别数据访问失败，请稍后重试。"));
            });
    connect(&m_categorySource, &TaskCategoryProjectionSource::refreshSucceeded,
            this, [this] {
                if (m_errorMessage
                    == QStringLiteral("类别数据访问失败，请稍后重试。")) {
                    setErrorMessage({});
                }
            });
    reload();
}

int TaskGraphViewModel::rowCount(const QModelIndex &parent) const
{ return parent.isValid() ? 0 : static_cast<int>(m_nodes.size()); }

QVariant TaskGraphViewModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_nodes.size()) return {};
    const NodeProjection &projection = m_nodes.at(index.row());
    const model::Task &task = projection.node.task;
    switch (role) {
    case TaskIdRole: return stableId(task.id());
    case ShortIdRole: return stableId(task.id()).left(8);
    case TitleRole: return task.title();
    case StatusTextRole: return taskStatusText(task.status());
    case StatusIndexRole: return static_cast<int>(taskStatusVisual(task.status()));
    case PriorityTextRole: return taskPriorityText(task.priority());
    case DeadlineTextRole:
        return taskDeadlineText(task, QStringLiteral("未设置截止时间"));
    case UnlockCountRole: return projection.node.dependencyState.unlockCount;
    case BlockedRole: return projection.node.dependencyState.blocked;
    case BlockingReasonTextRole: return projection.blockingReasonText;
    case ArchivedRole: return task.status() == model::TaskStatus::Archived;
    case CanEditDependenciesRole:
        return projection.node.availability.canEditDependencies;
    case NodeXRole: return projection.x;
    case NodeYRole: return projection.y;
    case NodeWidthRole: return taskGraphNodeWidth;
    case NodeHeightRole: return taskGraphNodeHeight;
    case SelectedRole: return task.id() == m_selectedTaskId;
    case EmphasisLevelRole: return emphasisFor(task.id());
    case FilterMatchedRole: return filterMatches(projection);
    case CategoryNameRole: {
        const auto *category = categoryForTask(task);
        return category ? category->name : QString{};
    }
    case CategoryAccentRole: {
        const auto *category = categoryForTask(task);
        return category ? taskCategoryAccent(category->color)
                        : taskUncategorizedAccent();
    }
    case HasCategoryRole: return categoryForTask(task) != nullptr;
    case CoreNodeRole: return projection.node.coreNode;
    default: return {};
    }
}

QHash<int, QByteArray> TaskGraphViewModel::roleNames() const
{
    return {{TaskIdRole, "taskId"}, {ShortIdRole, "shortId"}, {TitleRole, "title"},
            {StatusTextRole, "statusText"}, {StatusIndexRole, "stateColorKey"},
            {PriorityTextRole, "priorityText"}, {DeadlineTextRole, "deadlineText"},
            {UnlockCountRole, "unlockCount"}, {BlockedRole, "blocked"},
            {BlockingReasonTextRole, "blockingReasonText"}, {ArchivedRole, "archived"},
            {CanEditDependenciesRole, "canEditDependencies"}, {NodeXRole, "nodeX"},
            {NodeYRole, "nodeY"}, {NodeWidthRole, "nodeWidth"},
            {NodeHeightRole, "nodeHeight"}, {SelectedRole, "selected"},
            {EmphasisLevelRole, "emphasisLevel"}, {FilterMatchedRole, "filterMatched"},
            {CategoryNameRole, "categoryName"}, {CategoryAccentRole, "categoryAccent"},
            {HasCategoryRole, "hasCategory"}, {CoreNodeRole, "coreNode"}};
}

QAbstractItemModel *TaskGraphViewModel::edges() noexcept { return m_edges; }
QAbstractItemModel *TaskGraphViewModel::selectedPredecessors() noexcept { return m_selectedPredecessors; }
QAbstractItemModel *TaskGraphViewModel::selectedSuccessors() noexcept { return m_selectedSuccessors; }
qreal TaskGraphViewModel::contentWidth() const noexcept { return m_contentWidth; }
qreal TaskGraphViewModel::contentHeight() const noexcept { return m_contentHeight; }
QString TaskGraphViewModel::searchText() const { return m_searchText; }
int TaskGraphViewModel::statusFilterIndex() const noexcept
{ return static_cast<int>(m_statusFilter); }
QStringList TaskGraphViewModel::statusFilterOptions() const
{ return taskGraphStatusFilterOptions(); }
QVariantList TaskGraphViewModel::categoryFilterOptions() const
{
    QVariantList options;
    options.reserve(m_categorySource.categories().size() + 2);
    options.append(QVariantMap{{QStringLiteral("mode"), 0},
                               {QStringLiteral("categoryId"), QString{}},
                               {QStringLiteral("name"), QStringLiteral("全部类别")},
                               {QStringLiteral("accent"), taskAllCategoriesAccent()}});
    options.append(QVariantMap{{QStringLiteral("mode"), 1},
                               {QStringLiteral("categoryId"), QString{}},
                               {QStringLiteral("name"), QStringLiteral("未分类")},
                               {QStringLiteral("accent"), taskUncategorizedAccent()}});
    for (const auto &category : m_categorySource.categories()) {
        options.append(QVariantMap{
            {QStringLiteral("mode"), 2},
            {QStringLiteral("categoryId"), category.id.toString(QUuid::WithoutBraces)},
            {QStringLiteral("name"), category.name},
            {QStringLiteral("accent"), taskCategoryAccent(category.color)}});
    }
    return options;
}
int TaskGraphViewModel::categoryFilterMode() const noexcept
{ return m_categoryFilterMode; }
QString TaskGraphViewModel::categoryFilterCategoryId() const
{
    return m_categoryFilterMode == 2 && !m_categoryFilterCategoryId.isNull()
        ? m_categoryFilterCategoryId.toString(QUuid::WithoutBraces)
        : QString{};
}
int TaskGraphViewModel::taskCount() const noexcept { return static_cast<int>(m_nodes.size()); }
int TaskGraphViewModel::blockedCount() const noexcept
{
    return static_cast<int>(std::count_if(m_nodes.cbegin(), m_nodes.cend(),
        [](const NodeProjection &node) { return node.node.dependencyState.blocked; }));
}
QString TaskGraphViewModel::currentTaskId() const
{
    const auto iterator = std::find_if(m_nodes.cbegin(), m_nodes.cend(), [](const NodeProjection &node) {
        return node.node.task.status() == model::TaskStatus::InProgress;
    });
    return iterator == m_nodes.cend() ? QString{} : stableId(iterator->node.task.id());
}
QString TaskGraphViewModel::selectedTaskId() const
{ return m_selectedTaskId.isNull() ? QString{} : stableId(m_selectedTaskId); }

const TaskGraphViewModel::NodeProjection *TaskGraphViewModel::selectedNode() const
{
    const int row = rowForTask(m_selectedTaskId);
    return row >= 0 ? &m_nodes.at(row) : nullptr;
}

QString TaskGraphViewModel::selectedTaskTitle() const
{ const auto *node = selectedNode(); return node ? node->node.task.title() : QString{}; }
QString TaskGraphViewModel::selectedDescription() const
{ const auto *node = selectedNode(); return node ? node->node.task.description() : QString{}; }
QString TaskGraphViewModel::selectedStatusText() const
{ const auto *node = selectedNode(); return node ? taskStatusText(node->node.task.status()) : QString{}; }
QString TaskGraphViewModel::selectedPriorityText() const
{ const auto *node = selectedNode(); return node ? taskPriorityText(node->node.task.priority()) : QString{}; }
QString TaskGraphViewModel::selectedDeadlineText() const
{ const auto *node = selectedNode(); return node ? taskDeadlineText(node->node.task, QStringLiteral("未设置截止时间")) : QString{}; }
QString TaskGraphViewModel::selectedEstimatedDurationText() const
{ const auto *node = selectedNode(); return node ? taskDurationText(node->node.task, QStringLiteral("未设置预计用时")) : QString{}; }
QString TaskGraphViewModel::selectedBlockingReason() const
{ const auto *node = selectedNode(); return node ? node->blockingReasonText : QString{}; }
int TaskGraphViewModel::selectedUnlockCount() const noexcept
{ const auto *node = selectedNode(); return node ? node->node.dependencyState.unlockCount : 0; }
int TaskGraphViewModel::selectedPredecessorCount() const noexcept
{ return m_predecessorCounts.value(m_selectedTaskId); }
int TaskGraphViewModel::selectedSuccessorCount() const noexcept
{ return m_successorCounts.value(m_selectedTaskId); }
qreal TaskGraphViewModel::selectedNodeCenterX() const noexcept
{ const auto *node = selectedNode(); return node ? node->x + taskGraphNodeWidth / 2.0 : 0.0; }
qreal TaskGraphViewModel::selectedNodeCenterY() const noexcept
{ const auto *node = selectedNode(); return node ? node->y + taskGraphNodeHeight / 2.0 : 0.0; }
bool TaskGraphViewModel::canEditSelectedDependencies() const noexcept
{
    const auto *node = selectedNode();
    return node && node->node.availability.canEditDependencies;
}
QString TaskGraphViewModel::selectedCategoryName() const
{
    const auto *node = selectedNode();
    const auto *category = node ? categoryForTask(node->node.task) : nullptr;
    return category ? category->name : QString{};
}
QString TaskGraphViewModel::selectedCategoryAccent() const
{
    const auto *node = selectedNode();
    const auto *category = node ? categoryForTask(node->node.task) : nullptr;
    return category ? taskCategoryAccent(category->color)
                    : taskUncategorizedAccent();
}
bool TaskGraphViewModel::selectedHasCategory() const noexcept
{
    const auto *node = selectedNode();
    return node && categoryForTask(node->node.task) != nullptr;
}
bool TaskGraphViewModel::selectedCoreNode() const noexcept
{
    const auto *node = selectedNode();
    return node && node->node.coreNode;
}
bool TaskGraphViewModel::empty() const noexcept { return m_nodes.isEmpty(); }
QString TaskGraphViewModel::errorMessage() const { return m_errorMessage; }

} // namespace smartmate::viewmodel
