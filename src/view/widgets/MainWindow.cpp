#include "view/widgets/MainWindow.h"

#include "view/widgets/settings/SettingsPage.h"
#include "view/widgets/task/TaskPage.h"
#include "view/widgets/graph/DependencyGraphPage.h"
#include "view/widgets/focus/FocusPage.h"
#include "view/widgets/statistics/StatisticsPage.h"
#include "view/widgets/pet/AttachedDesktopPetWindow.h"
#include "view/widgets/pet/FloatingDesktopPetWindow.h"
#include "view/widgets/theme/WidgetTheme.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMoveEvent>
#include <QPushButton>
#include <QScreen>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWindowStateChangeEvent>
#include <QTimer>

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

QString themedStyleSheet(const WidgetTheme &theme, const QFont &font)
{
    // QWidget 样式表会形成字体传播边界；把基础字体写入同一份 QSS，确保未单独
    // 指定标题字号的子控件与 Contract 当前字体档位保持一致。
    QString family = font.family();
    family.replace(u'\\', QStringLiteral("\\\\"));
    family.replace(u'"', QStringLiteral("\\\""));
    return QStringLiteral("QWidget { font-family: \"%1\"; font-size: %2pt; }\n%3")
        .arg(family,
             QString::number(font.pointSizeF(), 'f', 2),
             theme.styleSheet());
}

} // namespace

MainWindow::MainWindow(MainWindowDependencies dependencies, QWidget *parent)
    : MainWindow(dependencies.appearanceSettings,
                 &dependencies.desktopPetSettings,
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
                 new FocusPage{dependencies.focus},
                 new StatisticsPage{dependencies.statistics},
                 parent)
{
    m_attachedPet = std::make_unique<pet::AttachedDesktopPetWindow>(this);
    m_floatingPet = std::make_unique<pet::FloatingDesktopPetWindow>(
        dependencies.desktopPetSettings, dependencies.taskFocus,
        dependencies.taskList);
    connect(&dependencies.desktopPetSettings,
            &viewmodel::DesktopPetSettingsContract::enabledChanged,
            this, &MainWindow::syncDesktopPetVisibility);
    connect(&dependencies.desktopPetSettings,
            &viewmodel::DesktopPetSettingsContract::floatingPlacementChanged,
            this, [this] {
                if (m_floatingPet != nullptr && isMinimized()) {
                    m_floatingPet->restorePosition(mainWindowScreen());
                }
            });
    connect(&dependencies.desktopPetSettings,
            &viewmodel::DesktopPetSettingsContract::notificationRaised,
            this, &MainWindow::showNotification);
    connect(m_floatingPet.get(),
            &pet::FloatingDesktopPetWindow::openMainWindowRequested,
            this, &MainWindow::openFromDesktopPet);
    for (QScreen *screen : QGuiApplication::screens()) {
        connectDesktopPetScreenSignals(screen);
    }
    connect(qApp, &QGuiApplication::screenAdded, this,
            [this](QScreen *screen) {
                connectDesktopPetScreenSignals(screen);
                QTimer::singleShot(0, this,
                    &MainWindow::syncDesktopPetVisibility);
            });
    connect(qApp, &QGuiApplication::screenRemoved, this,
            [this] { QTimer::singleShot(0, this, &MainWindow::syncDesktopPetVisibility); });
    auto *taskPage = qobject_cast<TaskPage *>(m_pages->widget(0));
    connect(taskPage, &TaskPage::showDependencyGraphRequested, m_pages,
            [this] { m_pages->setCurrentIndex(1); });
    auto *focusPage = qobject_cast<FocusPage *>(m_pages->widget(2));
    connect(focusPage, &FocusPage::showTasksRequested, m_pages,
            [this] { m_pages->setCurrentIndex(0); });
    // 各窄 Contract 的一次性展示通知统一汇聚到窗口状态栏；
    // 通知只负责反馈，不承担 ViewModel 之间的数据同步。
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
    connect(&dependencies.statistics,
            &viewmodel::StatisticsContract::notificationRaised,
            this, &MainWindow::showNotification);
    connect(&dependencies.focus,
            &viewmodel::FocusContract::notificationRaised,
            this, &MainWindow::showNotification);
    syncDesktopPetVisibility();
}

MainWindow::MainWindow(viewmodel::AppearanceSettingsContract &appearanceSettings,
                       QWidget *parent)
    : MainWindow(appearanceSettings,
                 nullptr,
                 migrationPlaceholder(tr("任务"), tr("任务页面未注入测试依赖。")),
                 migrationPlaceholder(tr("依赖图"), tr("依赖图页面未注入测试依赖。")),
                 migrationPlaceholder(tr("专注"), tr("专注页面未注入测试依赖。")),
                 migrationPlaceholder(tr("统计"), tr("统计页面未注入测试依赖。")),
                 parent)
{
}

MainWindow::MainWindow(viewmodel::AppearanceSettingsContract &appearanceSettings,
                       viewmodel::DesktopPetSettingsContract *desktopPetSettings,
                       QWidget *taskPage, QWidget *graphPage,
                       QWidget *focusPage, QWidget *statisticsPage, QWidget *parent)
    : QMainWindow(parent)
    , m_appearanceSettings(appearanceSettings)
    , m_desktopPetSettings(desktopPetSettings)
    , m_baselineFont(QApplication::font())
    , m_pages(new QStackedWidget(this))
    , m_navigation(new QFrame(this))
    , m_brand(new QLabel(QStringLiteral("SmartMate"), m_navigation))
    , m_taskNavigation(nullptr)
    , m_graphNavigation(nullptr)
    , m_focusNavigation(nullptr)
    , m_statisticsNavigation(nullptr)
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
    m_graphNavigation->setAccessibleName(tr("依赖图"));
    m_focusNavigation = navigationButton(
        tr("专注"), QStringLiteral("focusNavigationButton"),
        *navigationGroup, *navigationLayout, 2);
    m_focusNavigation->setAccessibleName(tr("专注"));
    m_statisticsNavigation = navigationButton(
        tr("统计"), QStringLiteral("statisticsNavigationButton"),
        *navigationGroup, *navigationLayout, 3);
    m_statisticsNavigation->setAccessibleName(tr("统计"));
    m_settingsNavigation = navigationButton(tr("设置"), QStringLiteral("settingsNavigationButton"),
                                            *navigationGroup, *navigationLayout, 4);
    m_settingsNavigation->setAccessibleName(tr("设置"));
    navigationLayout->addStretch();

    rootLayout->addWidget(m_navigation);
    rootLayout->addWidget(m_pages, 1);

    m_pages->addWidget(taskPage);
    m_pages->addWidget(graphPage);
    m_pages->addWidget(focusPage);
    m_pages->addWidget(statisticsPage);
    if (desktopPetSettings != nullptr) {
        m_pages->addWidget(
            new SettingsPage{appearanceSettings, *desktopPetSettings, m_pages});
    } else {
        m_pages->addWidget(new SettingsPage{appearanceSettings, m_pages});
    }

    // 导航是纯 View 会话状态，只切换页面索引，不写入任何业务对象。
    connect(navigationGroup, &QButtonGroup::idClicked, m_pages,
            [this](const int index) { m_pages->setCurrentIndex(index); });
    m_taskNavigation->setChecked(true);
    m_pages->setCurrentIndex(0);

    statusBar()->setObjectName(QStringLiteral("notificationStatusBar"));
    // 先建立通知连接再执行初始同步，后续 Contract 变化使用同一路径刷新主题。
    connect(&appearanceSettings,
            &viewmodel::AppearanceSettingsContract::appearanceChanged,
            this, &MainWindow::applyAppearance);
    connect(&appearanceSettings,
            &viewmodel::AppearanceSettingsContract::notificationRaised,
            this, &MainWindow::showNotification);
    applyAppearance();
    applyNavigationMode();
}

MainWindow::~MainWindow() = default;

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    applyNavigationMode();
    updateAttachedPetAnchor();
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    updateAttachedPetAnchor();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    QTimer::singleShot(0, this, &MainWindow::syncDesktopPetVisibility);
}

void MainWindow::hideEvent(QHideEvent *event)
{
    QMainWindow::hideEvent(event);
    QTimer::singleShot(0, this, &MainWindow::syncDesktopPetVisibility);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_closing = true;
    if (m_attachedPet != nullptr) {
        m_attachedPet->hide();
    }
    if (m_floatingPet != nullptr) {
        m_floatingPet->hide();
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        auto *stateEvent = static_cast<QWindowStateChangeEvent *>(event);
        if (!stateEvent->oldState().testFlag(Qt::WindowMinimized)) {
            m_restoreWindowState = stateEvent->oldState()
                & ~Qt::WindowMinimized;
        }
        QTimer::singleShot(0, this, &MainWindow::syncDesktopPetVisibility);
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::applyNavigationMode()
{
    const bool compact = width() < 1040;
    m_navigation->setFixedWidth(compact ? 64 : 208);
    m_brand->setText(compact ? QStringLiteral("S") : QStringLiteral("SmartMate"));
    m_taskNavigation->setText(compact ? QStringLiteral("✓") : tr("任务"));
    m_graphNavigation->setText(compact ? QStringLiteral("↗") : tr("依赖图"));
    m_focusNavigation->setText(compact ? QStringLiteral("◎") : tr("专注"));
    m_statisticsNavigation->setText(compact ? QStringLiteral("▥") : tr("统计"));
    m_settingsNavigation->setText(compact ? QStringLiteral("⚙") : tr("设置"));
    m_taskNavigation->setToolTip(compact ? tr("任务") : QString{});
    m_graphNavigation->setToolTip(compact ? tr("依赖图") : QString{});
    m_focusNavigation->setToolTip(compact ? tr("专注") : QString{});
    m_statisticsNavigation->setToolTip(compact ? tr("统计") : QString{});
    m_settingsNavigation->setToolTip(compact ? tr("设置") : QString{});
}

void MainWindow::applyAppearance()
{
    const auto &settings = m_appearanceSettings;
    const WidgetTheme theme = WidgetTheme::fromAccentIndex(
        settings.accentThemeIndex());
    const QFont targetFont = appearanceFont(m_baselineFont, settings);
    // 旧 QSS 可能继续覆盖 Base 等 Palette role；先完整安装新主题，再传播字体，
    // 让自绘卡片与普通控件读取同一套颜色。解除和安装 QSS 都可能重置子控件的
    // 继承字体，因此最后先传播基准字体，再传播目标字体；即使字号档位未变，
    // 也不能因 QMainWindow 的 QFont 幂等优化而让子控件停留在默认字号。
    setStyleSheet({});
    setPalette(theme.palette());
    setStyleSheet(themedStyleSheet(theme, targetFont));
    setFont(m_baselineFont);
    setFont(targetFont);
}

void MainWindow::syncDesktopPetVisibility()
{
    if (m_desktopPetSettings == nullptr || m_attachedPet == nullptr
        || m_floatingPet == nullptr) {
        return;
    }
    const bool unavailable = m_closing || !m_desktopPetSettings->enabled()
        || ((!isVisible()) && !isMinimized())
        || !m_attachedPet->assetReady() || !m_floatingPet->assetReady();
    if (unavailable) {
        m_attachedPet->hide();
        m_floatingPet->hide();
        return;
    }

    if (isMinimized()) {
        m_attachedPet->hide();
        m_floatingPet->restorePosition(mainWindowScreen());
        m_floatingPet->show();
        m_floatingPet->raise();
        return;
    }

    m_floatingPet->hide();
    if (isMaximized() || isFullScreen()) {
        m_attachedPet->hide();
        return;
    }
    updateAttachedPetAnchor();
    m_attachedPet->show();
    m_attachedPet->raise();
}

void MainWindow::updateAttachedPetAnchor()
{
    if (!isMinimized()) {
        if (QScreen *screen = QGuiApplication::screenAt(frameGeometry().center())) {
            m_lastMainScreenName = screen->name();
        }
    }
    if (m_attachedPet != nullptr) {
        m_attachedPet->updateAnchor(frameGeometry(), mainWindowScreen());
    }
}

void MainWindow::openFromDesktopPet()
{
    const Qt::WindowStates target = m_restoreWindowState
        & ~Qt::WindowMinimized;
    if (target.testFlag(Qt::WindowFullScreen)) {
        showFullScreen();
    } else if (target.testFlag(Qt::WindowMaximized)) {
        showMaximized();
    } else {
        showNormal();
    }
    raise();
    activateWindow();
    QTimer::singleShot(0, this, &MainWindow::syncDesktopPetVisibility);
}

QScreen *MainWindow::mainWindowScreen() const
{
    QScreen *screen = QGuiApplication::screenAt(frameGeometry().center());
    if (screen != nullptr) {
        return screen;
    }
    for (QScreen *candidate : QGuiApplication::screens()) {
        if (candidate->name() == m_lastMainScreenName) {
            return candidate;
        }
    }
    return QGuiApplication::primaryScreen();
}

void MainWindow::connectDesktopPetScreenSignals(QScreen *screen)
{
    if (screen == nullptr) {
        return;
    }
    const auto resync = [this] {
        QTimer::singleShot(0, this, &MainWindow::syncDesktopPetVisibility);
    };
    connect(screen, &QScreen::geometryChanged, this, resync);
    connect(screen, &QScreen::availableGeometryChanged, this, resync);
    connect(screen, &QScreen::logicalDotsPerInchChanged, this,
            [resync](qreal) { resync(); });
}

void MainWindow::showNotification(const common::UiNotification &notification)
{
    // View 决定通知的颜色和呈现位置；ViewModel 只提供严重级别与文案。
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
