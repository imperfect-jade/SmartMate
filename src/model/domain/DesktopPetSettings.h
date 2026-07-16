#pragma once

#include <QString>

#include <optional>

namespace smartmate::model {

/// 悬浮桌宠在某块屏幕可用移动区域中的持久化位置。
struct DesktopPetPlacement final {
    QString screenName;
    double xRatio{0.0};
    double yRatio{0.0};

    friend bool operator==(const DesktopPetPlacement &,
                           const DesktopPetPlacement &) = default;
};

/// 桌宠设置是普通值类型，不依赖具体窗口或 QSettings。
struct DesktopPetSettings final {
    bool enabled{false};
    std::optional<DesktopPetPlacement> placement;

    friend bool operator==(const DesktopPetSettings &,
                           const DesktopPetSettings &) = default;
};

/// 校验完整设置；位置不存在始终合法，存在时屏幕名与归一化坐标必须有效。
[[nodiscard]] bool isValid(const DesktopPetSettings &settings) noexcept;

} // namespace smartmate::model
