#include "view/widgets/MainWindow.h"

#include "view/widgets/settings/SettingsPage.h"
#include "view/widgets/task/TaskPage.h"
#include "view/widgets/graph/DependencyGraphPage.h"
#include "view/widgets/theme/WidgetTheme.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>

namespace smartmate::view::widgets {
namespace {

QWidget *migrationPlaceholder(const QString &title, const QString &description)
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("pageSurface"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(36, 30, 36, 30);
    auto *titleLabel = new QLabel{title};
    titleLabel->setObjectName(QStringLiteral("pageTitle"));
    auto *descriptionLabel = new QLabel{description};
    descriptionLabel->setObjectName(QStringLiteral("secondaryText"));
    descriptionLabel->setWordWrap(true);
    layout->addWidget(titleLabel);
    layout->addWidget(descriptionLabel);
    layout->addStretch();
    return page;
}

QPushButton *navigationButton(const QString &text,
                              const QString &objectName,
                              QButtonGroup &group,
                              QVBoxLayout &layout,
                              const int index)
{
    auto *button = new QPushButton{text};
    button->setObjectName(objectName);
    button->setCheckable(true);
    group.addButton(button, index);
    layout.addWidget(button);
    return button;
}

} // namespace

MainWindow::MainWindow(MainWindowDependencies dependencies, QWidget *parent)
    : MainWindow(dependencies.appearanceSettings,
                 new TaskPage{{dependencies.taskList,
                               dependencies.taskFocus,
                               dependencies.taskDetails,
                               dependencies.taskEditor,
                               dependencies.taskCategories,
                               dependencies.taskDependencies}},
                 new DependencyGraphPage{{dependencies.appearanceSettings,
                                          dependencies.taskGraph,
                                          dependencies.taskDetails,
                                          dependencies.taskDependencies}},
                 parent)
{
    auto *taskPage = qobject_cast<TaskPage *>(m_pages->widget(0));
    connect(taskPage, &TaskPage::showDependencyGraphRequested, m_pages,
            [this] { m_pages->setCurrentIndex(1); });
    connect(&dependencies.taskList, &viewmodel::TaskListContract::notificationRaised,
            this, &MainWindow::showNotification);
    connect(&dependencies.taskFocus, &viewmodel::TaskFocusContract::notificationRaised,
            this, &MainWindow::showNotification);
    connect(&dependencies.taskDetails, &viewmodel::TaskDetailsContract::notificationRaised,
            this, &MainWindow::showNotification);
    connect(&dependencies.taskEditor, &viewmodel::TaskEditorContract::notificationRaised,
            this, &MainWindow::showNotification);
    connect(&dependencies.taskCategories,
            &viewmodel::TaskCategoryContract::notificationRaised,
            this, &MainWindow::showNotification);
    connect(&dependencies.taskDependencies,
            &viewmodel::TaskDependencyContract::notificationRaised,
            this, &MainWindow::showNotification);
    connect(&dependencies.taskGraph,
            &viewmodel::TaskGraphContract::notificationRaised,
            this, &MainWindow::showNotification);
}

MainWindow::MainWindow(viewmodel::AppearanceSettingsContract &appearanceSettings,
                       QWidget *parent)
    : MainWindow(appearanceSettings,
                 migrationPlaceholder(tr("任务"), tr("任务页面未注入测试依赖。")),
                 migrationPlaceholder(tr("依赖图"), tr("依赖图页面未注入测试依赖。")),
                 parent)
{
}

MainWindow::MainWindow(viewmodel::AppearanceSettingsContract &appearanceSettings,
                       QWidget *taskPage, QWidget *graphPage, QWidget *parent)
    : QMainWindow(parent)
    , m_appearanceSettings(appearanceSettings)
    , m_baselineFont(QApplication::font())
    , m_pages(new QStackedWidget(this))
    , m_navigation(new QFrame(this))
    , m_brand(new QLabel(QStringLiteral("SmartMate"), m_navigation))
    , m_taskNavigation(nullptr)
    , m_graphNavigation(nullptr)
    , m_settingsNavigation(nullptr)
{
    setObjectName(QStringLiteral("mainWindow"));
    setWindowTitle(QCoreApplication::applicationName().isEmpty()
                       ? QStringLiteral("SmartMate")
                       : QCoreApplication::applicationName());
    resize(1180, 760);
    setMinimumSize(900, 620);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("pageSurface"));
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    setCentralWidget(central);

    m_navigation->setParent(central);
    m_navigation->setObjectName(QStringLiteral("navigationPanel"));
    m_navigation->setFixedWidth(208);
    auto *navigationLayout = new QVBoxLayout(m_navigation);
    navigationLayout->setContentsMargins(14, 18, 14, 18);
    navigationLayout->setSpacing(8);

    m_brand->setObjectName(QStringLiteral("sectionTitle"));
    m_brand->setAlignment(Qt::AlignCenter);
    navigationLayout->addWidget(m_brand);
    navigationLayout->addSpacing(18);

    auto *navigationGroup = new QButtonGroup(this);
    navigationGroup->setExclusive(true);
    m_taskNavigation = navigationButton(tr("任务"),
                                        QStringLiteral("taskNavigationButton"),
                                        *navigationGroup,
                                        *navigationLayout, 0);
    m_taskNavigation->setAccessibleName(tr("任务"));
    m_graphNavigation = navigationButton(tr("依赖图"), QStringLiteral("graphNavigationButton"),
                                         *navigationGroup, *navigationLayout, 1);
    m_settingsNavigation = navigationButton(tr("设置"), QStringLiteral("settingsNavigationButton"),
                                            *navigationGroup, *navigationLayout, 2);
    navigationLayout->addStretch();

    rootLayout->addWidget(m_navigation);
    rootLayout->addWidget(m_pages, 1);

    m_pages->addWidget(taskPage);
    m_pages->addWidget(graphPage);
    m_pages->addWidget(new SettingsPage{appearanceSettings, m_pages});

    connect(navigationGroup, &QButtonGroup::idClicked, m_pages,
            [this](const int index) { m_pages->setCurrentIndex(index); });
    m_taskNavigation->setChecked(true);
    m_pages->setCurrentIndex(0);

    statusBar()->setObjectName(QStringLiteral("notificationStatusBar"));
    connect(&appearanceSettings,
            &viewmodel::AppearanceSettingsContract::appearanceChanged,
            this, &MainWindow::applyAppearance);
    connect(&appearanceSettings,
            &viewmodel::AppearanceSettingsContract::notificationRaised,
            this, &MainWindow::showNotification);
    applyAppearance();
    applyNavigationMode();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    applyNavigationMode();
}

void MainWindow::applyNavigationMode()
{
    const bool compact = width() < 1040;
    m_navigation->setFixedWidth(compact ? 64 : 208);
    m_brand->setText(compact ? QStringLiteral("S") : QStringLiteral("SmartMate"));
    m_taskNavigation->setText(compact ? QStringLiteral("✓") : tr("任务"));
    m_graphNavigation->setText(compact ? QStringLiteral("↗") : tr("依赖图"));
    m_settingsNavigation->setText(compact ? QStringLiteral("⚙") : tr("设置"));
    m_taskNavigation->setToolTip(compact ? tr("任务") : QString{});
    m_graphNavigation->setToolTip(compact ? tr("依赖图") : QString{});
    m_settingsNavigation->setToolTip(compact ? tr("设置") : QString{});
}

void MainWindow::applyAppearance()
{
    const auto &settings = m_appearanceSettings;
    const WidgetTheme theme = WidgetTheme::fromAccentIndex(
        settings.accentThemeIndex());
    setPalette(theme.palette());
    setFont(appearanceFont(m_baselineFont, settings));
    setStyleSheet(theme.styleSheet());
}

void MainWindow::showNotification(const common::UiNotification &notification)
{
    const QString text = notification.title.isEmpty()
        ? notification.message
        : QStringLiteral("%1：%2").arg(notification.title, notification.message);
    const WidgetTheme theme = WidgetTheme::fromAccentIndex(
        m_appearanceSettings.accentThemeIndex());
    QPalette notificationPalette = statusBar()->palette();
    switch (notification.severity) {
    case common::UiSeverity::Information:
        notificationPalette.setColor(QPalette::WindowText, theme.textBody);
        break;
    case common::UiSeverity::Warning:
        notificationPalette.setColor(QPalette::WindowText, theme.warning);
        break;
    case common::UiSeverity::Error:
        notificationPalette.setColor(QPalette::WindowText, theme.danger);
        break;
    }
    statusBar()->setPalette(notificationPalette);
    statusBar()->showMessage(text);
}

} // namespace smartmate::view::widgets
