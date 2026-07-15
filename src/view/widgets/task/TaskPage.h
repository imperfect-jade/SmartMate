#pragma once

#include "TaskFocusPanel.h"
#include "TaskListView.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
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
