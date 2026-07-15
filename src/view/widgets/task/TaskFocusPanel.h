#pragma once

#include <QFrame>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDropEvent;
class QEvent;
class QLabel;
class QPushButton;

namespace smartmate::viewmodel {
class TaskFocusContract;
class TaskListContract;
}

namespace smartmate::view::widgets {

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

} // namespace smartmate::view::widgets
