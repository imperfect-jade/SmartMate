#include "TaskGraphViewModel.h"

#include "TaskErrorMapper.h"
#include "TaskPresentationFormatter.h"
#include "TaskCategoryPresentation.h"
#include "TaskGraphProjectionModels.h"
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

void TaskGraphViewModel::setSearchText(const QString &searchText)
{
    if (m_searchText == searchText) return;
    m_searchText = searchText;
    emit searchTextChanged();
    notifyInteractionRoles();
}

void TaskGraphViewModel::setStatusFilterIndex(const int index)
{
    const TaskGraphStatusFilter normalized = taskGraphStatusFilterFromIndex(index);
    if (m_statusFilter == normalized) return;
    m_statusFilter = normalized;
    emit statusFilterIndexChanged();
    notifyInteractionRoles();
}

void TaskGraphViewModel::reload()
{
    // 拓扑层级、闭包、类别裁剪和边满足状态均由 Model 计算；此处只请求快照并布局。
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
            m_categorySource.categories().cbegin(),
            m_categorySource.categories().cend(),
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
    // Widget 只提交稳定 TaskId；行号仅用于当前快照内定位，不作为任务身份。
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
    switch (m_statusFilter) {
    case TaskGraphStatusFilter::Todo:
        statusMatches = task.status() == model::TaskStatus::Todo;
        break;
    case TaskGraphStatusFilter::InProgress:
        statusMatches = task.status() == model::TaskStatus::InProgress;
        break;
    case TaskGraphStatusFilter::Blocked:
        statusMatches = projection.node.dependencyState.blocked;
        break;
    case TaskGraphStatusFilter::Done:
        statusMatches = task.status() == model::TaskStatus::Done;
        break;
    case TaskGraphStatusFilter::All:
        break;
    }
    return textMatches && statusMatches;
}

void TaskGraphViewModel::replaceGraph(const model::TaskGraphSnapshot &snapshot)
{
    // 节点坐标和连线路径是展示投影，不回写 Repository，也不污染领域快照。
    TaskGraphLayoutResult layout = layoutTaskGraph(snapshot);

    const model::TaskId oldSelection = m_selectedTaskId;
    // 图结构和行序整体变化时使用模型重置，保证 Widget 不会读到新旧节点混合状态。
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
    // 搜索、悬停和选择只影响交互 Role，无需重置节点或重新执行图布局。
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
    // 侧栏只投影所选节点的直接入边、出边；连接闭包仍直接使用 Model 快照结果。
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
        target->append({task.id(), task.title(), taskStatusText(task.status()), relation});
    }
    m_selectedPredecessors->replaceRows(std::move(predecessors));
    m_selectedSuccessors->replaceRows(std::move(successors));
}

void TaskGraphViewModel::setErrorMessage(const QString &message)
{
    // notificationRaised 是一次性展示事件；errorMessageChanged 支持 Widget 同步绑定当前状态。
    if (!message.isEmpty()) {
        emit notificationRaised({smartmate::common::UiSeverity::Error,
                                 QStringLiteral("依赖图操作失败"),
                                 message});
    }
    if (m_errorMessage == message) return;
    m_errorMessage = message;
    emit errorMessageChanged();
}

void TaskGraphViewModel::applyCategories()
{
    emit categoryOptionsChanged();

    bool filterInvalidated = false;
    if (m_categoryFilterMode == 2) {
        const auto &categories = m_categorySource.categories();
        const bool exists = std::any_of(
            categories.cbegin(), categories.cend(),
            [this](const auto &category) {
                return category.id == m_categoryFilterCategoryId;
            });
        if (!exists) {
            // 类别被删除后退回“未分类”范围，避免 Contract 保留已失效的稳定类别 ID。
            m_categoryFilterMode = 1;
            m_categoryFilterCategoryId = model::TaskCategoryId{};
            emit categoryFilterChanged();
            filterInvalidated = true;
        }
    }
    if (!m_nodes.isEmpty()) {
        emit dataChanged(index(0), index(m_nodes.size() - 1),
                         {CategoryNameRole, CategoryAccentRole,
                          HasCategoryRole});
    }
    if (filterInvalidated) reload();
    emit selectionChanged();
}

const model::TaskCategory *TaskGraphViewModel::categoryForTask(
    const model::Task &task) const
{
    if (!task.categoryId().has_value()) return nullptr;
    const auto &categories = m_categorySource.categories();
    const auto iterator = std::find_if(
        categories.cbegin(), categories.cend(), [&](const auto &category) {
            return category.id == *task.categoryId();
        });
    return iterator == categories.cend() ? nullptr : &*iterator;
}

} // namespace smartmate::viewmodel
