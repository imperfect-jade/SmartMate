#include "domain/AppearanceSettings.h"

namespace smartmate::model {

bool isValid(const AppearanceSettings &settings) noexcept
{
    // 枚举可能由持久化整数或外部强制转换产生，不能假定传入值天然合法。
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
