#include "view/widgets/pet/AttachedDesktopPetWindow.h"

#include "view/widgets/pet/DesktopPetGeometry.h"
#include "view/widgets/pet/DesktopPetSpriteWidget.h"

#include <QScreen>
#include <QVBoxLayout>

namespace smartmate::view::widgets::pet {

AttachedDesktopPetWindow::AttachedDesktopPetWindow(QWidget *mainWindow)
    : QWidget(mainWindow, Qt::Tool | Qt::FramelessWindowHint
                           | Qt::WindowDoesNotAcceptFocus
                           | Qt::WindowTransparentForInput)
    , m_sprite(new DesktopPetSpriteWidget(
          DesktopPetSpriteWidget::Animation::Waiting, this))
{
    setObjectName(QStringLiteral("attachedDesktopPetWindow"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_sprite);
    setFixedSize(m_sprite->size());
}

void AttachedDesktopPetWindow::updateAnchor(const QRect &mainFrame,
                                             QScreen *screen)
{
    if (screen == nullptr) {
        return;
    }
    // 右侧预留原生标题栏按钮区域，底部 12 像素进入主窗口形成趴靠接触。
    const QPoint desired{mainFrame.right() - 168 - width() + 1,
                         mainFrame.top() - height() + 12};
    move(clampTopLeft(desired, size(), screen->availableGeometry()));
}

bool AttachedDesktopPetWindow::assetReady() const noexcept
{
    return m_sprite->assetReady();
}

} // namespace smartmate::view::widgets::pet
