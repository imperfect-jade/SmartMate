#include "AppBootstrapper.h"

#include "AppViewModel.h"
#include "persistence/SqliteTaskRepository.h"
#include "persistence/QSettingsAppearanceRepository.h"
#include "services/AppearanceSettingsService.h"
#include "services/TaskCategoryService.h"
#include "services/TaskService.h"

#include <QByteArray>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QString>
#include <QVariant>
#include <QVariantMap>

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

void AppBootstrapper::configure(QQmlApplicationEngine &engine)
{
    // QML 只能观察这些对象，生命周期仍由 C++ 管理；否则 QML 引擎可能误删
    // AppViewModel 内部拥有的子 ViewModel。
    QQmlEngine::setObjectOwnership(m_appViewModel.get(), QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(m_appViewModel->taskList(), QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(m_appViewModel->taskEditor(), QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(m_appViewModel->taskDependencies(),
                                   QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(m_appViewModel->taskGraph(),
                                   QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(m_appViewModel->taskCategories(),
                                   QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(m_appViewModel->appearanceSettings(),
                                   QQmlEngine::CppOwnership);

    // 通过根组件的 required property 显式注入依赖，避免隐式全局状态。
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("appViewModel"),
                             QVariant::fromValue(m_appViewModel.get()));
    engine.setInitialProperties(initialProperties);
}

} // namespace smartmate::app
