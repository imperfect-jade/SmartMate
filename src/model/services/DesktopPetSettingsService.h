#pragma once

#include "domain/DesktopPetSettings.h"
#include "repositories/IDesktopPetSettingsRepository.h"

#include <QString>

#include <optional>

namespace smartmate::model {

enum class DesktopPetSettingsError {
    None,
    InvalidValue,
    PersistenceFailure,
};

struct DesktopPetSettingsResult final {
    std::optional<DesktopPetSettings> value;
    DesktopPetSettingsError error{DesktopPetSettingsError::None};
    QString detail;

    [[nodiscard]] bool ok() const noexcept;
    [[nodiscard]] static DesktopPetSettingsResult success(
        DesktopPetSettings settings);
    [[nodiscard]] static DesktopPetSettingsResult failure(
        DesktopPetSettingsError error, QString detail = {});
};

/// 负责桌宠设置校验、容错读取与持久化编排。
class DesktopPetSettingsService final {
public:
    explicit DesktopPetSettingsService(
        IDesktopPetSettingsRepository &repository);

    [[nodiscard]] DesktopPetSettingsResult load() const;
    [[nodiscard]] DesktopPetSettingsResult save(
        const DesktopPetSettings &settings);

private:
    IDesktopPetSettingsRepository &m_repository;
};

} // namespace smartmate::model
