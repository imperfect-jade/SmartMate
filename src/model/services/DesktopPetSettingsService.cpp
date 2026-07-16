#include "services/DesktopPetSettingsService.h"

#include "repositories/RepositoryException.h"

#include <utility>

namespace smartmate::model {

bool DesktopPetSettingsResult::ok() const noexcept
{
    return error == DesktopPetSettingsError::None && value.has_value();
}

DesktopPetSettingsResult DesktopPetSettingsResult::success(
    DesktopPetSettings settings)
{
    return {std::move(settings), DesktopPetSettingsError::None, {}};
}

DesktopPetSettingsResult DesktopPetSettingsResult::failure(
    const DesktopPetSettingsError error, QString detail)
{
    return {std::nullopt, error, std::move(detail)};
}

DesktopPetSettingsService::DesktopPetSettingsService(
    IDesktopPetSettingsRepository &repository)
    : m_repository(repository)
{
}

DesktopPetSettingsResult DesktopPetSettingsService::load() const
{
    try {
        DesktopPetSettings settings = m_repository.load();
        // 损坏的位置不能阻止桌宠设置加载；保留独立的启用状态并清空位置。
        if (!isValid(settings)) {
            settings.placement.reset();
        }
        return DesktopPetSettingsResult::success(std::move(settings));
    } catch (const RepositoryException &exception) {
        return DesktopPetSettingsResult::failure(
            DesktopPetSettingsError::PersistenceFailure,
            QString::fromUtf8(exception.what()));
    } catch (...) {
        return DesktopPetSettingsResult::failure(
            DesktopPetSettingsError::PersistenceFailure,
            QStringLiteral("Unexpected desktop pet settings failure."));
    }
}

DesktopPetSettingsResult DesktopPetSettingsService::save(
    const DesktopPetSettings &settings)
{
    if (!isValid(settings)) {
        return DesktopPetSettingsResult::failure(
            DesktopPetSettingsError::InvalidValue,
            QStringLiteral("Desktop pet settings contain an invalid value."));
    }
    try {
        m_repository.save(settings);
        return DesktopPetSettingsResult::success(settings);
    } catch (const RepositoryException &exception) {
        return DesktopPetSettingsResult::failure(
            DesktopPetSettingsError::PersistenceFailure,
            QString::fromUtf8(exception.what()));
    } catch (...) {
        return DesktopPetSettingsResult::failure(
            DesktopPetSettingsError::PersistenceFailure,
            QStringLiteral("Unexpected desktop pet settings failure."));
    }
}

} // namespace smartmate::model
