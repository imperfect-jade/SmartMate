#pragma once

#include "viewmodel/contracts/TaskPresentationTypes.h"
#include "viewmodel/contracts/StatisticsContract.h"
#include "viewmodel/contracts/FocusContract.h"

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
    /// 基础表面、边框和文字颜色，均为运行期展示派生值。
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
    /// 任务状态和反馈语义色，不代表领域状态本身。
    QColor todo;
    QColor inProgress;
    QColor done;
    QColor cancelled;
    QColor archived;
    QColor warning;
    QColor danger;

    [[nodiscard]] QColor statusColor(viewmodel::TaskStatusVisual status) const;
    [[nodiscard]] QColor priorityColor(viewmodel::TaskPriorityVisual priority) const;
    /// 将 Statistics Contract 的稳定颜色枚举解释为 Widgets/Charts 颜色。
    [[nodiscard]] QColor statisticsCategoryColor(
        viewmodel::StatisticsCategoryContract::Color color) const;
    /// 将 Focus Contract 的稳定类别色解释为 Widgets 颜色。
    [[nodiscard]] QColor focusCategoryColor(
        viewmodel::FocusContract::CategoryColor color) const;

    [[nodiscard]] static WidgetTheme fromAccentIndex(int accentThemeIndex);
    [[nodiscard]] static WidgetTheme fromPalette(const QPalette &palette);
    [[nodiscard]] QPalette palette() const;
    [[nodiscard]] QString styleSheet() const;
};

/// 从固定基准字体重新计算外观，避免连续切换字号产生累计缩放。
[[nodiscard]] QFont appearanceFont(
    const QFont &baseline,
    const viewmodel::AppearanceSettingsContract &settings);

} // namespace smartmate::view::widgets
