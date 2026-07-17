#include "view/widgets/theme/WidgetTheme.h"

#include "viewmodel/contracts/AppearanceSettingsContract.h"

namespace smartmate::view::widgets {

WidgetTheme WidgetTheme::fromAccentIndex(const int accentThemeIndex)
{
    // Contract 暴露稳定选项索引，View 在此一次映射为完整 Widgets 色板。
    const bool blue = accentThemeIndex == 1;
    return {
        QColor{blue ? "#2563eb" : "#507936"},
        QColor{blue ? "#ddeaff" : "#e7f0d6"},
        QColor{blue ? "#eaf2fc" : "#eef5e5"},
        QColor{blue ? "#dce9fa" : "#dde9c5"},
        QColor{blue ? "#f8fbff" : "#fafdf5"},
        QColor{blue ? "#ecf3fc" : "#f0f6e7"},
        QColor{blue ? "#e0ecfa" : "#e4eed3"},
        QColor{"#ffffff"},
        QColor{blue ? "#e4effc" : "#eaf2dd"},
        QColor{blue ? "#abc4e4" : "#a9be7b"},
        QColor{blue ? "#ceddf0" : "#cdddb0"},
        QColor{blue ? "#769ac8" : "#719847"},
        QColor{blue ? "#142c4a" : "#24351f"},
        QColor{blue ? "#294765" : "#354b2e"},
        QColor{blue ? "#496783" : "#506449"},
        QColor{blue ? "#61778e" : "#677761"},
        QColor{"#98a2b3"},
        QColor{"#175cd3"},
        QColor{"#387a4a"},
        QColor{"#067647"},
        QColor{"#667085"},
        QColor{"#475467"},
        QColor{"#b54708"},
        QColor{"#b42318"},
    };
}

WidgetTheme WidgetTheme::fromPalette(const QPalette &palette)
{
    const WidgetTheme blue = fromAccentIndex(1);
    return palette.color(QPalette::Highlight) == blue.primary
        ? blue : fromAccentIndex(0);
}

QColor WidgetTheme::statusColor(const viewmodel::TaskStatusVisual status) const
{
    switch (status) {
    case viewmodel::TaskStatusVisual::Todo: return todo;
    case viewmodel::TaskStatusVisual::InProgress: return inProgress;
    case viewmodel::TaskStatusVisual::Done: return done;
    case viewmodel::TaskStatusVisual::Cancelled: return cancelled;
    case viewmodel::TaskStatusVisual::Archived: return archived;
    }
    return archived;
}

QColor WidgetTheme::priorityColor(const viewmodel::TaskPriorityVisual priority) const
{
    switch (priority) {
    case viewmodel::TaskPriorityVisual::Low: return textSecondary;
    case viewmodel::TaskPriorityVisual::Normal: return todo;
    case viewmodel::TaskPriorityVisual::High: return warning;
    case viewmodel::TaskPriorityVisual::Urgent: return danger;
    }
    return danger;
}

QColor WidgetTheme::statisticsCategoryColor(
    const viewmodel::StatisticsCategoryContract::Color color) const
{
    using Color = viewmodel::StatisticsCategoryContract::Color;
    switch (color) {
    case Color::Blue: return QColor{"#2563eb"};
    case Color::Teal: return QColor{"#0f766e"};
    case Color::Green: return QColor{"#15803d"};
    case Color::Amber: return QColor{"#b45309"};
    case Color::Orange: return QColor{"#c2410c"};
    case Color::Rose: return QColor{"#be123c"};
    case Color::Violet: return QColor{"#7c3aed"};
    case Color::Slate: return QColor{"#475569"};
    case Color::Unclassified: return QColor{"#94a3b8"};
    case Color::Other: return QColor{"#64748b"};
    }
    return textMuted;
}

QColor WidgetTheme::focusCategoryColor(
    const viewmodel::FocusContract::CategoryColor color) const
{
    using Color = viewmodel::FocusContract::CategoryColor;
    switch (color) {
    case Color::Blue: return QColor{"#2563eb"};
    case Color::Teal: return QColor{"#0f766e"};
    case Color::Green: return QColor{"#15803d"};
    case Color::Amber: return QColor{"#b45309"};
    case Color::Orange: return QColor{"#c2410c"};
    case Color::Rose: return QColor{"#be123c"};
    case Color::Violet: return QColor{"#7c3aed"};
    case Color::Slate: return QColor{"#475569"};
    case Color::Unclassified: return QColor{"#94a3b8"};
    }
    return textMuted;
}

QPalette WidgetTheme::palette() const
{
    // Palette 与 QSS 共用同一主题对象，避免自绘图元和标准控件出现颜色分裂。
    QPalette result;
    result.setColor(QPalette::Window, background);
    result.setColor(QPalette::WindowText, textPrimary);
    result.setColor(QPalette::Base, surface);
    result.setColor(QPalette::AlternateBase, surfaceSubtle);
    result.setColor(QPalette::Text, textBody);
    result.setColor(QPalette::Button, surfaceStrong);
    result.setColor(QPalette::ButtonText, textPrimary);
    result.setColor(QPalette::Light, QColor{"#ffffff"});
    result.setColor(QPalette::Midlight, borderSoft);
    result.setColor(QPalette::Mid, border);
    result.setColor(QPalette::Dark, borderStrong);
    result.setColor(QPalette::PlaceholderText, textMuted);
    result.setColor(QPalette::Link, primary);
    result.setColor(QPalette::BrightText, danger);
    result.setColor(QPalette::LinkVisited, warning);
    result.setColor(QPalette::Highlight, primary);
    result.setColor(QPalette::HighlightedText, QColor{"#ffffff"});
    return result;
}

QString WidgetTheme::styleSheet() const
{
    return QStringLiteral(R"(
        QMainWindow, QWidget#pageSurface, QScrollArea#statisticsPage,
        QWidget#statisticsViewport, QWidget#statisticsContent,
        QScrollArea#focusPage, QWidget#focusViewport, QWidget#focusContent {
            background: %1; color: %2;
        }
        QFrame#navigationPanel { background: %3; border-right: 1px solid %4; }
        QFrame#settingsCard, QFrame#desktopPetSettingsCard, QFrame#previewCard {
            background: %5; border: 1px solid %4; border-radius: 10px;
        }
        QFrame#previewCard { background: %6; }
        QPushButton { padding: 7px 12px; border: 1px solid %7; border-radius: 7px; background: %8; }
        QPushButton:hover { border-color: %9; }
        QPushButton:checked { color: %10; border-color: %10; background: %11; font-weight: 600; }
        QPushButton#taskNavigationButton, QPushButton#graphNavigationButton,
        QPushButton#focusNavigationButton,
        QPushButton#statisticsNavigationButton, QPushButton#settingsNavigationButton {
            text-align: left; padding: 10px 14px; border: none; background: transparent;
        }
        QPushButton#taskNavigationButton:checked, QPushButton#graphNavigationButton:checked,
        QPushButton#focusNavigationButton:checked,
        QPushButton#statisticsNavigationButton:checked, QPushButton#settingsNavigationButton:checked {
            color: %10; background: %11;
        }
        QComboBox { padding: 6px 10px; border: 1px solid %7; border-radius: 6px; background: %5; }
        QDialog#taskEditorDialog { background: %14; color: %2; }
        QFrame#taskEditorHeader, QFrame#taskEditorFooter {
            background: %6; border: none;
        }
        QFrame#taskEditorHeader { border-bottom: 1px solid %4; }
        QFrame#taskEditorFooter { border-top: 1px solid %4; }
        QScrollArea#taskEditorScrollView, QWidget#taskEditorContent {
            background: %14; border: none;
        }
        QFrame#taskEditorBasicSection, QFrame#taskEditorPlanningSection,
        QFrame#taskEditorScheduleSection {
            background: %5; border: 1px solid %4; border-radius: 11px;
        }
        QLabel#taskEditorHeaderTitle { color: %2; font-size: 21px; font-weight: 700; }
        QLabel#taskEditorHeaderSubtitle { color: %12; font-size: 12px; }
        QLabel#taskEditorSectionTitle { color: %10; font-size: 16px; font-weight: 700; }
        QLabel#taskEditorFieldLabel { color: %15; font-weight: 600; }
        QDialog#taskEditorDialog QLineEdit, QDialog#taskEditorDialog QPlainTextEdit {
            color: %15; background: %5; border: 1px solid %7;
            border-radius: 8px; padding: 8px 10px;
        }
        QDialog#taskEditorDialog QLineEdit:focus,
        QDialog#taskEditorDialog QPlainTextEdit:focus { border: 2px solid %10; }
        QFrame#taskEditorReadOnlyField {
            background: %6; border: 1px solid %7; border-radius: 8px;
        }
        QLabel#taskEditorValidation { color: %16; border: none; background: transparent; }
        QPushButton#saveTaskButton {
            color: white; background: %10; border-color: %10; font-weight: 700;
        }
        QPushButton#clearCreationPredecessorButton, QPushButton#clearDeadlineButton,
        QPushButton#clearDurationButton { min-width: 20px; padding: 7px 8px; }
        QDialog#taskDetailsDialog { background: %14; color: %2; }
        QFrame#taskDetailsHeader, QFrame#taskDetailsFooter {
            background: %6; border: none;
        }
        QFrame#taskDetailsHeader { border-bottom: 1px solid %4; }
        QFrame#taskDetailsFooter { border-top: 1px solid %4; }
        QScrollArea#taskDetailsScrollView, QWidget#taskDetailsContent {
            background: %14; border: none;
        }
        QLabel#taskDetailsEyebrow { color: %10; font-size: 12px; font-weight: 700; }
        QLabel#taskDetailsTitle { color: %2; font-size: 21px; font-weight: 700; }
        QFrame#taskDetailsDescriptionSection, QFrame#taskDetailsScheduleSection,
        QFrame#taskDetailsInsightSection {
            background: %5; border: 1px solid %4; border-radius: 11px;
        }
        QLabel#taskDetailsSectionTitle { color: %10; font-size: 15px; font-weight: 700; }
        QLabel#taskDetailsDescription { color: %15; background: transparent; border: none; }
        QFrame#taskDetailsMetadataItem, QFrame#taskDetailsMetric {
            background: %6; border: 1px solid %4; border-radius: 8px;
        }
        QLabel#taskDetailsMetadataCaption, QLabel#taskDetailsMetricCaption,
        QLabel#taskDetailsInsightCaption {
            color: %12; font-size: 11px; font-weight: 600;
            background: transparent; border: none;
        }
        QLabel#taskDetailsDeadlineValue, QLabel#taskDetailsEstimateValue,
        QLabel#taskDetailsCreatedValue, QLabel#taskDetailsUpdatedValue {
            color: %15; background: transparent; border: none;
        }
        QLabel#taskDetailsPredecessorCount, QLabel#taskDetailsUnlockCount {
            color: %10; font-size: 18px; font-weight: 700;
            background: transparent; border: none;
        }
        QPushButton#editSelectedTaskButton {
            color: white; background: %10; border-color: %10; font-weight: 700;
        }
        QDialog#deadlinePickerDialog, QDialog#durationPickerDialog {
            background: %14; color: %2;
        }
        QFrame#pickerHeader, QFrame#pickerFooter {
            background: %6; border: none;
        }
        QFrame#pickerHeader { border-bottom: 1px solid %4; }
        QFrame#pickerFooter { border-top: 1px solid %4; }
        QLabel#pickerHeaderTitle { color: %2; font-size: 20px; font-weight: 700; }
        QLabel#pickerHeaderSubtitle { color: %12; font-size: 12px; }
        QLabel#pickerSectionTitle, QLabel#deadlineMonthTitle {
            color: %2; font-size: 15px; font-weight: 700;
        }
        QScrollArea#deadlinePickerScrollView, QScrollArea#durationPickerScrollView,
        QWidget#pickerContent { background: %14; border: none; }
        QFrame#deadlineCalendarCard, QFrame#deadlineTimeCard,
        QFrame#durationValueCard {
            background: %5; border: 1px solid %4; border-radius: 11px;
        }
        QCalendarWidget#deadlineCalendar { background: %5; border: none; }
        QCalendarWidget#deadlineCalendar QAbstractItemView {
            color: %15; background: %5; alternate-background-color: %6;
            selection-color: %10; selection-background-color: %11; border: none;
        }
        QTimeEdit#deadlineTimeEdit, QSpinBox#durationDaysSpinBox,
        QSpinBox#durationHoursSpinBox, QSpinBox#durationMinutesSpinBox {
            color: %15; background: %14; border: 1px solid %7;
            border-radius: 8px; padding: 8px 10px; font-size: 15px;
        }
        QLabel#durationValueLabel { color: %12; font-weight: 600; }
        QLabel#durationSummaryLabel {
            color: %10; background: %11; border: 1px solid %4;
            border-radius: 9px; padding: 8px 12px; font-weight: 700;
        }
        QPushButton#confirmDeadlineSelectionButton,
        QPushButton#confirmDurationSelectionButton {
            color: white; background: %10; border-color: %10; font-weight: 700;
        }
        QLabel#pageTitle { color: %2; font-size: 24px; font-weight: 700; }
        QLabel#sectionTitle { color: %2; font-size: 17px; font-weight: 700; }
        QLabel#settingsPreviewTitle { color: %2; font-size: 17px; font-weight: 700; }
        QLabel#settingsPreviewDescription { color: %12; font-size: 14px; }
        QLabel#secondaryText { color: %12; }
        QLabel#previewStatus { color: %13; font-size: 13px; }
        QLabel#focusEyebrow { color: %10; font-size: 13px; font-weight: 700; }
        QLabel#focusTaskTitle { color: %2; font-size: 20px; font-weight: 700; }
        QLabel#focusTaskDescription { color: %12; font-size: 13px; }
        QLabel#focusTaskMeta { color: %12; font-size: 12px; }
        QLabel#focusOverdueReminder { color: %16; font-size: 12px; font-weight: 600; }
        QFrame#focusSessionCard, QFrame#focusHistoryRow {
            background: %5; border: 1px solid %4; border-radius: 11px;
        }
        QLabel#focusSessionState { color: %10; font-size: 13px; font-weight: 700; }
        QLabel#focusSessionTaskTitle { color: %2; font-size: 21px; font-weight: 700; }
        QLabel#focusElapsedText { color: %2; font-size: 46px; font-weight: 700; padding: 14px; }
        QLabel#focusCategoryText, QLabel#focusEstimateText, QLabel#focusStartedAtText,
        QLabel#focusHistoryCount, QLabel#focusHistoryMetadata { color: %12; }
        QLabel#focusEmptyState, QLabel#focusHistoryEmptyState {
            color: %12; background: %6; border: 1px dashed %7;
            border-radius: 9px; padding: 20px;
        }
        QLabel#focusStorageWarning {
            color: %16; background: %6; border: 1px solid %7;
            border-radius: 8px; padding: 9px 12px; font-weight: 600;
        }
        QLabel#focusHistoryTaskTitle, QLabel#focusHistoryDuration {
            color: %2; font-weight: 700;
        }
        QPushButton#startFocusButton, QPushButton#resumeFocusButton,
        QPushButton#completeFocusButton {
            color: white; background: %10; border-color: %10; font-weight: 700;
        }
        QPushButton#abandonFocusButton { color: %16; }
        QFrame#todayStatisticsCard, QFrame#weekStatisticsCard,
        QFrame#onTimeStatisticsCard, QFrame#overdueStatisticsCard,
        QFrame#trendStatisticsCard, QFrame#categoryStatisticsCard,
        QFrame#healthStatisticsCard {
            background: %5; border: 1px solid %4; border-radius: 11px;
        }
        QLabel#statisticsCardTitle { color: %12; font-size: 12px; font-weight: 700; }
        QLabel#todayStatisticsValue, QLabel#weekStatisticsValue,
        QLabel#onTimeStatisticsValue, QLabel#overdueStatisticsValue {
            color: %2; font-size: 24px; font-weight: 700;
        }
        QLabel#todayStatisticsDetail, QLabel#weekStatisticsDetail,
        QLabel#onTimeStatisticsDetail, QLabel#overdueStatisticsDetail,
        QLabel#trendAccessibleSummary, QLabel#categoryAccessibleSummary,
        QLabel#healthAccessibleSummary { color: %12; font-size: 12px; }
        QLabel#trendEmptyState, QLabel#categoryEmptyState {
            color: %12; background: %6; border: 1px dashed %7;
            border-radius: 9px; padding: 20px;
        }
        QStackedWidget#trendStatisticsStack, QStackedWidget#categoryStatisticsStack {
            background: transparent; border: none;
        }
        QPushButton#last7DaysButton, QPushButton#last30DaysButton,
        QPushButton#last12WeeksButton { padding: 6px 10px; }
        QStatusBar { background: %5; color: %12; border-top: 1px solid %4; }
    )")
        .arg(background.name(), textPrimary.name(), navigation.name(),
             borderSoft.name(), surface.name(), surfaceSubtle.name(),
             border.name(), surfaceStrong.name(), borderStrong.name(),
             primary.name(), primarySoft.name(), textSecondary.name(),
             inProgress.name(), surfaceElevated.name(), textBody.name(),
             danger.name());
}

QFont appearanceFont(const QFont &baseline,
                     const viewmodel::AppearanceSettingsContract &settings)
{
    // 每次从基准字体重算比例，禁止在当前字体上连续乘法造成累计漂移。
    QFont result = baseline;
    const QString family = settings.fontFamilyName();
    if (!family.isEmpty()) {
        result.setFamily(family);
    }
    const qreal baseSize = baseline.pointSizeF() > 0.0
        ? baseline.pointSizeF()
        : 10.0;
    result.setPointSizeF(baseSize * settings.fontScale());
    return result;
}

} // namespace smartmate::view::widgets
