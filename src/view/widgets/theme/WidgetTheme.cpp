#include "view/widgets/theme/WidgetTheme.h"

#include "viewmodel/contracts/AppearanceSettingsContract.h"

namespace smartmate::view::widgets {

WidgetTheme WidgetTheme::fromAccentIndex(const int accentThemeIndex)
{
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

QColor WidgetTheme::statusColor(const int statusIndex) const
{
    switch (statusIndex) {
    case 0: return todo;
    case 1: return inProgress;
    case 2: return done;
    case 3: return cancelled;
    default: return archived;
    }
}

QPalette WidgetTheme::palette() const
{
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
    result.setColor(QPalette::Highlight, primary);
    result.setColor(QPalette::HighlightedText, QColor{"#ffffff"});
    return result;
}

QString WidgetTheme::styleSheet() const
{
    return QStringLiteral(R"(
        QMainWindow, QWidget#pageSurface { background: %1; color: %2; }
        QFrame#navigationPanel { background: %3; border-right: 1px solid %4; }
        QFrame#settingsCard, QFrame#previewCard {
            background: %5; border: 1px solid %4; border-radius: 10px;
        }
        QFrame#previewCard { background: %6; }
        QPushButton { padding: 7px 12px; border: 1px solid %7; border-radius: 7px; background: %8; }
        QPushButton:hover { border-color: %9; }
        QPushButton:checked { color: %10; border-color: %10; background: %11; font-weight: 600; }
        QPushButton#taskNavigationButton, QPushButton#graphNavigationButton, QPushButton#settingsNavigationButton {
            text-align: left; padding: 10px 14px; border: none; background: transparent;
        }
        QPushButton#taskNavigationButton:checked, QPushButton#graphNavigationButton:checked, QPushButton#settingsNavigationButton:checked {
            color: %10; background: %11;
        }
        QComboBox { padding: 6px 10px; border: 1px solid %7; border-radius: 6px; background: %5; }
        QLabel#pageTitle { color: %2; font-size: 24px; font-weight: 700; }
        QLabel#sectionTitle { color: %2; font-size: 17px; font-weight: 700; }
        QLabel#secondaryText { color: %12; }
        QLabel#previewStatus { color: %13; }
        QStatusBar { background: %5; color: %12; border-top: 1px solid %4; }
    )")
        .arg(background.name(), textPrimary.name(), navigation.name(),
             borderSoft.name(), surface.name(), surfaceSubtle.name(),
             border.name(), surfaceStrong.name(), borderStrong.name(),
             primary.name(), primarySoft.name(), textSecondary.name(),
             inProgress.name());
}

QFont appearanceFont(const QFont &baseline,
                     const viewmodel::AppearanceSettingsContract &settings)
{
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
