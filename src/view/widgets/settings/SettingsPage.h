#pragma once

#include <QWidget>

class QButtonGroup;
class QComboBox;

namespace smartmate::viewmodel {
class AppearanceSettingsContract;
}

namespace smartmate::view::widgets {

/// 外观设置页只绑定 AppearanceSettingsContract，不读取具体 ViewModel 兼容 API。
class SettingsPage final : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(viewmodel::AppearanceSettingsContract &settings,
                          QWidget *parent = nullptr);

private:
    /// 非拥有 Contract；用户点击只调用语义 setter，通知到达后再回填控件。
    viewmodel::AppearanceSettingsContract &m_settings;
    /// 三组控件分别绑定强调色、字体族和字号比例展示状态。
    QButtonGroup *m_accentButtons;
    QComboBox *m_fontFamilyComboBox;
    QButtonGroup *m_fontScaleButtons;
};

} // namespace smartmate::view::widgets
