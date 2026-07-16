#pragma once

#include "repositories/IDesktopPetSettingsRepository.h"

#include <QString>

#include <memory>

class QSettings;

namespace smartmate::model::persistence {

/// 使用 QSettings 持久化桌宠开关和悬浮位置。
class QSettingsDesktopPetRepository final
    : public IDesktopPetSettingsRepository {
public:
    explicit QSettingsDesktopPetRepository(QString iniFilePath = {});
    ~QSettingsDesktopPetRepository() override;

    [[nodiscard]] DesktopPetSettings load() const override;
    void save(const DesktopPetSettings &settings) override;

private:
    std::unique_ptr<QSettings> m_settings;
};

} // namespace smartmate::model::persistence
