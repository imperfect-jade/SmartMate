#include "DesktopPetSettingsViewModel.h"

#include "services/DesktopPetSettingsService.h"

namespace smartmate::viewmodel {

DesktopPetSettingsViewModel::DesktopPetSettingsViewModel(QObject *parent)
    : DesktopPetSettingsContract(parent)
{
}

DesktopPetSettingsViewModel::DesktopPetSettingsViewModel(
    model::DesktopPetSettingsService &service, QObject *parent)
    : DesktopPetSettingsContract(parent)
    , m_service(&service)
{
    load();
}

bool DesktopPetSettingsViewModel::enabled() const noexcept
{
    return m_settings.enabled;
}

bool DesktopPetSettingsViewModel::hasFloatingPlacement() const noexcept
{
    return m_settings.placement.has_value();
}

QString DesktopPetSettingsViewModel::floatingScreenName() const
{
    return m_settings.placement.has_value()
        ? m_settings.placement->screenName : QString{};
}

qreal DesktopPetSettingsViewModel::floatingXRatio() const noexcept
{
    return m_settings.placement.has_value()
        ? m_settings.placement->xRatio : 0.0;
}

qreal DesktopPetSettingsViewModel::floatingYRatio() const noexcept
{
    return m_settings.placement.has_value()
        ? m_settings.placement->yRatio : 0.0;
}

void DesktopPetSettingsViewModel::setEnabled(const bool enabled)
{
    if (m_settings.enabled == enabled) {
        return;
    }
    auto candidate = m_settings;
    candidate.enabled = enabled;
    saveCandidate(candidate);
}

void DesktopPetSettingsViewModel::saveFloatingPlacement(
    const QString &screenName, const qreal xRatio, const qreal yRatio)
{
    auto candidate = m_settings;
    candidate.placement = model::DesktopPetPlacement{
        screenName.trimmed(), static_cast<double>(xRatio),
        static_cast<double>(yRatio)};
    if (candidate == m_settings) {
        return;
    }
    saveCandidate(candidate);
}

void DesktopPetSettingsViewModel::load()
{
    const auto result = m_service->load();
    if (result.ok()) {
        apply(*result.value);
        return;
    }
    emit notificationRaised({common::UiSeverity::Error,
                             QStringLiteral("桌宠设置读取失败"),
                             QStringLiteral("已使用默认设置。")});
}

void DesktopPetSettingsViewModel::saveCandidate(
    const model::DesktopPetSettings &candidate)
{
    if (m_service != nullptr) {
        const auto result = m_service->save(candidate);
        if (!result.ok()) {
            raiseSaveError();
            return;
        }
    }
    apply(candidate);
}

void DesktopPetSettingsViewModel::apply(
    const model::DesktopPetSettings &settings)
{
    const bool enabledValueChanged = m_settings.enabled != settings.enabled;
    const bool placementValueChanged = m_settings.placement != settings.placement;
    if (!enabledValueChanged && !placementValueChanged) {
        return;
    }
    m_settings = settings;
    if (enabledValueChanged) {
        emit enabledChanged();
    }
    if (placementValueChanged) {
        emit floatingPlacementChanged();
    }
}

void DesktopPetSettingsViewModel::raiseSaveError()
{
    emit notificationRaised({common::UiSeverity::Error,
                             QStringLiteral("桌宠设置保存失败"),
                             QStringLiteral("无法保存桌宠设置，请稍后重试。")});
}

} // namespace smartmate::viewmodel
