#include "view/widgets/pet/DesktopPetGeometry.h"

#include <algorithm>

namespace smartmate::view::widgets::pet {
namespace {
qreal boundedRatio(const qreal value)
{
    return std::clamp(value, 0.0, 1.0);
}
}

QPoint clampTopLeft(const QPoint &position, const QSize &windowSize,
                    const QRect &availableGeometry)
{
    const int maxX = std::max(availableGeometry.left(),
                              availableGeometry.right() - windowSize.width() + 1);
    const int maxY = std::max(availableGeometry.top(),
                              availableGeometry.bottom() - windowSize.height() + 1);
    return {std::clamp(position.x(), availableGeometry.left(), maxX),
            std::clamp(position.y(), availableGeometry.top(), maxY)};
}

QPoint positionFromRatios(const QRect &availableGeometry,
                          const QSize &windowSize,
                          const qreal xRatio, const qreal yRatio)
{
    const int xSpan = std::max(0, availableGeometry.width() - windowSize.width());
    const int ySpan = std::max(0, availableGeometry.height() - windowSize.height());
    return clampTopLeft(
        {availableGeometry.left()
             + qRound(xSpan * boundedRatio(xRatio)),
         availableGeometry.top()
             + qRound(ySpan * boundedRatio(yRatio))},
        windowSize, availableGeometry);
}

QPointF ratiosFromPosition(const QRect &availableGeometry,
                           const QSize &windowSize, const QPoint &position)
{
    const QPoint clamped = clampTopLeft(position, windowSize, availableGeometry);
    const int xSpan = std::max(0, availableGeometry.width() - windowSize.width());
    const int ySpan = std::max(0, availableGeometry.height() - windowSize.height());
    return {xSpan == 0 ? 0.0
                      : static_cast<qreal>(clamped.x() - availableGeometry.left())
                            / xSpan,
            ySpan == 0 ? 0.0
                      : static_cast<qreal>(clamped.y() - availableGeometry.top())
                            / ySpan};
}

QPoint taskPopupPosition(const QRect &availableGeometry,
                         const QRect &petGeometry, const QSize &popupSize,
                         const int gap)
{
    const int leftX = petGeometry.left() - gap - popupSize.width();
    const int rightX = petGeometry.right() + 1 + gap;
    const int x = leftX >= availableGeometry.left() ? leftX : rightX;
    const int y = petGeometry.center().y() - popupSize.height() / 2;
    return clampTopLeft({x, y}, popupSize, availableGeometry);
}

QPoint attachedPetPosition(const QRect &mainFrame,
                           const QSize &petWindowSize,
                           const QRect &availableGeometry,
                           const int contactLineY,
                           const int overlap,
                           const int rightClearance)
{
    const QPoint desired{
        mainFrame.right() - rightClearance - petWindowSize.width() + 1,
        mainFrame.top() + overlap - contactLineY};
    return clampTopLeft(desired, petWindowSize, availableGeometry);
}

} // namespace smartmate::view::widgets::pet
