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

    bool openTask(const QString &taskId);
    void setActionsVisible(bool visible);

signals:
    void editRequested(const QString &taskId);
    void editDependenciesRequested(const QString &taskId);

protected:
    void done(int result) override;

private:
    void synchronize();

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
