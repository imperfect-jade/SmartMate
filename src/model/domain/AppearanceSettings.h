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

/// 外观偏好是普通值类型，不依赖具体 View 或持久化 API。
struct AppearanceSettings final {
    /// 窗口表面、导航和控件使用的主题强调色。
    AccentTheme accentTheme{AccentTheme::Green};
    /// 正文和控件使用的受支持字体档位。
    UiFontFamily fontFamily{UiFontFamily::System};
    /// 界面字号缩放档位，不直接保存像素大小。
    UiFontScale fontScale{UiFontScale::Standard};

    friend bool operator==(const AppearanceSettings &,
                           const AppearanceSettings &) = default;
};

/// 校验所有枚举均属于当前支持集合，防御持久化损坏或非法强制转换。
[[nodiscard]] bool isValid(const AppearanceSettings &settings) noexcept;

} // namespace smartmate::model
