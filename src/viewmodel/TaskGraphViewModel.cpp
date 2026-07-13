#include "TaskGraphViewModel.h"

#include "TaskErrorMapper.h"
#include "TaskPresentationFormatter.h"
#include "TaskCategoryPresentation.h"
#include "TaskGraphProjectionModels.h"
#include "services/TaskCategoryService.h"
#include "services/TaskService.h"

#include <QDateTime>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <utility>

namespace smartmate::viewmodel {

namespace {
QString stableId(const model::TaskId &id)
{
    return id.toString(QUuid::WithoutBraces);
}

} // namespace

TaskGraphViewModel::TaskGraphViewModel(model::TaskService &taskService, QObject *parent)
    : TaskGraphViewModel(taskService, nullptr, parent)
{
}

TaskGraphViewModel::TaskGraphViewModel(
    model::TaskService &taskService,
    model::TaskCategoryService &categoryService,
    QObject *parent)
    : TaskGraphViewModel(taskService, &categoryService, parent)
{
}

TaskGraphViewModel::TaskGraphViewModel(
    model::TaskService &taskService,
    model::TaskCategoryService *categoryService,
    QObject *parent)
    : QAbstractListModel(parent)
    , m_taskService(taskService)
    , m_categoryService(categoryService)
    , m_edges(new TaskGraphEdgeListModel(this))
    , m_selectedPredecessors(new TaskGraphRelationListModel(this))
    , m_selectedSuccessors(new TaskGraphRelationListModel(this))
{
    connect(&m_taskService, &model::TaskService::tasksChanged,
            this, &TaskGraphViewModel::reload);
    connect(&m_taskService, &model::TaskService::dependenciesChanged,
            this, &TaskGraphViewModel::reload);
    if (m_categoryService) {
        connect(m_categoryService, &model::TaskCategoryService::categoriesChanged,
                this, &TaskGraphViewModel::reloadCategories);
        connect(m_categoryService,
                &model::TaskCategoryService::taskCategoryAssignmentsChanged,
                this, &TaskGraphViewModel::reload);
    }
    reloadCategories();
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
    case StatusTextRole: return statusText(task.status());
    case StatusIndexRole: return static_cast<int>(task.status());
    case PriorityTextRole: return priorityText(task.priority());
    case DeadlineTextRole: return deadlineText(task);
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
        return category ? taskCategoryAccent(category->color) : QStringLiteral("#94a3b8");
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
int TaskGraphViewModel::statusFilterIndex() const noexcept { return m_statusFilterIndex; }
QVariantList TaskGraphViewModel::categoryFilterOptions() const
{
    QVariantList options;
    options.reserve(m_categories.size() + 2);
    options.append(QVariantMap{{QStringLiteral("mode"), 0},
                               {QStringLiteral("categoryId"), QString{}},
                               {QStringLiteral("name"), QStringLiteral("全部类别")},
                               {QStringLiteral("accent"), QStringLiteral("#64748b")}});
    options.append(QVariantMap{{QStringLiteral("mode"), 1},
                               {QStringLiteral("categoryId"), QString{}},
                               {QStringLiteral("name"), QStringLiteral("未分类")},
                               {QStringLiteral("accent"), QStringLiteral("#94a3b8")}});
    for (const auto &category : m_categories) {
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
{ const auto *node = selectedNode(); return node ? statusText(node->node.task.status()) : QString{}; }
QString TaskGraphViewModel::selectedPriorityText() const
{ const auto *node = selectedNode(); return node ? priorityText(node->node.task.priority()) : QString{}; }
QString TaskGraphViewModel::selectedDeadlineText() const
{ const auto *node = selectedNode(); return node ? deadlineText(node->node.task) : QString{}; }
QString TaskGraphViewModel::selectedEstimatedDurationText() const
{ const auto *node = selectedNode(); return node ? durationText(node->node.task) : QString{}; }
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
    return category ? taskCategoryAccent(category->color) : QStringLiteral("#94a3b8");
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

void TaskGraphViewModel::setSearchText(const QString &searchText)
{
    if (m_searchText == searchText) return;
    m_searchText = searchText;
    emit searchTextChanged();
    notifyInteractionRoles();
}

void TaskGraphViewModel::setStatusFilterIndex(const int index)
{
    const int normalized = std::clamp(index, 0, 4);
    if (m_statusFilterIndex == normalized) return;
    m_statusFilterIndex = normalized;
    emit statusFilterIndexChanged();
    notifyInteractionRoles();
}

void TaskGraphViewModel::reload()
{
    model::TaskGraphQuery query;
    query.scope = static_cast<model::TaskGraphCategoryScope>(m_categoryFilterMode);
    if (m_categoryFilterMode == 2) query.categoryId = m_categoryFilterCategoryId;
    const model::TaskGraphResult result = m_taskService.taskGraphSnapshot(query);
    if (!result.ok()) { setErrorMessage(taskErrorMessage(result.error)); return; }
    setErrorMessage({});
    replaceGraph(result.value.value_or(model::TaskGraphSnapshot{}));
}

bool TaskGraphViewModel::setCategoryFilter(const int mode,
                                           const QString &categoryId)
{
    if (mode < 0 || mode > 2) return false;
    model::TaskCategoryId id;
    if (mode == 2) {
        id = QUuid::fromString(categoryId.trimmed());
        const bool exists = std::any_of(
            m_categories.cbegin(), m_categories.cend(),
            [&id](const auto &category) { return category.id == id; });
        if (id.isNull() || !exists) return false;
    }
    if (m_categoryFilterMode == mode
        && (mode != 2 || m_categoryFilterCategoryId == id)) return true;

    model::TaskGraphQuery query;
    query.scope = static_cast<model::TaskGraphCategoryScope>(mode);
    if (mode == 2) query.categoryId = id;
    const model::TaskGraphResult result = m_taskService.taskGraphSnapshot(query);
    if (!result.ok()) {
        // 查询失败时保留旧筛选与旧画布，避免下拉框和图快照表达不同范围。
        setErrorMessage(taskErrorMessage(result.error));
        return false;
    }

    m_categoryFilterMode = mode;
    m_categoryFilterCategoryId = mode == 2 ? id : model::TaskCategoryId{};
    setErrorMessage({});
    emit categoryFilterChanged();
    replaceGraph(result.value.value_or(model::TaskGraphSnapshot{}));
    return true;
}

bool TaskGraphViewModel::selectTask(const QString &taskId)
{
    const model::TaskId id{QUuid{taskId}};
    if (id.isNull() || rowForTask(id) < 0) return false;
    if (m_selectedTaskId == id) return true;
    m_selectedTaskId = id;
    rebuildRelationModels();
    notifyInteractionRoles();
    emit selectionChanged();
    return true;
}

void TaskGraphViewModel::clearSelection()
{
    if (m_selectedTaskId.isNull()) return;
    m_selectedTaskId = model::TaskId{};
    rebuildRelationModels();
    notifyInteractionRoles();
    emit selectionChanged();
}

bool TaskGraphViewModel::locateFirstMatch()
{
    const QString term = m_searchText.trimmed();
    if (term.isEmpty()) return false;
    const auto iterator = std::find_if(m_nodes.cbegin(), m_nodes.cend(), [&term](const NodeProjection &node) {
        return node.node.task.title().contains(term, Qt::CaseInsensitive)
            || node.node.task.description().contains(term, Qt::CaseInsensitive);
    });
    return iterator != m_nodes.cend() && selectTask(stableId(iterator->node.task.id()));
}

bool TaskGraphViewModel::selectCurrentTask()
{
    const QString taskId = currentTaskId();
    return !taskId.isEmpty() && selectTask(taskId);
}

void TaskGraphViewModel::setHoveredTask(const QString &taskId)
{
    const model::TaskId id{QUuid{taskId}};
    if (m_hoveredTaskId == id) return;
    m_hoveredTaskId = id;
    notifyInteractionRoles();
}
void TaskGraphViewModel::clearHoveredTask()
{
    if (m_hoveredTaskId.isNull()) return;
    m_hoveredTaskId = model::TaskId{};
    notifyInteractionRoles();
}

int TaskGraphViewModel::rowForTask(const model::TaskId &taskId) const
{
    for (int row = 0; row < m_nodes.size(); ++row)
        if (m_nodes.at(row).node.task.id() == taskId) return row;
    return -1;
}

int TaskGraphViewModel::emphasisFor(const model::TaskId &taskId) const
{
    if (m_selectedTaskId.isNull()) return NormalEmphasis;
    if (taskId == m_selectedTaskId) return SelectedEmphasis;
    const NodeProjection *selected = selectedNode();
    if (!selected) return NormalEmphasis;
    for (const model::TaskGraphEdge &edge : m_snapshotEdges) {
        if ((edge.dependency.predecessorId == m_selectedTaskId && edge.dependency.successorId == taskId)
            || (edge.dependency.successorId == m_selectedTaskId && edge.dependency.predecessorId == taskId))
            return DirectEmphasis;
    }
    if (selected->node.predecessorClosureIds.contains(taskId)
        || selected->node.successorClosureIds.contains(taskId)) return TransitiveEmphasis;
    return UnrelatedEmphasis;
}

bool TaskGraphViewModel::filterMatches(const NodeProjection &projection) const
{
    const model::Task &task = projection.node.task;
    const QString term = m_searchText.trimmed();
    const bool textMatches = term.isEmpty()
        || task.title().contains(term, Qt::CaseInsensitive)
        || task.description().contains(term, Qt::CaseInsensitive);
    bool statusMatches = true;
    switch (m_statusFilterIndex) {
    case 1: statusMatches = task.status() == model::TaskStatus::Todo; break;
    case 2: statusMatches = task.status() == model::TaskStatus::InProgress; break;
    case 3: statusMatches = projection.node.dependencyState.blocked; break;
    case 4: statusMatches = task.status() == model::TaskStatus::Done; break;
    default: break;
    }
    return textMatches && statusMatches;
}

QString TaskGraphViewModel::statusText(const model::TaskStatus status)
{
    return taskStatusText(status);
}

QString TaskGraphViewModel::priorityText(const model::TaskPriority priority)
{
    return taskPriorityText(priority);
}

QString TaskGraphViewModel::deadlineText(const model::Task &task)
{
    return taskDeadlineText(task, QStringLiteral("未设置截止时间"));
}

QString TaskGraphViewModel::durationText(const model::Task &task)
{
    return taskDurationText(task, QStringLiteral("未设置预计用时"));
}

void TaskGraphViewModel::replaceGraph(const model::TaskGraphSnapshot &snapshot)
{
    TaskGraphLayoutResult layout = layoutTaskGraph(snapshot);

    const model::TaskId oldSelection = m_selectedTaskId;
    beginResetModel();
    m_nodes = std::move(layout.nodes);
    m_snapshotEdges = snapshot.edges;
    m_predecessorCounts = std::move(layout.predecessorCounts);
    m_successorCounts = std::move(layout.successorCounts);
    if (rowForTask(m_selectedTaskId) < 0) m_selectedTaskId = model::TaskId{};
    endResetModel();
    m_edges->replaceRows(std::move(layout.edges));
    rebuildRelationModels();

    const qreal newWidth = layout.contentWidth;
    const qreal newHeight = layout.contentHeight;
    if (m_contentWidth != newWidth) { m_contentWidth = newWidth; emit contentWidthChanged(); }
    if (m_contentHeight != newHeight) { m_contentHeight = newHeight; emit contentHeightChanged(); }
    notifyInteractionRoles();
    emit graphChanged();
    if (oldSelection != m_selectedTaskId || !m_selectedTaskId.isNull()) emit selectionChanged();
}

void TaskGraphViewModel::notifyInteractionRoles()
{
    if (!m_nodes.isEmpty()) {
        emit dataChanged(index(0), index(m_nodes.size() - 1),
                         {SelectedRole, EmphasisLevelRole, FilterMatchedRole});
    }
    QSet<model::TaskId> related;
    const NodeProjection *selected = selectedNode();
    if (selected) {
        related.insert(m_selectedTaskId);
        for (const model::TaskId &id : selected->node.predecessorClosureIds) {
            related.insert(id);
        }
        for (const model::TaskId &id : selected->node.successorClosureIds) {
            related.insert(id);
        }
    }
    m_edges->setInteraction(m_selectedTaskId, m_hoveredTaskId, std::move(related));
}

void TaskGraphViewModel::rebuildRelationModels()
{
    QList<RelationProjection> predecessors;
    QList<RelationProjection> successors;
    for (const model::TaskGraphEdge &edge : m_snapshotEdges) {
        model::TaskId relatedId;
        QList<RelationProjection> *target = nullptr;
        QString relation;
        if (edge.dependency.successorId == m_selectedTaskId) {
            relatedId = edge.dependency.predecessorId;
            target = &predecessors;
            relation = edge.resolution == model::TaskDependencyResolution::Satisfied
                ? QStringLiteral("已满足")
                : edge.resolution == model::TaskDependencyResolution::Cancelled
                    ? QStringLiteral("已取消") : QStringLiteral("待完成");
        } else if (edge.dependency.predecessorId == m_selectedTaskId) {
            relatedId = edge.dependency.successorId;
            target = &successors;
            relation = QStringLiteral("后继任务");
        }
        if (!target) continue;
        const int row = rowForTask(relatedId);
        if (row < 0) continue;
        const model::Task &task = m_nodes.at(row).node.task;
        target->append({task.id(), task.title(), statusText(task.status()), relation});
    }
    m_selectedPredecessors->replaceRows(std::move(predecessors));
    m_selectedSuccessors->replaceRows(std::move(successors));
}

void TaskGraphViewModel::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message) return;
    m_errorMessage = message;
    emit errorMessageChanged();
}

void TaskGraphViewModel::reloadCategories()
{
    QList<model::TaskCategory> categories;
    if (m_categoryService) {
        const auto result = m_categoryService->listCategories();
        if (!result.ok()) {
            setErrorMessage(QStringLiteral("类别数据访问失败，请稍后重试。"));
            return;
        }
        categories = *result.value;
    }
    m_categories = std::move(categories);
    emit categoryOptionsChanged();

    if (m_categoryFilterMode == 2) {
        const bool exists = std::any_of(
            m_categories.cbegin(), m_categories.cend(),
            [this](const auto &category) {
                return category.id == m_categoryFilterCategoryId;
            });
        if (!exists) {
            m_categoryFilterMode = 1;
            m_categoryFilterCategoryId = model::TaskCategoryId{};
            emit categoryFilterChanged();
        }
    }
    reload();
    emit selectionChanged();
}

const model::TaskCategory *TaskGraphViewModel::categoryForTask(
    const model::Task &task) const
{
    if (!task.categoryId().has_value()) return nullptr;
    const auto iterator = std::find_if(
        m_categories.cbegin(), m_categories.cend(), [&](const auto &category) {
            return category.id == *task.categoryId();
        });
    return iterator == m_categories.cend() ? nullptr : &*iterator;
}

} // namespace smartmate::viewmodel
