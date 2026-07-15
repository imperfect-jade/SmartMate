#include "TaskProjectionSources.h"

#include "fakes/FakeTaskBatchTransitionRepository.h"
#include "fakes/FakeTaskCategoryRepository.h"
#include "fakes/FakeTaskCreationRepository.h"
#include "fakes/FakeTaskDeletionRepository.h"
#include "fakes/FakeTaskDependencyRepository.h"
#include "fakes/FakeTaskRepository.h"
#include "services/TaskCategoryService.h"
#include "services/TaskService.h"

#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>

using namespace smartmate;

namespace {

model::Task makeTask(const QString &id, const QString &title,
                     std::optional<model::TaskCategoryId> categoryId = std::nullopt)
{
    const QDateTime now = QDateTime::fromMSecsSinceEpoch(
        1'900'000'000'000LL, QTimeZone::UTC);
    return {QUuid{id}, title, {}, model::TaskPriority::Normal,
            model::TaskStatus::Todo, std::nullopt, std::nullopt, std::nullopt,
            now, now, categoryId};
}

model::TaskCategory makeCategory(const QString &id, const QString &name)
{
    const QDateTime now = QDateTime::fromMSecsSinceEpoch(
        1'900'000'000'000LL, QTimeZone::UTC);
    return {QUuid{id}, name, model::TaskCategoryColor::Blue, now, now};
}

struct Services final {
    tests::FakeTaskRepository tasks;
    tests::FakeTaskDependencyRepository dependencies;
    tests::FakeTaskCreationRepository creation{tasks, dependencies};
    tests::FakeTaskBatchTransitionRepository batch{tasks};
    tests::FakeTaskDeletionRepository deletion{tasks, dependencies};
    tests::FakeTaskCategoryRepository categories;
    model::TaskService taskService{tasks, dependencies, creation, batch,
                                   deletion, categories};
    model::TaskCategoryService categoryService{categories};

    Services(QList<model::Task> initialTasks,
             QList<model::TaskCategory> initialCategories)
        : tasks(std::move(initialTasks))
        , categories(std::move(initialCategories))
    {
    }
};

} // namespace

class TaskProjectionSourcesTest final : public QObject {
    Q_OBJECT

private slots:
    void planSourcePublishesOnlyChangedSnapshots();
    void planSourceRetainsSnapshotAcrossFailuresAndRecovers();
    void categorySourceSupportsChangesFailuresAndEmptyMode();
    void serviceInvalidationsRouteToTheExpectedSource();
};

void TaskProjectionSourcesTest::planSourcePublishesOnlyChangedSnapshots()
{
    Services services{{makeTask(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
                                QStringLiteral("first"))}, {}};
    viewmodel::TaskPlanProjectionSource source{
        services.taskService, &services.categoryService};
    QCOMPARE(source.projection().tasks.size(), 1);
    QCOMPARE(source.lastError(), model::TaskError::None);

    QSignalSpy changed{&source,
                       &viewmodel::TaskPlanProjectionSource::projectionChanged};
    QSignalSpy succeeded{&source,
                         &viewmodel::TaskPlanProjectionSource::refreshSucceeded};
    const int taskReads = services.tasks.findAllCount();
    const int dependencyReads = services.dependencies.findAllCount();
    source.refresh();
    QCOMPARE(services.tasks.findAllCount(), taskReads + 1);
    QCOMPARE(services.dependencies.findAllCount(), dependencyReads + 1);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(succeeded.count(), 1);
}

void TaskProjectionSourcesTest::planSourceRetainsSnapshotAcrossFailuresAndRecovers()
{
    Services services{{makeTask(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
                                QStringLiteral("first"))}, {}};
    viewmodel::TaskPlanProjectionSource source{services.taskService};
    const auto original = source.projection();
    QSignalSpy failed{&source,
                      &viewmodel::TaskPlanProjectionSource::refreshFailed};
    QSignalSpy succeeded{&source,
                         &viewmodel::TaskPlanProjectionSource::refreshSucceeded};

    services.tasks.setReadFailure(true);
    source.refresh();
    source.refresh();
    QCOMPARE(failed.count(), 2);
    QCOMPARE(source.lastError(), model::TaskError::PersistenceFailure);
    QCOMPARE(source.projection(), original);

    services.tasks.setReadFailure(false);
    source.refresh();
    QCOMPARE(succeeded.count(), 1);
    QCOMPARE(source.lastError(), model::TaskError::None);
    QCOMPARE(source.projection(), original);
}

void TaskProjectionSourcesTest::categorySourceSupportsChangesFailuresAndEmptyMode()
{
    const model::TaskCategory category = makeCategory(
        QStringLiteral("{aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa}"),
        QStringLiteral("study"));
    Services services{{}, {category}};
    viewmodel::TaskCategoryProjectionSource source{&services.categoryService};
    QCOMPARE(source.categories(), QList<model::TaskCategory>{category});

    QSignalSpy changed{&source,
                       &viewmodel::TaskCategoryProjectionSource::categoriesChanged};
    QSignalSpy succeeded{&source,
                         &viewmodel::TaskCategoryProjectionSource::refreshSucceeded};
    QSignalSpy failed{&source,
                      &viewmodel::TaskCategoryProjectionSource::refreshFailed};
    source.refresh();
    QCOMPARE(changed.count(), 0);
    QCOMPARE(succeeded.count(), 1);

    services.categories.setReadFailure(true);
    source.refresh();
    QCOMPARE(failed.count(), 1);
    QCOMPARE(source.categories(), QList<model::TaskCategory>{category});
    QCOMPARE(source.lastError(), model::TaskCategoryError::PersistenceFailure);

    viewmodel::TaskCategoryProjectionSource emptySource;
    QSignalSpy emptySucceeded{
        &emptySource, &viewmodel::TaskCategoryProjectionSource::refreshSucceeded};
    emptySource.refresh();
    QCOMPARE(emptySucceeded.count(), 1);
    QVERIFY(emptySource.categories().isEmpty());
    QCOMPARE(emptySource.lastError(), model::TaskCategoryError::None);
}

void TaskProjectionSourcesTest::serviceInvalidationsRouteToTheExpectedSource()
{
    const model::TaskCategory category = makeCategory(
        QStringLiteral("{aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa}"),
        QStringLiteral("study"));
    Services services{{makeTask(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
                                QStringLiteral("first"), category.id)},
                      {category}};
    viewmodel::TaskPlanProjectionSource plan{
        services.taskService, &services.categoryService};
    viewmodel::TaskCategoryProjectionSource categories{&services.categoryService};
    QSignalSpy planChanged{
        &plan, &viewmodel::TaskPlanProjectionSource::projectionChanged};
    QSignalSpy categoriesChanged{
        &categories,
        &viewmodel::TaskCategoryProjectionSource::categoriesChanged};
    QSignalSpy assignmentsChanged{
        &categories,
        &viewmodel::TaskCategoryProjectionSource::taskCategoryAssignmentsChanged};

    model::TaskDraft draft;
    draft.title = QStringLiteral("second");
    QVERIFY(services.taskService.createTask(draft).ok());
    QCOMPARE(planChanged.count(), 1);
    QCOMPARE(plan.projection().tasks.size(), 2);

    const int taskReadsBeforeDelete = services.tasks.findAllCount();
    const int categoryReadsBeforeDelete = services.categories.findAllCount();
    services.categories.setUnassignedTaskCount(1);
    QVERIFY(services.categoryService.deleteCategory(category.id).ok());
    QCOMPARE(categoriesChanged.count(), 1);
    QCOMPARE(assignmentsChanged.count(), 1);
    QCOMPARE(services.tasks.findAllCount(), taskReadsBeforeDelete + 1);
    QCOMPARE(services.categories.findAllCount(),
             categoryReadsBeforeDelete + 1);
    QVERIFY(categories.categories().isEmpty());
    // Fake Repository 不修改任务归属，因此计划读取发生但内容幂等，不发布变化。
    QCOMPARE(planChanged.count(), 1);
}

QTEST_GUILESS_MAIN(TaskProjectionSourcesTest)

#include "tst_TaskProjectionSources.moc"
