#pragma once

#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;
class QShowEvent;
class QStackedLayout;

namespace smartmate::viewmodel {
class AppearanceSettingsContract;
class TaskDependencyContract;
class TaskDetailsContract;
class TaskGraphContract;
}

namespace smartmate::view::widgets {

class DependencyGraphView;
class DependencyGraphToolbar;
class TaskGraphDetailsPanel;
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
    void applyTheme();
    void updateResponsiveLayout();
    void setDetailsExpanded(bool expanded);
    void selectAndCenter(const QString &taskId);

    DependencyGraphPageDependencies m_dependencies;
    /// 顶部筛选、统计和视口命令控件。
    DependencyGraphToolbar *m_toolbar;
    QLabel *m_notification;
    /// 中央画布及空状态；几何和边路径只从 TaskGraphContract 读取。
    QFrame *m_canvasFrame;
    DependencyGraphView *m_view;
    QLabel *m_empty;
    QStackedLayout *m_canvasStack;
    /// 所选节点详情和直接关系列表，不自行遍历依赖图。
    QPushButton *m_openDetails;
    TaskGraphDetailsPanel *m_detailsPanel;
    /// 复用窄 Contract 的子对话框，由 QObject 父子关系随页面销毁。
    TaskDetailsDialog *m_fullDetails;
    TaskDependencyDialog *m_dependencyEditor;
    /// 仅属于页面会话的展开、固定和首屏适配状态，不写回 ViewModel。
    bool m_detailsExpanded{false};
    bool m_detailsPinned{false};
    bool m_viewportInitialized{false};
};

} // namespace smartmate::view::widgets
