#include "AppViewModel.h"
#include "persistence/SqliteTaskRepository.h"
#include "services/TaskService.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

#include <memory>

namespace {

/// 为 QML 测试注入完整但不落盘的 MVVM 纵向链路。
class QuickTestSetup final : public QObject {
    Q_OBJECT

public slots:
    void applicationAvailable()
    {
        m_repository = std::make_unique<smartmate::model::persistence::SqliteTaskRepository>(
            QStringLiteral(":memory:"));
        m_service = std::make_unique<smartmate::model::TaskService>(
            *m_repository, *m_repository, *m_repository);
        m_appViewModel =
            std::make_unique<smartmate::viewmodel::AppViewModel>(*m_service);
    }

    void qmlEngineAvailable(QQmlEngine *engine)
    {
        QQmlEngine::setObjectOwnership(m_appViewModel.get(), QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(m_appViewModel->taskList(), QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(m_appViewModel->taskEditor(), QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(m_appViewModel->taskDependencies(),
                                       QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(m_appViewModel->taskGraph(),
                                       QQmlEngine::CppOwnership);
        engine->rootContext()->setContextProperty(
            QStringLiteral("testAppViewModel"), m_appViewModel.get());
    }

    void cleanupTestCase()
    {
        // 按依赖反序释放，确保 Qt SQL 连接在应用对象仍存在时关闭。
        m_appViewModel.reset();
        m_service.reset();
        m_repository.reset();
    }

private:
    std::unique_ptr<smartmate::model::persistence::SqliteTaskRepository> m_repository;
    std::unique_ptr<smartmate::model::TaskService> m_service;
    std::unique_ptr<smartmate::viewmodel::AppViewModel> m_appViewModel;
};

} // namespace

// 使用独立QuickTest进程加载测试目录，避免测试代码进入生产QML模块。
QUICK_TEST_MAIN_WITH_SETUP(smartmate_qml_view, QuickTestSetup)

#include "quicktest_main.moc"
