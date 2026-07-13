#pragma once

#include "domain/TaskGraph.h"
#include "TaskGraphLayout.h"

#include <QAbstractListModel>
#include <QHash>
#include <QtQmlIntegration/qqmlintegration.h>
#include <QVariantList>

namespace smartmate::model {
class TaskService;
class TaskCategoryService;
}

namespace smartmate::viewmodel {

class TaskGraphEdgeListModel;
class TaskGraphRelationListModel;

/// 将领域依赖图投影为纵向分层节点、正交边路径和只读详情数据。
/// 拓扑层级与上下游闭包来自 Model；像素布局、筛选和交互强调属于 ViewModel。
class TaskGraphViewModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *edges READ edges CONSTANT)
    Q_PROPERTY(QAbstractItemModel *selectedPredecessors READ selectedPredecessors CONSTANT)
    Q_PROPERTY(QAbstractItemModel *selectedSuccessors READ selectedSuccessors CONSTANT)
    Q_PROPERTY(qreal contentWidth READ contentWidth NOTIFY contentWidthChanged)
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY contentHeightChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(int statusFilterIndex READ statusFilterIndex WRITE setStatusFilterIndex
                   NOTIFY statusFilterIndexChanged)
    Q_PROPERTY(QVariantList categoryFilterOptions READ categoryFilterOptions
                   NOTIFY categoryOptionsChanged)
    Q_PROPERTY(int categoryFilterMode READ categoryFilterMode NOTIFY categoryFilterChanged)
    Q_PROPERTY(QString categoryFilterCategoryId READ categoryFilterCategoryId
                   NOTIFY categoryFilterChanged)
    Q_PROPERTY(int taskCount READ taskCount NOTIFY graphChanged)
    Q_PROPERTY(int blockedCount READ blockedCount NOTIFY graphChanged)
    Q_PROPERTY(QString currentTaskId READ currentTaskId NOTIFY graphChanged)
    Q_PROPERTY(QString selectedTaskId READ selectedTaskId NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedTaskTitle READ selectedTaskTitle NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedDescription READ selectedDescription NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedStatusText READ selectedStatusText NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedPriorityText READ selectedPriorityText NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedDeadlineText READ selectedDeadlineText NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedEstimatedDurationText READ selectedEstimatedDurationText
                   NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedBlockingReason READ selectedBlockingReason NOTIFY selectionChanged)
    Q_PROPERTY(int selectedUnlockCount READ selectedUnlockCount NOTIFY selectionChanged)
    Q_PROPERTY(int selectedPredecessorCount READ selectedPredecessorCount NOTIFY selectionChanged)
    Q_PROPERTY(int selectedSuccessorCount READ selectedSuccessorCount NOTIFY selectionChanged)
    Q_PROPERTY(qreal selectedNodeCenterX READ selectedNodeCenterX NOTIFY selectionChanged)
    Q_PROPERTY(qreal selectedNodeCenterY READ selectedNodeCenterY NOTIFY selectionChanged)
    Q_PROPERTY(bool canEditSelectedDependencies READ canEditSelectedDependencies
                   NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedCategoryName READ selectedCategoryName NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedCategoryAccent READ selectedCategoryAccent NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedHasCategory READ selectedHasCategory NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedCoreNode READ selectedCoreNode NOTIFY selectionChanged)
    Q_PROPERTY(bool empty READ empty NOTIFY graphChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    QML_NAMED_ELEMENT(TaskGraphViewModel)
    QML_UNCREATABLE("TaskGraphViewModel is owned by AppViewModel")

public:
    enum Role {
        TaskIdRole = Qt::UserRole + 1,
        ShortIdRole,
        TitleRole,
        StatusTextRole,
        StatusIndexRole,
        PriorityTextRole,
        DeadlineTextRole,
        UnlockCountRole,
        BlockedRole,
        BlockingReasonTextRole,
        ArchivedRole,
        CanEditDependenciesRole,
        NodeXRole,
        NodeYRole,
        NodeWidthRole,
        NodeHeightRole,
        SelectedRole,
        EmphasisLevelRole,
        FilterMatchedRole,
        CategoryNameRole,
        CategoryAccentRole,
        HasCategoryRole,
        CoreNodeRole,
    };
    Q_ENUM(Role)

    enum EmphasisLevel {
        NormalEmphasis = 0,
        UnrelatedEmphasis,
        TransitiveEmphasis,
        DirectEmphasis,
        SelectedEmphasis,
    };
    Q_ENUM(EmphasisLevel)

    explicit TaskGraphViewModel(model::TaskService &taskService,
                                QObject *parent = nullptr);
    TaskGraphViewModel(model::TaskService &taskService,
                       model::TaskCategoryService &categoryService,
                       QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QAbstractItemModel *edges() noexcept;
    [[nodiscard]] QAbstractItemModel *selectedPredecessors() noexcept;
    [[nodiscard]] QAbstractItemModel *selectedSuccessors() noexcept;
    [[nodiscard]] qreal contentWidth() const noexcept;
    [[nodiscard]] qreal contentHeight() const noexcept;
    [[nodiscard]] QString searchText() const;
    void setSearchText(const QString &searchText);
    [[nodiscard]] int statusFilterIndex() const noexcept;
    void setStatusFilterIndex(int index);
    [[nodiscard]] QVariantList categoryFilterOptions() const;
    [[nodiscard]] int categoryFilterMode() const noexcept;
    [[nodiscard]] QString categoryFilterCategoryId() const;
    [[nodiscard]] int taskCount() const noexcept;
    [[nodiscard]] int blockedCount() const noexcept;
    [[nodiscard]] QString currentTaskId() const;
    [[nodiscard]] QString selectedTaskId() const;
    [[nodiscard]] QString selectedTaskTitle() const;
    [[nodiscard]] QString selectedDescription() const;
    [[nodiscard]] QString selectedStatusText() const;
    [[nodiscard]] QString selectedPriorityText() const;
    [[nodiscard]] QString selectedDeadlineText() const;
    [[nodiscard]] QString selectedEstimatedDurationText() const;
    [[nodiscard]] QString selectedBlockingReason() const;
    [[nodiscard]] int selectedUnlockCount() const noexcept;
    [[nodiscard]] int selectedPredecessorCount() const noexcept;
    [[nodiscard]] int selectedSuccessorCount() const noexcept;
    [[nodiscard]] qreal selectedNodeCenterX() const noexcept;
    [[nodiscard]] qreal selectedNodeCenterY() const noexcept;
    [[nodiscard]] bool canEditSelectedDependencies() const noexcept;
    [[nodiscard]] QString selectedCategoryName() const;
    [[nodiscard]] QString selectedCategoryAccent() const;
    [[nodiscard]] bool selectedHasCategory() const noexcept;
    [[nodiscard]] bool selectedCoreNode() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] QString errorMessage() const;

    Q_INVOKABLE void reload();
    Q_INVOKABLE bool selectTask(const QString &taskId);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE bool locateFirstMatch();
    Q_INVOKABLE bool selectCurrentTask();
    /// 重新请求Model裁剪后的类别子图；QML不得自行删除节点或边。
    Q_INVOKABLE bool setCategoryFilter(int mode, const QString &categoryId = {});
    Q_INVOKABLE void setHoveredTask(const QString &taskId);
    Q_INVOKABLE void clearHoveredTask();

signals:
    void contentWidthChanged();
    void contentHeightChanged();
    void searchTextChanged();
    void statusFilterIndexChanged();
    void categoryOptionsChanged();
    void categoryFilterChanged();
    void selectionChanged();
    void graphChanged();
    void errorMessageChanged();

private:
    using NodeProjection = TaskGraphNodeProjection;

    [[nodiscard]] int rowForTask(const model::TaskId &taskId) const;
    [[nodiscard]] const NodeProjection *selectedNode() const;
    [[nodiscard]] int emphasisFor(const model::TaskId &taskId) const;
    [[nodiscard]] bool filterMatches(const NodeProjection &projection) const;
    [[nodiscard]] static QString statusText(model::TaskStatus status);
    [[nodiscard]] static QString priorityText(model::TaskPriority priority);
    [[nodiscard]] static QString deadlineText(const model::Task &task);
    [[nodiscard]] static QString durationText(const model::Task &task);
    void replaceGraph(const model::TaskGraphSnapshot &snapshot);
    void notifyInteractionRoles();
    void rebuildRelationModels();
    void setErrorMessage(const QString &message);
    void reloadCategories();
    [[nodiscard]] const model::TaskCategory *categoryForTask(
        const model::Task &task) const;

    TaskGraphViewModel(model::TaskService &taskService,
                       model::TaskCategoryService *categoryService,
                       QObject *parent);

    model::TaskService &m_taskService;
    model::TaskCategoryService *m_categoryService{nullptr};
    TaskGraphEdgeListModel *m_edges;
    TaskGraphRelationListModel *m_selectedPredecessors;
    TaskGraphRelationListModel *m_selectedSuccessors;
    QList<NodeProjection> m_nodes;
    QList<model::TaskGraphEdge> m_snapshotEdges;
    QHash<model::TaskId, int> m_predecessorCounts;
    QHash<model::TaskId, int> m_successorCounts;
    model::TaskId m_selectedTaskId;
    model::TaskId m_hoveredTaskId;
    QString m_searchText;
    int m_statusFilterIndex{0};
    QList<model::TaskCategory> m_categories;
    int m_categoryFilterMode{0};
    model::TaskCategoryId m_categoryFilterCategoryId;
    qreal m_contentWidth{0.0};
    qreal m_contentHeight{0.0};
    QString m_errorMessage;
};

} // namespace smartmate::viewmodel
