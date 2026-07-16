#pragma once

#include <QImage>
#include <QTimer>
#include <QWidget>

namespace smartmate::view::widgets::pet {

/// 负责从固定 8×9 图集读取帧并以最近邻方式绘制三花猫。
class DesktopPetSpriteWidget final : public QWidget {
    Q_OBJECT

public:
    enum class Animation {
        Idle,
        Waiting,
    };

    explicit DesktopPetSpriteWidget(Animation animation,
                                    QWidget *parent = nullptr);

    [[nodiscard]] bool assetReady() const noexcept;
    [[nodiscard]] int currentFrame() const noexcept;
    [[nodiscard]] bool animationRunning() const noexcept;

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void scheduleNextFrame(int delayMs = 167);

    Animation m_animation;
    QImage m_atlas;
    QTimer m_timer;
    int m_frame{0};
};

} // namespace smartmate::view::widgets::pet
