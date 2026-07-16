#include "domain/DesktopPetSettings.h"

#include <cmath>

namespace smartmate::model {

bool isValid(const DesktopPetSettings &settings) noexcept
{
    if (!settings.placement.has_value()) {
        return true;
    }
    const auto &placement = *settings.placement;
    return !placement.screenName.trimmed().isEmpty()
        && std::isfinite(placement.xRatio)
        && std::isfinite(placement.yRatio)
        && placement.xRatio >= 0.0 && placement.xRatio <= 1.0
        && placement.yRatio >= 0.0 && placement.yRatio <= 1.0;
}

} // namespace smartmate::model
