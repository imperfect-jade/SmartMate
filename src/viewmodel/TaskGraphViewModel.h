#pragma once

#include "domain/TaskGraph.h"

#include <QAbstractItemModel>
#include <QAbstractListModel>
#include <QHash>
#include <QtQmlIntegration/qqmlintegration.h>

namespace smartmate::model {
class TaskService;
}

namespace smartmate::viewmodel {

class TaskGraphEdgeListModel;

/// 将 Model 的结构化依赖图投影为 QML 可声明式绘制的节点和箭头几何。
///
/// 拓扑层级与可见节点由 Model 决定；本类只计算像素坐标、贝塞尔控制点和
/// 箭头三角形，QML 因而无需遍历依赖图或实现几何算法。
class TaskGraphViewModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *edges READ edges CONSTANT)
    Q_PROPERTY(qreal contentWidth READ contentWidth NOTIFY contentWidthChanged)
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY contentHeightChanged)
    Q_PROPERTY(QString selectedTaskId READ selectedTaskId NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedTaskTitle READ selectedTaskTitle NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedStatusText READ selectedStatusText NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedPriorityText READ selectedPriorityText NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedBlockingReason READ selectedBlockingReason
                   NOTIFY selectionChanged)
    Q_PROPERTY(int selectedPredecessorCount READ selectedPredecessorCount
                   NOTIFY selectionChanged)
    Q_PROPERTY(int selectedSuccessorCount READ selectedSuccessorCount
                   NOTIFY selectionChanged)
    Q_PROPERTY(bool canEditSelectedDependencies READ canEditSelectedDependencies
                   NOTIFY selectionChanged)
    Q_PROPERTY(bool empty READ empty NOTIFY graphChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    QML_NAMED_ELEMENT(TaskGraphViewModel)
    QML_UNCREATABLE("TaskGraphViewModel is owned by AppViewModel")

public:
    /// nodeX/nodeY 避开 QML Item 的内建 x/y 属性，稳定 TaskId 仍是唯一身份。
    enum Role {
        TaskIdRole = Qt::UserRole + 1,
        ShortIdRole,
        TitleRole,
        StatusTextRole,
        PriorityTextRole,
        BlockedRole,
        BlockingReasonTextRole,
        ArchivedRole,
        CanEditDependenciesRole,
        NodeXRole,
        NodeYRole,
        NodeWidthRole,
        NodeHeightRole,
        SelectedRole,
    };
    Q_ENUM(Role)

    explicit TaskGraphViewModel(model::TaskService &taskService,
                                QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QAbstractItemModel *edges() noexcept;
    [[nodiscard]] qreal contentWidth() const noexcept;
    [[nodiscard]] qreal contentHeight() const noexcept;
    [[nodiscard]] QString selectedTaskId() const;
    [[nodiscard]] QString selectedTaskTitle() const;
    [[nodiscard]] QString selectedStatusText() const;
    [[nodiscard]] QString selectedPriorityText() const;
    [[nodiscard]] QString selectedBlockingReason() const;
    [[nodiscard]] int selectedPredecessorCount() const noexcept;
    [[nodiscard]] int selectedSuccessorCount() const noexcept;
    [[nodiscard]] bool canEditSelectedDependencies() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] QString errorMessage() const;

    /// 重新读取 Service 图快照并构建几何；仍可见的选中 TaskId 会被保留。
    Q_INVOKABLE void reload();
    Q_INVOKABLE bool selectTask(const QString &taskId);
    Q_INVOKABLE void clearSelection();

signals:
    void contentWidthChanged();
    void contentHeightChanged();
    void selectionChanged();
    void graphChanged();
    void errorMessageChanged();

private:
    struct NodeProjection final {
        model::TaskGraphNode node;
        QString blockingReasonText;
        qreal x{0.0};
        qreal y{0.0};
    };

    [[nodiscard]] int rowForTask(const model::TaskId &taskId) const;
    [[nodiscard]] const NodeProjection *selectedNode() const;
    [[nodiscard]] static QString statusText(model::TaskStatus status);
    [[nodiscard]] static QString priorityText(model::TaskPriority priority);
    void replaceGraph(const model::TaskGraphSnapshot &snapshot);
    void notifySelectedRoles(const model::TaskId &oldSelection);
    void setErrorMessage(const QString &message);

    /// 组合根拥有 Service；本对象只持有非拥有引用并监听其状态通知。
    model::TaskService &m_taskService;
    /// 内部只读边模型由本对象拥有，不暴露修改命令或业务能力。
    TaskGraphEdgeListModel *m_edges;
    QList<NodeProjection> m_nodes;
    QHash<model::TaskId, int> m_predecessorCounts;
    QHash<model::TaskId, int> m_successorCounts;
    model::TaskId m_selectedTaskId;
    qreal m_contentWidth{0.0};
    qreal m_contentHeight{0.0};
    QString m_errorMessage;
};

} // namespace smartmate::viewmodel
