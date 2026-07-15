#pragma once

#include "domain/TaskGraph.h"
#include "TaskProjectionSources.h"
#include "TaskGraphLayout.h"
#include "viewmodel/contracts/TaskGraphContract.h"

#include <QHash>
#include <QVariantList>

namespace smartmate::model {
class TaskService;
}

namespace smartmate::viewmodel {

class TaskGraphEdgeListModel;
class TaskGraphRelationListModel;

/// 将领域依赖图投影为纵向分层节点、正交边路径和只读详情数据。
/// 拓扑层级与上下游闭包来自 Model；像素布局、筛选和交互强调属于 ViewModel。
/// Service 变化通知触发完整重载，搜索/选择/悬停只更新会话投影和最小 Role 范围。
class TaskGraphViewModel final : public TaskGraphContract {
    Q_OBJECT
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
public:
    TaskGraphViewModel(model::TaskService &taskService,
                       TaskCategoryProjectionSource &categorySource,
                       QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QAbstractItemModel *edges() noexcept override;
    [[nodiscard]] QAbstractItemModel *selectedPredecessors() noexcept override;
    [[nodiscard]] QAbstractItemModel *selectedSuccessors() noexcept override;
    [[nodiscard]] qreal contentWidth() const noexcept override;
    [[nodiscard]] qreal contentHeight() const noexcept override;
    [[nodiscard]] QString searchText() const override;
    void setSearchText(const QString &searchText) override;
    [[nodiscard]] int statusFilterIndex() const noexcept override;
    [[nodiscard]] QStringList statusFilterOptions() const override;
    void setStatusFilterIndex(int index) override;
    [[nodiscard]] QVariantList categoryFilterOptions() const override;
    [[nodiscard]] int categoryFilterMode() const noexcept override;
    [[nodiscard]] QString categoryFilterCategoryId() const override;
    [[nodiscard]] int taskCount() const noexcept override;
    [[nodiscard]] int blockedCount() const noexcept override;
    [[nodiscard]] QString currentTaskId() const override;
    [[nodiscard]] QString selectedTaskId() const override;
    [[nodiscard]] QString selectedTaskTitle() const override;
    [[nodiscard]] QString selectedDescription() const override;
    [[nodiscard]] QString selectedStatusText() const override;
    [[nodiscard]] QString selectedPriorityText() const override;
    [[nodiscard]] QString selectedDeadlineText() const override;
    [[nodiscard]] QString selectedEstimatedDurationText() const override;
    [[nodiscard]] QString selectedBlockingReason() const override;
    [[nodiscard]] int selectedUnlockCount() const noexcept override;
    [[nodiscard]] int selectedPredecessorCount() const noexcept override;
    [[nodiscard]] int selectedSuccessorCount() const noexcept override;
    [[nodiscard]] qreal selectedNodeCenterX() const noexcept override;
    [[nodiscard]] qreal selectedNodeCenterY() const noexcept override;
    [[nodiscard]] bool canEditSelectedDependencies() const noexcept override;
    [[nodiscard]] QString selectedCategoryName() const override;
    [[nodiscard]] QString selectedCategoryAccent() const override;
    [[nodiscard]] bool selectedHasCategory() const noexcept override;
    [[nodiscard]] bool selectedCoreNode() const noexcept override;
    [[nodiscard]] bool empty() const noexcept override;
    [[nodiscard]] QString errorMessage() const;

    void reload() override;
    bool selectTask(const QString &taskId) override;
    void clearSelection() override;
    bool locateFirstMatch() override;
    bool selectCurrentTask() override;
    /// 重新请求 Model 裁剪后的类别子图；Widget 不得自行删除节点或边。
    bool setCategoryFilter(int mode, const QString &categoryId = {}) override;
    void setHoveredTask(const QString &taskId) override;
    void clearHoveredTask() override;

signals:
    void errorMessageChanged();

private:
    using NodeProjection = TaskGraphNodeProjection;

    /// 将领域稳定 ID 投影为 Contract 使用的不带花括号字符串。
    [[nodiscard]] static QString stableId(const model::TaskId &id);
    [[nodiscard]] int rowForTask(const model::TaskId &taskId) const;
    [[nodiscard]] const NodeProjection *selectedNode() const;
    [[nodiscard]] int emphasisFor(const model::TaskId &taskId) const;
    [[nodiscard]] bool filterMatches(const NodeProjection &projection) const;
    /// 布局领域快照并用模型重置协议原子替换节点、边和统计。
    void replaceGraph(const model::TaskGraphSnapshot &snapshot);
    /// 选择/悬停变化时只通知强调相关节点和边 Role。
    void notifyInteractionRoles();
    /// 根据当前选择和 Model 闭包重建直接前置/后继子模型。
    void rebuildRelationModels();
    /// 去重错误属性通知并发布 UiNotification。
    void setErrorMessage(const QString &message);
    /// 类别变化时刷新筛选选项与节点类别展示，不重算业务关系。
    void applyCategories();
    [[nodiscard]] const model::TaskCategory *categoryForTask(
        const model::Task &task) const;

    /// 非拥有任务 Service 引用，是图领域快照的唯一来源。
    model::TaskService &m_taskService;
    TaskCategoryProjectionSource &m_categorySource;
    /// QObject 子模型，父对象为本 ViewModel；指针地址供 Contract CONSTANT 属性稳定返回。
    TaskGraphEdgeListModel *m_edges;
    TaskGraphRelationListModel *m_selectedPredecessors;
    TaskGraphRelationListModel *m_selectedSuccessors;
    /// 当前节点布局快照，是主 QAbstractListModel 的行数据源。
    QList<NodeProjection> m_nodes;
    /// Model 边语义快照，供选择关系子模型重建。
    QList<model::TaskGraphEdge> m_snapshotEdges;
    /// 直接关系数量的展示缓存。
    QHash<model::TaskId, int> m_predecessorCounts;
    QHash<model::TaskId, int> m_successorCounts;
    /// 选择与悬停稳定 ID，只属于当前图会话。
    model::TaskId m_selectedTaskId;
    model::TaskId m_hoveredTaskId;
    /// 搜索和状态筛选只改变 FilterMatched/定位投影，不持久化。
    QString m_searchText;
    TaskGraphStatusFilter m_statusFilter{TaskGraphStatusFilter::All};
    int m_categoryFilterMode{0};
    model::TaskCategoryId m_categoryFilterCategoryId;
    /// 布局内容尺寸，单位为场景像素。
    qreal m_contentWidth{0.0};
    qreal m_contentHeight{0.0};
    /// 最近一次图查询错误。
    QString m_errorMessage;
};

} // namespace smartmate::viewmodel
