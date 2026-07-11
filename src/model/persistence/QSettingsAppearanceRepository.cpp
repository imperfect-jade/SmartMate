#include "QSettingsAppearanceRepository.h"

#include "repositories/RepositoryException.h"

#include <QSettings>

#include <utility>

namespace smartmate::model::persistence {
namespace {
constexpr auto accentKey = "appearance/accent";
constexpr auto fontFamilyKey = "appearance/fontFamily";
constexpr auto fontScaleKey = "appearance/fontScale";

[[nodiscard]] AccentTheme accentFromString(const QString &value)
{
    return value == QStringLiteral("blue") ? AccentTheme::Blue
                                            : AccentTheme::Green;
}

[[nodiscard]] UiFontFamily familyFromString(const QString &value)
{
    if (value == QStringLiteral("microsoft-yahei-ui")) {
        return UiFontFamily::MicrosoftYaHeiUI;
    }
    if (value == QStringLiteral("dengxian")) {
        return UiFontFamily::DengXian;
    }
    return UiFontFamily::System;
}

[[nodiscard]] UiFontScale scaleFromString(const QString &value)
{
    if (value == QStringLiteral("small")) {
        return UiFontScale::Small;
    }
    if (value == QStringLiteral("large")) {
        return UiFontScale::Large;
    }
    return UiFontScale::Standard;
}

[[nodiscard]] QString accentString(const AccentTheme value)
{
    return value == AccentTheme::Blue ? QStringLiteral("blue")
                                      : QStringLiteral("green");
}

[[nodiscard]] QString familyString(const UiFontFamily value)
{
    switch (value) {
    case UiFontFamily::MicrosoftYaHeiUI:
        return QStringLiteral("microsoft-yahei-ui");
    case UiFontFamily::DengXian:
        return QStringLiteral("dengxian");
    case UiFontFamily::System:
        return QStringLiteral("system");
    }
    return QStringLiteral("system");
}

[[nodiscard]] QString scaleString(const UiFontScale value)
{
    switch (value) {
    case UiFontScale::Small:
        return QStringLiteral("small");
    case UiFontScale::Large:
        return QStringLiteral("large");
    case UiFontScale::Standard:
        return QStringLiteral("standard");
    }
    return QStringLiteral("standard");
}
}

QSettingsAppearanceRepository::QSettingsAppearanceRepository(QString iniFilePath)
{
    if (iniFilePath.isEmpty()) {
        m_settings = std::make_unique<QSettings>();
    } else {
        m_settings = std::make_unique<QSettings>(std::move(iniFilePath),
                                                 QSettings::IniFormat);
    }
}

QSettingsAppearanceRepository::~QSettingsAppearanceRepository() = default;

AppearanceSettings QSettingsAppearanceRepository::load() const
{
    if (m_settings->status() != QSettings::NoError) {
        throw RepositoryException{"Unable to read appearance settings."};
    }
    return {
        accentFromString(m_settings->value(accentKey, QStringLiteral("green")).toString()),
        familyFromString(m_settings->value(fontFamilyKey, QStringLiteral("system")).toString()),
        scaleFromString(m_settings->value(fontScaleKey, QStringLiteral("standard")).toString()),
    };
}

void QSettingsAppearanceRepository::save(const AppearanceSettings &settings)
{
    m_settings->setValue(accentKey, accentString(settings.accentTheme));
    m_settings->setValue(fontFamilyKey, familyString(settings.fontFamily));
    m_settings->setValue(fontScaleKey, scaleString(settings.fontScale));
    m_settings->sync();
    if (m_settings->status() != QSettings::NoError) {
        throw RepositoryException{"Unable to save appearance settings."};
    }
}

} // namespace smartmate::model::persistence
