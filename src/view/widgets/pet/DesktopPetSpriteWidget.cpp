#include "view/widgets/pet/DesktopPetSpriteWidget.h"

#include <QDebug>
#include <QHideEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QShowEvent>

int qInitResources_desktop_pet_assets();

static void ensureDesktopPetResources()
{
    qInitResources_desktop_pet_assets();
}

namespace smartmate::view::widgets::pet {
namespace {
constexpr int atlasWidth = 1536;
constexpr int atlasHeight = 1872;
constexpr int cellWidth = 192;
constexpr int cellHeight = 208;
constexpr int logicalWidth = 96;
constexpr int logicalHeight = 104;
constexpr int animationFrameCount = 6;
}

DesktopPetSpriteWidget::DesktopPetSpriteWidget(const Animation animation,
                                               QWidget *parent)
    : QWidget(parent)
    , m_animation(animation)
{
    ensureDesktopPetResources();
    setObjectName(QStringLiteral("desktopPetSprite"));
    setFixedSize(logicalWidth, logicalHeight);
    setAttribute(Qt::WA_TranslucentBackground);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        ++m_frame;
        if (m_frame >= animationFrameCount) {
            m_frame = 0;
            update();
            scheduleNextFrame(QRandomGenerator::global()->bounded(1500, 3001));
            return;
        }
        update();
        scheduleNextFrame();
    });

    m_atlas.load(QStringLiteral(
        ":/smartmate/pets/calico-cat-spritesheet.png"));
    if (m_atlas.size() != QSize{atlasWidth, atlasHeight}) {
        qWarning() << "Desktop pet spritesheet is missing or has an invalid size"
                   << m_atlas.size();
        m_atlas = {};
    }
}

bool DesktopPetSpriteWidget::assetReady() const noexcept
{
    return !m_atlas.isNull();
}

int DesktopPetSpriteWidget::currentFrame() const noexcept
{
    return m_frame;
}

bool DesktopPetSpriteWidget::animationRunning() const noexcept
{
    return m_timer.isActive();
}

void DesktopPetSpriteWidget::paintEvent(QPaintEvent *)
{
    if (m_atlas.isNull()) {
        return;
    }
    const int row = m_animation == Animation::Idle ? 0 : 6;
    const QRect source{m_frame * cellWidth, row * cellHeight,
                       cellWidth, cellHeight};
    QPainter painter{this};
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(rect(), m_atlas, source);
}

void DesktopPetSpriteWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (assetReady() && !m_timer.isActive()) {
        scheduleNextFrame();
    }
}

void DesktopPetSpriteWidget::hideEvent(QHideEvent *event)
{
    m_timer.stop();
    QWidget::hideEvent(event);
}

void DesktopPetSpriteWidget::scheduleNextFrame(const int delayMs)
{
    if (isVisible() && assetReady()) {
        m_timer.start(delayMs);
    }
}

} // namespace smartmate::view::widgets::pet
