#pragma once

#include "common/presentation/UiNotification.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;

namespace smartmate::viewmodel {
class TaskFocusContract;
class TaskListContract;
}

namespace smartmate::view::widgets::pet {

/// 悬浮桌宠的轻量任务气泡，只读取焦点 Contract 并转发稳定 TaskId 命令。
class DesktopPetTaskPopup final : public QWidget {
    Q_OBJECT

public:
    DesktopPetTaskPopup(viewmodel::TaskFocusContract &focus,
                        viewmodel::TaskListContract &tasks,
                        QWidget *parent = nullptr);

    void showNextTo(const QRect &petGeometry);
    void toggleNextTo(const QRect &petGeometry);

signals:
    void openMainWindowRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void refresh();
    void showError(const common::UiNotification &notification);

    viewmodel::TaskFocusContract &m_focus;
    viewmodel::TaskListContract &m_tasks;
    QLabel *m_stateLabel;
    QLabel *m_titleLabel;
    QLabel *m_detailLabel;
    QLabel *m_errorLabel;
    QPushButton *m_startButton;
    QPushButton *m_completeButton;
    QPushButton *m_openButton;
    QTimer *m_errorTimer;
    QString m_taskId;
};

} // namespace smartmate::view::widgets::pet
