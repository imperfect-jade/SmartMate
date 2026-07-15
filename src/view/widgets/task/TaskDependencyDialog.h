#pragma once

#include <QDialog>

class QLabel;
class QListView;
class QPushButton;

namespace smartmate::viewmodel { class TaskDependencyContract; }

namespace smartmate::view::widgets {

/// 已有任务的完整前置集合编辑器；保存时只调用一次原子 Contract 命令。
class TaskDependencyDialog final : public QDialog {
    Q_OBJECT
public:
    explicit TaskDependencyDialog(viewmodel::TaskDependencyContract &dependencies,
                                  QWidget *parent = nullptr);

    /// 仅在 Contract 成功建立稳定 TaskId 草稿后显示窗口。
    bool openTask(const QString &taskId);
    /// 关闭未保存窗口时请求 Contract 放弃整份依赖草稿。
    void reject() override;

private:
    void synchronize();

    /// 非拥有依赖 Contract，同时作为候选任务列表模型。
    viewmodel::TaskDependencyContract &m_dependencies;
    QListView *m_list;
    QLabel *m_description;
    QLabel *m_count;
    QLabel *m_empty;
    QLabel *m_notification;
    QPushButton *m_save;
    /// 防止窗口关闭流程重复调用 cancel()。
    bool m_draftActive{false};
};

} // namespace smartmate::view::widgets
