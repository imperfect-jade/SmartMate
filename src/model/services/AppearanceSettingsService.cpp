#include "services/AppearanceSettingsService.h"

#include "repositories/RepositoryException.h"

#include <exception>
#include <utility>

namespace smartmate::model {

bool AppearanceSettingsResult::ok() const noexcept
{
    return error == AppearanceSettingsError::None && value.has_value();
}

AppearanceSettingsResult AppearanceSettingsResult::success(
    AppearanceSettings settings)
{
    return {std::move(settings), AppearanceSettingsError::None, {}};
}

AppearanceSettingsResult AppearanceSettingsResult::failure(
    const AppearanceSettingsError error, QString detail)
{
    return {std::nullopt, error, std::move(detail)};
}

AppearanceSettingsService::AppearanceSettingsService(
    IAppearanceSettingsRepository &repository)
    : m_repository(repository)
{
}

AppearanceSettingsResult AppearanceSettingsService::load() const
{
    try {
        const AppearanceSettings settings = m_repository.load();
        return isValid(settings)
            ? AppearanceSettingsResult::success(settings)
            : AppearanceSettingsResult::success(AppearanceSettings{});
    } catch (const RepositoryException &exception) {
        return AppearanceSettingsResult::failure(
            AppearanceSettingsError::PersistenceFailure,
            QString::fromUtf8(exception.what()));
    } catch (...) {
        return AppearanceSettingsResult::failure(
            AppearanceSettingsError::PersistenceFailure,
            QStringLiteral("Unexpected appearance settings failure."));
    }
}

AppearanceSettingsResult AppearanceSettingsService::save(
    const AppearanceSettings &settings)
{
    if (!isValid(settings)) {
        return AppearanceSettingsResult::failure(
            AppearanceSettingsError::InvalidValue,
            QStringLiteral("Appearance settings contain an invalid value."));
    }
    try {
        m_repository.save(settings);
        return AppearanceSettingsResult::success(settings);
    } catch (const RepositoryException &exception) {
        return AppearanceSettingsResult::failure(
            AppearanceSettingsError::PersistenceFailure,
            QString::fromUtf8(exception.what()));
    } catch (...) {
        return AppearanceSettingsResult::failure(
            AppearanceSettingsError::PersistenceFailure,
            QStringLiteral("Unexpected appearance settings failure."));
    }
}

} // namespace smartmate::model
