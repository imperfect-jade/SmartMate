#include "AppBootstrapper.h"

#include "AppViewModel.h"
#include "persistence/SqliteTaskRepository.h"
#include "persistence/QSettingsAppearanceRepository.h"
#include "services/AppearanceSettingsService.h"
#include "services/TaskCategoryService.h"
#include "services/TaskService.h"
#include "view/widgets/MainWindowDependencies.h"

#include <QByteArray>
#include <QString>

#include <stdexcept>
#include <utility>

namespace smartmate::app {

AppBootstrapper::AppBootstrapper(QString databasePath)
    : m_taskRepository(
          std::make_unique<model::persistence::SqliteTaskRepository>(std::move(databasePath)))
    , m_taskService(
          std::make_unique<model::TaskService>(
              *m_taskRepository,
              *m_taskRepository,
              *m_taskRepository,
              *m_taskRepository,
              *m_taskRepository,
              *m_taskRepository))
    , m_taskCategoryService(
          std::make_unique<model::TaskCategoryService>(*m_taskRepository))
    , m_appearanceRepository(
          std::make_unique<model::persistence::QSettingsAppearanceRepository>())
    , m_appearanceService(
          std::make_unique<model::AppearanceSettingsService>(*m_appearanceRepository))
{
    // 在创建界面前验证数据源可读，避免用一个已经失效的 Service 启动 ViewModel。
    const model::TaskListResult initialTasks = m_taskService->listTasks();
    if (!initialTasks.ok()) {
        const QByteArray detail = initialTasks.detail.toUtf8();
        throw std::runtime_error(detail.isEmpty()
                                     ? "Unable to read the SmartMate task database"
                                     : detail.constData());
    }

    // 同时验证依赖端口，避免任务表可读但Schema v3关系表损坏时仍启动界面。
    const auto initialDependencies = m_taskService->listDependencies();
    if (!initialDependencies.ok()) {
        const QByteArray detail = initialDependencies.detail.toUtf8();
        throw std::runtime_error(detail.isEmpty()
                                     ? "Unable to read the SmartMate dependency database"
                                     : detail.constData());
    }

    const model::TaskCategoryListResult initialCategories =
        m_taskCategoryService->listCategories();
    if (!initialCategories.ok()) {
        const QByteArray detail = initialCategories.detail.toUtf8();
        throw std::runtime_error(detail.isEmpty()
                                     ? "Unable to read the SmartMate category database"
                                     : detail.constData());
    }

    m_appViewModel = std::make_unique<viewmodel::AppViewModel>(
        *m_taskService, *m_taskCategoryService, *m_appearanceService);
}

AppBootstrapper::~AppBootstrapper() = default;

view::widgets::MainWindowDependencies AppBootstrapper::widgetDependencies() noexcept
{
    return {*m_appViewModel->appearanceSettings(),
            *m_appViewModel->taskList(),
            *m_appViewModel->taskFocus(),
            *m_appViewModel->taskDetails(),
            *m_appViewModel->taskEditor(),
            *m_appViewModel->taskCategories(),
            *m_appViewModel->taskDependencies(),
            *m_appViewModel->taskGraph()};
}

} // namespace smartmate::app
