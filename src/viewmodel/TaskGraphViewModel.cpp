#include "TaskGraphViewModel.h"

#include "TaskErrorMapper.h"
#include "services/TaskService.h"

#include <QMap>
#include <QPointF>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <utility>

namespace smartmate::viewmodel {

namespace {
constexpr qreal nodeWidth = 220.0;
constexpr qreal nodeHeight = 88.0;
constexpr qreal canvasMargin = 48.0;
constexpr qreal layerGap = 120.0;
constexpr qreal rowGap = 28.0;
constexpr qreal arrowLength = 12.0;
constexpr qreal arrowHalfWidth = 6.0;

struct EdgeProjection final {
    model::TaskGraphEdge edge;
    QPointF start;
    QPointF control1;
    QPointF control2;
    QPointF end;
    QPointF arrowTip;
    QPointF arrowLeft;
    QPointF arrowRight;
};

[[nodiscard]] QString blockingReasonText(
    const QList<model::TaskId> &blockingIds,
    const QHash<model::TaskId, QString> &taskTitles,
    const QHash<QString, int> &titleCounts)
{
    if (blockingIds.isEmpty()) {
        return QStringLiteral("存在尚未完成的前置任务");
    }

    QStringList names;
    names.reserve(blockingIds.size());
    for (const model::TaskId &id : blockingIds) {
        const QString title = taskTitles.value(id);
        const QString shortId = id.toString(QUuid::WithoutBraces).left(8);
        if (title.isEmpty()) {
            names.append(QStringLiteral("未知任务（%1）").arg(shortId));
        } else if (titleCounts.value(title) > 1) {
            names.append(QStringLiteral("“%1”（%2）").arg(title, shortId));
        } else {
            names.append(QStringLiteral("“%1”").arg(title));
        }
    }
    return QStringLiteral("等待%1完成").arg(names.join(QStringLiteral("、")));
}
} // namespace

/// QML 只读取此模型的几何角色；修改入口只保留在 TaskGraphViewModel 内部。
class TaskGraphEdgeListModel final : public QAbstractListModel {
public:
    enum Role {
        PredecessorIdRole = Qt::UserRole + 1,
        SuccessorIdRole,
        StartXRole,
        StartYRole,
        Control1XRole,
        Control1YRole,
        Control2XRole,
        Control2YRole,
        EndXRole,
        EndYRole,
        ArrowTipXRole,
        ArrowTipYRole,
        ArrowLeftXRole,
        ArrowLeftYRole,
        ArrowRightXRole,
        ArrowRightYRole,
        SatisfiedRole,
        HighlightedRole,
    };

    explicit TaskGraphEdgeListModel(QObject *parent)
        : QAbstractListModel(parent)
    {
    }

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
    }

    [[nodiscard]] QVariant data(const QModelIndex &index, const int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
            return {};
        }

        const EdgeProjection &row = m_rows.at(index.row());
        switch (role) {
        case PredecessorIdRole:
            return row.edge.dependency.predecessorId.toString(QUuid::WithoutBraces);
        case SuccessorIdRole:
            return row.edge.dependency.successorId.toString(QUuid::WithoutBraces);
        case StartXRole:
            return row.start.x();
        case StartYRole:
            return row.start.y();
        case Control1XRole:
            return row.control1.x();
        case Control1YRole:
            return row.control1.y();
        case Control2XRole:
            return row.control2.x();
        case Control2YRole:
            return row.control2.y();
        case EndXRole:
            return row.end.x();
        case EndYRole:
            return row.end.y();
        case ArrowTipXRole:
            return row.arrowTip.x();
        case ArrowTipYRole:
            return row.arrowTip.y();
        case ArrowLeftXRole:
            return row.arrowLeft.x();
        case ArrowLeftYRole:
            return row.arrowLeft.y();
        case ArrowRightXRole:
            return row.arrowRight.x();
        case ArrowRightYRole:
            return row.arrowRight.y();
        case SatisfiedRole:
            return row.edge.satisfied;
        case HighlightedRole:
            return !m_selectedTaskId.isNull()
                && (row.edge.dependency.predecessorId == m_selectedTaskId
                    || row.edge.dependency.successorId == m_selectedTaskId);
        default:
            return {};
        }
    }

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override
    {
        return {
            {PredecessorIdRole, "predecessorId"},
            {SuccessorIdRole, "successorId"},
            {StartXRole, "startX"},
            {StartYRole, "startY"},
            {Control1XRole, "control1X"},
            {Control1YRole, "control1Y"},
            {Control2XRole, "control2X"},
            {Control2YRole, "control2Y"},
            {EndXRole, "endX"},
            {EndYRole, "endY"},
            {ArrowTipXRole, "arrowTipX"},
            {ArrowTipYRole, "arrowTipY"},
            {ArrowLeftXRole, "arrowLeftX"},
            {ArrowLeftYRole, "arrowLeftY"},
            {ArrowRightXRole, "arrowRightX"},
            {ArrowRightYRole, "arrowRightY"},
            {SatisfiedRole, "satisfied"},
            {HighlightedRole, "highlighted"},
        };
    }

    void replaceRows(QList<EdgeProjection> rows)
    {
        beginResetModel();
        m_rows = std::move(rows);
        endResetModel();
    }

    void setSelectedTask(const model::TaskId &taskId)
    {
        if (m_selectedTaskId == taskId) {
            return;
        }
        m_selectedTaskId = taskId;
        if (!m_rows.isEmpty()) {
            emit dataChanged(index(0), index(m_rows.size() - 1), {HighlightedRole});
        }
    }

private:
    QList<EdgeProjection> m_rows;
    model::TaskId m_selectedTaskId;
};

TaskGraphViewModel::TaskGraphViewModel(model::TaskService &taskService, QObject *parent)
    : QAbstractListModel(parent)
    , m_taskService(taskService)
    , m_edges(new TaskGraphEdgeListModel(this))
{
    connect(&m_taskService, &model::TaskService::tasksChanged,
            this, &TaskGraphViewModel::reload);
    connect(&m_taskService, &model::TaskService::dependenciesChanged,
            this, &TaskGraphViewModel::reload);
    reload();
}

int TaskGraphViewModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_nodes.size());
}

QVariant TaskGraphViewModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_nodes.size()) {
        return {};
    }

    const NodeProjection &projection = m_nodes.at(index.row());
    const model::Task &task = projection.node.task;
    switch (role) {
    case TaskIdRole:
        return task.id().toString(QUuid::WithoutBraces);
    case ShortIdRole:
        return task.id().toString(QUuid::WithoutBraces).left(8);
    case TitleRole:
        return task.title();
    case StatusTextRole:
        return statusText(task.status());
    case PriorityTextRole:
        return priorityText(task.priority());
    case BlockedRole:
        return projection.node.dependencyState.blocked;
    case BlockingReasonTextRole:
        return projection.blockingReasonText;
    case ArchivedRole:
        return task.status() == model::TaskStatus::Archived;
    case CanEditDependenciesRole:
        return task.status() == model::TaskStatus::Todo;
    case NodeXRole:
        return projection.x;
    case NodeYRole:
        return projection.y;
    case NodeWidthRole:
        return nodeWidth;
    case NodeHeightRole:
        return nodeHeight;
    case SelectedRole:
        return task.id() == m_selectedTaskId;
    default:
        return {};
    }
}

QHash<int, QByteArray> TaskGraphViewModel::roleNames() const
{
    return {
        {TaskIdRole, "taskId"},
        {ShortIdRole, "shortId"},
        {TitleRole, "title"},
        {StatusTextRole, "statusText"},
        {PriorityTextRole, "priorityText"},
        {BlockedRole, "blocked"},
        {BlockingReasonTextRole, "blockingReasonText"},
        {ArchivedRole, "archived"},
        {CanEditDependenciesRole, "canEditDependencies"},
        {NodeXRole, "nodeX"},
        {NodeYRole, "nodeY"},
        {NodeWidthRole, "nodeWidth"},
        {NodeHeightRole, "nodeHeight"},
        {SelectedRole, "selected"},
    };
}

QAbstractItemModel *TaskGraphViewModel::edges() noexcept
{
    return m_edges;
}

qreal TaskGraphViewModel::contentWidth() const noexcept
{
    return m_contentWidth;
}

qreal TaskGraphViewModel::contentHeight() const noexcept
{
    return m_contentHeight;
}

QString TaskGraphViewModel::selectedTaskId() const
{
    return m_selectedTaskId.isNull()
        ? QString{}
        : m_selectedTaskId.toString(QUuid::WithoutBraces);
}

QString TaskGraphViewModel::selectedTaskTitle() const
{
    const NodeProjection *projection = selectedNode();
    return projection != nullptr ? projection->node.task.title() : QString{};
}

QString TaskGraphViewModel::selectedStatusText() const
{
    const NodeProjection *projection = selectedNode();
    return projection != nullptr ? statusText(projection->node.task.status()) : QString{};
}

QString TaskGraphViewModel::selectedPriorityText() const
{
    const NodeProjection *projection = selectedNode();
    return projection != nullptr ? priorityText(projection->node.task.priority()) : QString{};
}

QString TaskGraphViewModel::selectedBlockingReason() const
{
    const NodeProjection *projection = selectedNode();
    return projection != nullptr ? projection->blockingReasonText : QString{};
}

int TaskGraphViewModel::selectedPredecessorCount() const noexcept
{
    return m_predecessorCounts.value(m_selectedTaskId);
}

int TaskGraphViewModel::selectedSuccessorCount() const noexcept
{
    return m_successorCounts.value(m_selectedTaskId);
}

bool TaskGraphViewModel::canEditSelectedDependencies() const noexcept
{
    const NodeProjection *projection = selectedNode();
    return projection != nullptr
        && projection->node.task.status() == model::TaskStatus::Todo;
}

bool TaskGraphViewModel::empty() const noexcept
{
    return m_nodes.isEmpty();
}

QString TaskGraphViewModel::errorMessage() const
{
    return m_errorMessage;
}

void TaskGraphViewModel::reload()
{
    const auto result = m_taskService.taskGraphSnapshot();
    if (!result.ok()) {
        setErrorMessage(taskErrorMessage(result.error));
        return;
    }

    replaceGraph(*result.value);
    setErrorMessage({});
}

bool TaskGraphViewModel::selectTask(const QString &taskId)
{
    const model::TaskId id = QUuid::fromString(taskId.trimmed());
    if (id.isNull() || rowForTask(id) < 0) {
        return false;
    }
    if (m_selectedTaskId == id) {
        return true;
    }

    const model::TaskId previous = m_selectedTaskId;
    m_selectedTaskId = id;
    notifySelectedRoles(previous);
    return true;
}

void TaskGraphViewModel::clearSelection()
{
    if (m_selectedTaskId.isNull()) {
        return;
    }
    const model::TaskId previous = m_selectedTaskId;
    m_selectedTaskId = model::TaskId{};
    notifySelectedRoles(previous);
}

int TaskGraphViewModel::rowForTask(const model::TaskId &taskId) const
{
    for (int row = 0; row < m_nodes.size(); ++row) {
        if (m_nodes.at(row).node.task.id() == taskId) {
            return row;
        }
    }
    return -1;
}

const TaskGraphViewModel::NodeProjection *TaskGraphViewModel::selectedNode() const
{
    const int row = rowForTask(m_selectedTaskId);
    return row >= 0 ? &m_nodes.at(row) : nullptr;
}

QString TaskGraphViewModel::statusText(const model::TaskStatus status)
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
    return QStringLiteral("未知");
}

QString TaskGraphViewModel::priorityText(const model::TaskPriority priority)
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
    return QStringLiteral("未知");
}

void TaskGraphViewModel::replaceGraph(const model::TaskGraphSnapshot &snapshot)
{
    QHash<model::TaskId, QString> taskTitles;
    QHash<QString, int> titleCounts;
    QMap<int, int> layerCounts;
    int maximumLevel = 0;
    int maximumRows = 0;
    for (const model::TaskGraphNode &node : snapshot.nodes) {
        taskTitles.insert(node.task.id(), node.task.title());
        titleCounts[node.task.title()] += 1;
        const int level = std::max(0, node.dependencyLevel);
        const int count = ++layerCounts[level];
        maximumLevel = std::max(maximumLevel, level);
        maximumRows = std::max(maximumRows, count);
    }

    const qreal newContentWidth = snapshot.nodes.isEmpty()
        ? 0.0
        : canvasMargin * 2.0 + (maximumLevel + 1) * nodeWidth
            + maximumLevel * layerGap;
    const qreal tallestLayerHeight = maximumRows > 0
        ? maximumRows * nodeHeight + (maximumRows - 1) * rowGap
        : 0.0;
    const qreal newContentHeight = snapshot.nodes.isEmpty()
        ? 0.0
        : canvasMargin * 2.0 + tallestLayerHeight;

    QList<NodeProjection> nodes;
    nodes.reserve(snapshot.nodes.size());
    QMap<int, int> rowsUsed;
    QHash<model::TaskId, QPointF> positions;
    for (const model::TaskGraphNode &node : snapshot.nodes) {
        const int level = std::max(0, node.dependencyLevel);
        const int row = rowsUsed[level]++;
        const qreal layerHeight = layerCounts.value(level) * nodeHeight
            + std::max(0, layerCounts.value(level) - 1) * rowGap;
        const qreal x = canvasMargin + level * (nodeWidth + layerGap);
        const qreal y = canvasMargin + (tallestLayerHeight - layerHeight) / 2.0
            + row * (nodeHeight + rowGap);
        positions.insert(node.task.id(), QPointF{x, y});
        nodes.append(NodeProjection{
            node,
            node.dependencyState.blocked
                ? blockingReasonText(node.dependencyState.unsatisfiedPredecessorIds,
                                     taskTitles, titleCounts)
                : QString{},
            x,
            y,
        });
    }

    QList<EdgeProjection> edges;
    edges.reserve(snapshot.edges.size());
    QHash<model::TaskId, int> predecessorCounts;
    QHash<model::TaskId, int> successorCounts;
    for (const model::TaskGraphEdge &edge : snapshot.edges) {
        const auto source = positions.constFind(edge.dependency.predecessorId);
        const auto target = positions.constFind(edge.dependency.successorId);
        if (source == positions.cend() || target == positions.cend()) {
            continue;
        }

        const QPointF start{source->x() + nodeWidth, source->y() + nodeHeight / 2.0};
        const QPointF end{target->x(), target->y() + nodeHeight / 2.0};
        const qreal controlDistance = std::max<qreal>(40.0, (end.x() - start.x()) * 0.45);
        const QPointF control1{start.x() + controlDistance, start.y()};
        const QPointF control2{end.x() - controlDistance, end.y()};

        qreal directionX = end.x() - control2.x();
        qreal directionY = end.y() - control2.y();
        const qreal directionLength = std::hypot(directionX, directionY);
        if (directionLength > 0.0) {
            directionX /= directionLength;
            directionY /= directionLength;
        } else {
            directionX = 1.0;
            directionY = 0.0;
        }
        const QPointF arrowBase{end.x() - directionX * arrowLength,
                                end.y() - directionY * arrowLength};
        const QPointF arrowLeft{arrowBase.x() - directionY * arrowHalfWidth,
                                arrowBase.y() + directionX * arrowHalfWidth};
        const QPointF arrowRight{arrowBase.x() + directionY * arrowHalfWidth,
                                 arrowBase.y() - directionX * arrowHalfWidth};
        edges.append({edge, start, control1, control2, end, end,
                      arrowLeft, arrowRight});
        predecessorCounts[edge.dependency.successorId] += 1;
        successorCounts[edge.dependency.predecessorId] += 1;
    }

    const model::TaskId oldSelection = m_selectedTaskId;
    beginResetModel();
    m_nodes = std::move(nodes);
    m_predecessorCounts = std::move(predecessorCounts);
    m_successorCounts = std::move(successorCounts);
    if (rowForTask(m_selectedTaskId) < 0) {
        m_selectedTaskId = model::TaskId{};
    }
    endResetModel();
    m_edges->replaceRows(std::move(edges));
    m_edges->setSelectedTask(m_selectedTaskId);

    const bool widthChanged = m_contentWidth != newContentWidth;
    const bool heightChanged = m_contentHeight != newContentHeight;
    m_contentWidth = newContentWidth;
    m_contentHeight = newContentHeight;
    if (widthChanged) {
        emit contentWidthChanged();
    }
    if (heightChanged) {
        emit contentHeightChanged();
    }
    emit graphChanged();
    if (!oldSelection.isNull() || !m_selectedTaskId.isNull()) {
        emit selectionChanged();
    }
}

void TaskGraphViewModel::notifySelectedRoles(const model::TaskId &oldSelection)
{
    const int oldRow = rowForTask(oldSelection);
    if (oldRow >= 0) {
        emit dataChanged(index(oldRow), index(oldRow), {SelectedRole});
    }
    const int newRow = rowForTask(m_selectedTaskId);
    if (newRow >= 0 && newRow != oldRow) {
        emit dataChanged(index(newRow), index(newRow), {SelectedRole});
    }
    m_edges->setSelectedTask(m_selectedTaskId);
    emit selectionChanged();
}

void TaskGraphViewModel::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message) {
        return;
    }
    m_errorMessage = message;
    emit errorMessageChanged();
}

} // namespace smartmate::viewmodel
