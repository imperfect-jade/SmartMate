#pragma once

#include <QWidget>

class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QListView;
class QPushButton;
class QShowEvent;
class QStackedLayout;
class QToolButton;

namespace smartmate::viewmodel {
class AppearanceSettingsContract;
class TaskDependencyContract;
class TaskDetailsContract;
class TaskGraphContract;
}

namespace smartmate::view::widgets {

class DependencyGraphView;
class TaskDependencyDialog;
class TaskDetailsDialog;

struct DependencyGraphPageDependencies {
    viewmodel::AppearanceSettingsContract &appearanceSettings;
    viewmodel::TaskGraphContract &taskGraph;
    viewmodel::TaskDetailsContract &taskDetails;
    viewmodel::TaskDependencyContract &taskDependencies;
};

/// 依赖图页面只消费 TaskGraphContract 的布局投影并管理纯展示会话状态。
class DependencyGraphPage final : public QWidget {
    Q_OBJECT
public:
    explicit DependencyGraphPage(DependencyGraphPageDependencies dependencies,
                                 QWidget *parent = nullptr);
    void activate();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void synchronizeControls();
    void synchronizeDetails();
    void synchronizeRelations();
    void applyTheme();
    void applyDetailsTheme();
    void updateResponsiveLayout();
    void setDetailsExpanded(bool expanded);
    void selectAndCenter(const QString &taskId);

    DependencyGraphPageDependencies m_dependencies;
    QLineEdit *m_search;
    QComboBox *m_statusFilter;
    QComboBox *m_categoryFilter;
    QLabel *m_taskCount;
    QLabel *m_blockedCount;
    QPushButton *m_locateCurrent;
    QToolButton *m_zoomOut;
    QToolButton *m_zoomIn;
    QLabel *m_zoomLabel;
    QLabel *m_notification;
    QFrame *m_canvasFrame;
    DependencyGraphView *m_view;
    QLabel *m_empty;
    QStackedLayout *m_canvasStack;
    QPushButton *m_openDetails;
    QFrame *m_detailsPanel;
    QToolButton *m_pinDetails;
    QLabel *m_selectedTitle;
    QLabel *m_selectedCategory;
    QLabel *m_selectedContext;
    QLabel *m_selectedMeta;
    QLabel *m_selectedDescription;
    QFrame *m_detailsDivider;
    QLabel *m_selectedDeadline;
    QLabel *m_selectedDuration;
    QLabel *m_selectedRelations;
    QLabel *m_selectedBlocking;
    QLabel *m_predecessorHeading;
    QLabel *m_successorHeading;
    QListView *m_predecessors;
    QListView *m_successors;
    QPushButton *m_editDependencies;
    TaskDetailsDialog *m_fullDetails;
    TaskDependencyDialog *m_dependencyEditor;
    bool m_detailsExpanded{false};
    bool m_detailsPinned{false};
    bool m_viewportInitialized{false};
};

} // namespace smartmate::view::widgets
