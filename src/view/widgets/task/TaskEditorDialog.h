#pragma once

#include <QDialog>

class QComboBox;
class QFrame;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QShowEvent;
class QWidget;

namespace smartmate::viewmodel { class TaskEditorContract; }

namespace smartmate::view::widgets {

class TaskCreationPredecessorDialog;

/// 使用类型化 Qt Widgets 控件编辑 TaskEditorContract 草稿。
class TaskEditorDialog final : public QDialog {
    Q_OBJECT
public:
    explicit TaskEditorDialog(viewmodel::TaskEditorContract &editor,
                              QWidget *parent = nullptr);

signals:
    /// 请求页面打开类别管理窗口，避免编辑器自行依赖具体类别 View。
    void manageCategoriesRequested();

protected:
    void reject() override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    /// 重读全部草稿 getter，并用 QSignalBlocker 安全回填控件。
    void synchronize();
    /// 根据 sessionActive 决定窗口显示/关闭，不把窗口状态写入 ViewModel。
    void synchronizeSession();
    void updateResponsiveLayout();
    void chooseDeadline();
    void chooseDuration();

    /// 非拥有编辑 Contract；字段事件均转发为草稿命令。
    viewmodel::TaskEditorContract &m_editor;
    QLabel *m_headerTitle;
    QLabel *m_headerSubtitle;
    QScrollArea *m_scroll;
    /// 规划字段网格在宽窄布局间重排，不改变字段数据。
    QGridLayout *m_planningGrid;
    QWidget *m_statusField;
    QWidget *m_priorityField;
    QWidget *m_categoryField;
    QLineEdit *m_title;
    QPlainTextEdit *m_description;
    QLabel *m_status;
    QComboBox *m_priority;
    QComboBox *m_category;
    QWidget *m_predecessorField;
    QLabel *m_predecessors;
    QPushButton *m_choosePredecessors;
    QPushButton *m_clearPredecessors;
    QLabel *m_deadline;
    QPushButton *m_deadlineClear;
    QLabel *m_duration;
    QPushButton *m_durationClear;
    QLabel *m_validation;
    QPushButton *m_save;
    /// 新建前置集合的子会话窗口，由本对话框拥有。
    TaskCreationPredecessorDialog *m_predecessorDialog;
};

} // namespace smartmate::view::widgets
