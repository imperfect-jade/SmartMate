#pragma once

#include "viewmodel/contracts/FocusContract.h"

#include <QScrollArea>

class QAbstractItemModel;
class QBoxLayout;
class QEvent;
class QFrame;
class QGridLayout;
class QLabel;
class QPushButton;
class QResizeEvent;
class QShowEvent;
class QVBoxLayout;
class QWidget;

namespace smartmate::view::widgets {

/// 纯 Widgets 专注页面；只消费 Focus Contract 的展示投影和强类型命令。
class FocusPage final : public QScrollArea {
    Q_OBJECT

public:
    explicit FocusPage(viewmodel::FocusContract &focus,
                       QWidget *parent = nullptr);

signals:
    /// 请求主窗口切换到任务页；导航仍由 View 层负责。
    void showTasksRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void syncAll();
    void syncCurrent();
    void rebuildHistory();
    void applyResponsiveLayout();
    void connectHistoryModel(QAbstractItemModel *model);

    viewmodel::FocusContract &m_focus;
    QWidget *m_content;
    QFrame *m_sessionCard;
    QLabel *m_state;
    QLabel *m_taskTitle;
    QLabel *m_elapsed;
    QLabel *m_category;
    QLabel *m_estimate;
    QLabel *m_startedAt;
    QLabel *m_emptyState;
    QLabel *m_storageWarning;
    QLabel *m_historyCount;
    QLabel *m_historyEmpty;
    QPushButton *m_showTasks;
    QPushButton *m_start;
    QPushButton *m_pause;
    QPushButton *m_resume;
    QPushButton *m_complete;
    QPushButton *m_abandon;
    QGridLayout *m_metadataLayout;
    QBoxLayout *m_actionLayout;
    QVBoxLayout *m_historyRows;
    bool m_wideLayout{false};
};

} // namespace smartmate::view::widgets
