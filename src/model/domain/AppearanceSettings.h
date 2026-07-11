#pragma once

namespace smartmate::model {

/// 主窗口可持久化的强调色方案；业务状态色不随该值变化。
enum class AccentTheme : int {
    Green = 0,
    Blue = 1,
};

/// 仅允许经过验证的 Windows UI 字体，避免任意字体破坏布局。
enum class UiFontFamily : int {
    System = 0,
    MicrosoftYaHeiUI = 1,
    DengXian = 2,
};

/// 界面字号采用有限档位，像素缩放由 View 的主题令牌解释。
enum class UiFontScale : int {
    Small = 0,
    Standard = 1,
    Large = 2,
};

/// 外观偏好是普通值类型，不依赖 QML 或具体持久化 API。
struct AppearanceSettings final {
    AccentTheme accentTheme{AccentTheme::Green};
    UiFontFamily fontFamily{UiFontFamily::System};
    UiFontScale fontScale{UiFontScale::Standard};

    friend bool operator==(const AppearanceSettings &,
                           const AppearanceSettings &) = default;
};

[[nodiscard]] bool isValid(const AppearanceSettings &settings) noexcept;

} // namespace smartmate::model
