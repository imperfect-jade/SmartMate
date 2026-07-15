#pragma once

#include <QFrame>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;

namespace smartmate::viewmodel {
class TaskGraphContract;
}

namespace smartmate::view::widgets {

/// 依赖图筛选、统计和视口命令栏；筛选直接转发 Contract，视口动作以信号上送页面。
class DependencyGraphToolbar final : public QFrame {
    Q_OBJECT
public:
    explicit DependencyGraphToolbar(viewmodel::TaskGraphContract &graph,
                                    QWidget *parent = nullptr);
    /// 从 Contract getter 同步筛选和统计，程序性回填不会触发反向命令。
    void synchronize();
    /// 同步纯 View 缩放状态及按钮资格。
    void setZoomFactor(qreal factor);

signals:
    void locateFirstMatchRequested();
    void locateCurrentRequested();
    void zoomOutRequested();
    void zoomInRequested();
    void resetZoomRequested();
    void fitRequested();

private:
    viewmodel::TaskGraphContract &m_graph;
    QLineEdit *m_search;
    QComboBox *m_statusFilter;
    QComboBox *m_categoryFilter;
    QLabel *m_taskCount;
    QLabel *m_blockedCount;
    QPushButton *m_locateCurrent;
    QToolButton *m_zoomOut;
    QToolButton *m_zoomIn;
    QLabel *m_zoomLabel;
};

} // namespace smartmate::view::widgets
