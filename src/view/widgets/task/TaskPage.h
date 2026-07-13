#pragma once

#include <QFrame>
#include <QListView>
#include <QPersistentModelIndex>
#include <QWidget>

class QComboBox;
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

class TaskListView final : public QListView {
    Q_OBJECT
public:
    explicit TaskListView(QWidget *parent = nullptr);
signals:
    /// 仅表示 View 已建立原生拖拽会话，供展示反馈和手势回归测试使用。
    void taskDragStarted(const QString &taskId);
protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
private:
    void clearDragCandidate();
    QPersistentModelIndex m_dragCandidate;
    QPoint m_dragStartPosition;
};

class TaskFocusPanel final : public QFrame {
    Q_OBJECT
public:
    TaskFocusPanel(viewmodel::TaskFocusContract &focus,
                   viewmodel::TaskListContract &tasks,
                   QWidget *parent = nullptr);
signals:
    void detailsRequested(const QString &taskId);
    void createRequested();
    void dependencyGraphRequested();
protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
private:
    void setDragActive(bool active);
    void synchronize();
    viewmodel::TaskFocusContract &m_focus;
    viewmodel::TaskListContract &m_tasks;
    QLabel *m_title;
    QLabel *m_description;
    QLabel *m_meta;
    QPushButton *m_details;
    QPushButton *m_primary;
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
    bool confirm(const QString &title, const QString &message);
    void openEditor(const QString &taskId);
    void updateControls();

    TaskPageDependencies m_dependencies;
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
    TaskDetailsDialog *m_details;
    TaskEditorDialog *m_editor;
    TaskCategoryDialog *m_categories;
    TaskCategoryDialog *m_editorCategories;
    TaskDependencyDialog *m_dependencyEditor;
};

} // namespace smartmate::view::widgets
