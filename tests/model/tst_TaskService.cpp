#include "fakes/FakeTaskRepository.h"
#include "fakes/FakeTaskDependencyRepository.h"
#include "fakes/FakeTaskCreationRepository.h"

#include "domain/Task.h"
#include "domain/TaskConstraints.h"
#include "services/TaskService.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>

#include <algorithm>
#include <optional>
#include <utility>

using smartmate::model::Task;
using smartmate::model::TaskCreationRequest;
using smartmate::model::TaskDraft;
using smartmate::model::TaskError;
using smartmate::model::TaskId;
using smartmate::model::TaskPriority;
using smartmate::model::TaskService;
using smartmate::model::TaskStatus;
using smartmate::tests::FakeTaskDependencyRepository;
using smartmate::tests::FakeTaskCreationRepository;
using smartmate::tests::FakeTaskRepository;

namespace {

[[nodiscard]] QDateTime timestamp(qint64 milliseconds = 1700000000000)
{
    return QDateTime::fromMSecsSinceEpoch(milliseconds, QTimeZone::UTC);
}

[[nodiscard]] Task storedTask(TaskStatus status = TaskStatus::Todo,
                              std::optional<TaskStatus> statusBeforeArchive = std::nullopt,
                              QString title = QStringLiteral("Stored task"))
{
    return Task{QUuid::createUuid(),
                std::move(title),
                QStringLiteral("Stored description"),
                TaskPriority::Normal,
                status,
                statusBeforeArchive,
                std::nullopt,
                30,
                timestamp(),
                timestamp()};
}

[[nodiscard]] TaskDraft validDraft()
{
    TaskDraft draft;
    draft.title = QStringLiteral("Valid task");
    draft.description = QStringLiteral("Description");
    return draft;
}

[[nodiscard]] TaskDraft draftFor(const Task &task, const TaskStatus status)
{
    return {task.title(),
            task.description(),
            task.priority(),
            status,
            task.deadline(),
            task.estimatedMinutes()};
}

} // namespace

// 在不启动 SQLite 或 QML 的情况下验证任务规则、错误映射和成功写入通知。
class TaskServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void listsAndFindsTasks();
    void createsTaskWithEveryField();
    void createsTaskAndPredecessorsAtomically();
    void rejectsInvalidCreationPredecessorsWithoutWriting();
    void rollsBackAtomicCreationFailureWithoutSignals();
    void listsEligibleCreationPredecessors();
    void normalizesOmittedDescriptionToEmptyText();
    void validatesDraftWithoutRepositoryAccess();
    void validatesDraftFields();
    void acceptsDeadlineAndEstimateBoundaries();
    void acceptsEveryStatusAndPriority();
    void enforcesSingleInProgressTask();
    void mapsConcurrentInProgressCreateConflict();
    void mapsConcurrentInProgressUpdateConflict();
    void mapsConcurrentInProgressRestoreConflict();
    void updatesTaskAndPreservesIdentity();
    void archivesAndRestoresOriginalStatus();
    void rejectsRestoreWhenInProgressConflicts();
    void reportsInvalidOperationsAndMissingTasks();
    void mapsRepositoryFailures();
    void replacesDependenciesAndReportsStructuredErrors();
    void enforcesDependencyStatusConsistency();
    void mapsDependencyRepositoryFailures();
    void buildsGraphSnapshotWithArchivedClosure();
};

void TaskServiceTest::listsAndFindsTasks()
{
    const Task first = storedTask(TaskStatus::InProgress, std::nullopt,
                                  QStringLiteral("Understand MVVM"));
    const Task second = storedTask(TaskStatus::Todo, std::nullopt,
                                   QStringLiteral("Build SmartMate"));
    FakeTaskRepository repository{{first, second}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    const TaskService service{repository, dependencyRepository, creationRepository};

    const auto listResult = service.listTasks();
    QVERIFY(listResult.ok());
    QCOMPARE(listResult.value->size(), 2);
    QCOMPARE(listResult.value->at(0), first);
    QCOMPARE(listResult.value->at(1), second);

    const auto findResult = service.findTask(second.id());
    QVERIFY(findResult.ok());
    QCOMPARE(*findResult.value, second);
}

void TaskServiceTest::createsTaskWithEveryField()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};
    QSignalSpy dependencySpy{&service, &TaskService::dependenciesChanged};
    const QDateTime localDeadline =
        QDateTime::fromString(QStringLiteral("2027-02-03T14:30:00+08:00"), Qt::ISODate);

    TaskDraft draft;
    draft.title = QStringLiteral("  完成大作业  ");
    draft.description = QStringLiteral("实现任务模块的完整纵向链路");
    draft.priority = TaskPriority::Urgent;
    draft.status = TaskStatus::Todo;
    draft.deadline = localDeadline;
    draft.estimatedMinutes = 180;

    const auto result = service.createTask(draft);

    QVERIFY(result.ok());
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(dependencySpy.count(), 0);
    QCOMPARE(repository.tasks().size(), 1);
    const Task &created = *result.value;
    QVERIFY(!created.id().isNull());
    QCOMPARE(created.title(), QStringLiteral("完成大作业"));
    QCOMPARE(created.description(), draft.description);
    QCOMPARE(created.priority(), TaskPriority::Urgent);
    QCOMPARE(created.status(), TaskStatus::Todo);
    QVERIFY(!created.statusBeforeArchive().has_value());
    QCOMPARE(created.deadline(), std::optional<QDateTime>{localDeadline.toUTC()});
    QCOMPARE(created.estimatedMinutes(), std::optional<int>{180});
    QVERIFY(created.createdAtUtc().isValid());
    QCOMPARE(created.createdAtUtc().timeSpec(), Qt::UTC);
    QCOMPARE(created.updatedAtUtc(), created.createdAtUtc());
    QCOMPARE(repository.tasks().constFirst(), created);
}

void TaskServiceTest::createsTaskAndPredecessorsAtomically()
{
    const Task first = storedTask(TaskStatus::Done, std::nullopt,
                                  QStringLiteral("Completed predecessor"));
    const Task second = storedTask(TaskStatus::Todo, std::nullopt,
                                   QStringLiteral("Pending predecessor"));
    FakeTaskRepository repository{{first, second}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy taskSpy{&service, &TaskService::tasksChanged};
    QSignalSpy dependencySpy{&service, &TaskService::dependenciesChanged};

    const TaskCreationRequest request{
        validDraft(),
        {second.id(), first.id()}};
    const auto result = service.createTask(request);

    QVERIFY(result.ok());
    QCOMPARE(creationRepository.insertCount(), 1);
    QCOMPARE(repository.tasks().size(), 3);
    QCOMPARE(taskSpy.count(), 1);
    QCOMPARE(dependencySpy.count(), 1);
    QCOMPARE(dependencyRepository.dependencies().size(), 2);
    QVERIFY(dependencyRepository.dependencies().contains(
        {first.id(), result.value->id()}));
    QVERIFY(dependencyRepository.dependencies().contains(
        {second.id(), result.value->id()}));
}

void TaskServiceTest::rejectsInvalidCreationPredecessorsWithoutWriting()
{
    const Task active = storedTask(TaskStatus::Todo);
    const Task archived = storedTask(TaskStatus::Archived, TaskStatus::Done);
    FakeTaskRepository repository{{active, archived}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy taskSpy{&service, &TaskService::tasksChanged};
    QSignalSpy dependencySpy{&service, &TaskService::dependenciesChanged};

    TaskCreationRequest request{validDraft(), {active.id(), active.id()}};
    QCOMPARE(service.createTask(request).error, TaskError::DependencyDuplicate);

    const TaskId missingId = QUuid::createUuid();
    request.predecessorIds = {missingId};
    const auto missing = service.createTask(request);
    QCOMPARE(missing.error, TaskError::DependencyEndpointNotFound);
    QCOMPARE(missing.context.conflictingTaskIds, QList<smartmate::model::TaskId>{missingId});

    request.predecessorIds = {archived.id()};
    QCOMPARE(service.createTask(request).error,
             TaskError::DependencyPredecessorNotEligible);

    request.predecessorIds = {active.id()};
    request.task.status = TaskStatus::Done;
    QCOMPARE(service.createTask(request).error,
             TaskError::DependencyTargetNotEditable);

    QCOMPARE(creationRepository.insertCount(), 0);
    QCOMPARE(repository.tasks().size(), 2);
    QVERIFY(dependencyRepository.dependencies().isEmpty());
    QCOMPARE(taskSpy.count(), 0);
    QCOMPARE(dependencySpy.count(), 0);
}

void TaskServiceTest::rollsBackAtomicCreationFailureWithoutSignals()
{
    const Task predecessor = storedTask(TaskStatus::Todo);
    FakeTaskRepository repository{{predecessor}};
    FakeTaskDependencyRepository dependencyRepository;
    dependencyRepository.setWriteFailure(true);
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy taskSpy{&service, &TaskService::tasksChanged};
    QSignalSpy dependencySpy{&service, &TaskService::dependenciesChanged};

    const auto result = service.createTask(
        TaskCreationRequest{validDraft(), {predecessor.id()}});

    QCOMPARE(result.error, TaskError::PersistenceFailure);
    QCOMPARE(repository.tasks(), QList<Task>{predecessor});
    QVERIFY(dependencyRepository.dependencies().isEmpty());
    QCOMPARE(creationRepository.insertCount(), 0);
    QCOMPARE(taskSpy.count(), 0);
    QCOMPARE(dependencySpy.count(), 0);
}

void TaskServiceTest::listsEligibleCreationPredecessors()
{
    const Task inProgress = storedTask(TaskStatus::InProgress);
    const Task todo = storedTask(TaskStatus::Todo);
    const Task done = storedTask(TaskStatus::Done);
    const Task cancelled = storedTask(TaskStatus::Cancelled);
    const Task archived = storedTask(TaskStatus::Archived, TaskStatus::Todo);
    FakeTaskRepository repository{{archived, cancelled, todo, done, inProgress}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    const TaskService service{repository, dependencyRepository, creationRepository};

    const auto result = service.listEligibleCreationPredecessors();

    QVERIFY(result.ok());
    QCOMPARE(result.value->size(), 4);
    QCOMPARE(result.value->constFirst().id(), inProgress.id());
    QVERIFY(std::none_of(result.value->cbegin(), result.value->cend(),
                         [&archived](const Task &task) {
        return task.id() == archived.id();
    }));
}

void TaskServiceTest::normalizesOmittedDescriptionToEmptyText()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskDraft draft;
    draft.title = QStringLiteral("只填写标题");
    // 模拟旧调用方或未触碰描述输入框时传入的 null QString。
    draft.description = QString{};

    const auto result = service.createTask(draft);

    QVERIFY(result.ok());
    QVERIFY(result.value->description().isEmpty());
    QVERIFY(!result.value->description().isNull());
    QCOMPARE(repository.tasks().size(), 1);
}

void TaskServiceTest::validatesDraftWithoutRepositoryAccess()
{
    FakeTaskRepository repository;
    repository.setReadFailure(true);
    repository.setWriteFailure(true);
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    TaskDraft draft = validDraft();
    draft.title = QStringLiteral("   ");
    const auto emptyTitleResult = service.validateDraft(draft);
    QVERIFY(!emptyTitleResult.ok());
    QCOMPARE(emptyTitleResult.error, TaskError::EmptyTitle);
    QVERIFY(!emptyTitleResult.detail.isEmpty());

    draft = validDraft();
    draft.estimatedMinutes = 525600;
    const auto maximumEstimateResult = service.validateDraft(draft);
    QVERIFY(maximumEstimateResult.ok());
    QCOMPARE(maximumEstimateResult.error, TaskError::None);
    QVERIFY(maximumEstimateResult.detail.isEmpty());

    draft.estimatedMinutes = 525601;
    const auto excessiveEstimateResult = service.validateDraft(draft);
    QVERIFY(!excessiveEstimateResult.ok());
    QCOMPARE(excessiveEstimateResult.error, TaskError::InvalidEstimate);
    QVERIFY(!excessiveEstimateResult.detail.isEmpty());

    QCOMPARE(changedSpy.count(), 0);
    QVERIFY(repository.tasks().isEmpty());
}

void TaskServiceTest::validatesDraftFields()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    const auto expectError = [&service](TaskDraft draft, TaskError expected) {
        const auto result = service.createTask(draft);
        QVERIFY(!result.ok());
        QCOMPARE(result.error, expected);
        QVERIFY(!result.detail.isEmpty());
    };

    TaskDraft draft = validDraft();
    draft.title = QStringLiteral("   ");
    expectError(draft, TaskError::EmptyTitle);

    draft = validDraft();
    draft.title = QString(201, QLatin1Char('a'));
    expectError(draft, TaskError::TitleTooLong);

    draft = validDraft();
    draft.description = QString(5001, QLatin1Char('a'));
    expectError(draft, TaskError::DescriptionTooLong);

    draft = validDraft();
    draft.deadline = QDateTime{};
    expectError(draft, TaskError::InvalidDeadline);

    draft = validDraft();
    draft.estimatedMinutes = 0;
    expectError(draft, TaskError::InvalidEstimate);

    draft.estimatedMinutes = 525601;
    expectError(draft, TaskError::InvalidEstimate);

    draft = validDraft();
    draft.priority = static_cast<TaskPriority>(999);
    expectError(draft, TaskError::InvalidPriority);

    draft = validDraft();
    draft.status = static_cast<TaskStatus>(999);
    expectError(draft, TaskError::InvalidStatus);

    QCOMPARE(changedSpy.count(), 0);
    QVERIFY(repository.tasks().isEmpty());
}

void TaskServiceTest::acceptsDeadlineAndEstimateBoundaries()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    const TaskService service{repository, dependencyRepository, creationRepository};
    TaskDraft draft = validDraft();

    // 截止时间表达约束而不是输入格式约束，因此过去的有效时刻仍可保存。
    draft.deadline = timestamp(0);
    draft.estimatedMinutes = smartmate::model::TaskConstraints::minimumEstimatedMinutes;
    QVERIFY(service.validateDraft(draft).ok());

    draft.estimatedMinutes = smartmate::model::TaskConstraints::maximumEstimatedMinutes;
    QVERIFY(service.validateDraft(draft).ok());

    draft.estimatedMinutes = 0;
    QCOMPARE(service.validateDraft(draft).error, TaskError::InvalidEstimate);

    draft.estimatedMinutes =
        smartmate::model::TaskConstraints::maximumEstimatedMinutes + 1;
    QCOMPARE(service.validateDraft(draft).error, TaskError::InvalidEstimate);
}

void TaskServiceTest::acceptsEveryStatusAndPriority()
{
    const QList<TaskStatus> statuses{TaskStatus::Todo,
                                     TaskStatus::InProgress,
                                     TaskStatus::Done,
                                     TaskStatus::Cancelled,
                                     TaskStatus::Archived};
    const QList<TaskPriority> priorities{TaskPriority::Low,
                                         TaskPriority::Normal,
                                         TaskPriority::High,
                                         TaskPriority::Urgent};

    for (const TaskStatus status : statuses) {
        for (const TaskPriority priority : priorities) {
            FakeTaskRepository repository;
            FakeTaskDependencyRepository dependencyRepository;
            FakeTaskCreationRepository creationRepository{repository,
                                                           dependencyRepository};
            TaskService service{repository, dependencyRepository,
                                creationRepository};
            TaskDraft draft = validDraft();
            draft.status = status;
            draft.priority = priority;

            const auto result = service.createTask(draft);

            QVERIFY(result.ok());
            QCOMPARE(result.value->status(), status);
            QCOMPARE(result.value->priority(), priority);
            if (status == TaskStatus::Archived) {
                QCOMPARE(result.value->statusBeforeArchive(),
                         std::optional<TaskStatus>{TaskStatus::Todo});
            } else {
                QVERIFY(!result.value->statusBeforeArchive().has_value());
            }
        }
    }
}

void TaskServiceTest::enforcesSingleInProgressTask()
{
    const Task active = storedTask(TaskStatus::InProgress);
    const Task todo = storedTask(TaskStatus::Todo);
    FakeTaskRepository repository{{active, todo}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    TaskDraft createDraft = validDraft();
    createDraft.status = TaskStatus::InProgress;
    const auto createResult = service.createTask(createDraft);
    QCOMPARE(createResult.error, TaskError::InProgressConflict);

    TaskDraft updateDraft = validDraft();
    updateDraft.status = TaskStatus::InProgress;
    const auto updateResult = service.updateTask(todo.id(), updateDraft);
    QCOMPARE(updateResult.error, TaskError::InProgressConflict);

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(repository.tasks().size(), 2);
    QCOMPARE(repository.findById(todo.id())->status(), TaskStatus::Todo);
}

void TaskServiceTest::mapsConcurrentInProgressCreateConflict()
{
    // 模拟业务预检通过后，另一执行方抢先写入进行中任务。
    FakeTaskRepository repository;
    repository.setCompetingTaskOnNextWrite(storedTask(TaskStatus::InProgress));
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};
    TaskDraft draft = validDraft();
    draft.status = TaskStatus::InProgress;

    const auto result = service.createTask(draft);

    QCOMPARE(result.error, TaskError::InProgressConflict);
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(repository.tasks().size(), 1);
    QCOMPARE(repository.tasks().constFirst().status(), TaskStatus::InProgress);
}

void TaskServiceTest::mapsConcurrentInProgressUpdateConflict()
{
    const Task target = storedTask(TaskStatus::Todo);
    FakeTaskRepository repository{{target}};
    repository.setCompetingTaskOnNextWrite(storedTask(TaskStatus::InProgress));
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};
    TaskDraft draft = validDraft();
    draft.status = TaskStatus::InProgress;

    const auto result = service.updateTask(target.id(), draft);

    QCOMPARE(result.error, TaskError::InProgressConflict);
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(repository.findById(target.id())->status(), TaskStatus::Todo);
    QCOMPARE(repository.tasks().size(), 2);
}

void TaskServiceTest::mapsConcurrentInProgressRestoreConflict()
{
    const Task archived = storedTask(TaskStatus::Archived, TaskStatus::InProgress);
    FakeTaskRepository repository{{archived}};
    repository.setCompetingTaskOnNextWrite(storedTask(TaskStatus::InProgress));
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    const auto result = service.restoreTask(archived.id());

    QCOMPARE(result.error, TaskError::InProgressConflict);
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(repository.findById(archived.id())->status(), TaskStatus::Archived);
    QCOMPARE(repository.tasks().size(), 2);
}

void TaskServiceTest::updatesTaskAndPreservesIdentity()
{
    const Task original = storedTask();
    FakeTaskRepository repository{{original}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    TaskDraft draft;
    draft.title = QStringLiteral("  Updated title ");
    draft.description = QStringLiteral("Updated description");
    draft.priority = TaskPriority::High;
    draft.status = TaskStatus::Done;
    draft.deadline = QDateTime::fromString(QStringLiteral("2028-01-02T12:00:00+02:00"),
                                           Qt::ISODate);
    draft.estimatedMinutes = 75;

    const auto result = service.updateTask(original.id(), draft);

    QVERIFY(result.ok());
    QCOMPARE(changedSpy.count(), 1);
    const Task &updated = *result.value;
    QCOMPARE(updated.id(), original.id());
    QCOMPARE(updated.createdAtUtc(), original.createdAtUtc());
    QVERIFY(updated.updatedAtUtc() >= original.updatedAtUtc());
    QCOMPARE(updated.title(), QStringLiteral("Updated title"));
    QCOMPARE(updated.description(), draft.description);
    QCOMPARE(updated.priority(), TaskPriority::High);
    QCOMPARE(updated.status(), TaskStatus::Done);
    QCOMPARE(updated.deadline(), std::optional<QDateTime>{draft.deadline->toUTC()});
    QCOMPARE(updated.estimatedMinutes(), std::optional<int>{75});
    QCOMPARE(repository.findById(original.id()), result.value);
}

void TaskServiceTest::archivesAndRestoresOriginalStatus()
{
    const Task original = storedTask(TaskStatus::InProgress);
    FakeTaskRepository repository{{original}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    const auto archiveResult = service.archiveTask(original.id());
    QVERIFY(archiveResult.ok());
    QCOMPARE(archiveResult.value->status(), TaskStatus::Archived);
    QCOMPARE(archiveResult.value->statusBeforeArchive(),
             std::optional<TaskStatus>{TaskStatus::InProgress});

    const auto restoreResult = service.restoreTask(original.id());
    QVERIFY(restoreResult.ok());
    QCOMPARE(restoreResult.value->status(), TaskStatus::InProgress);
    QVERIFY(!restoreResult.value->statusBeforeArchive().has_value());
    QCOMPARE(changedSpy.count(), 2);
}

void TaskServiceTest::rejectsRestoreWhenInProgressConflicts()
{
    const Task archived = storedTask(TaskStatus::Archived, TaskStatus::InProgress);
    const Task active = storedTask(TaskStatus::InProgress);
    FakeTaskRepository repository{{archived, active}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    const auto result = service.restoreTask(archived.id());

    QCOMPARE(result.error, TaskError::InProgressConflict);
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(repository.findById(archived.id())->status(), TaskStatus::Archived);
    QCOMPARE(repository.findById(archived.id())->statusBeforeArchive(),
             std::optional<TaskStatus>{TaskStatus::InProgress});
}

void TaskServiceTest::reportsInvalidOperationsAndMissingTasks()
{
    const Task todo = storedTask();
    FakeTaskRepository repository{{todo}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};
    const QUuid missingId = QUuid::createUuid();

    QCOMPARE(service.findTask(missingId).error, TaskError::NotFound);
    QCOMPARE(service.updateTask(missingId, validDraft()).error, TaskError::NotFound);
    QCOMPARE(service.archiveTask(missingId).error, TaskError::NotFound);
    QCOMPARE(service.restoreTask(missingId).error, TaskError::NotFound);
    QCOMPARE(service.restoreTask(todo.id()).error, TaskError::InvalidStatus);
    QCOMPARE(changedSpy.count(), 0);
}

void TaskServiceTest::mapsRepositoryFailures()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    repository.setReadFailure(true);
    const auto listResult = service.listTasks();
    QCOMPARE(listResult.error, TaskError::PersistenceFailure);
    QVERIFY(!listResult.detail.isEmpty());
    QCOMPARE(service.findTask(QUuid::createUuid()).error, TaskError::PersistenceFailure);

    repository.setReadFailure(false);
    repository.setWriteFailure(true);
    const auto createResult = service.createTask(validDraft());
    QCOMPARE(createResult.error, TaskError::PersistenceFailure);
    QVERIFY(!createResult.detail.isEmpty());
    QCOMPARE(changedSpy.count(), 0);
}

void TaskServiceTest::replacesDependenciesAndReportsStructuredErrors()
{
    const Task predecessor = storedTask(TaskStatus::Todo, std::nullopt,
                                        QStringLiteral("Predecessor"));
    const Task target = storedTask(TaskStatus::Todo, std::nullopt,
                                   QStringLiteral("Target"));
    const Task archived = storedTask(TaskStatus::Archived, TaskStatus::Todo,
                                     QStringLiteral("Archived"));
    const Task completed = storedTask(TaskStatus::Done, std::nullopt,
                                      QStringLiteral("Completed"));
    FakeTaskRepository repository{{predecessor, target, archived, completed}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy changedSpy{&service, &TaskService::dependenciesChanged};

    QVERIFY(service.listDependencies().ok());
    QVERIFY(service.listDependencies().value->isEmpty());

    const auto replaced = service.replaceTaskPredecessors(
        target.id(), {predecessor.id()});
    QVERIFY(replaced.ok());
    QCOMPARE(replaced.value->size(), 1);
    QCOMPARE(dependencyRepository.replaceCount(), 1);
    QCOMPARE(changedSpy.count(), 1);

    // 相同集合是无写入的幂等成功。
    QVERIFY(service.replaceTaskPredecessors(target.id(), {predecessor.id()}).ok());
    QCOMPARE(dependencyRepository.replaceCount(), 1);
    QCOMPARE(changedSpy.count(), 1);

    const auto selfReference = service.replaceTaskPredecessors(
        target.id(), {target.id()});
    QCOMPARE(selfReference.error, TaskError::DependencySelfReference);
    QCOMPARE(selfReference.context.conflictingTaskIds,
             QList<smartmate::model::TaskId>({target.id()}));

    const auto duplicate = service.replaceTaskPredecessors(
        target.id(), {predecessor.id(), predecessor.id()});
    QCOMPARE(duplicate.error, TaskError::DependencyDuplicate);
    QCOMPARE(duplicate.context.conflictingTaskIds,
             QList<smartmate::model::TaskId>({predecessor.id()}));

    const QUuid missingId = QUuid::createUuid();
    const auto missing = service.replaceTaskPredecessors(target.id(), {missingId});
    QCOMPARE(missing.error, TaskError::DependencyEndpointNotFound);
    QCOMPARE(missing.context.conflictingTaskIds,
             QList<smartmate::model::TaskId>({missingId}));

    QCOMPARE(service.replaceTaskPredecessors(completed.id(), {}).error,
             TaskError::DependencyTargetNotEditable);
    const auto archivedPredecessor = service.replaceTaskPredecessors(
        target.id(), {archived.id()});
    QCOMPARE(archivedPredecessor.error,
             TaskError::DependencyPredecessorNotEligible);
    QCOMPARE(archivedPredecessor.context.conflictingTaskIds,
             QList<smartmate::model::TaskId>({archived.id()}));

    // 现有 A→B 上再把 B 设为 A 的前置会形成 B→A→B 的闭合环。
    const auto cycle = service.replaceTaskPredecessors(
        predecessor.id(), {target.id()});
    QCOMPARE(cycle.error, TaskError::DependencyCycle);
    QVERIFY(cycle.context.cyclePath.size() >= 3);
    QCOMPARE(cycle.context.cyclePath.constFirst(),
             cycle.context.cyclePath.constLast());
    QCOMPARE(dependencyRepository.replaceCount(), 1);

    const auto cleared = service.replaceTaskPredecessors(target.id(), {});
    QVERIFY(cleared.ok());
    QVERIFY(cleared.value->isEmpty());
    QVERIFY(dependencyRepository.dependencies().isEmpty());
    QCOMPARE(dependencyRepository.replaceCount(), 2);
    QCOMPARE(changedSpy.count(), 2);

    // 已存在的归档前置可以继续保留或移除，但不能作为新前置加入。
    FakeTaskDependencyRepository legacyDependencies{
        {{archived.id(), target.id()}}};
    FakeTaskCreationRepository legacyCreation{repository, legacyDependencies};
    TaskService legacyService{repository, legacyDependencies, legacyCreation};
    QSignalSpy legacyChangedSpy{&legacyService, &TaskService::dependenciesChanged};
    const auto retainArchived = legacyService.replaceTaskPredecessors(
        target.id(), {archived.id(), predecessor.id()});
    QVERIFY(retainArchived.ok());
    QCOMPARE(retainArchived.value->size(), 2);
    const auto removeArchived = legacyService.replaceTaskPredecessors(
        target.id(), {predecessor.id()});
    QVERIFY(removeArchived.ok());
    QCOMPARE(removeArchived.value->size(), 1);
    QCOMPARE(removeArchived.value->constFirst().predecessorId, predecessor.id());
    QCOMPARE(legacyChangedSpy.count(), 2);
}

void TaskServiceTest::enforcesDependencyStatusConsistency()
{
    const Task predecessor = storedTask(TaskStatus::Todo, std::nullopt,
                                        QStringLiteral("Unfinished predecessor"));
    const Task target = storedTask(TaskStatus::Todo, std::nullopt,
                                   QStringLiteral("Blocked target"));
    FakeTaskRepository repository{{predecessor, target}};
    FakeTaskDependencyRepository dependencyRepository{
        {{predecessor.id(), target.id()}}};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};

    const auto startBlocked = service.updateTask(
        target.id(), draftFor(target, TaskStatus::InProgress));
    QCOMPARE(startBlocked.error, TaskError::TaskBlocked);
    QCOMPARE(startBlocked.context.blockingTaskIds,
             QList<smartmate::model::TaskId>({predecessor.id()}));
    QCOMPARE(startBlocked.context.conflictingTaskIds,
             QList<smartmate::model::TaskId>({target.id()}));

    const auto completeBlocked = service.updateTask(
        target.id(), draftFor(target, TaskStatus::Done));
    QCOMPARE(completeBlocked.error, TaskError::TaskBlocked);

    const Task completedPredecessor = storedTask(
        TaskStatus::Done, std::nullopt, QStringLiteral("Completed predecessor"));
    const Task activeSuccessor = storedTask(
        TaskStatus::InProgress, std::nullopt, QStringLiteral("Active successor"));
    FakeTaskRepository activeRepository{{completedPredecessor, activeSuccessor}};
    FakeTaskDependencyRepository activeDependencies{
        {{completedPredecessor.id(), activeSuccessor.id()}}};
    FakeTaskCreationRepository activeCreation{activeRepository,
                                              activeDependencies};
    TaskService activeService{activeRepository, activeDependencies,
                              activeCreation};

    const auto invalidateActive = activeService.updateTask(
        completedPredecessor.id(),
        draftFor(completedPredecessor, TaskStatus::Todo));
    QCOMPARE(invalidateActive.error, TaskError::DependencyStateConflict);
    QCOMPARE(invalidateActive.context.blockingTaskIds,
             QList<smartmate::model::TaskId>({completedPredecessor.id()}));
    QCOMPARE(invalidateActive.context.conflictingTaskIds,
             QList<smartmate::model::TaskId>({activeSuccessor.id()}));
    QCOMPARE(activeRepository.findById(completedPredecessor.id())->status(),
             TaskStatus::Done);

    const Task archivedCompletedSuccessor = storedTask(
        TaskStatus::Archived, TaskStatus::Done,
        QStringLiteral("Archived completed successor"));
    FakeTaskRepository archivedSuccessorRepository{
        {completedPredecessor, archivedCompletedSuccessor}};
    FakeTaskDependencyRepository archivedSuccessorDependencies{
        {{completedPredecessor.id(), archivedCompletedSuccessor.id()}}};
    FakeTaskCreationRepository archivedSuccessorCreation{
        archivedSuccessorRepository, archivedSuccessorDependencies};
    TaskService archivedSuccessorService{
        archivedSuccessorRepository, archivedSuccessorDependencies,
        archivedSuccessorCreation};
    QCOMPARE(archivedSuccessorService.updateTask(
                 completedPredecessor.id(),
                 draftFor(completedPredecessor, TaskStatus::Cancelled)).error,
             TaskError::DependencyStateConflict);

    // Archived-before-Done 前置仍满足关系，因此目标可以开始。
    const Task archivedCompletedPredecessor = storedTask(
        TaskStatus::Archived, TaskStatus::Done,
        QStringLiteral("Archived completed predecessor"));
    const Task readyTarget = storedTask(TaskStatus::Todo);
    FakeTaskRepository readyRepository{{archivedCompletedPredecessor, readyTarget}};
    FakeTaskDependencyRepository readyDependencies{
        {{archivedCompletedPredecessor.id(), readyTarget.id()}}};
    FakeTaskCreationRepository readyCreation{readyRepository, readyDependencies};
    TaskService readyService{readyRepository, readyDependencies, readyCreation};
    QVERIFY(readyService.updateTask(
        readyTarget.id(), draftFor(readyTarget, TaskStatus::InProgress)).ok());

    const Task archivedActiveTarget = storedTask(
        TaskStatus::Archived, TaskStatus::InProgress,
        QStringLiteral("Archived active target"));
    FakeTaskRepository restoreRepository{{predecessor, archivedActiveTarget}};
    FakeTaskDependencyRepository restoreDependencies{
        {{predecessor.id(), archivedActiveTarget.id()}}};
    FakeTaskCreationRepository restoreCreation{restoreRepository,
                                               restoreDependencies};
    TaskService restoreService{restoreRepository, restoreDependencies,
                               restoreCreation};
    const auto restoreBlocked = restoreService.restoreTask(archivedActiveTarget.id());
    QCOMPARE(restoreBlocked.error, TaskError::TaskBlocked);
    QCOMPARE(restoreRepository.findById(archivedActiveTarget.id())->status(),
             TaskStatus::Archived);
}

void TaskServiceTest::mapsDependencyRepositoryFailures()
{
    const Task predecessor = storedTask(TaskStatus::Todo);
    const Task target = storedTask(TaskStatus::Todo);
    FakeTaskRepository repository{{predecessor, target}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    QSignalSpy changedSpy{&service, &TaskService::dependenciesChanged};

    dependencyRepository.setReadFailure(true);
    QCOMPARE(service.listDependencies().error, TaskError::PersistenceFailure);
    QCOMPARE(service.listRecommendedTasks().error, TaskError::PersistenceFailure);
    QCOMPARE(service.taskGraphSnapshot().error, TaskError::PersistenceFailure);

    dependencyRepository.setReadFailure(false);
    dependencyRepository.setWriteFailure(true);
    const auto writeFailure = service.replaceTaskPredecessors(
        target.id(), {predecessor.id()});
    QCOMPARE(writeFailure.error, TaskError::PersistenceFailure);
    QVERIFY(!writeFailure.detail.isEmpty());
    QCOMPARE(changedSpy.count(), 0);
}

void TaskServiceTest::buildsGraphSnapshotWithArchivedClosure()
{
    const Task archivedCompleted = storedTask(
        TaskStatus::Archived, TaskStatus::Done,
        QStringLiteral("Archived completed predecessor"));
    const Task root = storedTask(TaskStatus::Todo, std::nullopt,
                                 QStringLiteral("Active root"));
    const Task connectedArchived = storedTask(
        TaskStatus::Archived, TaskStatus::Todo,
        QStringLiteral("Connected archived successor"));
    const Task isolatedActive = storedTask(
        TaskStatus::Cancelled, std::nullopt,
        QStringLiteral("Isolated active task"));
    const Task pureArchived = storedTask(
        TaskStatus::Archived, TaskStatus::Todo,
        QStringLiteral("Pure archived component"));
    FakeTaskRepository repository{{pureArchived, connectedArchived, root,
                                   isolatedActive, archivedCompleted}};
    FakeTaskDependencyRepository dependencyRepository{
        {{root.id(), connectedArchived.id()},
         {archivedCompleted.id(), root.id()}}};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    const TaskService service{repository, dependencyRepository, creationRepository};

    const auto result = service.taskGraphSnapshot();

    QVERIFY(result.ok());
    QCOMPARE(result.value->nodes.size(), 4);
    QCOMPARE(result.value->edges.size(), 2);
    const auto findNode = [&result](const smartmate::model::TaskId &taskId) {
        return std::find_if(result.value->nodes.cbegin(), result.value->nodes.cend(),
                            [&taskId](const smartmate::model::TaskGraphNode &node) {
            return node.task.id() == taskId;
        });
    };
    const auto archivedNode = findNode(archivedCompleted.id());
    const auto rootNode = findNode(root.id());
    const auto connectedNode = findNode(connectedArchived.id());
    const auto isolatedNode = findNode(isolatedActive.id());
    QVERIFY(archivedNode != result.value->nodes.cend());
    QVERIFY(rootNode != result.value->nodes.cend());
    QVERIFY(connectedNode != result.value->nodes.cend());
    QVERIFY(isolatedNode != result.value->nodes.cend());
    QVERIFY(findNode(pureArchived.id()) == result.value->nodes.cend());
    QCOMPARE(archivedNode->dependencyLevel, 0);
    QCOMPARE(rootNode->dependencyLevel, 1);
    QCOMPARE(connectedNode->dependencyLevel, 2);
    QCOMPARE(isolatedNode->dependencyLevel, 0);

    const auto satisfiedEdge = std::find_if(
        result.value->edges.cbegin(), result.value->edges.cend(),
        [&archivedCompleted](const smartmate::model::TaskGraphEdge &edge) {
            return edge.dependency.predecessorId == archivedCompleted.id();
        });
    const auto unsatisfiedEdge = std::find_if(
        result.value->edges.cbegin(), result.value->edges.cend(),
        [&root](const smartmate::model::TaskGraphEdge &edge) {
            return edge.dependency.predecessorId == root.id();
        });
    QVERIFY(satisfiedEdge != result.value->edges.cend());
    QVERIFY(unsatisfiedEdge != result.value->edges.cend());
    QVERIFY(satisfiedEdge->satisfied);
    QVERIFY(!unsatisfiedEdge->satisfied);
}

QTEST_APPLESS_MAIN(TaskServiceTest)

#include "tst_TaskService.moc"
