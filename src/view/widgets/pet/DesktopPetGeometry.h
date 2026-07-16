#pragma once

#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSize>

namespace smartmate::view::widgets::pet {

/// 将窗口左上角夹紧到屏幕可用区域，保证整个窗口可见。
[[nodiscard]] QPoint clampTopLeft(const QPoint &position,
                                  const QSize &windowSize,
                                  const QRect &availableGeometry);
/// 把归一化坐标映射到可用移动范围；零跨度轴固定在可用区域起点。
[[nodiscard]] QPoint positionFromRatios(const QRect &availableGeometry,
                                        const QSize &windowSize,
                                        qreal xRatio, qreal yRatio);
/// 把窗口左上角转换为可用移动范围内的归一化坐标。
[[nodiscard]] QPointF ratiosFromPosition(const QRect &availableGeometry,
                                         const QSize &windowSize,
                                         const QPoint &position);
/// 优先把气泡放在宠物左侧，空间不足时放到右侧并夹紧。
[[nodiscard]] QPoint taskPopupPosition(const QRect &availableGeometry,
                                       const QRect &petGeometry,
                                       const QSize &popupSize,
                                       int gap = 8);

} // namespace smartmate::view::widgets::pet
