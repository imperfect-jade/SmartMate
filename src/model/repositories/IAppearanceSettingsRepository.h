#pragma once

#include "domain/AppearanceSettings.h"

namespace smartmate::model {

/// 外观偏好的持久化端口；接口不暴露 QSettings 或其他存储类型。
class IAppearanceSettingsRepository {
public:
    virtual ~IAppearanceSettingsRepository() = default;

    [[nodiscard]] virtual AppearanceSettings load() const = 0;
    virtual void save(const AppearanceSettings &settings) = 0;
};

} // namespace smartmate::model
