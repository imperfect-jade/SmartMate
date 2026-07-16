#pragma once

#include "domain/DesktopPetSettings.h"

namespace smartmate::model {

/// 桌宠设置持久化端口；不向上层暴露 QSettings。
class IDesktopPetSettingsRepository {
public:
    virtual ~IDesktopPetSettingsRepository() = default;

    [[nodiscard]] virtual DesktopPetSettings load() const = 0;
    virtual void save(const DesktopPetSettings &settings) = 0;
};

} // namespace smartmate::model
