#pragma once

#include <QFrame>

class QLabel;
class QListView;
class QPushButton;
class QToolButton;

namespace smartmate::viewmodel {
class TaskGraphContract;
}

namespace smartmate::view::widgets {

struct WidgetTheme;

/// 所选图节点的只读详情与直接关系列表；不遍历主图或计算业务资格。
class TaskGraphDetailsPanel final : public QFrame {
    Q_OBJECT
public:
    explicit TaskGraphDetailsPanel(viewmodel::TaskGraphContract &graph,
                                   QWidget *parent = nullptr);
    void synchronize();
    void synchronizeRelations();
    void applyTheme(const WidgetTheme &theme);

signals:
    void collapseRequested();
    void pinnedChanged(bool pinned);
    void centerRequested();
    void fullDetailsRequested(const QString &taskId);
    void editDependenciesRequested(const QString &taskId);
    void taskActivated(const QString &taskId);

private:
    viewmodel::TaskGraphContract &m_graph;
    QToolButton *m_pin;
    QLabel *m_selectedTitle;
    QLabel *m_selectedCategory;
    QLabel *m_selectedContext;
    QLabel *m_selectedMeta;
    QLabel *m_selectedDescription;
    QFrame *m_divider;
    QLabel *m_selectedDeadline;
    QLabel *m_selectedDuration;
    QLabel *m_selectedRelations;
    QLabel *m_selectedBlocking;
    QLabel *m_predecessorHeading;
    QLabel *m_successorHeading;
    QListView *m_predecessors;
    QListView *m_successors;
    QPushButton *m_editDependencies;
};

} // namespace smartmate::view::widgets
