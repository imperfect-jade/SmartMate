#pragma once

#include <QListView>
#include <QPersistentModelIndex>
#include <QPoint>

class QColor;
class QMouseEvent;

namespace smartmate::view::widgets {

/// 扩展原生 QListView 的稳定 ID 拖拽手势，不解释任务业务规则。
class TaskListView final : public QListView {
    Q_OBJECT
public:
    explicit TaskListView(QWidget *parent = nullptr);
    /// 返回未受透明 viewport 样式覆盖的主题卡片表面色。
    [[nodiscard]] QColor cardSurfaceColor() const;
signals:
    /// 仅表示 View 已建立原生拖拽会话，供展示反馈和手势回归测试使用。
    void taskDragStarted(const QString &taskId);
protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
private:
    /// 清除一次拖拽候选；持久索引只在当前模型快照内有效。
    void clearDragCandidate();
    QPersistentModelIndex m_dragCandidate;
    QPoint m_dragStartPosition;
};

} // namespace smartmate::view::widgets
