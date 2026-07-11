#include "AppearanceSettingsViewModel.h"

#include "services/AppearanceSettingsService.h"

namespace smartmate::viewmodel {

AppearanceSettingsViewModel::AppearanceSettingsViewModel(QObject *parent)
    : QObject(parent)
{
}

AppearanceSettingsViewModel::AppearanceSettingsViewModel(
    model::AppearanceSettingsService &service, QObject *parent)
    : QObject(parent)
    , m_service(&service)
{
    load();
}

int AppearanceSettingsViewModel::accentThemeIndex() const noexcept
{
    return static_cast<int>(m_settings.accentTheme);
}

int AppearanceSettingsViewModel::fontFamilyIndex() const noexcept
{
    return static_cast<int>(m_settings.fontFamily);
}

int AppearanceSettingsViewModel::fontScaleIndex() const noexcept
{
    return static_cast<int>(m_settings.fontScale);
}

QStringList AppearanceSettingsViewModel::accentThemeOptions() const
{
    return {QStringLiteral("青绿清新"), QStringLiteral("清蓝专注")};
}

QStringList AppearanceSettingsViewModel::fontFamilyOptions() const
{
    return {QStringLiteral("系统默认"),
            QStringLiteral("Microsoft YaHei UI"),
            QStringLiteral("等线")};
}

QStringList AppearanceSettingsViewModel::fontScaleOptions() const
{
    return {QStringLiteral("较小"), QStringLiteral("标准"), QStringLiteral("较大")};
}

QString AppearanceSettingsViewModel::fontFamilyName() const
{
    switch (m_settings.fontFamily) {
    case model::UiFontFamily::MicrosoftYaHeiUI:
        return QStringLiteral("Microsoft YaHei UI");
    case model::UiFontFamily::DengXian:
        return QStringLiteral("DengXian");
    case model::UiFontFamily::System:
        return {};
    }
    return {};
}

qreal AppearanceSettingsViewModel::fontScale() const noexcept
{
    switch (m_settings.fontScale) {
    case model::UiFontScale::Small:
        return 0.9;
    case model::UiFontScale::Large:
        return 1.1;
    case model::UiFontScale::Standard:
        return 1.0;
    }
    return 1.0;
}

QString AppearanceSettingsViewModel::errorMessage() const
{
    return m_errorMessage;
}

void AppearanceSettingsViewModel::setAccentThemeIndex(const int index)
{
    if (index < 0 || index > 1 || index == accentThemeIndex()) {
        return;
    }
    auto candidate = m_settings;
    candidate.accentTheme = static_cast<model::AccentTheme>(index);
    saveCandidate(candidate);
}

void AppearanceSettingsViewModel::setFontFamilyIndex(const int index)
{
    if (index < 0 || index > 2 || index == fontFamilyIndex()) {
        return;
    }
    auto candidate = m_settings;
    candidate.fontFamily = static_cast<model::UiFontFamily>(index);
    saveCandidate(candidate);
}

void AppearanceSettingsViewModel::setFontScaleIndex(const int index)
{
    if (index < 0 || index > 2 || index == fontScaleIndex()) {
        return;
    }
    auto candidate = m_settings;
    candidate.fontScale = static_cast<model::UiFontScale>(index);
    saveCandidate(candidate);
}

void AppearanceSettingsViewModel::resetDefaults()
{
    saveCandidate(model::AppearanceSettings{});
}

void AppearanceSettingsViewModel::clearError()
{
    setError({});
}

void AppearanceSettingsViewModel::load()
{
    const auto result = m_service->load();
    if (result.ok()) {
        apply(*result.value);
        return;
    }
    setError(QStringLiteral("无法读取外观设置，已使用默认值。"));
}

void AppearanceSettingsViewModel::apply(
    const model::AppearanceSettings &settings)
{
    if (m_settings == settings) {
        return;
    }
    m_settings = settings;
    emit appearanceChanged();
}

void AppearanceSettingsViewModel::saveCandidate(
    const model::AppearanceSettings &candidate)
{
    if (candidate == m_settings) {
        return;
    }
    if (m_service != nullptr) {
        const auto result = m_service->save(candidate);
        if (!result.ok()) {
            setError(QStringLiteral("无法保存外观设置，请稍后重试。"));
            return;
        }
    }
    apply(candidate);
    setError({});
}

void AppearanceSettingsViewModel::setError(const QString &message)
{
    if (m_errorMessage == message) {
        return;
    }
    m_errorMessage = message;
    emit errorMessageChanged();
    if (!message.isEmpty()) {
        emit errorOccurred(message);
    }
}

} // namespace smartmate::viewmodel
