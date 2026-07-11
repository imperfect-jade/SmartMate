#include "domain/AppearanceSettings.h"

namespace smartmate::model {

bool isValid(const AppearanceSettings &settings) noexcept
{
    const bool validAccent = settings.accentTheme == AccentTheme::Green
        || settings.accentTheme == AccentTheme::Blue;
    const bool validFamily = settings.fontFamily == UiFontFamily::System
        || settings.fontFamily == UiFontFamily::MicrosoftYaHeiUI
        || settings.fontFamily == UiFontFamily::DengXian;
    const bool validScale = settings.fontScale == UiFontScale::Small
        || settings.fontScale == UiFontScale::Standard
        || settings.fontScale == UiFontScale::Large;
    return validAccent && validFamily && validScale;
}

} // namespace smartmate::model
