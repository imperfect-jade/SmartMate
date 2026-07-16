#pragma once

#include <QPoint>
#include <QWidget>

class QMouseEvent;
class QScreen;

namespace smartmate::viewmodel {
class DesktopPetSettingsContract;
class TaskFocusContract;
class TaskListContract;
}

namespace smartmate::view::widgets::pet {

class DesktopPetSpriteWidget;
class DesktopPetTaskPopup;

/// 主窗口最小化后独立存在的可拖动坐姿桌宠。
class FloatingDesktopPetWindow final : public QWidget {
    Q_OBJECT

public:
    FloatingDesktopPetWindow(
        viewmodel::DesktopPetSettingsContract &settings,
        viewmodel::TaskFocusContract &focus,
        viewmodel::TaskListContract &tasks);

    void restorePosition(QScreen *fallbackScreen);
    void clampToVisibleScreen(QScreen *fallbackScreen);
    [[nodiscard]] bool assetReady() const noexcept;

signals:
    void openMainWindowRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QScreen *savedOrFallbackScreen(QScreen *fallbackScreen) const;
    void persistPosition();

    viewmodel::DesktopPetSettingsContract &m_settings;
    DesktopPetSpriteWidget *m_sprite;
    DesktopPetTaskPopup *m_popup;
    QPoint m_pressGlobal;
    QPoint m_pressWindowPosition;
    bool m_dragging{false};
};

} // namespace smartmate::view::widgets::pet
