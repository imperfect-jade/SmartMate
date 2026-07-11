#pragma once

#include "domain/AppearanceSettings.h"
#include "repositories/IAppearanceSettingsRepository.h"

#include <QString>

#include <optional>

namespace smartmate::model {

enum class AppearanceSettingsError {
    None,
    InvalidValue,
    PersistenceFailure,
};

struct AppearanceSettingsResult final {
    std::optional<AppearanceSettings> value;
    AppearanceSettingsError error{AppearanceSettingsError::None};
    QString detail;

    [[nodiscard]] bool ok() const noexcept;
    [[nodiscard]] static AppearanceSettingsResult success(AppearanceSettings settings);
    [[nodiscard]] static AppearanceSettingsResult failure(
        AppearanceSettingsError error, QString detail = {});
};

/// 校验并编排外观偏好读写，ViewModel 不直接接触 Repository。
class AppearanceSettingsService final {
public:
    explicit AppearanceSettingsService(IAppearanceSettingsRepository &repository);

    [[nodiscard]] AppearanceSettingsResult load() const;
    [[nodiscard]] AppearanceSettingsResult save(const AppearanceSettings &settings);

private:
    IAppearanceSettingsRepository &m_repository;
};

} // namespace smartmate::model
