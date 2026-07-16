#pragma once

#include <QWidget>

class QScreen;

namespace smartmate::view::widgets::pet {

class DesktopPetSpriteWidget;

/// 普通窗口上边缘的鼠标穿透趴卧桌宠。
class AttachedDesktopPetWindow final : public QWidget {
    Q_OBJECT

public:
    explicit AttachedDesktopPetWindow(QWidget *mainWindow);

    void updateAnchor(const QRect &mainFrame, QScreen *screen);
    [[nodiscard]] bool assetReady() const noexcept;

private:
    DesktopPetSpriteWidget *m_sprite;
};

} // namespace smartmate::view::widgets::pet
