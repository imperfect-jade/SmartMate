#include "view/widgets/pet/FloatingDesktopPetWindow.h"

#include "view/widgets/pet/DesktopPetGeometry.h"
#include "view/widgets/pet/DesktopPetSpriteWidget.h"
#include "view/widgets/pet/DesktopPetTaskPopup.h"
#include "viewmodel/contracts/DesktopPetSettingsContract.h"
#include "viewmodel/contracts/TaskFocusContract.h"
#include "viewmodel/contracts/TaskListContract.h"

#include <QApplication>
#include <QGuiApplication>
#include <QHideEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QVBoxLayout>

namespace smartmate::view::widgets::pet {

FloatingDesktopPetWindow::FloatingDesktopPetWindow(
    viewmodel::DesktopPetSettingsContract &settings,
    viewmodel::TaskFocusContract &focus,
    viewmodel::TaskListContract &tasks)
    : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint
                           | Qt::WindowStaysOnTopHint
                           | Qt::WindowDoesNotAcceptFocus)
    , m_settings(settings)
    , m_sprite(new DesktopPetSpriteWidget(
          DesktopPetSpriteWidget::Animation::Idle, this))
    , m_popup(new DesktopPetTaskPopup(focus, tasks, this))
{
    setObjectName(QStringLiteral("floatingDesktopPetWindow"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    // 精灵只负责绘制；鼠标事件必须由顶层悬浮窗统一区分单击与拖动。
    m_sprite->setAttribute(Qt::WA_TransparentForMouseEvents);
    setCursor(Qt::OpenHandCursor);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_sprite);
    setFixedSize(m_sprite->size());
    connect(m_popup, &DesktopPetTaskPopup::openMainWindowRequested,
            this, [this] {
                dismissTaskPopup();
                emit openMainWindowRequested();
            });
}

void FloatingDesktopPetWindow::restorePosition(QScreen *fallbackScreen)
{
    QScreen *screen = savedOrFallbackScreen(fallbackScreen);
    if (screen == nullptr) {
        return;
    }
    if (m_settings.hasFloatingPlacement()) {
        move(positionFromRatios(screen->availableGeometry(), size(),
                                m_settings.floatingXRatio(),
                                m_settings.floatingYRatio()));
    } else {
        const QRect available = screen->availableGeometry();
        move(clampTopLeft({available.right() - width() + 1 - 24,
                           available.bottom() - height() + 1 - 24},
                          size(), available));
    }
}

void FloatingDesktopPetWindow::clampToVisibleScreen(QScreen *fallbackScreen)
{
    QScreen *screen = QGuiApplication::screenAt(frameGeometry().center());
    if (screen == nullptr) {
        screen = savedOrFallbackScreen(fallbackScreen);
    }
    if (screen != nullptr) {
        move(clampTopLeft(pos(), size(), screen->availableGeometry()));
    }
}

bool FloatingDesktopPetWindow::assetReady() const noexcept
{
    return m_sprite->assetReady();
}

void FloatingDesktopPetWindow::hideEvent(QHideEvent *event)
{
    // 气泡是独立的顶层 Qt::Tool 窗口，不能依赖悬浮窗的原生隐藏行为同步关闭。
    dismissTaskPopup();
    QWidget::hideEvent(event);
}

void FloatingDesktopPetWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressGlobal = event->globalPosition().toPoint();
        m_pressWindowPosition = pos();
        m_dragging = false;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void FloatingDesktopPetWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons().testFlag(Qt::LeftButton)) {
        const QPoint delta = event->globalPosition().toPoint() - m_pressGlobal;
        if (!m_dragging
            && delta.manhattanLength() >= QApplication::startDragDistance()) {
            m_dragging = true;
            dismissTaskPopup();
        }
        if (m_dragging) {
            move(m_pressWindowPosition + delta);
            clampToVisibleScreen(QGuiApplication::primaryScreen());
        }
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void FloatingDesktopPetWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setCursor(Qt::OpenHandCursor);
        if (m_dragging) {
            persistPosition();
        } else {
            m_popup->toggleNextTo(frameGeometry());
        }
        m_dragging = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

QScreen *FloatingDesktopPetWindow::savedOrFallbackScreen(
    QScreen *fallbackScreen) const
{
    if (m_settings.hasFloatingPlacement()) {
        for (QScreen *screen : QGuiApplication::screens()) {
            if (screen->name() == m_settings.floatingScreenName()) {
                return screen;
            }
        }
    }
    return fallbackScreen != nullptr ? fallbackScreen
                                     : QGuiApplication::primaryScreen();
}

void FloatingDesktopPetWindow::persistPosition()
{
    QScreen *screen = QGuiApplication::screenAt(frameGeometry().center());
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen == nullptr) {
        return;
    }
    const QPoint clamped = clampTopLeft(pos(), size(), screen->availableGeometry());
    move(clamped);
    const QPointF ratios = ratiosFromPosition(screen->availableGeometry(),
                                               size(), clamped);
    m_settings.saveFloatingPlacement(screen->name(), ratios.x(), ratios.y());
}

void FloatingDesktopPetWindow::dismissTaskPopup()
{
    m_popup->hide();
}

} // namespace smartmate::view::widgets::pet
