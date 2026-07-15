#pragma once

#include <QFrame>
#include <QListView>
#include <QPersistentModelIndex>
#include <QWidget>

class QComboBox;
class QColor;
class QEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QDragLeaveEvent;
class QMouseEvent;
class QStackedWidget;
class QToolButton;

namespace smartmate::viewmodel {
class TaskListContract;
class TaskFocusContract;
class TaskDetailsContract;
class TaskEditorContract;
class TaskCategoryContract;
class TaskDependencyContract;
}

namespace smartmate::view::widgets {

struct TaskPageDependencies {
    /// 页面只保存组合根提供的非拥有窄 Contract，不接触具体 ViewModel 或 Service。
    viewmodel::TaskListContract &taskList;
    viewmodel::TaskFocusContract &taskFocus;
    viewmodel::TaskDetailsContract &taskDetails;
    viewmodel::TaskEditorContract &taskEditor;
    viewmodel::TaskCategoryContract &taskCategories;
    viewmodel::TaskDependencyContract &taskDependencies;
};

class TaskCategoryDialog;
class TaskDependencyDialog;
class TaskEditorDialog;
class TaskDetailsDialog;

/// 扩展原生 QListView 的稳定 ID 拖拽手势，不解释任务业务规则。
class TaskListView final : public QListView {
    Q_OBJECT
public:
    explicit TaskListView(QWidget *parent = nullptr);
    /// 返回未受透明 viewport 样式覆盖的主题卡片表面色。
    [[nodiscard]] QColor cardSurfaceColor() const;
signals:
    /// 仅表示 View 已建立原生拖拽会话，供展示反馈和手势回归测试使用。
    void taskDragStarted(const QString &taskId);
protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
private:
    /// 清除一次拖拽候选；持久索引只在当前模型快照内有效。
    void clearDragCandidate();
    QPersistentModelIndex m_dragCandidate;
    QPoint m_dragStartPosition;
};

/// 聚焦任务摘要面板；读取 Focus Contract，动作通过 List Contract 转发。
class TaskFocusPanel final : public QFrame {
    Q_OBJECT
public:
    TaskFocusPanel(viewmodel::TaskFocusContract &focus,
                   viewmodel::TaskListContract &tasks,
                   QWidget *parent = nullptr);
signals:
    /// 页面级导航请求，不直接打开具体对话框或切换主窗口页面。
    void detailsRequested(const QString &taskId);
    void createRequested();
    void dependencyGraphRequested();
protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void changeEvent(QEvent *event) override;
private:
    void setDragActive(bool active);
    void synchronize();
    void applyPresentationStyle();
    /// 两个 Contract 均为非拥有引用：一个供展示，一个供状态命令。
    viewmodel::TaskFocusContract &m_focus;
    viewmodel::TaskListContract &m_tasks;
    QFrame *m_iconFrame;
    QLabel *m_icon;
    QLabel *m_eyebrow;
    QLabel *m_title;
    QLabel *m_description;
    QLabel *m_meta;
    QLabel *m_categoryBadge;
    QLabel *m_overdueBadge;
    QLabel *m_overdueReminder;
    QPushButton *m_details;
    QPushButton *m_primary;
    /// 防止样式变更事件递归；拖拽状态仅用于临时视觉反馈。
    bool m_applyingStyle{false};
    bool m_dragActive{false};
};

/// 任务主流程页面，只组合抽象 Contract 并转发稳定 TaskId。
class TaskPage final : public QWidget {
    Q_OBJECT
public:
    explicit TaskPage(TaskPageDependencies dependencies,
                      QWidget *parent = nullptr);
signals:
    void showDependencyGraphRequested();
private:
    /// 需要用户确认的破坏性动作留在 View，确认后只提交一次 Contract 命令。
    bool confirm(const QString &title, const QString &message);
    void openEditor(const QString &taskId);
    void updateControls();

    TaskPageDependencies m_dependencies;
    /// 顶部焦点区和列表筛选/批量控件。
    TaskFocusPanel *m_focus;
    QLineEdit *m_search;
    QComboBox *m_priority;
    QComboBox *m_category;
    QToolButton *m_active;
    QToolButton *m_archived;
    QPushButton *m_bulk;
    QPushButton *m_newTask;
    QPushButton *m_manageCategories;
    QPushButton *m_clearFilters;
    QWidget *m_bulkBar;
    QLabel *m_bulkCount;
    QPushButton *m_selectAll;
    QPushButton *m_bulkArchive;
    QPushButton *m_bulkRestore;
    QPushButton *m_bulkDelete;
    QStackedWidget *m_content;
    QLabel *m_empty;
    TaskListView *m_list;
    /// 子对话框由页面拥有，仅通过 Contract 和稳定 ID 协作。
    TaskDetailsDialog *m_details;
    TaskEditorDialog *m_editor;
    TaskCategoryDialog *m_categories;
    TaskCategoryDialog *m_editorCategories;
    TaskDependencyDialog *m_dependencyEditor;
};

} // namespace smartmate::view::widgets
