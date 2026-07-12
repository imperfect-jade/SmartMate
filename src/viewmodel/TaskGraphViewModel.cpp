#include "TaskGraphViewModel.h"

#include "TaskErrorMapper.h"
#include "services/TaskService.h"

#include <QDateTime>
#include <QMap>
#include <QPointF>
#include <QSet>
#include <QStringList>
#include <QVariantList>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace smartmate::viewmodel {

namespace {
constexpr qreal nodeWidth = 220.0;
constexpr qreal nodeHeight = 92.0;
constexpr qreal canvasMargin = 52.0;
constexpr qreal layerGap = 110.0;
constexpr qreal columnGap = 36.0;
constexpr qreal arrowLength = 12.0;
constexpr qreal arrowHalfWidth = 6.0;
constexpr int layoutSweeps = 4;

QString stableId(const model::TaskId &id)
{
    return id.toString(QUuid::WithoutBraces);
}

struct EdgeProjection final {
    model::TaskGraphEdge edge;
    QVariantList routePoints;
    QPointF arrowTip;
    QPointF arrowLeft;
    QPointF arrowRight;
};

struct RelationProjection final {
    model::TaskId taskId;
    QString title;
    QString statusText;
    QString relationText;
};

struct LayoutItem final {
    QString key;
    int stableOrder{0};
};

QString blockingReasonText(const QList<model::TaskId> &blockingIds,
                           const QHash<model::TaskId, QString> &taskTitles,
                           const QHash<QString, int> &titleCounts)
{
    if (blockingIds.isEmpty()) {
        return QStringLiteral("存在尚未完成或取消的前置任务");
    }
    QStringList names;
    for (const model::TaskId &id : blockingIds) {
        const QString title = taskTitles.value(id);
        const QString shortId = stableId(id).left(8);
        names.append(title.isEmpty() ? QStringLiteral("未知任务（%1）").arg(shortId)
                     : titleCounts.value(title) > 1
                         ? QStringLiteral("“%1”（%2）").arg(title, shortId)
                         : QStringLiteral("“%1”").arg(title));
    }
    return QStringLiteral("等待%1完成或取消").arg(names.join(QStringLiteral("、")));
}

qreal medianPosition(const QList<QString> &neighbors,
                     const QHash<QString, qreal> &positions,
                     const qreal fallback)
{
    QList<qreal> values;
    for (const QString &neighbor : neighbors) {
        if (positions.contains(neighbor)) {
            values.append(positions.value(neighbor));
        }
    }
    if (values.isEmpty()) {
        return fallback;
    }
    std::sort(values.begin(), values.end());
    const qsizetype middle = values.size() / 2;
    return values.size() % 2 == 0
        ? (values.at(middle - 1) + values.at(middle)) / 2.0
        : values.at(middle);
}

QHash<QString, qreal> layerPositions(const QList<LayoutItem> &items)
{
    QHash<QString, qreal> positions;
    for (qsizetype index = 0; index < items.size(); ++index) {
        positions.insert(items.at(index).key, static_cast<qreal>(index));
    }
    return positions;
}

void appendPoint(QVariantList &points, const QPointF &point)
{
    if (!points.isEmpty() && points.constLast().toPointF() == point) {
        return;
    }
    points.append(QVariant::fromValue(point));
}

} // namespace

class TaskGraphEdgeListModel final : public QAbstractListModel {
public:
    enum Role {
        PredecessorIdRole = Qt::UserRole + 1,
        SuccessorIdRole,
        RoutePointsRole,
        ArrowTipXRole,
        ArrowTipYRole,
        ArrowLeftXRole,
        ArrowLeftYRole,
        ArrowRightXRole,
        ArrowRightYRole,
        SatisfiedRole,
        CancelledRole,
        HighlightedRole,
        DimmedRole,
        HoveredRole,
    };

    explicit TaskGraphEdgeListModel(QObject *parent) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
    }

    QVariant data(const QModelIndex &index, const int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
            return {};
        }
        const EdgeProjection &row = m_rows.at(index.row());
        const model::TaskId source = row.edge.dependency.predecessorId;
        const model::TaskId target = row.edge.dependency.successorId;
        const bool selectedPath = !m_selectedTaskId.isNull()
            && m_relatedTaskIds.contains(source) && m_relatedTaskIds.contains(target);
        const bool hovered = !m_hoveredTaskId.isNull()
            && (source == m_hoveredTaskId || target == m_hoveredTaskId);
        switch (role) {
        case PredecessorIdRole: return stableId(source);
        case SuccessorIdRole: return stableId(target);
        case RoutePointsRole: return row.routePoints;
        case ArrowTipXRole: return row.arrowTip.x();
        case ArrowTipYRole: return row.arrowTip.y();
        case ArrowLeftXRole: return row.arrowLeft.x();
        case ArrowLeftYRole: return row.arrowLeft.y();
        case ArrowRightXRole: return row.arrowRight.x();
        case ArrowRightYRole: return row.arrowRight.y();
        case SatisfiedRole:
            return row.edge.resolution == model::TaskDependencyResolution::Satisfied;
        case CancelledRole:
            return row.edge.resolution == model::TaskDependencyResolution::Cancelled;
        case HighlightedRole: return selectedPath;
        case DimmedRole: return !m_selectedTaskId.isNull() && !selectedPath && !hovered;
        case HoveredRole: return hovered;
        default: return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {{PredecessorIdRole, "predecessorId"},
                {SuccessorIdRole, "successorId"},
                {RoutePointsRole, "routePoints"},
                {ArrowTipXRole, "arrowTipX"}, {ArrowTipYRole, "arrowTipY"},
                {ArrowLeftXRole, "arrowLeftX"}, {ArrowLeftYRole, "arrowLeftY"},
                {ArrowRightXRole, "arrowRightX"}, {ArrowRightYRole, "arrowRightY"},
                {SatisfiedRole, "satisfied"}, {CancelledRole, "cancelled"},
                {HighlightedRole, "highlighted"}, {DimmedRole, "dimmed"},
                {HoveredRole, "hovered"}};
    }

    void replaceRows(QList<EdgeProjection> rows)
    {
        beginResetModel();
        m_rows = std::move(rows);
        endResetModel();
    }

    void setInteraction(const model::TaskId &selectedTaskId,
                        const model::TaskId &hoveredTaskId,
                        QSet<model::TaskId> relatedTaskIds)
    {
        m_selectedTaskId = selectedTaskId;
        m_hoveredTaskId = hoveredTaskId;
        m_relatedTaskIds = std::move(relatedTaskIds);
        if (!m_rows.isEmpty()) {
            emit dataChanged(index(0), index(m_rows.size() - 1),
                             {HighlightedRole, DimmedRole, HoveredRole});
        }
    }

private:
    QList<EdgeProjection> m_rows;
    model::TaskId m_selectedTaskId;
    model::TaskId m_hoveredTaskId;
    QSet<model::TaskId> m_relatedTaskIds;
};

class TaskGraphRelationListModel final : public QAbstractListModel {
public:
    enum Role { TaskIdRole = Qt::UserRole + 1, TitleRole, StatusTextRole, RelationTextRole };
    explicit TaskGraphRelationListModel(QObject *parent) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
    }
    QVariant data(const QModelIndex &index, const int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) return {};
        const RelationProjection &row = m_rows.at(index.row());
        switch (role) {
        case TaskIdRole: return stableId(row.taskId);
        case TitleRole: return row.title;
        case StatusTextRole: return row.statusText;
        case RelationTextRole: return row.relationText;
        default: return {};
        }
    }
    QHash<int, QByteArray> roleNames() const override
    {
        return {{TaskIdRole, "taskId"}, {TitleRole, "title"},
                {StatusTextRole, "statusText"}, {RelationTextRole, "relationText"}};
    }
    void replaceRows(QList<RelationProjection> rows)
    {
        beginResetModel(); m_rows = std::move(rows); endResetModel();
    }
private:
    QList<RelationProjection> m_rows;
};

TaskGraphViewModel::TaskGraphViewModel(model::TaskService &taskService, QObject *parent)
    : QAbstractListModel(parent)
    , m_taskService(taskService)
    , m_edges(new TaskGraphEdgeListModel(this))
    , m_selectedPredecessors(new TaskGraphRelationListModel(this))
    , m_selectedSuccessors(new TaskGraphRelationListModel(this))
{
    connect(&m_taskService, &model::TaskService::tasksChanged,
            this, &TaskGraphViewModel::reload);
    connect(&m_taskService, &model::TaskService::dependenciesChanged,
            this, &TaskGraphViewModel::reload);
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
    case CanEditDependenciesRole: return task.status() == model::TaskStatus::Todo;
    case NodeXRole: return projection.x;
    case NodeYRole: return projection.y;
    case NodeWidthRole: return nodeWidth;
    case NodeHeightRole: return nodeHeight;
    case SelectedRole: return task.id() == m_selectedTaskId;
    case EmphasisLevelRole: return emphasisFor(task.id());
    case FilterMatchedRole: return filterMatches(projection);
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
            {EmphasisLevelRole, "emphasisLevel"}, {FilterMatchedRole, "filterMatched"}};
}

QAbstractItemModel *TaskGraphViewModel::edges() noexcept { return m_edges; }
QAbstractItemModel *TaskGraphViewModel::selectedPredecessors() noexcept { return m_selectedPredecessors; }
QAbstractItemModel *TaskGraphViewModel::selectedSuccessors() noexcept { return m_selectedSuccessors; }
qreal TaskGraphViewModel::contentWidth() const noexcept { return m_contentWidth; }
qreal TaskGraphViewModel::contentHeight() const noexcept { return m_contentHeight; }
QString TaskGraphViewModel::searchText() const { return m_searchText; }
int TaskGraphViewModel::statusFilterIndex() const noexcept { return m_statusFilterIndex; }
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
{ const auto *node = selectedNode(); return node ? node->x + nodeWidth / 2.0 : 0.0; }
qreal TaskGraphViewModel::selectedNodeCenterY() const noexcept
{ const auto *node = selectedNode(); return node ? node->y + nodeHeight / 2.0 : 0.0; }
bool TaskGraphViewModel::canEditSelectedDependencies() const noexcept
{ const auto *node = selectedNode(); return node && node->node.task.status() == model::TaskStatus::Todo; }
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
    const model::TaskGraphResult result = m_taskService.taskGraphSnapshot();
    if (!result.ok()) { setErrorMessage(taskErrorMessage(result.error)); return; }
    setErrorMessage({});
    replaceGraph(result.value.value_or(model::TaskGraphSnapshot{}));
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
    switch (status) {
    case model::TaskStatus::Todo: return QStringLiteral("待办");
    case model::TaskStatus::InProgress: return QStringLiteral("进行中");
    case model::TaskStatus::Done: return QStringLiteral("已完成");
    case model::TaskStatus::Cancelled: return QStringLiteral("已取消");
    case model::TaskStatus::Archived: return QStringLiteral("已归档");
    }
    return QStringLiteral("未知");
}

QString TaskGraphViewModel::priorityText(const model::TaskPriority priority)
{
    switch (priority) {
    case model::TaskPriority::Low: return QStringLiteral("低");
    case model::TaskPriority::Normal: return QStringLiteral("普通");
    case model::TaskPriority::High: return QStringLiteral("高");
    case model::TaskPriority::Urgent: return QStringLiteral("紧急");
    }
    return QStringLiteral("未知");
}

QString TaskGraphViewModel::deadlineText(const model::Task &task)
{
    return task.deadline().has_value()
        ? task.deadline()->toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
        : QStringLiteral("未设置截止时间");
}

QString TaskGraphViewModel::durationText(const model::Task &task)
{
    if (!task.estimatedMinutes().has_value()) return QStringLiteral("未设置预计用时");
    const int minutes = *task.estimatedMinutes();
    const int days = minutes / (24 * 60);
    const int hours = (minutes % (24 * 60)) / 60;
    const int minutePart = minutes % 60;
    QStringList parts;
    if (days > 0) parts.append(QStringLiteral("%1天").arg(days));
    if (hours > 0) parts.append(QStringLiteral("%1小时").arg(hours));
    if (minutePart > 0 || parts.isEmpty()) parts.append(QStringLiteral("%1分钟").arg(minutePart));
    return parts.join(QStringLiteral(" "));
}

void TaskGraphViewModel::replaceGraph(const model::TaskGraphSnapshot &snapshot)
{
    QHash<model::TaskId, QString> taskTitles;
    QHash<QString, int> titleCounts;
    QMap<int, QList<LayoutItem>> layers;
    QHash<QString, int> levelByKey;
    QHash<QString, int> stableOrder;
    int maximumLevel = 0;
    int nextStableOrder = 0;
    for (const model::TaskGraphNode &node : snapshot.nodes) {
        const QString key = stableId(node.task.id());
        const int level = std::max(0, node.dependencyLevel);
        taskTitles.insert(node.task.id(), node.task.title());
        ++titleCounts[node.task.title()];
        stableOrder.insert(key, nextStableOrder);
        layers[level].append({key, nextStableOrder++});
        levelByKey.insert(key, level);
        maximumLevel = std::max(maximumLevel, level);
    }

    QHash<QString, QList<QString>> predecessors;
    QHash<QString, QList<QString>> successors;
    QList<QStringList> edgeChains;
    edgeChains.reserve(snapshot.edges.size());
    for (int edgeIndex = 0; edgeIndex < snapshot.edges.size(); ++edgeIndex) {
        const model::TaskGraphEdge &edge = snapshot.edges.at(edgeIndex);
        const QString source = stableId(edge.dependency.predecessorId);
        const QString target = stableId(edge.dependency.successorId);
        const int sourceLevel = levelByKey.value(source);
        const int targetLevel = levelByKey.value(target);
        QStringList chain{source};
        QString previous = source;
        for (int level = sourceLevel + 1; level < targetLevel; ++level) {
            const QString dummy = QStringLiteral("dummy:%1:%2").arg(edgeIndex).arg(level);
            stableOrder.insert(dummy, nextStableOrder);
            layers[level].append({dummy, nextStableOrder++});
            levelByKey.insert(dummy, level);
            successors[previous].append(dummy);
            predecessors[dummy].append(previous);
            previous = dummy;
            chain.append(dummy);
        }
        successors[previous].append(target);
        predecessors[target].append(previous);
        chain.append(target);
        edgeChains.append(chain);
    }

    for (int sweep = 0; sweep < layoutSweeps; ++sweep) {
        for (int level = 1; level <= maximumLevel; ++level) {
            const QHash<QString, qreal> positions = layerPositions(layers.value(level - 1));
            auto &items = layers[level];
            std::stable_sort(items.begin(), items.end(), [&](const LayoutItem &left, const LayoutItem &right) {
                const qreal leftValue = medianPosition(predecessors.value(left.key), positions, stableOrder.value(left.key));
                const qreal rightValue = medianPosition(predecessors.value(right.key), positions, stableOrder.value(right.key));
                return leftValue == rightValue ? left.stableOrder < right.stableOrder : leftValue < rightValue;
            });
        }
        for (int level = maximumLevel - 1; level >= 0; --level) {
            const QHash<QString, qreal> positions = layerPositions(layers.value(level + 1));
            auto &items = layers[level];
            std::stable_sort(items.begin(), items.end(), [&](const LayoutItem &left, const LayoutItem &right) {
                const qreal leftValue = medianPosition(successors.value(left.key), positions, stableOrder.value(left.key));
                const qreal rightValue = medianPosition(successors.value(right.key), positions, stableOrder.value(right.key));
                return leftValue == rightValue ? left.stableOrder < right.stableOrder : leftValue < rightValue;
            });
        }
    }

    int maximumColumns = 0;
    QHash<QString, QPointF> keyCenters;
    QHash<model::TaskId, QPointF> positions;
    for (auto layer = layers.cbegin(); layer != layers.cend(); ++layer) {
        maximumColumns = std::max(maximumColumns, static_cast<int>(layer.value().size()));
    }
    const qreal widestLayer = maximumColumns > 0
        ? maximumColumns * nodeWidth + (maximumColumns - 1) * columnGap : 0.0;
    for (auto layer = layers.cbegin(); layer != layers.cend(); ++layer) {
        const qreal layerWidth = layer.value().size() * nodeWidth
            + std::max<qsizetype>(0, layer.value().size() - 1) * columnGap;
        const qreal left = canvasMargin + (widestLayer - layerWidth) / 2.0;
        for (qsizetype column = 0; column < layer.value().size(); ++column) {
            const QString key = layer.value().at(column).key;
            const qreal x = left + column * (nodeWidth + columnGap);
            const qreal y = canvasMargin + layer.key() * (nodeHeight + layerGap);
            keyCenters.insert(key, {x + nodeWidth / 2.0, y + nodeHeight / 2.0});
        }
    }

    QList<NodeProjection> nodes;
    nodes.reserve(snapshot.nodes.size());
    for (const model::TaskGraphNode &node : snapshot.nodes) {
        const QPointF center = keyCenters.value(stableId(node.task.id()));
        positions.insert(node.task.id(), {center.x() - nodeWidth / 2.0,
                                          center.y() - nodeHeight / 2.0});
        nodes.append({node,
                      node.dependencyState.blocked
                          ? blockingReasonText(node.dependencyState.unsatisfiedPredecessorIds,
                                               taskTitles, titleCounts)
                          : QString{},
                      center.x() - nodeWidth / 2.0,
                      center.y() - nodeHeight / 2.0});
    }

    QHash<model::TaskId, QList<int>> outgoingEdges;
    QHash<model::TaskId, QList<int>> incomingEdges;
    QMap<int, QList<int>> bandEdges;
    for (int edgeIndex = 0; edgeIndex < snapshot.edges.size(); ++edgeIndex) {
        const auto &edge = snapshot.edges.at(edgeIndex);
        outgoingEdges[edge.dependency.predecessorId].append(edgeIndex);
        incomingEdges[edge.dependency.successorId].append(edgeIndex);
        const int sourceLevel = levelByKey.value(stableId(edge.dependency.predecessorId));
        const int targetLevel = levelByKey.value(stableId(edge.dependency.successorId));
        for (int level = sourceLevel; level < targetLevel; ++level) bandEdges[level].append(edgeIndex);
    }
    const auto sortByOppositeX = [&](QList<int> &indexes, const bool outgoing) {
        std::sort(indexes.begin(), indexes.end(), [&](const int left, const int right) {
            const auto &leftEdge = snapshot.edges.at(left).dependency;
            const auto &rightEdge = snapshot.edges.at(right).dependency;
            const model::TaskId leftId = outgoing ? leftEdge.successorId : leftEdge.predecessorId;
            const model::TaskId rightId = outgoing ? rightEdge.successorId : rightEdge.predecessorId;
            const qreal leftX = keyCenters.value(stableId(leftId)).x();
            const qreal rightX = keyCenters.value(stableId(rightId)).x();
            return leftX == rightX ? left < right : leftX < rightX;
        });
    };
    for (auto it = outgoingEdges.begin(); it != outgoingEdges.end(); ++it) sortByOppositeX(it.value(), true);
    for (auto it = incomingEdges.begin(); it != incomingEdges.end(); ++it) sortByOppositeX(it.value(), false);
    for (auto it = bandEdges.begin(); it != bandEdges.end(); ++it) std::sort(it.value().begin(), it.value().end());

    const auto groupedIndexes = [&](const QList<int> &indexes, const int edgeIndex) {
        QList<int> grouped;
        const model::TaskDependencyResolution resolution =
            snapshot.edges.at(edgeIndex).resolution;
        for (const int candidate : indexes) {
            if (snapshot.edges.at(candidate).resolution == resolution) {
                grouped.append(candidate);
            }
        }
        return grouped;
    };
    const auto portOffset = [&](const QList<int> &indexes, const int edgeIndex) {
        // 三条以上同语义扇入/扇出共享中心主干；较小分组继续使用分散端口。
        if (groupedIndexes(indexes, edgeIndex).size() >= 3) return 0.0;
        if (indexes.size() <= 1) return 0.0;
        const qreal span = std::min<qreal>(120.0, nodeWidth - 48.0);
        return -span / 2.0 + indexes.indexOf(edgeIndex) * span / (indexes.size() - 1);
    };
    const auto laneY = [&](const int level, const int edgeIndex,
                           const int sourceLevel, const int targetLevel) {
        const QList<int> indexes = bandEdges.value(level);
        const qreal center = canvasMargin + level * (nodeHeight + layerGap)
            + nodeHeight + layerGap / 2.0;
        if (indexes.size() <= 1) return center;
        int laneEdgeIndex = edgeIndex;
        const model::TaskDependency &dependency = snapshot.edges.at(edgeIndex).dependency;
        if (level == sourceLevel) {
            const QList<int> group = groupedIndexes(
                outgoingEdges.value(dependency.predecessorId), edgeIndex);
            if (group.size() >= 3) laneEdgeIndex = group.constFirst();
        }
        if (level == targetLevel - 1) {
            const QList<int> group = groupedIndexes(
                incomingEdges.value(dependency.successorId), edgeIndex);
            if (group.size() >= 3) laneEdgeIndex = group.constFirst();
        }
        const qreal usable = layerGap - 28.0;
        return center - usable / 2.0
            + indexes.indexOf(laneEdgeIndex) * usable / (indexes.size() - 1);
    };

    QList<EdgeProjection> edges;
    QHash<model::TaskId, int> predecessorCounts;
    QHash<model::TaskId, int> successorCounts;
    for (int edgeIndex = 0; edgeIndex < snapshot.edges.size(); ++edgeIndex) {
        const model::TaskGraphEdge &edge = snapshot.edges.at(edgeIndex);
        const QPointF sourceTopLeft = positions.value(edge.dependency.predecessorId);
        const QPointF targetTopLeft = positions.value(edge.dependency.successorId);
        const qreal startX = sourceTopLeft.x() + nodeWidth / 2.0
            + portOffset(outgoingEdges.value(edge.dependency.predecessorId), edgeIndex);
        const qreal endX = targetTopLeft.x() + nodeWidth / 2.0
            + portOffset(incomingEdges.value(edge.dependency.successorId), edgeIndex);
        const QPointF start{startX, sourceTopLeft.y() + nodeHeight};
        const QPointF end{endX, targetTopLeft.y()};
        QVariantList points;
        appendPoint(points, start);
        const QStringList chain = edgeChains.at(edgeIndex);
        qreal currentX = start.x();
        const int sourceLevel = levelByKey.value(chain.constFirst());
        for (int chainIndex = 0; chainIndex < chain.size() - 1; ++chainIndex) {
            const int level = sourceLevel + chainIndex;
            const qreal nextX = chainIndex == chain.size() - 2
                ? end.x() : keyCenters.value(chain.at(chainIndex + 1)).x();
            const qreal y = laneY(level, edgeIndex, sourceLevel,
                                  levelByKey.value(chain.constLast()));
            appendPoint(points, {currentX, y});
            appendPoint(points, {nextX, y});
            currentX = nextX;
        }
        appendPoint(points, end);
        const QPointF arrowLeft{end.x() - arrowHalfWidth, end.y() - arrowLength};
        const QPointF arrowRight{end.x() + arrowHalfWidth, end.y() - arrowLength};
        edges.append({edge, points, end, arrowLeft, arrowRight});
        ++predecessorCounts[edge.dependency.successorId];
        ++successorCounts[edge.dependency.predecessorId];
    }

    const model::TaskId oldSelection = m_selectedTaskId;
    beginResetModel();
    m_nodes = std::move(nodes);
    m_snapshotEdges = snapshot.edges;
    m_predecessorCounts = std::move(predecessorCounts);
    m_successorCounts = std::move(successorCounts);
    if (rowForTask(m_selectedTaskId) < 0) m_selectedTaskId = model::TaskId{};
    endResetModel();
    m_edges->replaceRows(std::move(edges));
    rebuildRelationModels();

    const qreal newWidth = snapshot.nodes.isEmpty() ? 0.0 : canvasMargin * 2.0 + widestLayer;
    const qreal newHeight = snapshot.nodes.isEmpty() ? 0.0
        : canvasMargin * 2.0 + (maximumLevel + 1) * nodeHeight + maximumLevel * layerGap;
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

} // namespace smartmate::viewmodel
