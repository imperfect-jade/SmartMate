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

    // 所有数据源就绪后再创建展示层，保证子 ViewModel 构造时可以安全加载初始投影。
    m_appViewModel = std::make_unique<viewmodel::AppViewModel>(
        *m_taskService, *m_taskCategoryService, *m_appearanceService);
}

AppBootstrapper::~AppBootstrapper() = default;

view::widgets::MainWindowDependencies AppBootstrapper::widgetDependencies() noexcept
{
    // View 只接收窄 Contract，不会获得具体 ViewModel、Service 或 Repository。
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
