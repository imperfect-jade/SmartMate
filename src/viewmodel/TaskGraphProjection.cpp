#include "TaskGraphViewModel.h"

#include "TaskGraphProjectionModels.h"
#include "TaskPresentationFormatter.h"

#include <QSet>

#include <algorithm>
#include <utility>

namespace smartmate::viewmodel {

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

