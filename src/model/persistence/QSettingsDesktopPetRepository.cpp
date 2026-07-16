#include "QSettingsDesktopPetRepository.h"

#include "repositories/RepositoryException.h"

#include <QSettings>

#include <cmath>
#include <utility>

namespace smartmate::model::persistence {
namespace {
constexpr auto enabledKey = "desktopPet/enabled";
constexpr auto screenNameKey = "desktopPet/placement/screenName";
constexpr auto xRatioKey = "desktopPet/placement/xRatio";
constexpr auto yRatioKey = "desktopPet/placement/yRatio";
}

QSettingsDesktopPetRepository::QSettingsDesktopPetRepository(QString iniFilePath)
{
    if (iniFilePath.isEmpty()) {
        m_settings = std::make_unique<QSettings>();
    } else {
        m_settings = std::make_unique<QSettings>(std::move(iniFilePath),
                                                 QSettings::IniFormat);
    }
}

QSettingsDesktopPetRepository::~QSettingsDesktopPetRepository() = default;

DesktopPetSettings QSettingsDesktopPetRepository::load() const
{
    if (m_settings->status() != QSettings::NoError) {
        throw RepositoryException{"Unable to read desktop pet settings."};
    }

    DesktopPetSettings result;
    result.enabled = m_settings->value(enabledKey, false).toBool();
    const bool hasAllPlacementKeys = m_settings->contains(screenNameKey)
        && m_settings->contains(xRatioKey) && m_settings->contains(yRatioKey);
    if (hasAllPlacementKeys) {
        bool xOk = false;
        bool yOk = false;
        const QString screenName = m_settings->value(screenNameKey).toString();
        const double x = m_settings->value(xRatioKey).toDouble(&xOk);
        const double y = m_settings->value(yRatioKey).toDouble(&yOk);
        if (xOk && yOk && std::isfinite(x) && std::isfinite(y)) {
            result.placement = DesktopPetPlacement{screenName, x, y};
        }
    }
    return result;
}

void QSettingsDesktopPetRepository::save(const DesktopPetSettings &settings)
{
    m_settings->setValue(enabledKey, settings.enabled);
    if (settings.placement.has_value()) {
        m_settings->setValue(screenNameKey, settings.placement->screenName);
        m_settings->setValue(xRatioKey, settings.placement->xRatio);
        m_settings->setValue(yRatioKey, settings.placement->yRatio);
    } else {
        m_settings->remove(QStringLiteral("desktopPet/placement"));
    }
    m_settings->sync();
    if (m_settings->status() != QSettings::NoError) {
        throw RepositoryException{"Unable to save desktop pet settings."};
    }
}

} // namespace smartmate::model::persistence
