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
    /// 均为组合根拥有的非拥有 Contract 引用，生命周期必须长于页面。
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
    /// 页面切入前请求最新图投影，并在首次显示时适配画布。
    void activate();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    /// 以下 synchronize 函数在通知到达后重读 Contract getter/Role，不缓存业务状态。
    void synchronizeControls();
    void synchronizeDetails();
    void synchronizeRelations();
    void applyTheme();
    void applyDetailsTheme();
    void updateResponsiveLayout();
    void setDetailsExpanded(bool expanded);
    void selectAndCenter(const QString &taskId);

    DependencyGraphPageDependencies m_dependencies;
    /// 顶部筛选、统计和视口命令控件。
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
    /// 中央画布及空状态；几何和边路径只从 TaskGraphContract 读取。
    QFrame *m_canvasFrame;
    DependencyGraphView *m_view;
    QLabel *m_empty;
    QStackedLayout *m_canvasStack;
    /// 所选节点详情和直接关系列表，不自行遍历依赖图。
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
    /// 复用窄 Contract 的子对话框，由 QObject 父子关系随页面销毁。
    TaskDetailsDialog *m_fullDetails;
    TaskDependencyDialog *m_dependencyEditor;
    /// 仅属于页面会话的展开、固定和首屏适配状态，不写回 ViewModel。
    bool m_detailsExpanded{false};
    bool m_detailsPinned{false};
    bool m_viewportInitialized{false};
};

} // namespace smartmate::view::widgets
