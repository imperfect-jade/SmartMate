#include "AppBootstrapper.h"

#include "AppViewModel.h"
#include "DesktopPetSettingsViewModel.h"
#include "persistence/SqliteTaskRepository.h"
#include "persistence/QSettingsAppearanceRepository.h"
#include "persistence/QSettingsDesktopPetRepository.h"
#include "services/AppearanceSettingsService.h"
#include "services/DesktopPetSettingsService.h"
#include "services/FocusService.h"
#include "services/TaskCategoryService.h"
#include "services/TaskService.h"
#include "services/StatisticsService.h"
#include "view/widgets/MainWindowDependencies.h"

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimeZone>

#include <stdexcept>
#include <utility>

namespace smartmate::app {

AppBootstrapper::AppBootstrapper(QString databasePath)
    : m_taskRepository(
          std::make_unique<model::persistence::SqliteTaskRepository>(std::move(databasePath)))
    , m_focusService(
          std::make_unique<model::FocusService>(
              *m_taskRepository, *m_taskRepository,
              *m_taskRepository, *m_taskRepository))
    , m_taskService(
          std::make_unique<model::TaskService>(
              *m_taskRepository,
              *m_taskRepository,
              *m_taskRepository,
              *m_taskRepository,
              *m_taskRepository,
              *m_taskRepository,
              m_taskRepository.get()))
    , m_taskCategoryService(
          std::make_unique<model::TaskCategoryService>(*m_taskRepository))
    , m_statisticsService(
          std::make_unique<model::StatisticsService>(
              *m_taskRepository, *m_taskRepository, *m_taskRepository))
    , m_appearanceRepository(
          std::make_unique<model::persistence::QSettingsAppearanceRepository>())
    , m_appearanceService(
          std::make_unique<model::AppearanceSettingsService>(*m_appearanceRepository))
    , m_desktopPetRepository(
          std::make_unique<model::persistence::QSettingsDesktopPetRepository>())
    , m_desktopPetService(
          std::make_unique<model::DesktopPetSettingsService>(
              *m_desktopPetRepository))
{
    QObject::connect(m_focusService.get(),
                     &model::FocusService::backgroundFailureRaised,
                     m_focusService.get(),
                     [](const model::FocusError error, const QString &detail) {
                         qWarning("SmartMate focus checkpoint failed (%d): %s",
                                  static_cast<int>(error),
                                  qPrintable(detail));
                     });
    const model::FocusOperationResult focusInitialization =
        m_focusService->initialize();
    if (!focusInitialization.ok()) {
        const QByteArray detail = focusInitialization.detail.toUtf8();
        throw std::runtime_error(detail.isEmpty()
                                     ? "Unable to initialize SmartMate focus sessions"
                                     : detail.constData());
    }

    // 在创建 ViewModel 和界面前验证任务数据源，避免应用带着失效 Service 启动。
    const model::TaskListResult initialTasks = m_taskService->listTasks();
    if (!initialTasks.ok()) {
        const QByteArray detail = initialTasks.detail.toUtf8();
        throw std::runtime_error(detail.isEmpty()
                                     ? "Unable to read the SmartMate task database"
                                     : detail.constData());
    }

    // 任务表可读不代表关系表可读，因此依赖 Repository 端口必须单独探测。
    const auto initialDependencies = m_taskService->listDependencies();
    if (!initialDependencies.ok()) {
        const QByteArray detail = initialDependencies.detail.toUtf8();
        throw std::runtime_error(detail.isEmpty()
                                     ? "Unable to read the SmartMate dependency database"
                                     : detail.constData());
    }

    // 类别与任务共享 SQLite 数据库，但属于独立业务端口，也要在启动前确认可读。
    const model::TaskCategoryListResult initialCategories =
        m_taskCategoryService->listCategories();
    if (!initialCategories.ok()) {
        const QByteArray detail = initialCategories.detail.toUtf8();
        throw std::runtime_error(detail.isEmpty()
                                     ? "Unable to read the SmartMate category database"
                                     : detail.constData());
    }

    // StatisticsService 会同时触达事件、任务与依赖查询端口；启动探测确保
    // Schema v4 统计链路可读，且不对旧任务推测或回填历史事件。
    const model::StatisticsResult initialStatistics = m_statisticsService->snapshot(
        {model::StatisticsRange::Last7Days,
         QDateTime::currentDateTimeUtc(),
         QTimeZone::systemTimeZone()});
    if (!initialStatistics.ok()) {
        const QByteArray detail = initialStatistics.detail.toUtf8();
        throw std::runtime_error(detail.isEmpty()
                                     ? "Unable to read the SmartMate statistics database"
                                     : detail.constData());
    }

    // 所有数据源就绪后再创建展示层，保证子 ViewModel 构造时可以安全加载初始投影。
    m_appViewModel = std::make_unique<viewmodel::AppViewModel>(
        *m_taskService, *m_taskCategoryService,
        *m_statisticsService, *m_focusService, *m_appearanceService);
    m_desktopPetViewModel =
        std::make_unique<viewmodel::DesktopPetSettingsViewModel>(
            *m_desktopPetService);
}

AppBootstrapper::~AppBootstrapper() = default;

view::widgets::MainWindowDependencies AppBootstrapper::widgetDependencies() noexcept
{
    // View 只接收窄 Contract，不会获得具体 ViewModel、Service 或 Repository。
    return {*m_appViewModel->appearanceSettings(),
            *m_desktopPetViewModel,
            *m_appViewModel->taskList(),
            *m_appViewModel->taskFocus(),
            *m_appViewModel->taskDetails(),
            *m_appViewModel->taskEditor(),
            *m_appViewModel->taskCategories(),
            *m_appViewModel->taskDependencies(),
            *m_appViewModel->taskGraph(),
            *m_appViewModel->focus(),
            *m_appViewModel->statistics()};
}

void AppBootstrapper::prepareForShutdown() noexcept
{
    if (!m_focusService) return;
    const model::FocusOperationResult result = m_focusService->prepareForShutdown();
    if (!result.ok()) {
        qWarning("SmartMate focus shutdown preparation failed (%d): %s",
                 static_cast<int>(result.error),
                 qPrintable(result.detail));
    }
}

} // namespace smartmate::app
