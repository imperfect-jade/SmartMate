#include "fakes/FakeTaskCreationRepository.h"
#include "fakes/FakeTaskBatchTransitionRepository.h"
#include "fakes/FakeTaskDependencyRepository.h"
#include "fakes/FakeTaskDeletionRepository.h"
#include "fakes/FakeTaskRepository.h"
#include "fakes/FakeTaskCategoryRepository.h"

#include "domain/Task.h"
#include "services/TaskService.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>

#include <algorithm>
#include <optional>
#include <utility>

using smartmate::model::Task;
using smartmate::model::TaskDependency;
using smartmate::model::TaskDraft;
using smartmate::model::TaskError;
using smartmate::model::TaskId;
using smartmate::model::TaskPriority;
using smartmate::model::TaskService;
using smartmate::model::TaskStatus;
using smartmate::tests::FakeTaskCreationRepository;
using smartmate::tests::FakeTaskBatchTransitionRepository;
using smartmate::tests::FakeTaskDependencyRepository;
using smartmate::tests::FakeTaskDeletionRepository;
using smartmate::tests::FakeTaskRepository;
using smartmate::tests::FakeTaskCategoryRepository;

namespace {

[[nodiscard]] Task storedTask(
    const TaskStatus status,
    const QString &title = QStringLiteral("状态转换任务"),
    const std::optional<TaskStatus> statusBeforeArchive = std::nullopt)
{
    const QDateTime now = QDateTime::fromMSecsSinceEpoch(
        1'700'000'000'000, QTimeZone::UTC);
    return {QUuid::createUuid(),
            title,
            QStringLiteral("普通字段与状态命令相互隔离"),
            TaskPriority::Normal,
            status,
            statusBeforeArchive,
            std::nullopt,
            30,
            now,
            now};
}

[[nodiscard]] TaskDraft validDraft()
{
    TaskDraft draft;
    draft.title = QStringLiteral("新建任务");
    draft.description = QStringLiteral("创建状态必须固定为待办");
    draft.priority = TaskPriority::High;
    return draft;
}

struct ServiceFixture final {
    explicit ServiceFixture(QList<Task> tasks = {},
                            QList<TaskDependency> dependencies = {})
        : repository(std::move(tasks))
        , dependencyRepository(std::move(dependencies))
        , creationRepository(repository, dependencyRepository)
        , batchTransitionRepository(repository)
        , deletionRepository(repository, dependencyRepository)
        , categoryRepository()
        , service(repository, dependencyRepository, creationRepository,
                  batchTransitionRepository, deletionRepository,
                  categoryRepository)
        , taskChanged(&service, &TaskService::tasksChanged)
        , dependencyChanged(&service, &TaskService::dependenciesChanged)
    {
    }

    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository;
    FakeTaskBatchTransitionRepository batchTransitionRepository;
    FakeTaskDeletionRepository deletionRepository;
    FakeTaskCategoryRepository categoryRepository;
    TaskService service;
    QSignalSpy taskChanged;
    QSignalSpy dependencyChanged;
};

} // namespace

/// Service测试验证状态命令的附加业务约束、单次写入和通知语义。
class TaskStateTransitionsTest final : public QObject {
    Q_OBJECT

private slots:
    void creationAlwaysStartsTodoAndDetailEditPreservesStatus();
    void executesTheAllowedLifecycleOneWriteAtATime();
    void rejectsIllegalCommandsWithoutWritingOrNotifying();
    void startChecksBlockingAndSingleInProgressRules();
    void completeCannotSkipStartAndRechecksDependencies();
    void cancellationUnlocksWithoutDeletingTheEdgeAndRedoReactivatesIt();
    void redoRejectsInvalidatingAnAlreadyStartedSuccessor();
    void legacyArchiveRestoreSafelyReturnsToTodo();
};

void TaskStateTransitionsTest::creationAlwaysStartsTodoAndDetailEditPreservesStatus()
{
    ServiceFixture creationFixture;
    const auto created = creationFixture.service.createTask(validDraft());
    QVERIFY(created.ok());
    QCOMPARE(created.value->status(), TaskStatus::Todo);
    QCOMPARE(creationFixture.repository.insertCount(), 1);
    QCOMPARE(creationFixture.taskChanged.count(), 1);

    const Task todo = storedTask(TaskStatus::Todo);
    ServiceFixture editFixture{{todo}};
    TaskDraft edited = validDraft();
    edited.title = QStringLiteral("仅修改详情");
    const auto result = editFixture.service.updateTask(todo.id(), edited);
    QVERIFY(result.ok());
    QCOMPARE(result.value->status(), TaskStatus::Todo);
    QCOMPARE(editFixture.repository.updateCount(), 1);
    QCOMPARE(editFixture.taskChanged.count(), 1);
}

void TaskStateTransitionsTest::executesTheAllowedLifecycleOneWriteAtATime()
{
    const Task original = storedTask(TaskStatus::Todo);
    ServiceFixture fixture{{original}};

    const auto start = fixture.service.startTask(original.id());
    QVERIFY(start.ok());
    QCOMPARE(start.value->status(), TaskStatus::InProgress);

    const auto complete = fixture.service.completeTask(original.id());
    QVERIFY(complete.ok());
    QCOMPARE(complete.value->status(), TaskStatus::Done);

    const auto redo = fixture.service.redoTask(original.id());
    QVERIFY(redo.ok());
    QCOMPARE(redo.value->status(), TaskStatus::Todo);

    const auto cancel = fixture.service.cancelTask(original.id());
    QVERIFY(cancel.ok());
    QCOMPARE(cancel.value->status(), TaskStatus::Cancelled);

    const auto archive = fixture.service.archiveTask(original.id());
    QVERIFY(archive.ok());
    QCOMPARE(archive.value->status(), TaskStatus::Archived);
    QCOMPARE(archive.value->statusBeforeArchive(),
             std::optional<TaskStatus>{TaskStatus::Cancelled});

    const auto restore = fixture.service.restoreTask(original.id());
    QVERIFY(restore.ok());
    QCOMPARE(restore.value->status(), TaskStatus::Cancelled);
    QVERIFY(!restore.value->statusBeforeArchive().has_value());

    QCOMPARE(fixture.repository.updateCount(), 6);
    QCOMPARE(fixture.taskChanged.count(), 6);
    QCOMPARE(fixture.dependencyChanged.count(), 0);
}

void TaskStateTransitionsTest::rejectsIllegalCommandsWithoutWritingOrNotifying()
{
    const Task todo = storedTask(TaskStatus::Todo);
    ServiceFixture fixture{{todo}};

    QCOMPARE(fixture.service.completeTask(todo.id()).error,
             TaskError::InvalidTaskTransition);
    QCOMPARE(fixture.service.redoTask(todo.id()).error,
             TaskError::InvalidTaskTransition);
    QCOMPARE(fixture.service.archiveTask(todo.id()).error,
             TaskError::InvalidTaskTransition);
    QCOMPARE(fixture.service.restoreTask(todo.id()).error,
             TaskError::InvalidTaskTransition);
    QCOMPARE(fixture.repository.updateCount(), 0);
    QCOMPARE(fixture.taskChanged.count(), 0);
    QCOMPARE(fixture.repository.tasks(), QList<Task>{todo});
}

void TaskStateTransitionsTest::startChecksBlockingAndSingleInProgressRules()
{
    const Task predecessor = storedTask(TaskStatus::Todo, QStringLiteral("前置"));
    const Task blocked = storedTask(TaskStatus::Todo, QStringLiteral("后继"));
    ServiceFixture blockedFixture{
        {predecessor, blocked}, {{predecessor.id(), blocked.id()}}};
    const auto blockedResult = blockedFixture.service.startTask(blocked.id());
    QCOMPARE(blockedResult.error, TaskError::TaskBlocked);
    QCOMPARE(blockedResult.context.blockingTaskIds,
             QList<TaskId>{predecessor.id()});
    QCOMPARE(blockedFixture.repository.updateCount(), 0);
    QCOMPARE(blockedFixture.taskChanged.count(), 0);

    const Task running = storedTask(TaskStatus::InProgress,
                                    QStringLiteral("已经进行中"));
    const Task second = storedTask(TaskStatus::Todo, QStringLiteral("第二项"));
    ServiceFixture conflictFixture{{running, second}};
    const auto conflict = conflictFixture.service.startTask(second.id());
    QCOMPARE(conflict.error, TaskError::InProgressConflict);
    QCOMPARE(conflictFixture.repository.updateCount(), 0);
    QCOMPARE(conflictFixture.taskChanged.count(), 0);
}

void TaskStateTransitionsTest::completeCannotSkipStartAndRechecksDependencies()
{
    const Task todo = storedTask(TaskStatus::Todo);
    ServiceFixture todoFixture{{todo}};
    QCOMPARE(todoFixture.service.completeTask(todo.id()).error,
             TaskError::InvalidTaskTransition);

    // 模拟旧数据库中的不一致快照；完成命令仍须执行依赖防线。
    const Task predecessor = storedTask(TaskStatus::Todo, QStringLiteral("未完成前置"));
    const Task running = storedTask(TaskStatus::InProgress,
                                    QStringLiteral("异常进行中后继"));
    ServiceFixture blockedFixture{
        {predecessor, running}, {{predecessor.id(), running.id()}}};
    const auto result = blockedFixture.service.completeTask(running.id());
    QCOMPARE(result.error, TaskError::TaskBlocked);
    QCOMPARE(blockedFixture.repository.updateCount(), 0);
    QCOMPARE(blockedFixture.taskChanged.count(), 0);
}

void TaskStateTransitionsTest::cancellationUnlocksWithoutDeletingTheEdgeAndRedoReactivatesIt()
{
    const Task predecessor = storedTask(TaskStatus::Todo, QStringLiteral("可取消前置"));
    const Task successor = storedTask(TaskStatus::Todo, QStringLiteral("待解锁后继"));
    const TaskDependency edge{predecessor.id(), successor.id()};
    ServiceFixture fixture{{predecessor, successor}, {edge}};

    QVERIFY(fixture.service.listRecommendedTasks().value->at(1).dependencyState.blocked);
    const auto cancelled = fixture.service.cancelTask(predecessor.id());
    QVERIFY(cancelled.ok());
    QCOMPARE(fixture.dependencyRepository.dependencies(), QList<TaskDependency>{edge});
    QCOMPARE(fixture.dependencyChanged.count(), 0);

    const auto unlockedPlan = fixture.service.listRecommendedTasks();
    QVERIFY(unlockedPlan.ok());
    const auto successorAfterCancel = std::find_if(
        unlockedPlan.value->cbegin(), unlockedPlan.value->cend(),
        [&successor](const auto &planned) {
            return planned.task.id() == successor.id();
        });
    QVERIFY(successorAfterCancel != unlockedPlan.value->cend());
    QVERIFY(!successorAfterCancel->dependencyState.blocked);
    QCOMPARE(successorAfterCancel->dependencyState.cancelledPredecessorIds,
             QList<TaskId>{predecessor.id()});

    const auto redone = fixture.service.redoTask(predecessor.id());
    QVERIFY(redone.ok());
    const auto reactivatedPlan = fixture.service.listRecommendedTasks();
    const auto successorAfterRedo = std::find_if(
        reactivatedPlan.value->cbegin(), reactivatedPlan.value->cend(),
        [&successor](const auto &planned) {
            return planned.task.id() == successor.id();
        });
    QVERIFY(successorAfterRedo != reactivatedPlan.value->cend());
    QVERIFY(successorAfterRedo->dependencyState.blocked);
    QCOMPARE(fixture.repository.updateCount(), 2);
    QCOMPARE(fixture.taskChanged.count(), 2);
    QCOMPARE(fixture.dependencyChanged.count(), 0);
}

void TaskStateTransitionsTest::redoRejectsInvalidatingAnAlreadyStartedSuccessor()
{
    const Task cancelled = storedTask(TaskStatus::Cancelled,
                                      QStringLiteral("已取消前置"));
    const Task running = storedTask(TaskStatus::InProgress,
                                    QStringLiteral("已开始后继"));
    ServiceFixture fixture{
        {cancelled, running}, {{cancelled.id(), running.id()}}};

    const auto result = fixture.service.redoTask(cancelled.id());
    QCOMPARE(result.error, TaskError::DependencyStateConflict);
    QCOMPARE(result.context.blockingTaskIds, QList<TaskId>{cancelled.id()});
    QCOMPARE(result.context.conflictingTaskIds, QList<TaskId>{running.id()});
    QCOMPARE(fixture.repository.updateCount(), 0);
    QCOMPARE(fixture.taskChanged.count(), 0);
    QCOMPARE(fixture.repository.findById(cancelled.id())->status(),
             TaskStatus::Cancelled);
}

void TaskStateTransitionsTest::legacyArchiveRestoreSafelyReturnsToTodo()
{
    const Task legacy = storedTask(TaskStatus::Archived,
                                   QStringLiteral("旧版归档进行中"),
                                   TaskStatus::InProgress);
    const Task running = storedTask(TaskStatus::InProgress,
                                    QStringLiteral("当前进行中"));
    ServiceFixture fixture{{legacy, running}};

    const auto result = fixture.service.restoreTask(legacy.id());
    QVERIFY(result.ok());
    QCOMPARE(result.value->status(), TaskStatus::Todo);
    QVERIFY(!result.value->statusBeforeArchive().has_value());
    QCOMPARE(fixture.repository.updateCount(), 1);
    QCOMPARE(fixture.taskChanged.count(), 1);
}

QTEST_APPLESS_MAIN(TaskStateTransitionsTest)

#include "tst_TaskStateTransitions.moc"
