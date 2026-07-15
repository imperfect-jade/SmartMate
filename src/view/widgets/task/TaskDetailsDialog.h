#pragma once

#include <QDialog>

class QLabel;
class QPushButton;

namespace smartmate::viewmodel { class TaskDetailsContract; }

namespace smartmate::view::widgets {

/// 复用 TaskDetailsContract 的纯展示对话框；动作可按调用场景整体隐藏。
class TaskDetailsDialog final : public QDialog {
    Q_OBJECT
public:
    explicit TaskDetailsDialog(viewmodel::TaskDetailsContract &details,
                               QWidget *parent = nullptr);

    /// 先让 Contract 按稳定 TaskId 建立详情投影，成功后才显示对话框。
    bool openTask(const QString &taskId);
    void setActionsVisible(bool visible);

signals:
    /// 只向页面转发稳定 TaskId；对话框不直接创建编辑器或依赖编辑器。
    void editRequested(const QString &taskId);
    void editDependenciesRequested(const QString &taskId);

protected:
    void done(int result) override;

private:
    void synchronize();

    /// 非拥有详情 Contract；所有标签内容均在 selectionChanged 后重读。
    viewmodel::TaskDetailsContract &m_details;
    QLabel *m_title;
    QLabel *m_summary;
    QLabel *m_description;
    QLabel *m_schedule;
    QLabel *m_insight;
    QPushButton *m_edit;
    QPushButton *m_editDependencies;
    bool m_actionsVisible{true};
};

} // namespace smartmate::view::widgets
