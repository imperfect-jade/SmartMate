#pragma once

#include <QDialog>

class QLabel;
class QListView;
class QPushButton;

namespace smartmate::viewmodel { class TaskEditorContract; }

namespace smartmate::view::widgets {

/// 新建任务的前置集合选择器；确认前只修改 TaskEditorContract 的局部候选草稿。
class TaskCreationPredecessorDialog final : public QDialog {
    Q_OBJECT
public:
    explicit TaskCreationPredecessorDialog(
        viewmodel::TaskEditorContract &editor, QWidget *parent = nullptr);

    /// 从编辑器当前前置集合建立可回滚的选择子会话并显示窗口。
    void openSelection();
    /// 取消时通知 Contract 回滚子会话，不影响主编辑草稿的其他字段。
    void reject() override;

private:
    void synchronize();

    /// 非拥有编辑 Contract，同时提供候选列表 Role 和选择命令。
    viewmodel::TaskEditorContract &m_editor;
    QListView *m_list;
    QLabel *m_count;
    QLabel *m_empty;
    QPushButton *m_clear;
    /// 标记本窗口是否拥有一个尚未确认/取消的候选选择会话。
    bool m_selectionActive{false};
};

} // namespace smartmate::view::widgets
