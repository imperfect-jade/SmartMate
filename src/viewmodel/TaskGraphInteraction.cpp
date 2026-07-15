#include "TaskGraphViewModel.h"

#include "TaskErrorMapper.h"
#include "TaskPresentationFormatter.h"
#include "services/TaskService.h"

#include <algorithm>

namespace smartmate::viewmodel {

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

} // namespace smartmate::viewmodel

