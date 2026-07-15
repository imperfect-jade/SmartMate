#include "TaskListView.h"

#include "TaskDragMime.h"
#include "TaskItemDelegate.h"
#include "viewmodel/contracts/TaskListContract.h"

#include <QApplication>
#include <QColor>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>

namespace smartmate::view::widgets {

using ListRole = viewmodel::TaskListContract::Role;

TaskListView::TaskListView(QWidget *parent) : QListView(parent)
{
    setObjectName(QStringLiteral("taskListView"));
    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(false);
    viewport()->setAutoFillBackground(false);
    viewport()->setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet(QStringLiteral(
        "QListView#taskListView { background: transparent; border: none; outline: none; }"));
    setSelectionMode(QAbstractItemView::SingleSelection);
    // 原生 item drag 依赖模型 flags，且会把整张卡片变为拖拽源；这里由 View
    // 显式识别专用拖拽柄，资格仍只读取 Contract 的 CanStartRole。
    setDragEnabled(false);
    setMouseTracking(true);
    setSpacing(2);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
}

QColor TaskListView::cardSurfaceColor() const
{
    // QListView 的透明 QSS 会在部分平台把自身 Base role 改写为黑色；窗口根节点
    // 仍持有 WidgetTheme 注入的真实表面色，因此卡片与拖拽预览统一从这里读取。
    const QWidget *themeRoot = window();
    QColor surface = themeRoot && themeRoot != this
        ? themeRoot->palette().color(QPalette::Base)
        : QApplication::palette().color(QPalette::Base);
    if (!surface.isValid() || surface.alpha() == 0) {
        surface = QColor(QStringLiteral("#ffffff"));
    }
    return surface;
}

void TaskListView::startDrag(Qt::DropActions)
{
    // MIME 只携带稳定 TaskId 和展示标题；拖放最终仍调用 Contract 命令并由 Model 复核。
    const QModelIndex index = m_dragCandidate.isValid()
        ? QModelIndex(m_dragCandidate) : currentIndex();
    if (!index.isValid() || !index.data(ListRole::CanStartRole).toBool()) return;
    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(task_drag_detail::mimeType),
                  index.data(ListRole::TaskIdRole).toString().toUtf8());
    mime->setText(index.data(ListRole::TitleRole).toString());
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    QPixmap preview(280, 66);
    preview.fill(Qt::transparent);
    QPainter painter(&preview);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(palette().highlight().color(), 2));
    painter.setBrush(cardSurfaceColor());
    painter.drawRoundedRect(preview.rect().adjusted(1, 1, -2, -2), 11, 11);
    QFont handleFont = font();
    handleFont.setPointSizeF(handleFont.pointSizeF() + 4);
    painter.setFont(handleFont);
    painter.setPen(palette().highlight().color());
    painter.drawText(QRect{10, 0, 34, preview.height()}, Qt::AlignCenter,
                     QStringLiteral("⠿"));
    QFont titleFont = font();
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(palette().text().color());
    painter.drawText(QRect{50, 9, 218, 24}, Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(
                         index.data(ListRole::TitleRole).toString(),
                         Qt::ElideRight, 218));
    painter.setFont(font());
    painter.setPen(palette().placeholderText().color());
    painter.drawText(QRect{50, 34, 218, 22}, Qt::AlignLeft | Qt::AlignVCenter,
                     tr("%1 · 拖到“现在做”开始")
                         .arg(index.data(ListRole::StatusTextRole).toString()));
    painter.end();
    drag->setPixmap(preview);
    drag->setHotSpot({22, preview.height() / 2});
    emit taskDragStarted(index.data(ListRole::TaskIdRole).toString());
    drag->exec(Qt::MoveAction);
    drag->deleteLater();
}

void TaskListView::mousePressEvent(QMouseEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    const auto *delegate = qobject_cast<TaskItemDelegate *>(itemDelegate());
    if (event->button() == Qt::LeftButton && index.isValid() && delegate
        && delegate->dragHandleRect(visualRect(index), index)
               .contains(event->position().toPoint())) {
        m_dragCandidate = index;
        m_dragStartPosition = event->position().toPoint();
        setCurrentIndex(index);
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    clearDragCandidate();
    QListView::mousePressEvent(event);
}

void TaskListView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragCandidate.isValid()) {
        if (!(event->buttons() & Qt::LeftButton)) {
            clearDragCandidate();
            return;
        }
        if ((event->position().toPoint() - m_dragStartPosition).manhattanLength()
            >= QApplication::startDragDistance()) {
            event->accept();
            startDrag(Qt::MoveAction);
            clearDragCandidate();
        }
        return;
    }
    QListView::mouseMoveEvent(event);
}

void TaskListView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragCandidate.isValid() && event->button() == Qt::LeftButton) {
        clearDragCandidate();
        event->accept();
        return;
    }
    QListView::mouseReleaseEvent(event);
}

void TaskListView::clearDragCandidate()
{
    m_dragCandidate = QPersistentModelIndex{};
    viewport()->unsetCursor();
}

} // namespace smartmate::view::widgets

