#pragma once

#include "repositories/IAppearanceSettingsRepository.h"

#include <QString>

#include <memory>

class QSettings;

namespace smartmate::model::persistence {

/// 使用 QSettings 保存轻量外观偏好；空文件路径使用应用组织与名称。
class QSettingsAppearanceRepository final
    : public IAppearanceSettingsRepository {
public:
    explicit QSettingsAppearanceRepository(QString iniFilePath = {});
    ~QSettingsAppearanceRepository() override;

    [[nodiscard]] AppearanceSettings load() const override;
    void save(const AppearanceSettings &settings) override;

private:
    std::unique_ptr<QSettings> m_settings;
};

} // namespace smartmate::model::persistence
