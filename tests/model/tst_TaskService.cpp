#include "fakes/FakeTaskRepository.h"

#include "domain/Task.h"
#include "domain/TaskConstraints.h"
#include "services/TaskService.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>

#include <optional>
#include <utility>

using smartmate::model::Task;
using smartmate::model::TaskDraft;
using smartmate::model::TaskError;
using smartmate::model::TaskPriority;
using smartmate::model::TaskService;
using smartmate::model::TaskStatus;
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

} // namespace

// 在不启动 SQLite 或 QML 的情况下验证任务规则、错误映射和成功写入通知。
class TaskServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void listsAndFindsTasks();
    void createsTaskWithEveryField();
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
};

void TaskServiceTest::listsAndFindsTasks()
{
    const Task first = storedTask(TaskStatus::InProgress, std::nullopt,
                                  QStringLiteral("Understand MVVM"));
    const Task second = storedTask(TaskStatus::Todo, std::nullopt,
                                   QStringLiteral("Build SmartMate"));
    FakeTaskRepository repository{{first, second}};
    const TaskService service{repository};

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
    TaskService service{repository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};
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

void TaskServiceTest::normalizesOmittedDescriptionToEmptyText()
{
    FakeTaskRepository repository;
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    const TaskService service{repository};
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
            TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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

QTEST_APPLESS_MAIN(TaskServiceTest)

#include "tst_TaskService.moc"
