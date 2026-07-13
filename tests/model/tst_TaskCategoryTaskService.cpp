#include "fakes/FakeTaskBatchTransitionRepository.h"
#include "fakes/FakeTaskCategoryRepository.h"
#include "fakes/FakeTaskCreationRepository.h"
#include "fakes/FakeTaskDeletionRepository.h"
#include "fakes/FakeTaskDependencyRepository.h"
#include "fakes/FakeTaskRepository.h"

#include "domain/TaskGraph.h"
#include "services/TaskService.h"

#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>

#include <algorithm>
#include <optional>
#include <utility>

using namespace smartmate::model;
using namespace smartmate::tests;

namespace {

[[nodiscard]] QDateTime timestamp()
{
    return QDateTime::fromMSecsSinceEpoch(1700000000000, QTimeZone::UTC);
}

[[nodiscard]] TaskCategory category(const QString &name)
{
    return {QUuid::createUuid(), name, TaskCategoryColor::Blue,
            timestamp(), timestamp()};
}

[[nodiscard]] Task task(const QString &title,
                         std::optional<TaskCategoryId> categoryId = std::nullopt,
                         const TaskStatus status = TaskStatus::Todo)
{
    return {QUuid::createUuid(), title, QStringLiteral("description"),
            TaskPriority::Normal, status, std::nullopt,
            std::nullopt, std::nullopt, timestamp(), timestamp(), categoryId};
}

[[nodiscard]] const Task *taskById(const QList<Task> &tasks,
                                   const TaskId &id)
{
    const auto iterator = std::find_if(
        tasks.cbegin(), tasks.cend(), [&id](const Task &candidate) {
            return candidate.id() == id;
        });
    return iterator == tasks.cend() ? nullptr : &*iterator;
}

struct Fixture final {
    Fixture(QList<Task> tasks,
            QList<TaskDependency> dependencies,
            QList<TaskCategory> categories)
        : repository(std::move(tasks))
        , dependencyRepository(std::move(dependencies))
        , creationRepository(repository, dependencyRepository)
        , batchRepository(repository)
        , deletionRepository(repository, dependencyRepository)
        , categoryRepository(std::move(categories))
        , service(repository, dependencyRepository, creationRepository,
                  batchRepository, deletionRepository, categoryRepository)
    {
    }

    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository;
    FakeTaskBatchTransitionRepository batchRepository;
    FakeTaskDeletionRepository deletionRepository;
    FakeTaskCategoryRepository categoryRepository;
    TaskService service;
};

[[nodiscard]] const TaskGraphNode *nodeById(const TaskGraphSnapshot &snapshot,
                                            const TaskId &id)
{
    const auto iterator = std::find_if(
        snapshot.nodes.cbegin(), snapshot.nodes.cend(), [&id](const auto &node) {
            return node.task.id() == id;
        });
    return iterator == snapshot.nodes.cend() ? nullptr : &*iterator;
}

} // namespace

/// 验证TaskService对类别端点和分类依赖图裁剪保持最终业务权威。
class TaskCategoryTaskServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void validatesCategoryOnCreateAndUpdate();
    void preservesCategoryAcrossStateCommands();
    void preservesCategoryAcrossBatchStateCommands();
    void scopesGraphToCoreAndDirectExternalContext();
};

void TaskCategoryTaskServiceTest::validatesCategoryOnCreateAndUpdate()
{
    const TaskCategory study = category(QStringLiteral("学习"));
    Fixture fixture{{}, {}, {study}};
    TaskDraft valid;
    valid.title = QStringLiteral("分类任务");
    valid.categoryId = study.id;

    const auto created = fixture.service.createTask(valid);
    QVERIFY(created.ok());
    QCOMPARE(created.value->categoryId(),
             std::optional<TaskCategoryId>{study.id});

    TaskDraft invalid = valid;
    invalid.categoryId = QUuid::createUuid();
    QCOMPARE(fixture.service.createTask(invalid).error,
             TaskError::TaskCategoryNotFound);
    QCOMPARE(fixture.repository.insertCount(), 1);
    QCOMPARE(fixture.service.updateTask(created.value->id(), invalid).error,
             TaskError::TaskCategoryNotFound);
    QCOMPARE(fixture.repository.updateCount(), 0);

    valid.categoryId.reset();
    const auto cleared = fixture.service.updateTask(created.value->id(), valid);
    QVERIFY(cleared.ok());
    QVERIFY(!cleared.value->categoryId().has_value());
}

void TaskCategoryTaskServiceTest::preservesCategoryAcrossStateCommands()
{
    const TaskCategory work = category(QStringLiteral("工作"));
    const Task stored = task(QStringLiteral("保持类别"), work.id);
    Fixture fixture{{stored}, {}, {work}};

    // 完整走过状态机的两条分支，防止任一详情重建路径遗漏稳定类别ID。
    const auto started = fixture.service.startTask(stored.id());
    QVERIFY(started.ok());
    QCOMPARE(started.value->categoryId(), std::optional<TaskCategoryId>{work.id});

    const auto completed = fixture.service.completeTask(stored.id());
    QVERIFY(completed.ok());
    QCOMPARE(completed.value->categoryId(), std::optional<TaskCategoryId>{work.id});

    const auto archivedDone = fixture.service.archiveTask(stored.id());
    QVERIFY(archivedDone.ok());
    QCOMPARE(archivedDone.value->categoryId(), std::optional<TaskCategoryId>{work.id});

    const auto restoredDone = fixture.service.restoreTask(stored.id());
    QVERIFY(restoredDone.ok());
    QCOMPARE(restoredDone.value->status(), TaskStatus::Done);
    QCOMPARE(restoredDone.value->categoryId(), std::optional<TaskCategoryId>{work.id});

    const auto redoneDone = fixture.service.redoTask(stored.id());
    QVERIFY(redoneDone.ok());
    QCOMPARE(redoneDone.value->categoryId(), std::optional<TaskCategoryId>{work.id});

    QVERIFY(fixture.service.startTask(stored.id()).ok());
    const auto cancelled = fixture.service.cancelTask(stored.id());
    QVERIFY(cancelled.ok());
    QCOMPARE(cancelled.value->categoryId(), std::optional<TaskCategoryId>{work.id});

    const auto archivedCancelled = fixture.service.archiveTask(stored.id());
    QVERIFY(archivedCancelled.ok());
    QCOMPARE(archivedCancelled.value->categoryId(),
             std::optional<TaskCategoryId>{work.id});

    const auto restoredCancelled = fixture.service.restoreTask(stored.id());
    QVERIFY(restoredCancelled.ok());
    QCOMPARE(restoredCancelled.value->status(), TaskStatus::Cancelled);
    QCOMPARE(restoredCancelled.value->categoryId(),
             std::optional<TaskCategoryId>{work.id});

    const auto redoneCancelled = fixture.service.redoTask(stored.id());
    QVERIFY(redoneCancelled.ok());
    QCOMPARE(redoneCancelled.value->status(), TaskStatus::Todo);
    QCOMPARE(redoneCancelled.value->categoryId(),
             std::optional<TaskCategoryId>{work.id});
}

void TaskCategoryTaskServiceTest::preservesCategoryAcrossBatchStateCommands()
{
    const TaskCategory work = category(QStringLiteral("工作"));
    const Task done = task(QStringLiteral("批量完成"), work.id, TaskStatus::Done);
    const Task cancelled = task(
        QStringLiteral("批量取消"), work.id, TaskStatus::Cancelled);
    Fixture fixture{{done, cancelled}, {}, {work}};
    QSignalSpy changedSpy{&fixture.service, &TaskService::tasksChanged};

    const auto archived = fixture.service.archiveTasks({cancelled.id(), done.id()});
    QVERIFY(archived.ok());
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(archived.value->tasks.size(), 2);
    for (const TaskId &id : {done.id(), cancelled.id()}) {
        const Task *resultTask = taskById(archived.value->tasks, id);
        QVERIFY(resultTask != nullptr);
        QCOMPARE(resultTask->status(), TaskStatus::Archived);
        QCOMPARE(resultTask->categoryId(), std::optional<TaskCategoryId>{work.id});

        const auto persisted = fixture.service.findTask(id);
        QVERIFY(persisted.ok());
        QCOMPARE(persisted.value->categoryId(),
                 std::optional<TaskCategoryId>{work.id});
    }

    const auto restored = fixture.service.restoreTasks({done.id(), cancelled.id()});
    QVERIFY(restored.ok());
    QCOMPARE(changedSpy.count(), 2);
    const Task *restoredDone = taskById(restored.value->tasks, done.id());
    const Task *restoredCancelled = taskById(restored.value->tasks, cancelled.id());
    QVERIFY(restoredDone != nullptr);
    QVERIFY(restoredCancelled != nullptr);
    QCOMPARE(restoredDone->status(), TaskStatus::Done);
    QCOMPARE(restoredCancelled->status(), TaskStatus::Cancelled);
    QCOMPARE(restoredDone->categoryId(), std::optional<TaskCategoryId>{work.id});
    QCOMPARE(restoredCancelled->categoryId(),
             std::optional<TaskCategoryId>{work.id});
}

void TaskCategoryTaskServiceTest::scopesGraphToCoreAndDirectExternalContext()
{
    const TaskCategory study = category(QStringLiteral("学习"));
    const TaskCategory work = category(QStringLiteral("工作"));
    const TaskCategory travel = category(QStringLiteral("旅游"));
    const Task a = task(QStringLiteral("A"), study.id);
    const Task b = task(QStringLiteral("B"), work.id);
    const Task c = task(QStringLiteral("C"), travel.id);
    const Task d = task(QStringLiteral("D"), work.id);
    const Task e = task(QStringLiteral("E"), travel.id);
    const Task uncategorized = task(QStringLiteral("U"));
    Fixture fixture{{a, b, c, d, e, uncategorized},
                    {{a.id(), b.id()}, {b.id(), c.id()},
                     {c.id(), d.id()}, {c.id(), e.id()}},
                    {study, work, travel}};

    const auto scoped = fixture.service.taskGraphSnapshot(
        {TaskGraphCategoryScope::SpecificCategory, work.id});
    QVERIFY(scoped.ok());
    QCOMPARE(scoped.value->nodes.size(), 4);
    QCOMPARE(scoped.value->edges.size(), 3);
    QVERIFY(nodeById(*scoped.value, a.id()) != nullptr);
    QVERIFY(nodeById(*scoped.value, b.id())->coreNode);
    QVERIFY(!nodeById(*scoped.value, c.id())->coreNode);
    QVERIFY(nodeById(*scoped.value, d.id())->coreNode);
    QVERIFY(nodeById(*scoped.value, e.id()) == nullptr);
    QVERIFY(!scoped.value->edges.contains({{c.id(), e.id()},
                                           TaskDependencyResolution::Pending}));

    const auto unclassified = fixture.service.taskGraphSnapshot(
        {TaskGraphCategoryScope::Uncategorized, std::nullopt});
    QVERIFY(unclassified.ok());
    QCOMPARE(unclassified.value->nodes.size(), 1);
    QCOMPARE(unclassified.value->nodes.constFirst().task.id(), uncategorized.id());
    QVERIFY(unclassified.value->nodes.constFirst().coreNode);

    QCOMPARE(fixture.service.taskGraphSnapshot(
                 {TaskGraphCategoryScope::SpecificCategory, QUuid::createUuid()}).error,
             TaskError::TaskCategoryNotFound);
}

QTEST_GUILESS_MAIN(TaskCategoryTaskServiceTest)

#include "tst_TaskCategoryTaskService.moc"
