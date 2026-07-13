#pragma once

#include "domain/TaskGraph.h"
#include "viewmodel/contracts/TaskGraphContract.h"

#include <QAbstractListModel>
#include <QPointF>
#include <QSet>
#include <QVariantList>

namespace smartmate::viewmodel {

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

/// 依赖边的只读 QML 投影；高亮与悬停仅属于会话级展示状态。
class TaskGraphEdgeListModel final : public QAbstractListModel {
public:
    enum Role {
        PredecessorIdRole = TaskGraphContract::EdgePredecessorIdRole,
        SuccessorIdRole = TaskGraphContract::EdgeSuccessorIdRole,
        RoutePointsRole = TaskGraphContract::EdgeRoutePointsRole,
        ArrowTipXRole = TaskGraphContract::EdgeArrowTipXRole,
        ArrowTipYRole = TaskGraphContract::EdgeArrowTipYRole,
        ArrowLeftXRole = TaskGraphContract::EdgeArrowLeftXRole,
        ArrowLeftYRole = TaskGraphContract::EdgeArrowLeftYRole,
        ArrowRightXRole = TaskGraphContract::EdgeArrowRightXRole,
        ArrowRightYRole = TaskGraphContract::EdgeArrowRightYRole,
        SatisfiedRole = TaskGraphContract::EdgeSatisfiedRole,
        CancelledRole = TaskGraphContract::EdgeCancelledRole,
        HighlightedRole = TaskGraphContract::EdgeHighlightedRole,
        DimmedRole = TaskGraphContract::EdgeDimmedRole,
        HoveredRole = TaskGraphContract::EdgeHoveredRole,
    };

    explicit TaskGraphEdgeListModel(QObject *parent);
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    void replaceRows(QList<EdgeProjection> rows);
    void setInteraction(const model::TaskId &selectedTaskId,
                        const model::TaskId &hoveredTaskId,
                        QSet<model::TaskId> relatedTaskIds);

private:
    QList<EdgeProjection> m_rows;
    model::TaskId m_selectedTaskId;
    model::TaskId m_hoveredTaskId;
    QSet<model::TaskId> m_relatedTaskIds;
};

/// 选中任务直接前置/后继的只读详情投影。
class TaskGraphRelationListModel final : public QAbstractListModel {
public:
    enum Role {
        TaskIdRole = TaskGraphContract::RelationTaskIdRole,
        TitleRole = TaskGraphContract::RelationTitleRole,
        StatusTextRole = TaskGraphContract::RelationStatusTextRole,
        RelationTextRole = TaskGraphContract::RelationTextRole,
    };

    explicit TaskGraphRelationListModel(QObject *parent);
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    void replaceRows(QList<RelationProjection> rows);

private:
    QList<RelationProjection> m_rows;
};

} // namespace smartmate::viewmodel
