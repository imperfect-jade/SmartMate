#include "AppViewModel.h"
#include "domain/TaskCreationRequest.h"
#include "persistence/SqliteTaskRepository.h"
#include "services/TaskCategoryService.h"
#include "services/TaskService.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

#include <memory>

namespace {

/// 为图页测试准备独立内存数据库，避免图数据污染列表与选择器 QuickTest。
class GraphQuickTestSetup final : public QObject {
    Q_OBJECT

public slots:
    void applicationAvailable()
    {
        using namespace smartmate::model;

        m_repository = std::make_unique<persistence::SqliteTaskRepository>(
            QStringLiteral(":memory:"));
        m_service = std::make_unique<TaskService>(
            *m_repository, *m_repository, *m_repository, *m_repository,
            *m_repository,
            *m_repository);
        m_categoryService = std::make_unique<TaskCategoryService>(*m_repository);

        const auto studyCategory = m_categoryService->createCategory(
            {QStringLiteral("学习"), TaskCategoryColor::Blue});
        const auto workCategory = m_categoryService->createCategory(
            {QStringLiteral("工作"), TaskCategoryColor::Orange});
        if (!studyCategory.ok() || !workCategory.ok()) {
            qFatal("Unable to create graph category fixtures");
        }

        TaskDraft predecessorDraft;
        predecessorDraft.title = QStringLiteral("需求分析");
        predecessorDraft.categoryId = studyCategory.value->id;
        const TaskResult predecessor = m_service->createTask(predecessorDraft);
        if (!predecessor.ok()) {
            qFatal("Unable to create graph predecessor fixture");
        }

        TaskDraft successorDraft;
        successorDraft.title = QStringLiteral("实现任务模块");
        successorDraft.categoryId = workCategory.value->id;
        const TaskResult successor = m_service->createTask(
            TaskCreationRequest{successorDraft, {predecessor.value->id()}});
        if (!successor.ok()) {
            qFatal("Unable to create graph successor fixture");
        }

        m_predecessorId = predecessor.value->id().toString(QUuid::WithoutBraces);
        m_successorId = successor.value->id().toString(QUuid::WithoutBraces);
        m_studyCategoryId = studyCategory.value->id.toString(QUuid::WithoutBraces);
        m_workCategoryId = workCategory.value->id.toString(QUuid::WithoutBraces);
        m_appViewModel = std::make_unique<smartmate::viewmodel::AppViewModel>(
            *m_service, *m_categoryService);
    }

    void qmlEngineAvailable(QQmlEngine *engine)
    {
        QQmlEngine::setObjectOwnership(m_appViewModel.get(),
                                       QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(m_appViewModel->taskList(),
                                       QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(m_appViewModel->taskEditor(),
                                       QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(m_appViewModel->taskDependencies(),
                                       QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(m_appViewModel->taskGraph(),
                                       QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(m_appViewModel->taskCategories(),
                                       QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(m_appViewModel->appearanceSettings(),
                                       QQmlEngine::CppOwnership);

        engine->rootContext()->setContextProperty(
            QStringLiteral("graphTestAppViewModel"), m_appViewModel.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("graphPredecessorId"), m_predecessorId);
        engine->rootContext()->setContextProperty(
            QStringLiteral("graphSuccessorId"), m_successorId);
        engine->rootContext()->setContextProperty(
            QStringLiteral("graphStudyCategoryId"), m_studyCategoryId);
        engine->rootContext()->setContextProperty(
            QStringLiteral("graphWorkCategoryId"), m_workCategoryId);
    }

    void cleanupTestCase()
    {
        m_appViewModel.reset();
        m_categoryService.reset();
        m_service.reset();
        m_repository.reset();
    }

private:
    std::unique_ptr<smartmate::model::persistence::SqliteTaskRepository>
        m_repository;
    std::unique_ptr<smartmate::model::TaskService> m_service;
    std::unique_ptr<smartmate::model::TaskCategoryService> m_categoryService;
    std::unique_ptr<smartmate::viewmodel::AppViewModel> m_appViewModel;
    QString m_predecessorId;
    QString m_successorId;
    QString m_studyCategoryId;
    QString m_workCategoryId;
};

} // namespace

QUICK_TEST_MAIN_WITH_SETUP(smartmate_qml_graph, GraphQuickTestSetup)

#include "quicktest_main.moc"
