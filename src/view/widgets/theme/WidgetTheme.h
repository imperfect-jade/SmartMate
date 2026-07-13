#pragma once

#include <QColor>
#include <QFont>
#include <QPalette>
#include <QString>

namespace smartmate::viewmodel {
class AppearanceSettingsContract;
}

class QWidget;

namespace smartmate::view::widgets {

/// Widgets 对外观 Contract 的纯展示解释；不承载业务状态或持久化语义。
struct WidgetTheme {
    QColor primary;
    QColor primarySoft;
    QColor background;
    QColor navigation;
    QColor surface;
    QColor surfaceSubtle;
    QColor surfaceStrong;
    QColor surfaceElevated;
    QColor controlHover;
    QColor border;
    QColor borderSoft;
    QColor borderStrong;
    QColor textPrimary;
    QColor textBody;
    QColor textSecondary;
    QColor textMuted;
    QColor textDisabled;
    QColor todo;
    QColor inProgress;
    QColor done;
    QColor cancelled;
    QColor archived;
    QColor warning;
    QColor danger;

    [[nodiscard]] QColor statusColor(int statusIndex) const;

    [[nodiscard]] static WidgetTheme fromAccentIndex(int accentThemeIndex);
    [[nodiscard]] QPalette palette() const;
    [[nodiscard]] QString styleSheet() const;
};

/// 从固定基准字体重新计算外观，避免连续切换字号产生累计缩放。
[[nodiscard]] QFont appearanceFont(
    const QFont &baseline,
    const viewmodel::AppearanceSettingsContract &settings);

} // namespace smartmate::view::widgets
