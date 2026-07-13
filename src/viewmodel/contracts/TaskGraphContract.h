#pragma once

#include "common/presentation/UiNotification.h"

#include <QAbstractListModel>
#include <QVariantList>

namespace smartmate::viewmodel {

/// 依赖图节点、布局与选择详情的抽象展示契约。
class TaskGraphContract : public QAbstractListModel {
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
    Q_PROPERTY(bool canEditSelectedDependencies READ canEditSelectedDependencies NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedCategoryName READ selectedCategoryName NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedCategoryAccent READ selectedCategoryAccent NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedHasCategory READ selectedHasCategory NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedCoreNode READ selectedCoreNode NOTIFY selectionChanged)
    Q_PROPERTY(bool empty READ empty NOTIFY graphChanged)

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

    /// edges() 子模型的稳定 Role；Widget 不得通过 roleName 字符串反射读取边。
    enum EdgeRole {
        EdgePredecessorIdRole = Qt::UserRole + 1,
        EdgeSuccessorIdRole,
        EdgeRoutePointsRole,
        EdgeArrowTipXRole,
        EdgeArrowTipYRole,
        EdgeArrowLeftXRole,
        EdgeArrowLeftYRole,
        EdgeArrowRightXRole,
        EdgeArrowRightYRole,
        EdgeSatisfiedRole,
        EdgeCancelledRole,
        EdgeHighlightedRole,
        EdgeDimmedRole,
        EdgeHoveredRole,
    };
    Q_ENUM(EdgeRole)

    /// selectedPredecessors()/selectedSuccessors() 子模型的稳定 Role。
    enum RelationRole {
        RelationTaskIdRole = Qt::UserRole + 1,
        RelationTitleRole,
        RelationStatusTextRole,
        RelationTextRole,
    };
    Q_ENUM(RelationRole)

    ~TaskGraphContract() override = default;

    [[nodiscard]] virtual QAbstractItemModel *edges() noexcept = 0;
    [[nodiscard]] virtual QAbstractItemModel *selectedPredecessors() noexcept = 0;
    [[nodiscard]] virtual QAbstractItemModel *selectedSuccessors() noexcept = 0;
    [[nodiscard]] virtual qreal contentWidth() const noexcept = 0;
    [[nodiscard]] virtual qreal contentHeight() const noexcept = 0;
    [[nodiscard]] virtual QString searchText() const = 0;
    virtual void setSearchText(const QString &searchText) = 0;
    [[nodiscard]] virtual int statusFilterIndex() const noexcept = 0;
    virtual void setStatusFilterIndex(int index) = 0;
    [[nodiscard]] virtual QVariantList categoryFilterOptions() const = 0;
    [[nodiscard]] virtual int categoryFilterMode() const noexcept = 0;
    [[nodiscard]] virtual QString categoryFilterCategoryId() const = 0;
    [[nodiscard]] virtual int taskCount() const noexcept = 0;
    [[nodiscard]] virtual int blockedCount() const noexcept = 0;
    [[nodiscard]] virtual QString currentTaskId() const = 0;
    [[nodiscard]] virtual QString selectedTaskId() const = 0;
    [[nodiscard]] virtual QString selectedTaskTitle() const = 0;
    [[nodiscard]] virtual QString selectedDescription() const = 0;
    [[nodiscard]] virtual QString selectedStatusText() const = 0;
    [[nodiscard]] virtual QString selectedPriorityText() const = 0;
    [[nodiscard]] virtual QString selectedDeadlineText() const = 0;
    [[nodiscard]] virtual QString selectedEstimatedDurationText() const = 0;
    [[nodiscard]] virtual QString selectedBlockingReason() const = 0;
    [[nodiscard]] virtual int selectedUnlockCount() const noexcept = 0;
    [[nodiscard]] virtual int selectedPredecessorCount() const noexcept = 0;
    [[nodiscard]] virtual int selectedSuccessorCount() const noexcept = 0;
    [[nodiscard]] virtual qreal selectedNodeCenterX() const noexcept = 0;
    [[nodiscard]] virtual qreal selectedNodeCenterY() const noexcept = 0;
    [[nodiscard]] virtual bool canEditSelectedDependencies() const noexcept = 0;
    [[nodiscard]] virtual QString selectedCategoryName() const = 0;
    [[nodiscard]] virtual QString selectedCategoryAccent() const = 0;
    [[nodiscard]] virtual bool selectedHasCategory() const noexcept = 0;
    [[nodiscard]] virtual bool selectedCoreNode() const noexcept = 0;
    [[nodiscard]] virtual bool empty() const noexcept = 0;

public slots:
    virtual void reload() = 0;
    virtual bool selectTask(const QString &taskId) = 0;
    virtual void clearSelection() = 0;
    virtual bool locateFirstMatch() = 0;
    virtual bool selectCurrentTask() = 0;
    virtual bool setCategoryFilter(int mode, const QString &categoryId = {}) = 0;
    virtual void setHoveredTask(const QString &taskId) = 0;
    virtual void clearHoveredTask() = 0;

signals:
    void contentWidthChanged();
    void contentHeightChanged();
    void searchTextChanged();
    void statusFilterIndexChanged();
    void categoryOptionsChanged();
    void categoryFilterChanged();
    void selectionChanged();
    void graphChanged();
    void notificationRaised(const smartmate::common::UiNotification &notification);

protected:
    explicit TaskGraphContract(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {
    }
};

} // namespace smartmate::viewmodel
